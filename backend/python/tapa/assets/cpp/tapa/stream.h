#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <cassert>
#include <cstdint>
#include <cstring>

#include <hls_stream.h>

namespace tapa {

class stream;

template <typename T>
struct elem_t {
  T val;
  bool eos;
};

template <typename T>
class istream {
 public:
  bool empty() const {
#pragma HLS inline
#pragma HLS protocol
    return fifo.empty();
  }

  bool try_eos(bool& eos) const {
#pragma HLS inline
#pragma HLS protocol
    return empty() ? false : (eos = peek_val.eos, true);
  }

  bool eos(bool& succeeded) const {
#pragma HLS inline
#pragma HLS protocol
    return (succeeded = !empty()) && peek_val.eos;
  }

  bool eos(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS protocol
    return !empty() && peek_val.eos;
  }

  bool try_peek(T& val) const {
#pragma HLS inline
#pragma HLS protocol
    return empty() ? false : (val = peek_val.val, true);
  }

  T peek(bool& succeeded) const {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    (succeeded = !empty()) && (elem = peek_val, true);
    return elem.val;
  }

  T peek(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS protocol
    return peek_val.val;
  }

  T peek(bool& succeeded, bool& is_eos) const {
#pragma HLS inline
#pragma HLS protocol
    succeeded = !empty();
    is_eos = peek_val.eos && succeeded;
    return peek_val.val;
  }

  bool try_read(T& val) {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    return fifo.read_nb(elem) && (val = elem.val, true);
  }

  T read() {
#pragma HLS inline
#pragma HLS latency max = 1
    return fifo.read().val;
  }

  T read(bool& succeeded) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    succeeded = fifo.read_nb(elem);
    return elem.val;
  }

  T read(std::nullptr_t) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    fifo.read_nb(elem);
    return elem.val;
  }

  T read(bool* succeeded_ret, const T& default_val) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    bool succeeded = fifo.read_nb(elem);
    if (succeeded_ret != nullptr) *succeeded_ret = succeeded;
    return succeeded ? elem.val : default_val;
  }

  void try_open() {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    assert(!fifo.read_nb(elem) || elem.eos);
  }

  void open() {
#pragma HLS inline
#pragma HLS latency max = 1
    assert(fifo.read().eos);
  }

  hls::stream<elem_t<T>> fifo;
  elem_t<T> peek_val;
};

template <typename T>
class ostream {
 public:
  bool full() const {
#pragma HLS inline
#pragma HLS protocol
    return fifo.full();
  }

  bool try_write(const T& val) {
#pragma HLS inline
#pragma HLS protocol
    return fifo.write_nb({val, false});
  }

  void write(const T& val) {
#pragma HLS inline
#pragma HLS protocol
    fifo.write({val, false});
  }

  bool try_close() {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eos = true;
    return fifo.write_nb(elem);
  }

  void close() {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eos = true;
    fifo.write(elem);
  }

  hls::stream<elem_t<T>> fifo;
};

template <typename T, uint64_t S>
class istreams {
 public:
  istream<T>& operator[](int idx) { return _[idx]; }
  istream<T> _[S];
};

template <typename T, uint64_t S>
class ostreams {
 public:
  ostream<T>& operator[](int idx) { return _[idx]; }
  ostream<T> _[S];
};

}  // namespace tapa

#endif  // TAPA_STREAM_H_
