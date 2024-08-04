#include <cmath>

#include <algorithm>
#include <bitset>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include <gflags/gflags.h>
#include <tapa.h>

using std::clog;
using std::endl;
using std::vector;

using pkt_t = uint64_t;
constexpr int kN = 8;  // kN x kN network
using pkt_vec_t = tapa::vec_t<pkt_t, kN>;

void Network(tapa::mmap<pkt_vec_t> input, tapa::mmap<pkt_vec_t> output,
             uint64_t n);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  uint64_t n = 1ULL << 15;

  vector<pkt_t> input(n);
  std::mt19937 gen;
  std::uniform_int_distribution<uint64_t> dist(0, 1ULL << 32);
  for (uint64_t i = 0; i < n; ++i) {
    input[i] = (dist(gen) & ~7) | i;
  }
  std::shuffle(input.begin(), input.end(), gen);

  vector<pkt_t> output(n);

  int64_t kernel_time_ns = tapa::invoke(
      Network, FLAGS_bitstream,
      tapa::read_only_mmap<pkt_t>(input).vectorized<kN>(),
      tapa::write_only_mmap<pkt_t>(output).vectorized<kN>(), n / kN);
  clog << "kernel time: " << kernel_time_ns * 1e-9 << " s" << endl;

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
