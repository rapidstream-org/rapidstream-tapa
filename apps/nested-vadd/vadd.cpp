#include <cstdint>

#include <tapa.h>

void Add(uint64_t n_int, tapa::istream<float>& a_int,
         tapa::istream<float>& b_int, tapa::ostream<float>& c_int) {
  float a, b;
  bool a_succeed = false, b_succeed = false;
  uint64_t read = 0;

  [[tapa::pipeline(1)]] while (read < n_int) {
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

  // Clear the eot tokens.
  a_int.open();
  b_int.open();
}

void Compute(uint64_t n_ext, tapa::istream<float>& a_ext,
             tapa::istream<float>& b_ext, tapa::ostream<float>& c_ext) {
  tapa::task().invoke(Add, n_ext, a_ext, b_ext, c_ext);
}

void Mmap2Stream_internal(tapa::async_mmap<float>& mmap_int, uint64_t n_int,
                          tapa::ostream<float>& stream_int) {
  [[tapa::pipeline(1)]] for (uint64_t rq_i = 0, rs_i = 0; rs_i < n_int;) {
    float elem;
    if (rq_i < n_int &&
        rq_i < rs_i + 50 &&  // TODO: resolve the DRAM lock issue
        mmap_int.read_addr.try_write(rq_i))
      rq_i++;
    if (mmap_int.read_data.try_read(elem)) {
      stream_int.write(elem);
      rs_i++;
    }
  }

  stream_int.close();
}

void Mmap2Stream(tapa::mmap<float> mmap_ext, uint64_t n_ext,
                 tapa::ostream<float>& stream_ext) {
  tapa::task().invoke(Mmap2Stream_internal, mmap_ext, n_ext, stream_ext);
}

void Load(tapa::mmap<float> a_array, tapa::mmap<float> b_array,
          tapa::ostream<float>& a_stream, tapa::ostream<float>& b_stream,
          uint64_t n) {
  tapa::task()
      .invoke(Mmap2Stream, a_array, n, a_stream)
      .invoke(Mmap2Stream, b_array, n, b_stream);
}

void Store(tapa::istream<float>& stream, tapa::mmap<float> mmap, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    mmap[i] = stream.read();
  }

  // Clear the eot token.
  stream.open();
}

void VecAddNested(tapa::mmap<float> a_array, tapa::mmap<float> b_array,
                  tapa::mmap<float> c_array, uint64_t n) {
  tapa::stream<float, 8> a_stream("a");
  tapa::stream<float, 8> b_stream("b");
  tapa::stream<float, 8> c_stream("c");

  tapa::task()
      .invoke(Load, a_array, b_array, a_stream, b_stream, n)
      .invoke(Compute, n, a_stream, b_stream, c_stream)
      .invoke(Store, c_stream, c_array, n);
}
