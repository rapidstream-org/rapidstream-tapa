#include <cmath>

#include <chrono>
#include <iostream>
#include <vector>

#include <tlp.h>

using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

void Jacobi(tlp::mmap<float> bank_0_t0, tlp::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num);

int main(int argc, char* argv[]) {
  const uint64_t width = 100;
  const uint64_t height = argc > 1 ? atoll(argv[1]) : 100;
  vector<float> t1_vec(height * width);
  vector<float> t0_vec(
      height * width +
      (width * 2 + 1));  // additional space is stencil distance
  auto t1 = reinterpret_cast<float(*)[width]>(t1_vec.data());
  auto t0 = reinterpret_cast<float(*)[width]>(t0_vec.data() + width);
  for (uint64_t i = 0; i < height; ++i) {
    for (uint64_t j = 0; j < width; ++j) {
      auto shuffle = [](uint64_t x, uint64_t n) -> float {
        return static_cast<float>((n / 2 - x) * (n / 2 - x));
      };
      t1[i][j] = pow(shuffle(i, height), 1.5f) + shuffle(j, width);
      t0[i][j] = 0.f;
    }
  }

  auto start = high_resolution_clock::now();
  Jacobi(t0_vec.data(), t1_vec.data(), height * width / 2);
  auto stop = high_resolution_clock::now();
  duration<double> elapsed = stop - start;
  clog << "elapsed time: " << elapsed.count() << " s" << endl;

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 1; i < height - 1; ++i) {
    for (uint64_t j = 1; j < width - 1; ++j) {
      auto expected =
          static_cast<uint64_t>((t1[i - 1][j] + t1[i][j - 1] + t1[i][j] +
                                 t1[i + 1][j] + t1[i][j + 1]) *
                                .2f);
      auto actual = static_cast<uint64_t>(t0[i][j]);
      if (actual != expected) {
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
