#include <cassert>
#include <cstring>

#include <tlp.h>

// p x p PEs
const int p = 2;

// Handles kN x kN matrices maximum.
const int kN = 64;  // Use fixed value for efficient hardware generation.

// Scatter n*n matrix into p*p blocks, each block.
void Scatter(const float* matrix_ptr, uint64_t n, tlp::stream<float>& block_00,
             tlp::stream<float>& block_01, tlp::stream<float>& block_10,
             tlp::stream<float>& block_11) {
  const uint64_t num_elems = (kN / p) * (kN / p);
  for (uint64_t i = 0; i < num_elems; ++i) {
    block_00.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
    block_01.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
    block_10.write(*matrix_ptr);
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
    block_11.write(*matrix_ptr);
    ++matrix_ptr;
  }
}

void Gather(float* matrix_ptr, uint64_t n, tlp::stream<float>& block_00,
            tlp::stream<float>& block_01, tlp::stream<float>& block_10,
            tlp::stream<float>& block_11) {
  const uint64_t num_elems = (kN / p) * (kN / p);
  for (uint64_t i = 0; i < num_elems; ++i) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
    *matrix_ptr = block_00.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
    *matrix_ptr = block_01.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
    *matrix_ptr = block_10.read();
    ++matrix_ptr;
  }
  for (uint64_t i = 0; i < num_elems; ++i) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
    *matrix_ptr = block_11.read();
    ++matrix_ptr;
  }
}

// Each PE processes n/p * n/p block of matrix.
template <int i, int j>
void ProcElem(tlp::stream<float>& a_fifo, tlp::stream<float>& b_fifo,
              tlp::stream<float>& c_fifo, uint64_t n,
              tlp::stream<float>& i_prev, tlp::stream<float>& i_next,
              tlp::stream<float>& j_prev, tlp::stream<float>& j_next) {
  const uint64_t num_elems = (kN / p) * (kN / p);
  static float a[kN / p * kN / p];
  static float b[kN / p * kN / p];
  static float c[kN / p * kN / p];

  // Initialize local a, b, and c.
  for (uint64_t ii = 0; ii < num_elems; ++ii) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
    a[ii] = a_fifo.read();
    b[ii] = b_fifo.read();
    c[ii] = 0.f;
  }

  for (int l = 0; l < p; ++l) {
    for (uint64_t ii = 0; ii < kN / p; ++ii) {
      for (uint64_t kk = 0; kk < kN / p; ++kk) {
        for (uint64_t jj = 0; jj < kN / p; ++jj) {
          c[ii * (kN / p) + jj] +=
              a[ii * (kN / p) + kk] * b[kk * (kN / p) + jj];
        }
      }
    }

    i_prev.write(b[0]);
    j_prev.write(a[0]);
    for (uint64_t ii = 0; ii < num_elems; ++ii) {
#pragma HLS loop_tripcount min = num_elems max = num_elems
      if (ii < num_elems - 1) {
        i_prev.write(b[ii + 1]);
      }
      b[ii] = i_next.read();
      if (ii < num_elems - 1) {
        j_prev.write(a[ii + 1]);
      }
      a[ii] = j_next.read();
    }
  }

  for (uint64_t ii = 0; ii < num_elems; ++ii) {
    c_fifo.write(c[ii]);
  }
}

void Cannon(const float* a_vec, const float* b_vec, float* c_vec, uint64_t n) {
  assert(kN % p == 0);
  assert(n <= kN);

  tlp::stream<float, 1> a_00("a->PE00");
  tlp::stream<float, 1> a_01("a->PE01");
  tlp::stream<float, 1> a_10("a->PE10");
  tlp::stream<float, 1> a_11("a->PE11");
  tlp::stream<float, 1> b_00("b->PE00");
  tlp::stream<float, 1> b_01("b->PE01");
  tlp::stream<float, 1> b_10("b->PE10");
  tlp::stream<float, 1> b_11("b->PE11");
  tlp::stream<float, 1> c_00("c->PE00");
  tlp::stream<float, 1> c_01("c->PE01");
  tlp::stream<float, 1> c_10("c->PE10");
  tlp::stream<float, 1> c_11("c->PE11");
  tlp::stream<float, 2> fifo_00_01("PE00->PE01");
  tlp::stream<float, 2> fifo_01_00("PE01->PE00");
  tlp::stream<float, 2> fifo_10_11("PE10->PE11");
  tlp::stream<float, 2> fifo_11_10("PE11->PE10");
  tlp::stream<float, 2> fifo_00_10("PE00->PE10");
  tlp::stream<float, 2> fifo_10_00("PE10->PE00");
  tlp::stream<float, 2> fifo_01_11("PE01->PE11");
  tlp::stream<float, 2> fifo_11_01("PE11->PE01");

  tlp::task()
      .invoke<0>(Scatter, a_vec, kN, a_00, a_01, a_10, a_11)
      .invoke<0>(Scatter, b_vec, kN, b_00, b_01, b_10, b_11)
      .invoke<0>(ProcElem<0, 0>, a_00, b_00, c_00, kN, fifo_00_10, fifo_10_00,
                 fifo_00_01, fifo_01_00)
      .invoke<0>(ProcElem<0, 1>, a_01, b_01, c_01, kN, fifo_01_11, fifo_11_01,
                 fifo_01_00, fifo_00_01)
      .invoke<0>(ProcElem<1, 0>, a_10, b_10, c_10, kN, fifo_10_00, fifo_00_10,
                 fifo_10_11, fifo_11_10)
      .invoke<0>(ProcElem<1, 1>, a_11, b_11, c_11, kN, fifo_11_01, fifo_01_11,
                 fifo_11_10, fifo_10_11)
      .invoke<0>(Gather, c_vec, kN, c_00, c_01, c_10, c_11);
}