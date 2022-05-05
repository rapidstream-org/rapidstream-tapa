/**********
 Copyright (c) 2018, Xilinx, Inc.
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********/


#include "xcl2.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <ap_int.h>
#include <cstdlib>
#include <chrono>
#include <iostream>

#include "mmio.h"
#include "sparse_helper.h"

//#define DEBUG_PRINT

using std::cout;
using std::endl;
using std::ifstream;
using std::string;
using std::vector;
using std::min;
using std::max;

const int NUM_CH_SPARSE = 32;
const int NUM_WINDOW_SIZE = 8192;

int main(int argc, char **argv) {
    printf("start host\n");
    
    float ALPHA = 0.85;
    float BETA = -2.06;
    int rp_time = 1;
    
    if (argc == 3) {
    } else if (argc == 5) {
        ALPHA = atof(argv[3]);
        BETA = atof(argv[4]);
    } else if (argc == 6) {
        ALPHA = atof(argv[3]);
        BETA = atof(argv[4]);
        rp_time = atoi(argv[5]);
    }
    else {
        cout << "Usage: " << argv[0] << " <XCLBIN File> [matrix A file] [ALPHA] [BETA] [rp_time]" << std::endl;
        return EXIT_FAILURE;
    }
    
    char * filename_A = argv[2];
    
    cout << "alpha = "  << ALPHA << "\n";
    cout << "beta = "  << BETA << "\n";
    
    int M, K, nnz;
    vector<int> CSRRowPtr;
    vector<int> CSRColIndex;
    vector<float> CSRVal;
    
    cout << "Reading sparse A matrix...";
    read_suitsparse_matrix(filename_A,
                           CSRRowPtr,
                           CSRColIndex,
                           CSRVal,
                           M,
                           K,
                           nnz,
                           CSR);
    
    int M_CSC, K_CSC, nnz_CSC;
    vector<int> CSCColPtr;
    vector<int> CSCRowIndex;
    vector<float> CSCVal;
    
    read_suitsparse_matrix(filename_A,
                           CSCColPtr,
                           CSCRowIndex,
                           CSCVal,
                           M_CSC,
                           K_CSC,
                           nnz_CSC,
                           CSC);
    cout <<  "done\n";
    
    cout << "Matrix size: \n";
    cout << "A: sparse matrix, " << M << " x " << K << ". NNZ = " << nnz <<  "\n";
    
    // initiate vec X and vec Y
    vector<float> vec_X_cpu, vec_Y_cpu;
    vec_X_cpu.resize(K, 0.0);
    vec_Y_cpu.resize(M, 0.0);
    
    cout << "Generating vector X ...";
    for (int kk = 0; kk < K; ++kk) {
        vec_X_cpu[kk] = 0.01 * (kk + 1);
    }
    
    cout << "Generating vector Y ...";
    for (int mm = 0; mm < M; ++mm) {
        vec_Y_cpu[mm] = -0.01 * (mm + 1);
    }
    
    cout <<  "done\n";
    
    //generate for fpga
    cout << "Preparing sparse A for FPGA ...";
    
    vector<vector<edge> > edge_list_pes;
    vector<unsigned int> edge_list_ptr;
    
    int NUM_PE;
    int WINDOE_SIZE;
    generate_edge_list_for_all_PEs(CSCColPtr, //const vector<int> & CSCColPtr,
                                   CSCRowIndex, //const vector<int> & CSCRowIndex,
                                   CSCVal, //const vector<float> & CSCVal,
                                   NUM_PE=NUM_CH_SPARSE*8, //const int NUM_PE,
                                   M_CSC, //const int NUM_ROW,
                                   K_CSC, //const int NUM_COLUMN,
                                   WINDOE_SIZE=NUM_WINDOW_SIZE, //const int WINDOE_SIZE,
                                   edge_list_pes, //vector<vector<edge> > & edge_list_pes,
                                   edge_list_ptr, //vector<int> & edge_list_ptr,
                                   10
                                   ); //const int DEP_DIST_LOAD_STORE = 10)
    
    vector<unsigned int, aligned_allocator<unsigned int> > edge_list_ptr_fpga;
    int edge_list_ptr_fpga_size = ((edge_list_ptr.size() + 15) / 16) * 16;
    int edge_list_ptr_fpga_chunk_size = ((edge_list_ptr_fpga_size + 1023)/1024) * 1024;
    edge_list_ptr_fpga.resize(edge_list_ptr_fpga_chunk_size, 0);
    for (unsigned int i = 0; i < edge_list_ptr.size(); ++i) {
        edge_list_ptr_fpga[i] = edge_list_ptr[i];
    }
    
#ifdef DEBUG_PRINT
    cout << "\n ############## DEBUG PRINT ################# \n";
    cout << "edge_list_ptr_fpga_size = " << edge_list_ptr_fpga_size << endl;
    cout << "edge_list_ptr_fpga_chunk_size = " << edge_list_ptr_fpga_chunk_size << endl;
    cout << "edge_list_ptr_fpga  = \n";
    for (unsigned int i = 0; i < edge_list_ptr.size(); ++i) {
        cout << edge_list_ptr_fpga[i] << endl;
    }
    cout << endl;
#endif
    
    vector<vector<unsigned long, aligned_allocator<unsigned long> > > sparse_A_fpga_vec(NUM_CH_SPARSE);
    int sparse_A_fpga_column_size = 8 * edge_list_ptr[edge_list_ptr.size()-1] * 4 / 4;
    //int sparse_A_fpga_chunk_size = ((sparse_A_fpga_column_size + 511)/512) * 512;
    
    edge_list_64bit(edge_list_pes,
                    edge_list_ptr,
                    sparse_A_fpga_vec,
                    NUM_CH_SPARSE);
    
#ifdef DEBUG_PRINT
    cout << "\n ############## DEBUG PRINT ################# \n";
    cout << "sparse_A_fpga_column_size = " << sparse_A_fpga_column_size << endl;
    cout << "sparse_A_fpga_chunk_size = " << sparse_A_fpga_chunk_size << endl;
    cout << endl;
#endif
    
    cout <<  "done\n";
    
    cout << "Preparing vector X for FPGA ...";
    
    int vec_X_fpga_column_size = ((K + 16 - 1) / 16) * 16;
    int vec_X_fpga_chunk_size = ((vec_X_fpga_column_size + 1023)/1024) * 1024;
    vector<float, aligned_allocator<float> > vec_X_fpga(vec_X_fpga_chunk_size, 0.0);
    
    for (int kk = 0; kk < K; ++kk) {
        vec_X_fpga[kk] = vec_X_cpu[kk];
    }
    
    cout << "Preparing vector Y for FPGA ...";
    int vec_Y_fpga_column_size = ((M + 16 - 1) / 16) * 16;
    int vec_Y_fpga_chunk_size = ((vec_Y_fpga_column_size + 1023)/1024) * 1024;
    vector<float, aligned_allocator<float> > vec_Y_fpga(vec_Y_fpga_chunk_size, 0.0);
    vector<float, aligned_allocator<float> > vec_Y_out_fpga(vec_Y_fpga_chunk_size, 0.0);
    
    for (int mm = 0; mm < M; ++mm) {
        vec_Y_fpga[mm] = vec_Y_cpu[mm];
    }
    
    cout <<  "done\n";
    
    cout << "Run spmv on cpu...";
    auto start_cpu = std::chrono::steady_clock::now();
    cpu_spmv_CSR(M, K, nnz, ALPHA,
                 CSRRowPtr,
                 CSRColIndex,
                 CSRVal,
                 vec_X_cpu,
                 BETA,
                 vec_Y_cpu);
    auto end_cpu = std::chrono::steady_clock::now();
    double time_cpu = std::chrono::duration_cast<std::chrono::nanoseconds>(end_cpu - start_cpu).count();
    time_cpu *= 1e-9;
    cout << "done (" << time_cpu*1000 << " msec)\n";
    cout <<"CPU GFLOPS: " << 2.0 * (nnz + M)/1000000000/time_cpu << "\n";
    
    std::string binaryFile = argv[1];
    cl_int err;
    cl::Context context;
    cl::Kernel krnl_serpens;
    cl::CommandQueue q;
    
    // OPENCL HOST CODE AREA START
    auto devices = xcl::get_xil_devices();
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    int valid_device = 0;
    printf("Num of devices : %d\n", (int)devices.size());
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context({device}, NULL, NULL, NULL, &err));
        OCL_CHECK(err,
                  q = cl::CommandQueue(
                                       context, {device}, CL_QUEUE_PROFILING_ENABLE, &err));
        
        //if( device.getInfo<CL_DEVICE_NAME>() != "xilinx_u50_gen3x16_xdma_201920_3" ){
        if( device.getInfo<CL_DEVICE_NAME>() != "xilinx_u280_xdma_201920_3" ){
            cout  << "Skipping device : " << device.getInfo<CL_DEVICE_NAME>() << endl;
            continue;
        }
        cout << "Trying to program device[" << i
        << "]: " << device.getInfo<CL_DEVICE_NAME>() << endl;
        OCL_CHECK(err,
                  cl::Program program(context, {device}, bins, NULL, &err));
        if (err != CL_SUCCESS) {
            cout << "Failed to program device[" << i
            << "] with xclbin file!\n";
        } else {
            cout << "Device[" << i << "]: program successful!\n";
            OCL_CHECK(err, krnl_serpens = cl::Kernel(program, "serpens", &err));
            valid_device++;
            break; // we break because we found a valid device
        }
    }
    if (valid_device == 0) {
        cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }
    
    std::vector<cl::Buffer> buffer_A;
    
    for (int i = 0; i < NUM_CH_SPARSE; i++) {
        OCL_CHECK(err,
                  cl::Buffer currA(context,
                                   CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                   sparse_A_fpga_column_size*sizeof(unsigned long),
                                   sparse_A_fpga_vec[i].data(),
                                   &err);
                  );
        buffer_A.push_back(std::move(currA));
    }
    
    OCL_CHECK(err,
              cl::Buffer buffer_vec_X(context,
                                      CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                      vec_X_fpga_column_size*sizeof(float),
                                      vec_X_fpga.data(),
                                      &err);
              );
    
    OCL_CHECK(err,
              cl::Buffer buffer_vec_Y(context,
                                      CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                      vec_Y_fpga_column_size*sizeof(float),
                                      vec_Y_fpga.data(),
                                      &err);
              );
    
    OCL_CHECK(err,
              cl::Buffer buffer_vec_Y_out(context,
                                          CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                          M*sizeof(float),
                                          vec_Y_out_fpga.data(),
                                          &err);
              );
    
    OCL_CHECK(err,
              cl::Buffer buffer_edge_list_ptr(context,
                                              CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                              edge_list_ptr_fpga_size*sizeof(unsigned int),
                                              edge_list_ptr_fpga.data(),
                                              &err);
              );
    
    // set argument
    int parameter_pos = 0;
    
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, buffer_edge_list_ptr));
    
    for (int i = 0; i < NUM_CH_SPARSE; i++) {
        OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, buffer_A[i]));
    }
    
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, buffer_vec_X));
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, buffer_vec_Y));
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, buffer_vec_Y_out));
    
    int MAX_SIZE_edge_LIST_PTR = edge_list_ptr.size() - 1;
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, MAX_SIZE_edge_LIST_PTR));
    
    int MAX_LEN_edge_PTR = edge_list_ptr[MAX_SIZE_edge_LIST_PTR];
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, MAX_LEN_edge_PTR));
    
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, M));
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, K));
    short para_P = (short) rp_time;
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, para_P));
    
    unsigned int * tmpPointer_v;
    tmpPointer_v = (unsigned int*) &ALPHA;
    unsigned int alpha_int = *tmpPointer_v;
    tmpPointer_v = (unsigned int*) &BETA;
    unsigned int beta_int = *tmpPointer_v;
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, alpha_int));
    OCL_CHECK(err, err = krnl_serpens.setArg(parameter_pos++, beta_int));
    
    cout << "move data to HBM and DRAM\n";
    for ( int i = 0; i < NUM_CH_SPARSE; i++) {
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_A[i]}, 0 /* 0 means from host*/));
    }
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_edge_list_ptr}, 0 /* 0 means from host*/));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_vec_X}, 0 /* 0 means from host*/));
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_vec_Y}, 0 /* 0 means from host*/));
    
    q.finish();
    
    int launch_num = 1;
    printf("start kernel\n");
    auto start = std::chrono::steady_clock::now();
    OCL_CHECK(err, err = q.enqueueTask(krnl_serpens));
    q.finish();
    auto end = std::chrono::steady_clock::now();
    double time_taken = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    time_taken *= 1e-9;
    
    printf("Kernel time is %.7e ms\n", time_taken*1000/launch_num/rp_time);
        
    float gflops =
            2.0f * (nnz + M)
            * launch_num // number of iterations of kernel launch
            * rp_time
            / 1e9 // convert to GB
            / time_taken // total time in second
            ;
    printf("GFLOPS:%f \n", gflops);
    
    cout << "move data to host\n";
    // Copy Result from Device Global Memory to Host Local Memory
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_vec_Y_out}, CL_MIGRATE_MEM_OBJECT_HOST));
    q.finish();
    cout << "finish\n";
    
    int mismatch_cnt = 0;

    for (int mm = 0; mm < M; ++mm) {
        float v_cpu = vec_Y_cpu[mm];
        float v_fpga = vec_Y_out_fpga[mm];
        float dff = fabs(v_cpu - v_fpga);
        float x = min(fabs(v_cpu), fabs(v_fpga)) + 1e-4;
        mismatch_cnt += (dff/x > 1e-4);
    }
    
    float diffpercent = 100.0 * mismatch_cnt / M;
    bool pass = diffpercent < 2.0;
    
    if(pass){
        cout << "Success!\n";
    } else{
        cout << "Failed.\n";
    }
    printf("num_mismatch = %d, percent = %.2f%%\n", mismatch_cnt, diffpercent);
    
    return EXIT_SUCCESS;
}
