#include <cassert>
#include <cstring>

#include <tlp.h>

// p x p PEs
const int p = 2;

// handles kN x kN matrices maximum
const int kN = 1024;

// scatter n*n matrix into p*p blocks, each block
void Scatter(const float* matrix_ptr, uint64_t n, float* block_00,
             float* block_01, float* block_10, float* block_11) {
  auto matrix = reinterpret_cast<const float(*)[p][n / p][n / p]>(matrix_ptr);
  std::memcpy(block_00, matrix[0][0], n / p * n / p * sizeof(float));
  std::memcpy(block_01, matrix[0][1], n / p * n / p * sizeof(float));
  std::memcpy(block_10, matrix[1][0], n / p * n / p * sizeof(float));
  std::memcpy(block_11, matrix[1][1], n / p * n / p * sizeof(float));
}

void Gather(float* matrix_ptr, uint64_t n, const float* block_00,
            const float* block_01, const float* block_10,
            const float* block_11) {
  auto matrix = reinterpret_cast<float(*)[p][n / p][n / p]>(matrix_ptr);
  std::memcpy(matrix[0][0], block_00, n / p * n / p * sizeof(float));
  std::memcpy(matrix[0][1], block_01, n / p * n / p * sizeof(float));
  std::memcpy(matrix[1][0], block_10, n / p * n / p * sizeof(float));
  std::memcpy(matrix[1][1], block_11, n / p * n / p * sizeof(float));
}

// each PE processes n/p * n/p block of matrix
template <int i, int j>
void ProcElem(float* a_ptr, float* b_ptr, float* c_ptr, uint64_t n,
              tlp::stream<float>& i_prev, tlp::stream<float>& i_next,
              tlp::stream<float>& j_prev, tlp::stream<float>& j_next) {
  auto a = reinterpret_cast<float(*)[n / p]>(a_ptr);
  auto b = reinterpret_cast<float(*)[n / p]>(b_ptr);
  auto c = reinterpret_cast<float(*)[n / p]>(c_ptr);

  for (uint64_t ii = 0; ii < n / p; ++ii) {
    for (uint64_t jj = 0; jj < n / p; ++jj) {
      c[ii][jj] = 0.f;
    }
  }

  for (int l = 0; l < p; ++l) {
    for (uint64_t ii = 0; ii < n / p; ++ii) {
      for (uint64_t jj = 0; jj < n / p; ++jj) {
        for (uint64_t kk = 0; kk < n / p; ++kk) {
          c[ii][jj] += a[ii][kk] * b[kk][jj];
        }
      }
    }

    for (uint64_t ii = 0; ii < n / p; ++ii) {
      for (uint64_t jj = 0; jj < n / p; ++jj) {
        if (i % 2 == 0) {
          i_prev.write(b[ii][jj]);
          b[ii][jj] = i_next.read();
        } else {
          auto tmp = i_next.read();
          i_prev.write(b[ii][jj]);
          b[ii][jj] = tmp;
        }
        if (j % 2 == 0) {
          j_prev.write(a[ii][jj]);
          a[ii][jj] = j_next.read();
        } else {
          auto tmp = j_next.read();
          j_prev.write(a[ii][jj]);
          a[ii][jj] = tmp;
        }
      }
    }
  }
}

void Cannon(const float* a_vec, const float* b_vec, float* c_vec, uint64_t n) {
  assert(n % p == 0);
  assert(n <= kN);

  static float a_00[kN / p * kN / p];
  static float a_01[kN / p * kN / p];
  static float a_10[kN / p * kN / p];
  static float a_11[kN / p * kN / p];
  static float b_00[kN / p * kN / p];
  static float b_01[kN / p * kN / p];
  static float b_10[kN / p * kN / p];
  static float b_11[kN / p * kN / p];
  static float c_00[kN / p * kN / p];
  static float c_01[kN / p * kN / p];
  static float c_10[kN / p * kN / p];
  static float c_11[kN / p * kN / p];

  tlp::stream<float, 2> fifo_00_01("PE1->PE01");
  tlp::stream<float, 2> fifo_01_00("PE1->PE00");
  tlp::stream<float, 2> fifo_10_11("PE1->PE11");
  tlp::stream<float, 2> fifo_11_10("PE1->PE10");
  tlp::stream<float, 2> fifo_00_10("PE1->PE10");
  tlp::stream<float, 2> fifo_10_00("PE1->PE00");
  tlp::stream<float, 2> fifo_01_11("PE1->PE11");
  tlp::stream<float, 2> fifo_11_01("PE1->PE01");

  tlp::task()
      .invoke<0>(Scatter, a_vec, n, a_00, a_01, a_10, a_11)
      .invoke<0>(Scatter, b_vec, n, b_00, b_01, b_10, b_11)
      .invoke<1>(ProcElem<0, 0>, a_00, b_00, c_00, n, fifo_00_10, fifo_10_00,
                 fifo_00_01, fifo_01_00)
      .invoke<1>(ProcElem<0, 1>, a_01, b_01, c_01, n, fifo_01_11, fifo_11_01,
                 fifo_01_00, fifo_00_01)
      .invoke<1>(ProcElem<1, 0>, a_10, b_10, c_10, n, fifo_10_00, fifo_00_10,
                 fifo_10_11, fifo_11_10)
      .invoke<1>(ProcElem<1, 1>, a_11, b_11, c_11, n, fifo_11_01, fifo_01_11,
                 fifo_11_10, fifo_10_11)
      .invoke<2>(Gather, c_vec, n, c_00, c_01, c_10, c_11);
}