#include <cassert>
#include <cstring>

#include <tapa.h>

// p x p PEs
const int p = 2;

// Handles kN x kN matrices maximum.
const int kN = 64;  // Use fixed value for efficient hardware generation.

// Scatter n*n matrix into p*p blocks, each block.
void Scatter(tapa::mmap<const float> matrix_ptr, tapa::ostream<float>& block_00,
             tapa::ostream<float>& block_01, tapa::ostream<float>& block_10,
             tapa::ostream<float>& block_11) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (uint64_t i = 0; i < kNumElems; ++i) {
    block_00.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    block_01.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    block_10.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    block_11.write(*matrix_ptr);
    ++matrix_ptr;
  }
}

void Gather(tapa::mmap<float> matrix_ptr, tapa::istream<float>& block_00,
            tapa::istream<float>& block_01, tapa::istream<float>& block_10,
            tapa::istream<float>& block_11) {
  const uint64_t kNumElems = (kN / p) * (kN / p);
  for (uint64_t i = 0; i < kNumElems; ++i) {
    *matrix_ptr = block_00.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    *matrix_ptr = block_01.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    *matrix_ptr = block_10.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < kNumElems; ++i) {
    *matrix_ptr = block_11.read();
    ++matrix_ptr;
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
  for (uint64_t i = 0; i < kNumElems; ++i) {
    a[i] = a_fifo.read();
    b[i] = b_fifo.read();
    c[i] = 0.f;
  }

  for (int l = 0; l < p; ++l) {
    for (uint64_t i = 0; i < kN / p; ++i) {
      for (uint64_t j = 0; j < kN / p; ++j) {
#pragma HLS pipeline II = 1
#pragma HLS dependence false variable = c
        float tmp = c[i * (kN / p) + j];
        for (uint64_t k = 0; k < kN / p; ++k) {
          tmp += a[i * (kN / p) + k] * b[k * (kN / p) + j];
        }
        c[i * (kN / p) + j] = tmp;
      }
    }
    for (uint64_t a_wr = 0, b_wr = 0, a_rd = 0, b_rd = 0;
         a_wr < kNumElems || b_wr < kNumElems || a_rd < kNumElems ||
         b_rd < kNumElems;) {
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

void Cannon(tapa::mmap<const float> a_vec, tapa::mmap<const float> b_vec,
            tapa::mmap<float> c_vec, uint64_t n) {
  assert(kN % p == 0);
  assert(n <= kN);

  tapa::stream<float, 2> a_00("a->PE00");
  tapa::stream<float, 2> a_01("a->PE01");
  tapa::stream<float, 2> a_10("a->PE10");
  tapa::stream<float, 2> a_11("a->PE11");
  tapa::stream<float, 2> b_00("b->PE00");
  tapa::stream<float, 2> b_01("b->PE01");
  tapa::stream<float, 2> b_10("b->PE10");
  tapa::stream<float, 2> b_11("b->PE11");
  tapa::stream<float, 2> c_00("c->PE00");
  tapa::stream<float, 2> c_01("c->PE01");
  tapa::stream<float, 2> c_10("c->PE10");
  tapa::stream<float, 2> c_11("c->PE11");
  tapa::stream<float, 8> fifo_00_01("PE00->PE01");
  tapa::stream<float, 8> fifo_01_00("PE01->PE00");
  tapa::stream<float, 8> fifo_10_11("PE10->PE11");
  tapa::stream<float, 8> fifo_11_10("PE11->PE10");
  tapa::stream<float, 8> fifo_00_10("PE00->PE10");
  tapa::stream<float, 8> fifo_10_00("PE10->PE00");
  tapa::stream<float, 8> fifo_01_11("PE01->PE11");
  tapa::stream<float, 8> fifo_11_01("PE11->PE01");

  tapa::task()
      .invoke<tapa::join>(Scatter, a_vec, a_00, a_01, a_10, a_11)
      .invoke<tapa::join>(Scatter, b_vec, b_00, b_01, b_10, b_11)
      .invoke<tapa::join>(ProcElem, a_00, b_00, c_00, fifo_00_10, fifo_10_00,
                          fifo_00_01, fifo_01_00)
      .invoke<tapa::join>(ProcElem, a_01, b_01, c_01, fifo_01_11, fifo_11_01,
                          fifo_01_00, fifo_00_01)
      .invoke<tapa::join>(ProcElem, a_10, b_10, c_10, fifo_10_00, fifo_00_10,
                          fifo_10_11, fifo_11_10)
      .invoke<tapa::join>(ProcElem, a_11, b_11, c_11, fifo_11_01, fifo_01_11,
                          fifo_11_10, fifo_10_11)
      .invoke<tapa::join>(Gather, c_vec, c_00, c_01, c_10, c_11);
}
