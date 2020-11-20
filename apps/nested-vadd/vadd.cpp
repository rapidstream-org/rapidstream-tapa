#include <cstdint>

#include <tapa.h>

void Add(uint64_t n_int,
        tapa::istream<float>& a_int,
        tapa::istream<float>& b_int,
        tapa::ostream<float>& c_int) {
  float a, b;
  bool a_succeed = false, b_succeed = false;
  uint64_t read = 0;

  while (read < n_int) {
#pragma HLS pipeline II = 1
    if (!a_succeed) {
      a = a_int.read(a_succeed);
    }
    if (!b_succeed) {
      b = b_int.read(b_succeed);
    }
    if (a_succeed && b_succeed) {
      c_int.write(a + b);
      a_succeed = b_succeed = false;
      read += 1;
    }
  }
  c_int.close();
}

void Compute(uint64_t n_ext,
         tapa::istream<float>& a_ext,
         tapa::istream<float>& b_ext,
         tapa::ostream<float>& c_ext) {
    tapa::task()
        .invoke<0>(Add, n_ext, a_ext, b_ext, c_ext);
}

void Mmap2Stream_internal(
        tapa::mmap<const float> mmap_int,
        uint64_t n_int,
        tapa::ostream<float>& stream_int) {
  for (uint64_t i = 0; i < n_int; ++i) {
    stream_int.write(mmap_int[i]);
  }
  stream_int.close();
}

void Mmap2Stream(
        tapa::mmap<const float> mmap_ext,
        uint64_t n_ext,
        tapa::ostream<float>& stream_ext) {
    tapa::task().invoke<0>(Mmap2Stream_internal,
            mmap_ext, n_ext, stream_ext);
}

void Load(
        tapa::mmap<const float> a_array,
        tapa::mmap<const float> b_array,
        tapa::ostream<float>&   a_stream,
        tapa::ostream<float>&   b_stream,
        uint64_t n) {
    tapa::task()
        .invoke<1>(Mmap2Stream, a_array, n, a_stream)
        .invoke<1>(Mmap2Stream, b_array, n, b_stream);
}

void Store(
        tapa::istream<float>& stream,
        tapa::mmap<float> mmap,
        uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    mmap[i] = stream.read();
  }
}

void VecAddNested(
        tapa::mmap<const float> a_array,
        tapa::mmap<const float> b_array,
        tapa::mmap<float> c_array, uint64_t n) {
  tapa::stream<float, 8> a_stream("a");
  tapa::stream<float, 8> b_stream("b");
  tapa::stream<float, 8> c_stream("c");

  tapa::task()
      .invoke<1>(Load,    a_array, b_array, a_stream, b_stream, n)
      .invoke<1>(Compute, n, a_stream, b_stream, c_stream)
      .invoke<0>(Store,   c_stream, c_array, n);
}
