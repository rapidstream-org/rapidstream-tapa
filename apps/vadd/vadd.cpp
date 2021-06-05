#include <cstdint>

#include <tapa.h>

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c) {
  TAPA_WHILE_NEITHER_EOS(a, b) {
#pragma HLS pipeline II = 1
    bool a_succeed;
    bool b_succeed;
    c.write(a.read(a_succeed) + b.read(b_succeed));
  }
  c.close();
}

void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream.write(mmap[i]);
  }
  stream.close();
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::mmap<float> mmap,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    mmap[i] = stream.read();
  }
}

void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
            tapa::mmap<float> c_array, uint64_t n) {
  tapa::stream<float, 1> a_stream("a");
  tapa::stream<float, 1> b_stream("b");
  tapa::stream<float, 1> c_stream("c");

  tapa::task()
      .invoke(Mmap2Stream, a_array, n, a_stream)
      .invoke(Mmap2Stream, b_array, n, b_stream)
      .invoke(Add, a_stream, b_stream, c_stream)
      .invoke(Stream2Mmap, c_stream, c_array, n);
}
