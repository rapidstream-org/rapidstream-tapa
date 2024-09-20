// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <iostream>
#include <vector>
#include "mmio.h"

using std::cout;
using std::endl;
using std::max;
using std::min;
using std::vector;

#ifndef SPARSE_HELPER
#define SPARSE_HELPER

struct rcv {
  int r;
  int c;
  float v;
};

enum MATRIX_FORMAT { CSR, CSC };

struct edge {
  int col;
  int row;
  float attr;

  edge(int d = -1, int s = -1, float v = 0.0) : col(d), row(s), attr(v) {}

  edge& operator=(const edge& rhs) {
    col = rhs.col;
    row = rhs.row;
    attr = rhs.attr;
    return *this;
  }
};

int cmp_by_row_column(const void* aa, const void* bb) {
  rcv* a = (rcv*)aa;
  rcv* b = (rcv*)bb;
  if (a->r > b->r) return +1;
  if (a->r < b->r) return -1;

  if (a->c > b->c) return +1;
  if (a->c < b->c) return -1;

  return 0;
}

int cmp_by_column_row(const void* aa, const void* bb) {
  rcv* a = (rcv*)aa;
  rcv* b = (rcv*)bb;

  if (a->c > b->c) return +1;
  if (a->c < b->c) return -1;

  if (a->r > b->r) return +1;
  if (a->r < b->r) return -1;

  return 0;
}

void sort_by_fn(int nnz_s, vector<int>& cooRowIndex, vector<int>& cooColIndex,
                vector<float>& cooVal,
                int (*cmp_func)(const void*, const void*)) {
  rcv* rcv_arr = new rcv[nnz_s];

  for (int i = 0; i < nnz_s; ++i) {
    rcv_arr[i].r = cooRowIndex[i];
    rcv_arr[i].c = cooColIndex[i];
    rcv_arr[i].v = cooVal[i];
  }

  qsort(rcv_arr, nnz_s, sizeof(rcv), cmp_func);

  for (int i = 0; i < nnz_s; ++i) {
    cooRowIndex[i] = rcv_arr[i].r;
    cooColIndex[i] = rcv_arr[i].c;
    cooVal[i] = rcv_arr[i].v;
  }

  delete[] rcv_arr;
}

void mm_init_read(FILE* f, char* filename, MM_typecode& matcode, int& m, int& n,
                  int& nnz) {
  // if ((f = fopen(filename, "r")) == NULL) {
  //         cout << "Could not open " << filename << endl;
  //         return 1;
  // }

  if (mm_read_banner(f, &matcode) != 0) {
    cout << "Could not process Matrix Market banner for " << filename << endl;
    exit(1);
  }

  int ret_code;
  if ((ret_code = mm_read_mtx_crd_size(f, &m, &n, &nnz)) != 0) {
    cout << "Could not read Matrix Market format for " << filename << endl;
    exit(1);
  }
}

void load_S_matrix(FILE* f_A, int nnz_mmio, int& nnz, vector<int>& cooRowIndex,
                   vector<int>& cooColIndex, vector<float>& cooVal,
                   MM_typecode& matcode) {
  if (mm_is_complex(matcode)) {
    cout << "Redaing in a complex matrix, not supported yet!" << endl;
    exit(1);
  }

  if (!mm_is_symmetric(matcode)) {
    cout << "It's an NS matrix.\n";
  } else {
    cout << "It's an S matrix.\n";
  }

  int r_idx, c_idx;
  float value;
  int idx = 0;

  for (int i = 0; i < nnz_mmio; ++i) {
    if (mm_is_pattern(matcode)) {
      fscanf(f_A, "%d %d\n", &r_idx, &c_idx);
      value = 1.0;
    } else {
      fscanf(f_A, "%d %d %f\n", &r_idx, &c_idx, &value);
    }

    unsigned int* tmpPointer_v = reinterpret_cast<unsigned int*>(&value);
    ;
    unsigned int uint_v = *tmpPointer_v;
    if (uint_v != 0) {
      if (r_idx < 1 || c_idx < 1) {  // report error
        cout << "idx = " << idx << " [" << r_idx - 1 << ", " << c_idx - 1
             << "] = " << value << endl;
        exit(1);
      }

      cooRowIndex[idx] = r_idx - 1;
      cooColIndex[idx] = c_idx - 1;
      cooVal[idx] = value;
      idx++;

      if (mm_is_symmetric(matcode)) {
        if (r_idx != c_idx) {
          cooRowIndex[idx] = c_idx - 1;
          cooColIndex[idx] = r_idx - 1;
          cooVal[idx] = value;
          idx++;
        }
      }
    }
  }
  nnz = idx;
}

void read_suitsparse_matrix(char* filename_A, vector<int>& elePtr,
                            vector<int>& eleIndex, vector<float>& eleVal,
                            int& M, int& K, int& nnz,
                            const MATRIX_FORMAT mf = CSR) {
  int nnz_mmio;
  MM_typecode matcode;
  FILE* f_A;

  if ((f_A = fopen(filename_A, "r")) == NULL) {
    cout << "Could not open " << filename_A << endl;
    exit(1);
  }

  mm_init_read(f_A, filename_A, matcode, M, K, nnz_mmio);

  if (!mm_is_coordinate(matcode)) {
    cout << "The input matrix file " << filename_A
         << "is not a coordinate file!" << endl;
    exit(1);
  }

  int nnz_alloc = (mm_is_symmetric(matcode)) ? (nnz_mmio * 2) : nnz_mmio;
  cout << "Matrix A -- #row: " << M << " #col: " << K << endl;

  vector<int> cooRowIndex(nnz_alloc);
  vector<int> cooColIndex(nnz_alloc);
  // eleIndex.resize(nnz_alloc);
  eleVal.resize(nnz_alloc);

  cout << "Loading input matrix A from " << filename_A << "\n";

  load_S_matrix(f_A, nnz_mmio, nnz, cooRowIndex, cooColIndex, eleVal, matcode);

  fclose(f_A);

  if (mf == CSR) {
    sort_by_fn(nnz, cooRowIndex, cooColIndex, eleVal, cmp_by_row_column);
  } else if (mf == CSC) {
    sort_by_fn(nnz, cooRowIndex, cooColIndex, eleVal, cmp_by_column_row);
  } else {
    cout << "Unknow format!\n";
    exit(1);
  }

  // convert to CSR/CSC format
  int M_K = (mf == CSR) ? M : K;
  elePtr.resize(M_K + 1);
  vector<int> counter(M_K, 0);

  if (mf == CSR) {
    for (int i = 0; i < nnz; i++) {
      counter[cooRowIndex[i]]++;
    }
  } else if (mf == CSC) {
    for (int i = 0; i < nnz; i++) {
      counter[cooColIndex[i]]++;
    }
  } else {
    cout << "Unknow format!\n";
    exit(1);
  }

  int t = 0;
  for (int i = 0; i < M_K; i++) {
    t += counter[i];
  }

  elePtr[0] = 0;
  for (int i = 1; i <= M_K; i++) {
    elePtr[i] = elePtr[i - 1] + counter[i - 1];
  }

  eleIndex.resize(nnz);
  if (mf == CSR) {
    for (int i = 0; i < nnz; ++i) {
      eleIndex[i] = cooColIndex[i];
    }
  } else if (mf == CSC) {
    for (int i = 0; i < nnz; ++i) {
      eleIndex[i] = cooRowIndex[i];
    }
  }

  if (mm_is_symmetric(matcode)) {
    // eleIndex.resize(nnz);
    eleVal.resize(nnz);
  }
}

void cpu_spmm_CSR(const int M, const int N, const int K, const int NNZ,
                  const float ALPHA, const vector<int>& CSRRowPtr,
                  const vector<int>& CSRColIndex, const vector<float>& CSRVal,
                  const vector<float>& mat_B, const float BETA,
                  vector<float>& mat_C) {
  // A: sparse matrix, M x K
  // B: dense matrix, K x N
  // C: dense matrix, M x N
  // output C = ALPHA * mat_A * mat_B + BETA * mat_C
  // dense matrices: column major

  for (int i = 0; i < M; ++i) {
    vector<float> psum(N, 0.0);
    for (int j = CSRRowPtr[i]; j < CSRRowPtr[i + 1]; ++j) {
      for (int nn = 0; nn < N; ++nn) {
        psum[nn] += CSRVal[j] * mat_B[CSRColIndex[j] + K * nn];
      }
    }
    for (int nn = 0; nn < N; ++nn) {
      mat_C[i + M * nn] = ALPHA * psum[nn] + BETA * mat_C[i + M * nn];
    }
  }
}

void generate_edge_list_for_one_PE(const vector<edge>& tmp_edge_list,
                                   vector<edge>& edge_list,
                                   const int base_col_index, const int i_start,
                                   const int NUM_Row, const int NUM_PE,
                                   const int DEP_DIST_LOAD_STORE = 10) {
  edge e_empty = {-1, -1, 0.0};
  // vector<edge> scheduled_edges(NUM_Row);
  // std::fill(scheduled_edges.begin(), scheduled_edges.end(), e_empty);
  vector<edge> scheduled_edges;

  // const int DEP_DIST_LOAD_STORE = 7;

  vector<int> cycles_rows(NUM_Row, -DEP_DIST_LOAD_STORE);
  int e_dst, e_src;
  float e_attr;
  for (unsigned int pp = 0; pp < tmp_edge_list.size(); ++pp) {
    e_src = tmp_edge_list[pp].col - base_col_index;
    e_dst = tmp_edge_list[pp].row / NUM_PE;  // e_dst = tmp_edge_list[pp].row;
    e_attr = tmp_edge_list[pp].attr;
    auto cycle = cycles_rows[e_dst] + DEP_DIST_LOAD_STORE;

    bool taken = true;
    while (taken) {
      if (cycle >= ((int)scheduled_edges.size())) {
        scheduled_edges.resize(cycle + 1, e_empty);
      }
      auto e = scheduled_edges[cycle];
      if (e.row != -1)
        cycle++;
      else
        taken = false;
    }
    scheduled_edges[cycle].col = e_src;
    scheduled_edges[cycle].row = e_dst;
    scheduled_edges[cycle].attr = e_attr;
    cycles_rows[e_dst] = cycle;
  }

  int scheduled_edges_size = scheduled_edges.size();
  if (scheduled_edges_size > 0) {
    // edge_list.resize(i_start + scheduled_edges_size + DEP_DIST_LOAD_STORE -
    // 1, e_empty);
    edge_list.resize(i_start + scheduled_edges_size, e_empty);
    for (int i = 0; i < scheduled_edges_size; ++i) {
      edge_list[i + i_start] = scheduled_edges[i];
    }
  }
}

void generate_edge_list_for_all_PEs(const vector<int>& CSCColPtr,
                                    const vector<int>& CSCRowIndex,
                                    const vector<float>& CSCVal,
                                    const int NUM_PE, const int NUM_ROW,
                                    const int NUM_COLUMN, const int WINDOE_SIZE,
                                    vector<vector<edge> >& edge_list_pes,
                                    vector<int>& edge_list_ptr,
                                    const int DEP_DIST_LOAD_STORE = 10) {
  /* -------------------------- */

  edge_list_pes.resize(NUM_PE);
  edge_list_ptr.resize((NUM_COLUMN + WINDOE_SIZE - 1) / WINDOE_SIZE + 1, 0);

  vector<vector<edge> > tmp_edge_list_pes(NUM_PE);
  for (int i = 0; i < (NUM_COLUMN + WINDOE_SIZE - 1) / WINDOE_SIZE; ++i) {
    for (int p = 0; p < NUM_PE; ++p) {
      tmp_edge_list_pes[p].resize(0);
    }

    // fill tmp_edge_lsit_pes
    for (int col = WINDOE_SIZE * i;
         col < min(WINDOE_SIZE * (i + 1), NUM_COLUMN); ++col) {
      for (int j = CSCColPtr[col]; j < CSCColPtr[col + 1]; ++j) {
        int p = CSCRowIndex[j] % NUM_PE;
        int pos = tmp_edge_list_pes[p].size();
        tmp_edge_list_pes[p].resize(pos + 1);
        tmp_edge_list_pes[p][pos] = edge(col, CSCRowIndex[j], CSCVal[j]);
      }
    }

    // form the scheduled edge list for each PE
    for (int p = 0; p < NUM_PE; ++p) {
      int i_start = edge_list_pes[p].size();
      int base_col_index = i * WINDOE_SIZE;
      generate_edge_list_for_one_PE(tmp_edge_list_pes[p], edge_list_pes[p],
                                    base_col_index, i_start, NUM_ROW, NUM_PE,
                                    DEP_DIST_LOAD_STORE);
    }

    // insert bubules to align edge list
    int max_len = 0;
    for (int p = 0; p < NUM_PE; ++p) {
      max_len = max((int)edge_list_pes[p].size(), max_len);
    }
    for (int p = 0; p < NUM_PE; ++p) {
      edge_list_pes[p].resize(max_len, edge(-1, -1, 0.0));
    }

    // pointer
    edge_list_ptr[i + 1] = max_len;
  }
}

void edge_list_64bit(
    const vector<vector<edge> >& edge_list_pes,
    const vector<int>& edge_list_ptr,
    vector<vector<unsigned long, tapa::aligned_allocator<unsigned long> > >&
        sparse_A_fpga_vec,
    const int NUM_CH_SPARSE = 8) {
  int sparse_A_fpga_column_size =
      8 * edge_list_ptr[edge_list_ptr.size() - 1] * 4 / 4;
  int sparse_A_fpga_chunk_size =
      ((sparse_A_fpga_column_size + 511) / 512) * 512;

  for (int cc = 0; cc < NUM_CH_SPARSE; ++cc) {
    sparse_A_fpga_vec[cc].resize(sparse_A_fpga_chunk_size, 0);
  }

  // col(12 bits) + row (20 bits) + value (32 bits)
  // ->
  // col(14 bits) + row (18 bits) + value (32 bits)
  for (int i = 0; i < edge_list_ptr[edge_list_ptr.size() - 1]; ++i) {
    for (int cc = 0; cc < NUM_CH_SPARSE; ++cc) {
      for (int j = 0; j < 8; ++j) {
        edge e = edge_list_pes[j + cc * 8][i];
        unsigned long x = 0;
        if (e.row == -1) {
          x = 0x3FFFF;  // 0xFFFFF; //x = 0x3FFFFF;
          x = x << 32;
        } else {
          unsigned long x_col = e.col;
          x_col = (x_col & 0x3FFF)
                  << (32 + 18);  // x_col = (x_col & 0xFFF) << (32 + 20);
                                 // //x_col = (x_col & 0x3FF) << (32 + 22);
          unsigned long x_row = e.row;
          x_row = (x_row & 0x3FFFF)
                  << 32;  // x_row = (x_row & 0xFFFFF) << 32; //x_row = (x_row &
                          // 0x3FFFFF) << 32;

          float x_float = e.attr;
          // float x_float = 1.0;
          unsigned int x_float_in_int = *((unsigned int*)(&x_float));
          unsigned long x_float_val_64 = ((unsigned long)x_float_in_int);
          x_float_val_64 = x_float_val_64 & 0xFFFFFFFF;

          x = x_col | x_row | x_float_val_64;
        }
        /* change 07-07-2021 START*/
        /* change 07-07-2021 ORIGINAL START*/
        // sparse_A_fpga_vec[cc][j + i * 8] = x;
        /* change 07-07-2021 ORIGINAL END*/
        /* change 07-07-2021 Change as follows*/
        if (NUM_CH_SPARSE * 8 <= 16) {
          sparse_A_fpga_vec[cc][j + i * 8] = x;
        } else if (NUM_CH_SPARSE == 8) {
          /*
          const int pe_idx = j + cc * 8;
          const int pix_m16 = pe_idx % 16;
          const int ch_idx = pix_m16 / 2;
          const int j_new = pe_idx % 2 + (pe_idx / 16) * 2;
          */
          const int pe_idx = j + cc * 8;
          const int ch_idx = j;
          const int idx_in8 = pe_idx / 8;
          // const int j_new = (idx_in8 % 4) * 2 + idx_in8 / 4; // (0,4), (1,5),
          // (2,6), (3,7)
          const int j_new =
              ((idx_in8 & 0x1) > 0) * 4 + ((idx_in8 & 0x2) > 0) * 2 +
              ((idx_in8 & 0x4) > 0) * 1;  // (0,4), (2,6), (1,5), (3,7)

          sparse_A_fpga_vec[ch_idx][j_new + i * 8] = x;
        } else if (NUM_CH_SPARSE == 4) {
          int pe_idx = j + cc * 8;
          sparse_A_fpga_vec[(pe_idx / 4) % 4]
                           [pe_idx % 4 + (pe_idx / 16) * 4 + i * 8] = x;
        }
        /* change 07-07-2021 END*/
      }
    }
  }
}

void CSC_2_CSR(int M, int K, int NNZ, const vector<int>& csc_col_Ptr,
               const vector<int>& csc_row_Index, const vector<float>& cscVal,
               vector<int>& csr_row_Ptr, vector<int>& csr_col_Index,
               vector<float>& csrVal) {
  csr_row_Ptr.resize(M + 1, 0);
  csrVal.resize(NNZ, 0.0);
  csr_col_Index.resize(NNZ, 0);

  for (int i = 0; i < NNZ; ++i) {
    csr_row_Ptr[csc_row_Index[i] + 1]++;
  }

  for (int i = 0; i < M; ++i) {
    csr_row_Ptr[i + 1] += csr_row_Ptr[i];
  }

  vector<int> row_nz(M, 0);
  for (int i = 0; i < K; ++i) {
    for (int j = csc_col_Ptr[i]; j < csc_col_Ptr[i + 1]; ++j) {
      int r = csc_row_Index[j];
      int c = i;
      float v = cscVal[j];

      int pos = csr_row_Ptr[r] + row_nz[r];
      csrVal[pos] = v;
      csr_col_Index[pos] = c;
      row_nz[r]++;
    }
  }
}

#endif
