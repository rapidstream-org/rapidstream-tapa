#include <cstdint>

#include <tlp.h>

void Add(tlp::stream<float>& a, tlp::stream<float>& b, tlp::stream<float>& c) {
  while (!a.eos() && !b.eos()) {
    c.write(a.read() + b.read());
  }
  c.close();
}

void Array2Stream(const float* array, uint64_t n, tlp::stream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream.write(array[i]);
  }
  stream.close();
}

void Stream2Array(tlp::stream<float>& stream, float* array) {
  for (uint64_t i = 0; !stream.eos(); ++i) {
    array[i] = stream.read();
  }
}

void VecAdd(const float* a_array, const float* b_array, float* c_array,
            uint64_t n) {
  tlp::stream<float, 8> a_stream("a");
  tlp::stream<float, 8> b_stream("b");
  tlp::stream<float, 8> c_stream("c");

  tlp::task()
      .invoke<0>(Array2Stream, a_array, n, a_stream)
      .invoke<0>(Array2Stream, b_array, n, b_stream)
      .invoke<0>(Add, a_stream, b_stream, c_stream)
      .invoke<0>(Stream2Array, c_stream, c_array);
}