#include <cstdint>

#include <tapa.h>

using tapa::istream;
using tapa::join;
using tapa::mmap;
using tapa::ostream;
using tapa::stream;
using tapa::task;

void Add(istream<float>& a, istream<float>& b, ostream<float>& c) {
  TAPA_WHILE_NEITHER_EOS(a, b) {
#pragma HLS pipeline II = 1
    c.write(a.read(nullptr) + b.read(nullptr));
  }
  c.close();
}

void Mmap2Stream(mmap<float> mmap, int offset, uint64_t n,
                 ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream.write(mmap[n * offset + i]);
  }
  stream.close();
}

void Stream2Mmap(istream<float>& stream, mmap<float> mmap, int offset,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    mmap[n * offset + i] = stream.read();
  }
}

void Load(mmap<float> srcs, uint64_t n, ostream<float>& a, ostream<float>& b) {
  task()
      .invoke<join>(Mmap2Stream, srcs, 0, n, a)
      .invoke<join>(Mmap2Stream, srcs, 1, n, b);
}

void Store(istream<float>& stream, mmap<float> mmap, uint64_t n) {
  task().invoke<join>(Stream2Mmap, stream, mmap, 2, n);
}

void VecAdd(mmap<float> data, uint64_t n) {
  stream<float, 8> a("a");
  stream<float, 8> b("b");
  stream<float, 8> c("c");

  task()
      .invoke<join>(Load, data, n, a, b)
      .invoke<join>(Add, a, b, c)
      .invoke<join>(Store, c, data, n);
}

void VecAddShared(mmap<float> elems, uint64_t n) {
  task().invoke<join>(VecAdd, elems, n);
}
