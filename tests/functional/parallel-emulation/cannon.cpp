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
void Scatter(tapa::mmap<const float> matrix_ptr,
             tapa::ostreams<float, p * p>& block) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (int j = 0; j < p * p; ++j) {
    for (uint64_t i = 0; i < kNumElems; ++i) {
      block[j].write(*matrix_ptr);
      ++matrix_ptr;
    }
  }
}

void Gather(tapa::mmap<float> matrix_ptr, tapa::istreams<float, p * p>& block) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (int j = 0; j < p * p; ++j) {
    for (uint64_t i = 0; i < kNumElems; ++i) {
      *matrix_ptr = block[j].read();
      ++matrix_ptr;
    }
  }
}

// Each PE processes n/p * n/p block of matrix.
void ProcElem(tapa::istream<float>& a_fifo, tapa::istream<float>& b_fifo,
              tapa::ostream<float>& c_fifo, tapa::ostream<float>& i_prev,
              tapa::istream<float>& i_next, tapa::ostream<float>& j_prev,
              tapa::istream<float>& j_next) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  float a[kN / p * kN / p];
  float b[kN / p * kN / p];
  float c[kN / p * kN / p];
#pragma HLS array_partition variable = a cyclic factor = 32
#pragma HLS array_partition variable = b block factor = 32
#pragma HLS array_partition variable = c cyclic factor = 32

  // Initialize local a, b, and c.
init:
  for (uint64_t i = 0; i < kNumElems; ++i) {
    a[i] = a_fifo.read();
    b[i] = b_fifo.read();
    c[i] = 0.f;
  }

outer:
  for (int l = 0; l < p; ++l) {
  compute:
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
  communicate:
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

finalize:
  for (uint64_t i = 0; i < kNumElems; ++i) {
    c_fifo.write(c[i]);
  }
}

void Cannon(tapa::mmap<const float> a_vec, tapa::mmap<const float> b_vec,
            tapa::mmap<float> c_vec, uint64_t n) {
  assert(kN % p == 0);
  assert(n <= kN);

  tapa::streams<float, p * p> a("a");
  tapa::streams<float, p * p> b("b");
  tapa::streams<float, p * p> c("c");
  /*
      00→01→02→03→
      ↓  ↓  ↓  ↓
      10→11→12→13→
      ↓  ↓  ↓  ↓
      20→21→22→23→
      ↓  ↓  ↓  ↓
      30→31→32→33→
      ↓  ↓  ↓  ↓
  */
  // Horizontal FIFOs
  tapa::stream<float, 16> fifo_00_01("PE00->PE01");
  tapa::stream<float, 16> fifo_01_02("PE01->PE02");
  tapa::stream<float, 16> fifo_02_03("PE02->PE03");
  tapa::stream<float, 16> fifo_03_00("PE03->PE00");
  tapa::stream<float, 16> fifo_10_11("PE10->PE11");
  tapa::stream<float, 16> fifo_11_12("PE11->PE12");
  tapa::stream<float, 16> fifo_12_13("PE12->PE13");
  tapa::stream<float, 16> fifo_13_10("PE13->PE10");
  tapa::stream<float, 16> fifo_20_21("PE20->PE21");
  tapa::stream<float, 16> fifo_21_22("PE21->PE22");
  tapa::stream<float, 16> fifo_22_23("PE22->PE23");
  tapa::stream<float, 16> fifo_23_20("PE23->PE20");
  tapa::stream<float, 16> fifo_30_31("PE30->PE31");
  tapa::stream<float, 16> fifo_31_32("PE31->PE32");
  tapa::stream<float, 16> fifo_32_33("PE32->PE33");
  tapa::stream<float, 16> fifo_33_30("PE33->PE30");

  // Vertical FIFOs
  tapa::stream<float, 16> fifo_00_10("PE00->PE10");
  tapa::stream<float, 16> fifo_10_20("PE10->PE20");
  tapa::stream<float, 16> fifo_20_30("PE20->PE30");
  tapa::stream<float, 16> fifo_30_00("PE30->PE00");
  tapa::stream<float, 16> fifo_01_11("PE01->PE11");
  tapa::stream<float, 16> fifo_11_21("PE11->PE21");
  tapa::stream<float, 16> fifo_21_31("PE21->PE31");
  tapa::stream<float, 16> fifo_31_01("PE31->PE01");
  tapa::stream<float, 16> fifo_02_12("PE02->PE12");
  tapa::stream<float, 16> fifo_12_22("PE12->PE22");
  tapa::stream<float, 16> fifo_22_32("PE22->PE32");
  tapa::stream<float, 16> fifo_32_02("PE32->PE02");
  tapa::stream<float, 16> fifo_03_13("PE03->PE13");
  tapa::stream<float, 16> fifo_13_23("PE13->PE23");
  tapa::stream<float, 16> fifo_23_33("PE23->PE33");
  tapa::stream<float, 16> fifo_33_03("PE33->PE03");

  tapa::task()
      .invoke(Scatter, tapa::executable(FLAGS_scatter_bitstream), a_vec, a)
      .invoke(Scatter, tapa::executable(FLAGS_scatter_bitstream), b_vec, b)
  // clang-format off
#if TAPA_CANNON_P == 2
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_00_10, fifo_10_20, fifo_00_01, fifo_01_02)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_01_11, fifo_11_21, fifo_01_02, fifo_00_01)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_10_20, fifo_00_10, fifo_10_11, fifo_11_12)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_11_21, fifo_01_11, fifo_11_12, fifo_10_11)
#elif TAPA_CANNON_P == 4
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_00_10, fifo_30_00, fifo_00_01, fifo_03_00)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_01_11, fifo_31_01, fifo_01_02, fifo_00_01)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_02_12, fifo_32_02, fifo_02_03, fifo_01_02)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_03_13, fifo_33_03, fifo_03_00, fifo_02_03)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_10_20, fifo_00_10, fifo_10_11, fifo_13_10)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_11_21, fifo_01_11, fifo_11_12, fifo_10_11)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_12_22, fifo_02_12, fifo_12_13, fifo_11_12)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_13_23, fifo_03_13, fifo_13_10, fifo_12_13)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_20_30, fifo_10_20, fifo_20_21, fifo_23_20)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_21_31, fifo_11_21, fifo_21_22, fifo_20_21)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_22_32, fifo_12_22, fifo_22_23, fifo_21_22)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_23_33, fifo_13_23, fifo_23_20, fifo_22_23)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_30_00, fifo_20_30, fifo_30_31, fifo_33_30)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_31_01, fifo_21_31, fifo_31_32, fifo_30_31)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_32_02, fifo_22_32, fifo_32_33, fifo_31_32)
      .invoke(ProcElem, tapa::executable(FLAGS_proc_elem_bitstream), a, b, c, fifo_33_03, fifo_23_33, fifo_33_30, fifo_32_33)
#else
#error "unimplemented PE count"
#endif
      // clang-format on
      .invoke(Gather, tapa::executable(FLAGS_gather_bitstream), c_vec, c);
}
