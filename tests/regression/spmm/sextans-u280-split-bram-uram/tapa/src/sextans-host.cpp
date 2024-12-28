// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <ap_int.h>
#include <tapa.h>

#include "mmio.h"
#include "sextans.h"
#include "sparse_helper.h"

using std::cout;
using std::endl;
using std::ifstream;
using std::max;
using std::min;
using std::string;
using std::vector;

template <typename T>
using aligned_vector = std::vector<T, tapa::aligned_allocator<T>>;

void Sextans(tapa::mmap<int> edge_list_ptr,
             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,
             tapa::mmaps<float_v16, NUM_CH_B> mat_B_ch,
             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch_in,
             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch, const int NUM_ITE,
             const int NUM_A_LEN, const int M, const int K, const int P_N,
             const int alpha_u, int beta_u);

int main(int argc, char** argv) {
  printf("start host\n");

  float ALPHA = 0.85;
  float BETA = -2.06;
  int rp_time = 1;

  if (argc == 5) {
    ALPHA = atof(argv[3]);
    BETA = atof(argv[4]);
  } else if (argc == 4) {
    rp_time = atoi(argv[3]);
  } else if (argc != 3) {
    cout << "Usage: " << argv[0]
         << " [matrix A file] [N] [rp_time] [alpha] [beta]" << std::endl;
    return EXIT_FAILURE;
  }

  char* filename_A = argv[1];
  int N = tapa::round_up<8>(atoi(argv[2]));

  cout << "N = " << N << "\n";
  cout << "alpha = " << ALPHA << "\n";
  cout << "beta = " << BETA << "\n";

  int M, K, nnz;
  vector<int> CSCColPtr;
  vector<int> CSCRowIndex;
  vector<float> CSCVal;
  vector<int> CSRRowPtr;
  vector<int> CSRColIndex;
  vector<float> CSRVal;

  cout << "Reading sparse A matrix...";

  read_suitsparse_matrix(filename_A, CSCColPtr, CSCRowIndex, CSCVal, M, K, nnz,
                         CSC);

  CSC_2_CSR(M, K, nnz, CSCColPtr, CSCRowIndex, CSCVal, CSRRowPtr, CSRColIndex,
            CSRVal);

  cout << "done\n";

  cout << "Matrix size: \n";
  cout << "A: sparse matrix, " << M << " x " << K << ". NNZ = " << nnz << "\n";
  cout << "B: dense matrix, " << K << " x " << N << "\n";
  cout << "C: dense matrix, " << M << " x " << N << "\n";

  // initiate matrix B and matrix C
  vector<float> mat_B_cpu, mat_C_cpu;
  mat_B_cpu.resize(K * N, 0.0);
  mat_C_cpu.resize(M * N, 0.0);

  cout << "Generating dense matirx B ...";
  for (int nn = 0; nn < N; ++nn) {
    for (int kk = 0; kk < K; ++kk) {
      mat_B_cpu[kk + K * nn] = 1.0;  //(1.0 + kk) + 0.1 * (1.0 + nn); //100.0 *
                                     //(kk + 1)  + 1.0 * (nn + 1);// / K / N;
    }
  }

  cout << "Generating dense matirx C ...";
  for (int nn = 0; nn < N; ++nn) {
    for (int mm = 0; mm < M; ++mm) {
      mat_C_cpu[mm + M * nn] = 1.0 * (mm + 1) * (nn + 1) / M / N;
    }
  }
  cout << "done\n";

  cout << "Preparing sparse A for FPGA ...";

  vector<vector<edge>> edge_list_pes;
  vector<int> edge_list_ptr;

  generate_edge_list_for_all_PEs(
      CSCColPtr,             // const vector<int> & CSCColPtr,
      CSCRowIndex,           // const vector<int> & CSCRowIndex,
      CSCVal,                // const vector<float> & CSCVal,
      NUM_CH_SPARSE * 8,     // const int NUM_PE,
      M,                     // const int NUM_ROW,
      K,                     // const int NUM_COLUMN,
      WINDOW_SIZE,           // const int WINDOW_SIZE,
      edge_list_pes,         // vector<vector<edge> > & edge_list_pes,
      edge_list_ptr,         // vector<int> & edge_list_ptr,
      DEP_DIST_LOAD_STORE);  // const int DEP_DIST_LOAD_STORE = 10)

  aligned_vector<int> edge_list_ptr_fpga;
  int edge_list_ptr_fpga_size = ((edge_list_ptr.size() + 15) / 16) * 16;
  int edge_list_ptr_fpga_chunk_size =
      ((edge_list_ptr_fpga_size + 1023) / 1024) * 1024;
  edge_list_ptr_fpga.resize(edge_list_ptr_fpga_chunk_size, 0);
  for (int i = 0; i < edge_list_ptr.size(); ++i) {
    edge_list_ptr_fpga[i] = edge_list_ptr[i];
  }

  vector<aligned_vector<unsigned long>> sparse_A_fpga_vec(NUM_CH_SPARSE);
  int sparse_A_fpga_column_size =
      8 * edge_list_ptr[edge_list_ptr.size() - 1] * 4 / 4;
  int sparse_A_fpga_chunk_size =
      ((sparse_A_fpga_column_size + 511) / 512) * 512;

  edge_list_64bit(edge_list_pes, edge_list_ptr, sparse_A_fpga_vec,
                  NUM_CH_SPARSE);

  cout << "done\n";

  cout << "Preparing dense B for FPGA ...";

  vector<aligned_vector<float>> mat_B_fpga_vec(NUM_CH_B);
  // int mat_B_fpga_column_size = ((K * N + 16 * NUM_CH_B - 1) / (16 *
  // NUM_CH_B)) * 16 * NUM_CH_B;
  //  07-21 int mat_B_fpga_column_size = ((K + 16 - 1) / 16) * 16;
  int mat_B_fpga_column_size;
  if (NUM_CH_B == 8) {
    mat_B_fpga_column_size = ((K + 16 - 1) / 16) * 16;
  } else if (NUM_CH_B == 4) {
    mat_B_fpga_column_size = ((K + 8 - 1) / 8) * 8 * 2;
  }
  int mat_B_fpga_chunk_size =
      ((mat_B_fpga_column_size * (N / 8) + 1023) / 1024) * 1024;

  for (int cc = 0; cc < NUM_CH_B; ++cc) {
    mat_B_fpga_vec[cc].resize(mat_B_fpga_chunk_size, 0.0);
  }
  for (int nn = 0; nn < N; ++nn) {
    for (int kk = 0; kk < K; ++kk) {
      if (NUM_CH_B == 4) {
        int pos = (kk / 8) * 16 + (nn % 2) * 8 + kk % 8 +
                  mat_B_fpga_column_size * (nn / 8);
        mat_B_fpga_vec[(nn / 2) % 4][pos] = mat_B_cpu[kk + K * nn];
      } else if (NUM_CH_B == 8) {
        int pos = kk + mat_B_fpga_column_size * (nn / 8);
        mat_B_fpga_vec[nn % 8][pos] = mat_B_cpu[kk + K * nn];
      }
    }
  }

  cout << "Preparing dense C for FPGA ...";
  vector<aligned_vector<float>> mat_C_fpga_in(8);
  int mat_C_fpga_column_size = ((M + 16 - 1) / 16) * 16;
  int mat_C_fpga_chunk_size =
      ((mat_C_fpga_column_size * (N / 8) + 1023) / 1024) * 1024;

  for (int nn = 0; nn < 8; ++nn) {
    mat_C_fpga_in[nn].resize(mat_C_fpga_chunk_size, 0.0);
  }

  for (int nn = 0; nn < N; ++nn) {
    for (int mm = 0; mm < M; ++mm) {
      // int pos = mat_C_fpga_column_size * (nn / 8) + mm;
      // mat_C_fpga_in[nn % 8][pos] = mat_C_cpu[mm + M * nn];
      int pos = mat_C_fpga_column_size * (nn / 8) + (mm / 8) * 8 + nn % 8;
      mat_C_fpga_in[mm % 8][pos] = mat_C_cpu[mm + M * nn];
    }
  }

  vector<aligned_vector<float>> mat_C_fpga_vec(NUM_CH_C);
  // int mat_C_fpga_column_size = ((M * N + 16 * NUM_CH_C - 1) / (16 *
  // NUM_CH_C)) * 16 * NUM_CH_C;

  for (int cc = 0; cc < NUM_CH_C; ++cc) {
    mat_C_fpga_vec[cc].resize(mat_C_fpga_chunk_size, 0.0);
  }

  cout << "done\n";

  cout << "Run spmm on cpu...";
  auto start_cpu = std::chrono::steady_clock::now();
  cpu_spmm_CSR(M, N, K, nnz, ALPHA, CSRRowPtr, CSRColIndex, CSRVal, mat_B_cpu,
               BETA, mat_C_cpu);
  auto end_cpu = std::chrono::steady_clock::now();
  double time_cpu =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_cpu - start_cpu)
          .count();
  time_cpu *= 1e-9;
  cout << "done (" << time_cpu * 1000 << " msec)\n";
  cout << "CPU GFLOPS: " << 2.0f * (nnz + M) * N / 1000000000 / time_cpu
       << "\n";

  int MAX_SIZE_edge_LIST_PTR = edge_list_ptr.size() - 1;
  int MAX_LEN_edge_PTR = edge_list_ptr[MAX_SIZE_edge_LIST_PTR];
  int para_N = (rp_time << 16) | N;

  int* tmpPointer_v;
  tmpPointer_v = (int*)&ALPHA;
  int alpha_int = *tmpPointer_v;
  tmpPointer_v = (int*)&BETA;
  int beta_int = *tmpPointer_v;

  std::string bitstream;
  if (const auto bitstream_ptr = getenv("TAPAB")) {
    bitstream = bitstream_ptr;
  }

  cout << "launch kernel\n";
  double time_taken = tapa::invoke(
      Sextans, bitstream, tapa::read_only_mmap<int>(edge_list_ptr_fpga),
      tapa::read_only_mmaps<unsigned long, NUM_CH_SPARSE>(sparse_A_fpga_vec)
          .reinterpret<ap_uint<512>>(),
      tapa::read_only_mmaps<float, NUM_CH_B>(mat_B_fpga_vec)
          .reinterpret<float_v16>(),
      tapa::read_only_mmaps<float, NUM_CH_C>(mat_C_fpga_in)
          .reinterpret<float_v16>(),
      tapa::write_only_mmaps<float, NUM_CH_C>(mat_C_fpga_vec)
          .reinterpret<float_v16>(),
      MAX_SIZE_edge_LIST_PTR, MAX_LEN_edge_PTR, M, K, para_N, alpha_int,
      beta_int);
  time_taken *= (1e-9 / rp_time);
  printf("Kernel time is %f ms\n", time_taken * 1000);

  float gflops = 2.0 * N * (nnz + M) / 1e9  // convert to GB
                 / time_taken               // total time in second
      ;
  printf("GFLOPS:%f \n", gflops);

  int mismatch_cnt = 0;

  for (int nn = 0; nn < N; ++nn) {
    for (int mm = 0; mm < M; ++mm) {
      float v_cpu = mat_C_cpu[mm + nn * M];
      // int pos = mat_C_fpga_column_size * (nn / 8) + mm;
      // float v_fpga = mat_C_fpga_vec[nn % 8][pos];
      int pos = mat_C_fpga_column_size * (nn / 8) + (mm / 8) * 8 + nn % 8;
      float v_fpga = mat_C_fpga_vec[mm % 8][pos];

      float dff = fabs(v_cpu - v_fpga);
      float x = min(fabs(v_cpu), fabs(v_fpga)) + 1e-4;
      if (dff / x > 1e-4) {
        mismatch_cnt++;
      }
      // cout << "n = " << nn << ", m = " << mm << ", cpu = " << v_cpu << ",
      // fpga = " << v_fpga << endl;
    }
  }

  float diffpercent = 100.0 * mismatch_cnt / M / N;
  bool pass = diffpercent < 2.0;

  if (pass) {
    cout << "Success!\n";
  } else {
    cout << "Failed.\n";
  }
  printf("num_mismatch = %d, percent = %.2f%%\n", mismatch_cnt, diffpercent);

  return EXIT_SUCCESS;
}
