#include <cmath>

#include <algorithm>
#include <bitset>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include <tapa.h>

using std::abs;
using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

using pkt_t = uint64_t;
constexpr int kN = 8;  // kN x kN network

void Network(tapa::mmap<tapa::vec_t<pkt_t, kN>> input,
             tapa::mmap<tapa::vec_t<pkt_t, kN>> output, uint64_t n);

int main(int argc, char* argv[]) {
  uint64_t n = 1ULL << 15;

  vector<pkt_t> input(n);
  std::mt19937 gen;
  std::uniform_int_distribution<uint64_t> dist(0, 1ULL << 32);
  for (uint64_t i = 0; i < n; ++i) {
    input[i] = (dist(gen) & ~7) | i;
  }
  std::random_shuffle(input.begin(), input.end());

  vector<pkt_t> output(n);

  const auto start = high_resolution_clock::now();

  Network({(tapa::vec_t<pkt_t, kN>*)input.data(), n / kN},
          {(tapa::vec_t<pkt_t, kN>*)output.data(), n / kN}, n / kN);

  const auto stop = high_resolution_clock::now();

  duration<double> elapsed = stop - start;
  clog << "elapsed time: " << elapsed.count() << " s" << endl;

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 0; i < n / kN; ++i) {
    for (uint64_t j = 0; j < kN; ++j) {
      std::bitset<3> actual = output[i * kN + j];
      std::bitset<3> expected = j;
      if (expected != actual) {
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
