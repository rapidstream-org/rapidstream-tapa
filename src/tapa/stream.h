#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef __SYNTHESIS__

#include <hls_stream.h>

#else  // __SYNTHESIS__

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <glog/logging.h>

#include "tapa/coroutine.h"

#endif  // __SYNTHESIS__

namespace tapa {

#ifndef __SYNTHESIS__

template <typename T>
class istream;

template <typename T>
class ostream;

template <typename T, uint64_t S>
class istreams;

template <typename T, uint64_t S>
class ostreams;

#endif  // __SYNTHESIS__

namespace internal {

template <typename T>
struct elem_t {
  T val;
  bool eot;
};

#ifndef __SYNTHESIS__

template <typename Param, typename Arg>
struct accessor;

class base_queue {
 public:
  // debug helpers
  const std::string& get_name() const { return this->name; }
  void set_name(const std::string& name) { this->name = name; }

 protected:
  std::string name;

  base_queue(const std::string& name) : name(name) {}

  virtual bool empty() const = 0;

  void check_leftover() {
    if (!this->empty()) {
      LOG(WARNING) << "channel '" << this->name
                   << "' destructed with leftovers; hardware behavior may be "
                      "unexpected in consecutive invocations";
    }
  }
};

template <typename T>
class lock_free_queue : public base_queue {
  // producer writes to head and consumer reads from tail
  // okay to keep incrementing because it'll take > 100 yr to overflow uint64_t
  std::atomic<uint64_t> tail{0};
  std::atomic<uint64_t> head{0};

  std::vector<T> buffer;

 public:
  // constructors
  lock_free_queue(size_t depth, const std::string& name = "")
      : base_queue(name) {
    this->buffer.resize(depth);
  }

  // debug helpers
  uint64_t get_depth() const { return this->buffer.size(); }

  // basic queue operations
  bool empty() const override { return this->head - this->tail <= 0; }
  bool full() const { return this->head - this->tail >= this->buffer.size(); }
  const T& front() const { return this->buffer[this->tail % buffer.size()]; }
  T pop() {
    auto val = this->front();
    ++this->tail;
    return val;
  }
  void push(const T& val) {
    this->buffer[this->head % buffer.size()] = val;
    ++this->head;
  }

  ~lock_free_queue() { this->check_leftover(); }
};

template <typename T>
class locked_queue : public base_queue {
  size_t depth;
  mutable std::mutex mtx;
  std::deque<T> buffer;

 public:
  // constructors
  locked_queue(size_t depth, const std::string& name = "")
      : base_queue(name), depth(depth) {}

  // debug helpers
  uint64_t get_depth() const { return this->depth; }

  // basic queue operations
  bool empty() const override {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.empty();
  }
  bool full() const {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.size() >= this->depth;
  }
  const T& front() const {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.front();
  }
  T pop() {
    std::unique_lock<std::mutex> lock(this->mtx);
    auto val = this->buffer.front();
    this->buffer.pop_front();
    return val;
  }
  void push(const T& val) {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->buffer.push_back(val);
  }

  ~locked_queue() { this->check_leftover(); }
};

template <typename T>
#ifdef TAPA_USE_LOCKED_QUEUE
using queue = locked_queue<T>;
#else   // TAPA_USE_LOCKED_QUEUE
using queue = lock_free_queue<T>;
#endif  // TAPA_USE_LOCKED_QUEUE

// shared pointer of a queue
template <typename T>
class basic_stream {
 public:
  // debug helpers
  const std::string& get_name() const { return this->ptr->get_name(); }
  void set_name(const std::string& name) { ptr->set_name(name); }
  uint64_t get_depth() const { return this->ptr->get_depth(); }

  // not protected since we'll use std::vector<basic_stream<T>>
  basic_stream(const std::shared_ptr<queue<elem_t<T>>>& ptr) : ptr(ptr) {}
  basic_stream(const basic_stream&) = default;
  basic_stream(basic_stream&&) = default;
  basic_stream& operator=(const basic_stream&) = default;
  basic_stream& operator=(basic_stream&&) = delete;

 protected:
  std::shared_ptr<queue<elem_t<T>>> ptr;
};

// shared pointer of multiple queues
template <typename T>
class basic_streams {
 protected:
  basic_streams(const std::shared_ptr<std::vector<basic_stream<T>>>& ptr)
      : ptr(ptr) {}
  basic_streams(const basic_streams&) = default;
  basic_streams(basic_streams&&) = default;
  basic_streams& operator=(const basic_streams&) = default;
  basic_streams& operator=(basic_streams&&) = delete;

  std::shared_ptr<std::vector<basic_stream<T>>> ptr;
};

// stream without a bound depth; can be default-constructed by a derived class
template <typename T>
class unbound_stream : public istream<T>, public ostream<T> {
 protected:
  unbound_stream() : basic_stream<T>(nullptr) {}
};

// streams without a bound depth; can be default-constructed by a derived class
template <typename T, int S>
class unbound_streams : public istreams<T, S>, public ostreams<T, S> {
 protected:
  unbound_streams() : basic_streams<T>(nullptr) {}
};

#endif  // __SYNTHESIS__

}  // namespace internal

template <typename T>
class istream
#ifndef __SYNTHESIS__
    : virtual public internal::basic_stream<T>
#endif  // __SYNTHESIS__
{
 public:
  // consumer const operations

  // whether stream is empty
  bool empty() const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    return _.empty();
#else   // __SYNTHESIS__
    bool is_empty = this->ptr->empty();
    if (is_empty) {
      internal::yield("channel '" + this->get_name() + "' is empty");
    }
    return is_empty;
#endif  // __SYNTHESIS__
  }

  // whether stream has ended
  bool try_eot(bool& eot) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    return !empty() && _peek.read_nb(elem) && (void(eot = elem.eot), true);
#else   // __SYNTHESIS__
    if (!empty()) {
      eot = this->ptr->front().eot;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  bool eot(bool& succeeded) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    bool eot = false;
    succeeded = try_eot(eot);
    return eot;
  }
  bool eot(std::nullptr_t) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    bool eot = false;
    try_eot(eot);
    return eot;
  }
  // non-blocking non-destructive read
  bool try_peek(T& val) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#pragma HLS inline
    internal::elem_t<T> elem;
    return !empty() && _peek.read_nb(elem) && (void(val = elem.val), true);
#else   // __SYNTHESIS__
    if (!empty()) {
      auto& elem = this->ptr->front();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' peeked when closed";
      }
      val = elem.val;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  // alternative non-blocking non-destructive read
  T peek(bool& succeeded) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    T val;
    succeeded = try_peek(val);
    return val;
  }
  T peek(std::nullptr_t) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    T val;
    try_peek(val);
    return val;
  }
  // peek val and eot at the same time
  T peek(bool& succeeded, bool& is_eot) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> peek_val;
    (succeeded = !empty()) && _peek.read_nb(peek_val);
    is_eot = peek_val.eot && succeeded;
    return peek_val.val;
#else   // __SYNTHESIS__
    if (!empty()) {
      auto& elem = this->ptr->front();
      succeeded = true;
      is_eot = elem.eot;
      return elem.val;
    }
    succeeded = false;
    is_eot = false;
    return {};
#endif  // __SYNTHESIS__
  }

  // consumer non-const operations

  // non-blocking destructive read
  bool try_read(T& val) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    return _.read_nb(elem) && (void(val = elem.val), true);
#else   // __SYNTHESIS__
    if (!empty()) {
      auto elem = this->ptr->pop();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' read when closed";
      }
      val = elem.val;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  // blocking destructive read
  T read() {
#ifdef __SYNTHESIS__
#pragma HLS inline
    return _.read().val;
#else   // __SYNTHESIS__
    T val;
    while (!try_read(val)) {
    }
    return val;
#endif  // __SYNTHESIS__
  }
  // alterative non-blocking destructive read
  T read(bool& succeeded) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    succeeded = _.read_nb(elem);
    return elem.val;
#else   // __SYNTHESIS__
    T val;
    memset(&val, 0xcc, sizeof(val));
    succeeded = try_read(val);
    return val;
#endif  // __SYNTHESIS__
  }
  T read(std::nullptr_t) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    _.read_nb(elem);
    return elem.val;
#else   // __SYNTHESIS__
    T val;
    memset(&val, 0xcc, sizeof(val));
    try_read(val);
    return val;
#endif  // __SYNTHESIS__
  }
  // non-blocking destructive read with default value
  T read(bool* succeeded_ret, const T& default_val) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    bool succeeded = _.read_nb(elem);
    if (succeeded_ret != nullptr) *succeeded_ret = succeeded;
    return succeeded ? elem.val : default_val;
#else   // __SYNTHESIS__
    T val;
    bool succeeded = try_read(val);
    if (succeeded_ret != nullptr) {
      *succeeded_ret = succeeded;
    }
    return succeeded ? val : default_val;
#endif  // __SYNTHESIS__
  }

  // non-blocking destructive open
  bool try_open() {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    const bool succeeded = _.read_nb(elem);
    assert(!succeeded || elem.eot);
    return succeeded;
#else   // __SYNTHESIS__
    if (!empty()) {
      auto elem = this->ptr->pop();
      if (!elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name()
                   << "' opened when not closed";
      }
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  // blocking destructive open
  void open() {
#ifdef __SYNTHESIS__
#pragma HLS inline
    const auto elem = _.read();
    assert(elem.eot);
#else   // __SYNTHESIS__
    while (!try_open()) {
    }
#endif  // __SYNTHESIS__
  }

#ifdef __SYNTHESIS__
  hls::stream<internal::elem_t<T>> _;
  mutable hls::stream<internal::elem_t<T>> _peek;
#else   // __SYNTHESIS__
 protected:
  // allow derived class to omit initialization
  istream() : internal::basic_stream<T>(nullptr) {}

 private:
  // allow istreams and streams to return istream
  template <typename U, uint64_t S>
  friend class istreams;
  template <typename U, uint64_t S, uint64_t N>
  friend class streams;
  istream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
#endif  // __SYNTHESIS__
};

template <typename T>
class ostream
#ifndef __SYNTHESIS__
    : virtual public internal::basic_stream<T>
#endif  // __SYNTHESIS__
{
 public:
  // producer const operations

  // whether stream is full
  bool full() const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    return _.full();
#else   // __SYNTHESIS__
    bool is_full = this->ptr->full();
    if (is_full) {
      internal::yield("channel '" + this->get_name() + "' is full");
    }
    return is_full;
#endif  // __SYNTHESIS__
  }

  // producer non-const operations

  // non-blocking write
  bool try_write(const T& val) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    return _.write_nb({val, false});
#else   // __SYNTHESIS__
    if (!full()) {
      this->ptr->push({val, false});
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  // blocking write
  void write(const T& val) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    _.write({val, false});
#else   // __SYNTHESIS__
    while (!try_write(val)) {
    }
#endif  // __SYNTHESIS__
  }
  // non-blocking close
  bool try_close() {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eot = true;
    return _.write_nb(elem);
#else   // __SYNTHESIS__
    if (!full()) {
      this->ptr->push({{}, true});
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }
  // blocking close
  void close() {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    memset(&elem.val, 0, sizeof(elem.val));
    elem.eot = true;
    _.write(elem);
#else   // __SYNTHESIS__
    while (!try_close()) {
    }
#endif  // __SYNTHESIS__
  }

#ifdef __SYNTHESIS__
  hls::stream<internal::elem_t<T>> _;
#else   // __SYNTHESIS__
 protected:
  // allow derived class to omit initialization
  ostream() : internal::basic_stream<T>(nullptr) {}

 private:
  // allow ostreams and streams to return ostream
  template <typename U, uint64_t S>
  friend class ostreams;
  template <typename U, uint64_t S, uint64_t N>
  friend class streams;
  ostream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
#endif  // __SYNTHESIS__
};

template <typename T, uint64_t N>
class stream
#ifndef __SYNTHESIS__
    : public internal::unbound_stream<T> {
 public:
  constexpr static int depth = N;

  stream()
      : internal::basic_stream<T>(
            std::make_shared<internal::queue<internal::elem_t<T>>>(N)) {}
  template <size_t S>
  stream(const char (&name)[S])
      : internal::basic_stream<T>(
            std::make_shared<internal::queue<internal::elem_t<T>>>(N, name)) {}

 private:
  template <typename U, uint64_t friend_length, uint64_t friend_depth>
  friend class streams;
  stream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
}
#endif  // __SYNTHESIS__
;

template <typename T, uint64_t depth>
using channel = stream<T, depth>;

template <typename T, uint64_t S>
#ifdef __SYNTHESIS__
using istreams = istream<T>[S];
#else   // __SYNTHESIS__
class istreams : virtual public internal::basic_streams<T> {
 public:
  constexpr static int length = S;

  istream<T> operator[](int pos) const {
    CHECK_NOTNULL(this->ptr);
    CHECK_GE(pos, 0);
    CHECK_LT(pos, this->ptr->size());
    return (*this->ptr)[pos];
  }

 protected:
  // allow derived class to omit initialization
  istreams() : internal::basic_streams<T>(nullptr) {}

  // allow streams of any length to return istreams
  template <typename U, uint64_t friend_length, uint64_t friend_depth>
  friend class streams;

  // allow istreams of any length to return istreams
  template <typename U, uint64_t friend_length>
  friend class istreams;

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int access_pos_ = 0;

  istream<T> access() {
    CHECK_LT(access_pos_, this->ptr->size());
    return (*this->ptr)[access_pos_++];
  }
  template <uint64_t length>
  istreams<T, length> access() {
    CHECK_NOTNULL(this->ptr);
    istreams<T, length> result;
    result.ptr = std::make_shared<std::vector<internal::basic_stream<T>>>();
    result.ptr->reserve(length);
    for (int i = 0; i < length; ++i) {
      CHECK_LT(access_pos_, this->ptr->size())
          << "istreams accessed for " << access_pos_ + 1
          << " times but it only contains " << this->ptr->size() << " istreams";
      result.ptr->emplace_back((*this->ptr)[access_pos_++]);
    }
    return result;
  }
};
#endif  // __SYNTHESIS__

template <typename T, uint64_t S>
#ifdef __SYNTHESIS__
using ostreams = ostream<T>[S];
#else   // __SYNTHESIS__
class ostreams : virtual public internal::basic_streams<T> {
 public:
  constexpr static int length = S;

  ostream<T> operator[](int pos) const {
    CHECK_NOTNULL(this->ptr);
    CHECK_GE(pos, 0);
    CHECK_LT(pos, this->ptr->size());
    return (*this->ptr)[pos];
  }

 protected:
  // allow derived class to omit initialization
  ostreams() : internal::basic_streams<T>(nullptr) {}

  // allow streams of any length to return ostreams
  template <typename U, uint64_t friend_length, uint64_t friend_depth>
  friend class streams;

  // allow ostreams of any length to return istreams
  template <typename U, uint64_t friend_length>
  friend class ostreams;

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int access_pos_ = 0;

  ostream<T> access() {
    CHECK_LT(access_pos_, this->ptr->size());
    return (*this->ptr)[access_pos_++];
  }
  template <uint64_t length>
  ostreams<T, length> access() {
    CHECK_NOTNULL(this->ptr);
    ostreams<T, length> result;
    result.ptr = std::make_shared<std::vector<internal::basic_stream<T>>>();
    result.ptr->reserve(length);
    for (int i = 0; i < length; ++i) {
      CHECK_LT(access_pos_, this->ptr->size())
          << "ostreams accessed for " << access_pos_ + 1
          << " times but it only contains " << this->ptr->size() << " ostreams";
      result.ptr->emplace_back((*this->ptr)[access_pos_++]);
    }
    return result;
  }
};
#endif  // __SYNTHESIS__

template <typename T, uint64_t S, uint64_t N>
class streams
#ifndef __SYNTHESIS__
    : public internal::unbound_streams<T, S> {
 public:
  constexpr static int length = S;
  constexpr static int depth = N;

  streams()
      : internal::basic_streams<T>(
            std::make_shared<std::vector<internal::basic_stream<T>>>()) {
    for (int i = 0; i < S; ++i) {
      this->ptr->emplace_back(
          std::make_shared<internal::queue<internal::elem_t<T>>>(N));
    }
  }

  template <size_t name_length>
  streams(const char (&name)[name_length])
      : internal::basic_streams<T>(
            std::make_shared<std::vector<internal::basic_stream<T>>>()),
        name(name) {
    for (int i = 0; i < S; ++i) {
      this->ptr->emplace_back(
          std::make_shared<internal::queue<internal::elem_t<T>>>(
              N, this->name + "[" + std::to_string(i) + "]"));
    }
  }

  stream<T, N> operator[](int pos) const {
    CHECK_NOTNULL(this->ptr);
    CHECK_GE(pos, 0);
    CHECK_LT(pos, this->ptr->size());
    return (*this->ptr)[pos];
  };

 private:
  std::string name;

  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int istream_access_pos_ = 0;
  int ostream_access_pos_ = 0;

  istream<T> access_as_istream() {
    CHECK_LT(istream_access_pos_, this->ptr->size());
    return (*this->ptr)[istream_access_pos_++];
  }
  ostream<T> access_as_ostream() {
    CHECK_LT(ostream_access_pos_, this->ptr->size());
    return (*this->ptr)[ostream_access_pos_++];
  }
  template <uint64_t length>
  istreams<T, length> access_as_istreams() {
    CHECK_NOTNULL(this->ptr);
    istreams<T, length> result;
    result.ptr = std::make_shared<std::vector<internal::basic_stream<T>>>();
    result.ptr->reserve(length);
    for (int i = 0; i < length; ++i) {
      CHECK_LT(istream_access_pos_, this->ptr->size())
          << "channels '" << name << "' accessed as istream for "
          << istream_access_pos_ + 1 << " times but it only contains "
          << this->ptr->size() << " channels";
      result.ptr->emplace_back((*this->ptr)[istream_access_pos_++]);
    }
    return result;
  }
  template <uint64_t length>
  ostreams<T, length> access_as_ostreams() {
    CHECK_NOTNULL(this->ptr);
    ostreams<T, length> result;
    result.ptr = std::make_shared<std::vector<internal::basic_stream<T>>>();
    result.ptr->reserve(length);
    for (int i = 0; i < length; ++i) {
      CHECK_LT(ostream_access_pos_, this->ptr->size())
          << "channels '" << name << "' accessed as ostream for "
          << ostream_access_pos_ + 1 << " times but it only contains "
          << this->ptr->size() << " channels";
      result.ptr->emplace_back((*this->ptr)[ostream_access_pos_++]);
    }
    return result;
  }
}
#endif  // __SYNTHESIS__
;

template <typename T, uint64_t S, uint64_t N>
using channels = streams<T, S, N>;

#ifndef __SYNTHESIS__

namespace internal {

#define TAPA_DEFINE_ACCESSER(io, reference)                              \
  /* param = i/ostream, arg = streams */                                 \
  template <typename T, uint64_t length, uint64_t depth>                 \
  struct accessor<io##stream<T> reference, streams<T, length, depth>&> { \
    static io##stream<T> access(streams<T, length, depth>& arg) {        \
      return arg.access_as_##io##stream();                               \
    }                                                                    \
  };                                                                     \
                                                                         \
  /* param = i/ostream, arg = i/ostreams */                              \
  template <typename T, uint64_t length>                                 \
  struct accessor<io##stream<T> reference, io##streams<T, length>&> {    \
    static io##stream<T> access(io##streams<T, length>& arg) {           \
      return arg.access();                                               \
    }                                                                    \
  };                                                                     \
                                                                         \
  /* param = i/ostreams, arg = streams */                                \
  template <typename T, uint64_t param_length, uint64_t arg_length,      \
            uint64_t depth>                                              \
  struct accessor<io##streams<T, param_length> reference,                \
                  streams<T, arg_length, depth>&> {                      \
    static io##streams<T, param_length> access(                          \
        streams<T, arg_length, depth>& arg) {                            \
      return arg.template access_as_##io##streams<param_length>();       \
    }                                                                    \
  };                                                                     \
                                                                         \
  /* param = i/ostreams, arg = i/ostreams */                             \
  template <typename T, uint64_t param_length, uint64_t arg_length>      \
  struct accessor<io##streams<T, param_length> reference,                \
                  io##streams<T, arg_length>&> {                         \
    static io##streams<T, param_length> access(                          \
        io##streams<T, arg_length>& arg) {                               \
      return arg.template access<param_length>();                        \
    }                                                                    \
  };

TAPA_DEFINE_ACCESSER(i, )
TAPA_DEFINE_ACCESSER(i, &)
TAPA_DEFINE_ACCESSER(i, &&)
TAPA_DEFINE_ACCESSER(o, )
TAPA_DEFINE_ACCESSER(o, &)
TAPA_DEFINE_ACCESSER(o, &&)

#undef TAPA_DEFINE_ACCESSER

}  // namespace internal

#endif  // __SYNTHESIS__

}  // namespace tapa

#endif  // TAPA_STREAM_H_
