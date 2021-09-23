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

/// Provides consumer-side operations to a @c tapa::stream where it is used as
/// an @a input.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T>
class istream
#ifndef __SYNTHESIS__
    : virtual public internal::basic_stream<T>
#endif  // __SYNTHESIS__
{
 public:
  /// Tests whether the stream is empty.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the stream is empty.
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

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @param[out] is_eot Uninitialized if the stream is empty. Otherwise,
  ///                    updated to indicate whether the next token is EoT.
  /// @return            Whether @c is_eot is updated.
  bool try_eot(bool& is_eot) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    const bool is_success = !empty() && _peek.read_nb(elem);
    is_eot = elem.eot;
    return is_success;
#else   // __SYNTHESIS__
    if (!empty()) {
      is_eot = this->ptr->front().eot;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @param[out] is_success Whether the next token is available.
  /// @return                Whether the next token is available and is EoT.
  bool eot(bool& is_success) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    bool eot = false;
    is_success = try_eot(eot);
    return eot;
  }

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the next token is available and is EoT.
  bool eot(std::nullptr_t) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    bool is_success;
    return eot(is_success) && is_success;
#else   // __SYNTHESIS__
    bool eot = false;
    try_eot(eot);
    return eot;
#endif  // __SYNTHESIS__
  }

  /// Peeks the stream.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @param[out] value Uninitialized if the stream is empty. Otherwise, updated
  ///                   to be the value of the next token.
  /// @return           Whether @c value is updated.
  bool try_peek(T& value) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#pragma HLS inline
    internal::elem_t<T> elem;
    const bool is_success = !empty() && _peek.read_nb(elem);
    value = elem.val;
    return is_success;
#else   // __SYNTHESIS__
    if (!empty()) {
      auto& elem = this->ptr->front();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' peeked when closed";
      }
      value = elem.val;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }

  /// Peeks the stream.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @param[out] is_success Whether the next token is available.
  /// @return                The value of the next token is returned if it is
  ///                        available. Otherwise, default-constructed @c T() is
  ///                        returned.
  T peek(bool& is_success) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    T val;
    is_success = try_peek(val);
    return val;
  }

  /// Peeks the stream.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @return The value of the next token is returned if it is available.
  ///         Otherwise, default-constructed @c T() is returned.
  T peek(std::nullptr_t) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
#endif  // __SYNTHESIS__
    T val;
    try_peek(val);
    return val;
  }

  /// Peeks the stream.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @param[out] is_success Whether the next token is available.
  /// @param[out] is_eot     Set to @c false if the stream is empty. Otherwise,
  ///                        updated to indicate whether the next token is EoT.
  /// @return                The value of the next token is returned if it is
  ///                        available. Otherwise, default-constructed @c T() is
  ///                        returned.
  T peek(bool& is_success, bool& is_eot) const {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> peek_val;
    (is_success = !empty()) && _peek.read_nb(peek_val);
    is_eot = peek_val.eot && is_success;
    return peek_val.val;
#else   // __SYNTHESIS__
    if (!empty()) {
      auto& elem = this->ptr->front();
      is_success = true;
      is_eot = elem.eot;
      return elem.val;
    }
    is_success = false;
    is_eot = false;
    return {};
#endif  // __SYNTHESIS__
  }

  /// Reads the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @param[out] value Uninitialized if the stream is empty. Otherwise, updated
  ///                   to be the value of the next token.
  /// @return           Whether @c value is updated.
  bool try_read(T& value) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    const bool is_success = _.read_nb(elem);
    value = elem.val;
    return is_success;
#else   // __SYNTHESIS__
    if (!empty()) {
      auto elem = this->ptr->pop();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' read when closed";
      }
      value = elem.val;
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }

  /// Reads the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @return The value of the next token.
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

  /// Reads the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @param[out] is_success Whether the next token is available.
  /// @return                The value of the next token is returned if it is
  ///                        available. Otherwise, default-constructed @c T() is
  ///                        returned.
  T read(bool& is_success) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    is_success = _.read_nb(elem);
    return elem.val;
#else   // __SYNTHESIS__
    T val;
    is_success = try_read(val);
    return val;
#endif  // __SYNTHESIS__
  }

  /// Reads the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @return The value of the next token is returned if it is available.
  ///         Otherwise, default-constructed @c T() is returned.
  T read(std::nullptr_t) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    _.read_nb(elem);
    return elem.val;
#else   // __SYNTHESIS__
    T val;
    try_read(val);
    return val;
#endif  // __SYNTHESIS__
  }

  /// Reads the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// @param[in] default_value Value to return if the stream is empty.
  /// @param[out] is_success   Updated to indicate whether the next token is
  ///                          available if @c is_success is not @c nullptr.
  /// @return                  The value of the next token is returned if it is
  ///                          available. Otherwise, @c default_value is
  ///                          returned.
  T read(const T& default_value, bool* is_success = nullptr) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    internal::elem_t<T> elem;
    bool is_success_val = _.read_nb(elem);
    if (is_success != nullptr) {
      *is_success = is_success_val;
    }
    return is_success_val ? elem.val : default_value;
#endif  // __SYNTHESIS__
    T val;
    bool succeeded = try_read(val);
    if (is_success != nullptr) {
      *is_success = succeeded;
    }
    return succeeded ? val : default_value;
  }

  /// Consumes an EoT token.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// The next token must be EoT.
  ///
  /// @return Whether an EoT token is consumed.
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

  /// Consumes an EoT token.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// The next token must be EoT.
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

/// Provides producer-side operations to a @c tapa::stream where it is used as
/// an @a output.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T>
class ostream
#ifndef __SYNTHESIS__
    : virtual public internal::basic_stream<T>
#endif  // __SYNTHESIS__
{
 public:
  /// Tests whether the stream is full.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the stream is full.
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

  /// Writes @c value to the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// @param[in] value The value to write.
  /// @return          Whether @c value has been written successfully.
  bool try_write(const T& value) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    return _.write_nb({value, false});
#else   // __SYNTHESIS__
    if (!full()) {
      this->ptr->push({value, false});
      return true;
    }
    return false;
#endif  // __SYNTHESIS__
  }

  /// Writes @c value to the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// @param[in] value The value to write.
  void write(const T& value) {
#ifdef __SYNTHESIS__
#pragma HLS inline
    _.write({value, false});
#else   // __SYNTHESIS__
    while (!try_write(value)) {
    }
#endif  // __SYNTHESIS__
  }

  /// Produces an EoT token to the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// @return Whether the EoT token has been written successfully.
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

  /// Produces an EoT token to the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
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

/// Defines a communication channel between two task instances.
template <typename T, uint64_t N>
class stream
#ifndef __SYNTHESIS__
    : public internal::unbound_stream<T> {
 public:
  /// Depth of the communication channel.
  constexpr static int depth = N;

  /// Constructs a @c tapa::stream.
  stream()
      : internal::basic_stream<T>(
            std::make_shared<internal::queue<internal::elem_t<T>>>(N)) {}

  /// Constructs a @c tapa::stream with the given name for debugging.
  ///
  /// @param[in] name Name of the communication channel (for debugging only).
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

/// Alternative name of @c tapa::stream.
template <typename T, uint64_t depth>
using channel = stream<T, depth>;

/// Provides consumer-side operations to an array of @c tapa::stream where they
/// are used as @a inputs.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T, uint64_t S>
#ifdef __SYNTHESIS__
using istreams = istream<T>[S];
#else   // __SYNTHESIS__
class istreams : virtual public internal::basic_streams<T> {
 public:
  /// Length of the @c tapa::stream array.
  constexpr static int length = S;

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return    @c tapa::istream referenced in the array.
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

/// Provides producer-side operations to an array of @c tapa::stream where they
/// are used as @a outputs.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T, uint64_t S>
#ifdef __SYNTHESIS__
using ostreams = ostream<T>[S];
#else   // __SYNTHESIS__
class ostreams : virtual public internal::basic_streams<T> {
 public:
  /// Length of the @c tapa::stream array.
  constexpr static int length = S;

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return @c tapa::ostream referenced in the array.
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

/// Defines an array of @c tapa::stream.
template <typename T, uint64_t S, uint64_t N>
class streams
#ifndef __SYNTHESIS__
    : public internal::unbound_streams<T, S> {
 public:
  /// Count of @c tapa::stream in the array.
  constexpr static int length = S;

  /// Depth of each @c tapa::stream in the array.
  constexpr static int depth = N;

  /// Constructs a @c tapa::streams array.
  streams()
      : internal::basic_streams<T>(
            std::make_shared<std::vector<internal::basic_stream<T>>>()) {
    for (int i = 0; i < S; ++i) {
      this->ptr->emplace_back(
          std::make_shared<internal::queue<internal::elem_t<T>>>(N));
    }
  }

  /// Constructs a @c tapa::streams array with the given base name for
  /// debugging.
  ///
  /// The actual name of each @c tapa::stream would be <tt>name[i]</tt>.
  ///
  /// @param[in] name Base name of the streams (for debugging only).
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

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return @c tapa::stream referenced in the array.
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

/// Alternative name of @c tapa::streams.
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
