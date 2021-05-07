#include <chrono>
#include <iostream>
#include <vector>

#include <tapa.h>

using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
            tapa::mmap<float> c_array, uint64_t n);

int main(int argc, char* argv[]) {
  const uint64_t n = argc > 1 ? atoll(argv[1]) : 1024 * 1024;
  vector<float> a(n);
  vector<float> b(n);
  vector<float> c(n);
  for (uint64_t i = 0; i < n; ++i) {
    a[i] = static_cast<float>(i);
    b[i] = static_cast<float>(i) * 2;
    c[i] = 0.f;
  }
  auto start = high_resolution_clock::now();
  VecAdd(tapa::read_only_mmap<const float>(a),
         tapa::read_only_mmap<const float>(b),
         tapa::write_only_mmap<float>(c), n);
  auto stop = high_resolution_clock::now();
  duration<double> elapsed = stop - start;
  clog << "elapsed time: " << elapsed.count() << " s" << endl;

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 0; i < n; ++i) {
    auto expected = i * 3;
    auto actual = static_cast<uint64_t>(c[i]);
    if (actual != expected) {
      if (num_errors < threshold) {
        clog << "expected: " << expected << ", actual: " << actual << endl;
      } else if (num_errors == threshold) {
        clog << "...";
      }
      ++num_errors;
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
