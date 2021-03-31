#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <cassert>
#include <cstdint>
#include <cstring>

#include <hls_stream.h>

namespace tapa {

class stream;
class streams;

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
#pragma HLS latency max = 0
    return _.empty();
  }

  bool try_eos(bool& eos) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    return empty() ? false : (eos = peek_val.eos, true);
  }

  bool eos(bool& succeeded) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    return (succeeded = !empty()) && peek_val.eos;
  }

  bool eos(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    return !empty() && peek_val.eos;
  }

  bool try_peek(T& val) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    return empty() ? false : (val = peek_val.val, true);
  }

  T peek(bool& succeeded) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    (succeeded = !empty()) && _peek.read_nb(elem);
    return elem.val;
  }

  T peek(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    return peek_val.val;
  }

  T peek(bool& succeeded, bool& is_eos) const {
#pragma HLS inline
#pragma HLS latency max = 0
    succeeded = !empty();
    elem_t<T> peek_val;
    _peek.read_nb(peek_val);
    is_eos = peek_val.eos && succeeded;
    return peek_val.val;
  }

  bool try_read(T& val) {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    return _.read_nb(elem) && (val = elem.val, true);
  }

  T read() {
#pragma HLS inline
#pragma HLS latency max = 0
    return _.read().val;
  }

  T read(bool& succeeded) {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    succeeded = _.read_nb(elem);
    return elem.val;
  }

  T read(std::nullptr_t) {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    _.read_nb(elem);
    return elem.val;
  }

  T read(bool* succeeded_ret, const T& default_val) {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    bool succeeded = _.read_nb(elem);
    if (succeeded_ret != nullptr) *succeeded_ret = succeeded;
    return succeeded ? elem.val : default_val;
  }

  bool try_open() {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    const bool succeeded = _.read_nb(elem);
    assert(!succeeded || elem.eos);
    return succeeded;
  }

  void open() {
#pragma HLS inline
#pragma HLS latency max = 0
    const auto elem = _.read();
    assert(elem.eos);
  }

  hls::stream<elem_t<T>> _;
  mutable hls::stream<elem_t<T>> _peek;
};  // namespace tapa

template <typename T>
class ostream {
 public:
  bool full() const {
#pragma HLS inline
#pragma HLS latency max = 0
    return _.full();
  }

  bool try_write(const T& val) {
#pragma HLS inline
#pragma HLS latency max = 0
    return _.write_nb({val, false});
  }

  void write(const T& val) {
#pragma HLS inline
#pragma HLS latency max = 0
    _.write({val, false});
  }

  bool try_close() {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eos = true;
    return _.write_nb(elem);
  }

  void close() {
#pragma HLS inline
#pragma HLS latency max = 0
    elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eos = true;
    _.write(elem);
  }

  hls::stream<elem_t<T>> _;
};

template <typename T, uint64_t S>
using istreams = istream<T>[S];
template <typename T, uint64_t S>
using ostreams = ostream<T>[S];

}  // namespace tapa

#endif  // TAPA_STREAM_H_
