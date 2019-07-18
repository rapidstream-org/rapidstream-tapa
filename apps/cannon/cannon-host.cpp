#include <cmath>

#include <chrono>
#include <iostream>
#include <limits>
#include <vector>

using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

void Cannon(const float* a, const float* b, float* c, uint64_t n);

int main(int argc, char* argv[]) {
  const uint64_t n = argc > 1 ? atoll(argv[1]) : 256;
  vector<float> a_vec(n * n);
  vector<float> b_vec(n * n);
  vector<float> c_vec(n * n);
  auto a = reinterpret_cast<float(*)[n]>(a_vec.data());
  auto b = reinterpret_cast<float(*)[n]>(b_vec.data());
  auto c = reinterpret_cast<float(*)[n]>(c_vec.data());
  for (uint64_t i = 0; i < n; ++i) {
    for (uint64_t j = 0; j < n; ++j) {
      auto shuffle = [](uint64_t x, uint64_t n) -> float {
        return static_cast<float>((n / 2 - x) * (n / 2 - x));
      };
      a[i][j] = pow(shuffle(i, n), 1.8f) + shuffle(j, n);
      b[i][j] = pow(shuffle(j, n), 1.2f) + shuffle(i, n);
      c[i][j] = 0.f;
    }
  }

  auto start = high_resolution_clock::now();
  // reshape the matrices into 2x2 blocks (each has size n/2 x n/2)
  const int p = 2;
  vector<float> a_buf(n * n);
  vector<float> b_buf(n * n);
  vector<float> c_buf(n * n);
  auto a_block = reinterpret_cast<float(*)[p][n / p][n / p]>(a_buf.data());
  auto b_block = reinterpret_cast<float(*)[p][n / p][n / p]>(b_buf.data());
  auto c_block = reinterpret_cast<float(*)[p][n / p][n / p]>(c_buf.data());

  for (uint64_t i = 0; i < p; ++i) {
    for (uint64_t j = 0; j < p; ++j) {
      for (uint64_t ii = 0; ii < n / p; ++ii) {
        for (uint64_t jj = 0; jj < n / p; ++jj) {
          a_block[i][(i + j) % p][ii][jj] = a[i * n / p + ii][j * n / p + jj];
          b_block[(i + j) % p][j][ii][jj] = b[i * n / p + ii][j * n / p + jj];
        }
      }
    }
  }

  Cannon(a_buf.data(), b_buf.data(), c_buf.data(), n);

  for (uint64_t i = 0; i < p; ++i) {
    for (uint64_t j = 0; j < p; ++j) {
      for (uint64_t ii = 0; ii < n / p; ++ii) {
        for (uint64_t jj = 0; jj < n / p; ++jj) {
          c[i * n / p + ii][j * n / p + jj] = c_block[i][j][ii][jj];
        }
      }
    }
  }
  auto stop = high_resolution_clock::now();
  duration<double> elapsed = stop - start;
  clog << "elapsed time: " << elapsed.count() << " s" << endl;

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 0; i < n; ++i) {
    for (uint64_t j = 0; j < n; ++j) {
      auto expected = 0.f;
      for (uint64_t k = 0; k < n; ++k) {
        expected += a[i][k] * b[k][j];
      }
      auto actual = c[i][j];
      if (fabs((actual - expected) / expected) > 1e-4) {
        if (num_errors < threshold) {
          clog << "expected: " << expected << ", actual: " << actual << endl;
        } else if (num_errors == threshold) {
          clog << "...";
        }
        ++num_errors;
      }
    }
  }
  if (num_errors == 0) {
    clog << "PASS!" << endl;
  } else {
    if (num_errors > threshold) {
      clog << " (+" << (num_errors - threshold) << " more errors)" << endl;
    }
    clog << "FAIL!" << endl;
  }
  return num_errors > 0 ? 1 : 0;
}
