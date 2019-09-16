#include <cstdint>

#include <tlp.h>

void Add(tlp::istream<float>& a, tlp::istream<float>& b,
         tlp::ostream<float>& c) {
  while (!a.eos() && !b.eos()) {
    c.write(a.read() + b.read());
  }
  c.close();
}

void Array2Stream(tlp::mmap<const float> array, uint64_t n,
                  tlp::ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream.write(array[i]);
  }
  stream.close();
}

void Stream2Array(tlp::istream<float>& stream, tlp::mmap<float> array,
                  uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    array[i] = stream.read();
  }
}

void VecAdd(tlp::mmap<const float> a_array, tlp::mmap<const float> b_array,
            tlp::mmap<float> c_array, uint64_t n) {
  tlp::stream<float, 8> a_stream("a");
  tlp::stream<float, 8> b_stream("b");
  tlp::stream<float, 8> c_stream("c");

  tlp::task()
      .invoke<0>(Array2Stream, a_array, n, a_stream)
      .invoke<0>(Array2Stream, b_array, n, b_stream)
      .invoke<0>(Add, a_stream, b_stream, c_stream)
      .invoke<0>(Stream2Array, c_stream, c_array, n);
}