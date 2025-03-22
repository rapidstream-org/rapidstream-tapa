// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "cannon.h"

#include <gflags/gflags.h>

DEFINE_string(scatter_bitstream, "",
              "path to scatter simulation file, run csim if empty");
DEFINE_string(gather_bitstream, "",
              "path to gather simulation file, run csim if empty");
DEFINE_string(proc_elem_bitstream, "",
              "path to proc_elem simulation file, run csim if empty");

// Scatter n*n matrix into p*p blocks, each block.
void Scatter_internal(tapa::mmap<const float> matrix_ptr,
                      tapa::ostreams<float, p * p>& block) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (int j = 0; j < p * p; ++j) {
    for (uint64_t i = 0; i < kNumElems; ++i) {
      block[j].write(*matrix_ptr);
      ++matrix_ptr;
    }
  }
}

void Scatter(tapa::mmap<const float> matrix_ptr,
             tapa::ostreams<float, p * p>& block) {
  tapa::task().invoke(Scatter_internal, matrix_ptr, block);
}

void Gather_internal(tapa::mmap<float> matrix_ptr,
                     tapa::istreams<float, p * p>& block) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (int j = 0; j < p * p; ++j) {
    for (uint64_t i = 0; i < kNumElems; ++i) {
      *matrix_ptr = block[j].read();
      ++matrix_ptr;
    }
  }
}

void Gather(tapa::mmap<float> matrix_ptr, tapa::istreams<float, p * p>& block) {
  tapa::task().invoke(Gather_internal, matrix_ptr, block);
}

// Each PE processes n/p * n/p block of matrix.
void ProcElem_internal(tapa::istream<float>& a_fifo,
                       tapa::istream<float>& b_fifo,
                       tapa::ostream<float>& c_fifo,
                       tapa::ostream<float>& i_prev,
                       tapa::istream<float>& i_next,
                       tapa::ostream<float>& j_prev,
                       tapa::istream<float>& j_next) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  float a[kN / p * kN / p];
  float b[kN / p * kN / p];
  float c[kN / p * kN / p];
#pragma HLS array_partition variable = a cyclic factor = 32
#pragma HLS array_partition variable = b block factor = 32
#pragma HLS array_partition variable = c cyclic factor = 32

  // Initialize local a, b, and c.
  for (uint64_t i = 0; i < kNumElems; ++i) {
    a[i] = a_fifo.read();
    b[i] = b_fifo.read();
    c[i] = 0.f;
  }

  for (int l = 0; l < p; ++l) {
    [[tapa::pipeline(1)]] for (int ij = 0; ij < kNumElems; ++ij) {
#pragma HLS dependence false variable = c
      float tmp = 0.f;
      const int i = ij / (kN / p);
      const int j = ij % (kN / p);
      for (int k = 0; k < kN / p; ++k) {
        tmp += a[i * (kN / p) + k] * b[k * (kN / p) + j];
      }
      c[ij] += tmp;
    }
    [[tapa::pipeline(1)]] for (uint64_t a_wr = 0, b_wr = 0, a_rd = 0, b_rd = 0;
                               a_wr < kNumElems || b_wr < kNumElems ||
                               a_rd < kNumElems || b_rd < kNumElems;) {
#pragma HLS loop_tripcount min = kNumElems max = kNumElems
#pragma HLS dependence false variable = a
#pragma HLS dependence false variable = b
      if (b_wr < kNumElems && i_prev.try_write(b[b_wr])) ++b_wr;
      if (a_wr < kNumElems && j_prev.try_write(a[a_wr])) ++a_wr;
      if (b_rd < b_wr && i_next.try_read(b[b_rd])) ++b_rd;
      if (a_rd < a_wr && j_next.try_read(a[a_rd])) ++a_rd;
    }
  }

  for (uint64_t i = 0; i < kNumElems; ++i) {
    c_fifo.write(c[i]);
  }
}

void ProcElem(tapa::istream<float>& a_fifo, tapa::istream<float>& b_fifo,
              tapa::ostream<float>& c_fifo, tapa::ostream<float>& i_prev,
              tapa::istream<float>& i_next, tapa::ostream<float>& j_prev,
              tapa::istream<float>& j_next) {
  tapa::task().invoke(ProcElem_internal, a_fifo, b_fifo, c_fifo, i_prev, i_next,
                      j_prev, j_next);
}

void Cannon(tapa::mmap<const float> a_vec, tapa::mmap<const float> b_vec,
            tapa::mmap<float> c_vec, uint64_t n) {
  assert(kN % p == 0);
  assert(n <= kN);

  tapa::streams<float, p * p> a("a");
  tapa::streams<float, p * p> b("b");
  tapa::streams<float, p * p> c("c");
  tapa::stream<float, 8> fifo_00_01("PE00->PE01");
  tapa::stream<float, 8> fifo_01_00("PE01->PE00");
  tapa::stream<float, 8> fifo_10_11("PE10->PE11");
  tapa::stream<float, 8> fifo_11_10("PE11->PE10");
  tapa::stream<float, 8> fifo_00_10("PE00->PE10");
  tapa::stream<float, 8> fifo_10_00("PE10->PE00");
  tapa::stream<float, 8> fifo_01_11("PE01->PE11");
  tapa::stream<float, 8> fifo_11_01("PE11->PE01");

  tapa::task()
      .invoke(Scatter, tapa::executable(FLAGS_scatter_bitstream), a_vec, a)
      .invoke(Scatter, tapa::executable(FLAGS_scatter_bitstream), b_vec, b)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c,
              fifo_00_10, fifo_10_00, fifo_00_01, fifo_01_00)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c,
              fifo_01_11, fifo_11_01, fifo_01_00, fifo_00_01)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c,
              fifo_10_00, fifo_00_10, fifo_10_11, fifo_11_10)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c,
              fifo_11_01, fifo_01_11, fifo_11_10, fifo_10_11)
      .invoke(Gather, tapa::executable(FLAGS_gather_bitstream), c_vec, c);
}
