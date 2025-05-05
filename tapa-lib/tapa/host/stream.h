// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_HOST_STREAM_H_
#define TAPA_HOST_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <array>
#include <atomic>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <frt.h>
#include <glog/logging.h>

#include <unistd.h>

#include "tapa/base/stream.h"
#include "tapa/host/coroutine.h"
#include "tapa/host/util.h"

namespace tapa {

template <typename T>
class istream;

template <typename T>
class ostream;

template <typename T, uint64_t S>
class istreams;

template <typename T, uint64_t S>
class ostreams;

namespace internal {

template <typename T>
std::string ToBinaryStringImpl(const elem_t<T>* val) {
  return fpga::ToBinaryString(val->eot) + fpga::ToBinaryString(val->val);
}
template <typename T>
void FromBinaryStringImpl(std::string_view str, elem_t<T>& val) {
  CHECK_GE(str.size(), 1);
  val.eot = fpga::FromBinaryString<bool>(str.substr(0, 1));
  str.remove_prefix(1);
  val.val = fpga::FromBinaryString<T>(str);
}

template <typename Param, typename Arg>
struct accessor;

class type_erased_queue {
 public:
  virtual ~type_erased_queue() = default;

  // debug helpers
  const std::string& get_name() const;
  void set_name(const std::string& name);

  virtual bool empty() const = 0;
  virtual bool full() const = 0;

 protected:
  struct LogContext {
    static std::unique_ptr<LogContext> New(std::string_view name);
    std::ofstream ofs;
    std::mutex mtx;
  };

  std::string name;
  const std::unique_ptr<LogContext> log;

  type_erased_queue(const std::string& name);

  void check_leftover() const;

  template <typename T>
  void maybe_log(const T& elem) {
    if (this->log != nullptr) {
      std::unique_lock<std::mutex> lock(this->log->mtx);
      this->log->ofs << elem << std::endl;
    }
  }
};

template <typename T>
class base_queue : public type_erased_queue {
 public:
  virtual void push(const T& val) = 0;
  virtual T pop() = 0;
  virtual T front() const = 0;

 protected:
  using type_erased_queue::type_erased_queue;
};

template <typename T>
class lock_free_queue : public base_queue<T> {
  // producer writes to head and consumer reads from tail
  // okay to keep incrementing because it'll take > 100 yr to overflow uint64_t
  std::atomic<uint64_t> tail{0};
  std::atomic<uint64_t> head{0};

  std::vector<T> buffer;

 public:
  // constructors
  lock_free_queue(size_t depth, const std::string& name) : base_queue<T>(name) {
    this->buffer.resize(depth);
  }

  // basic queue operations
  bool empty() const override { return this->head - this->tail <= 0; }
  bool full() const override {
    return this->head - this->tail >= this->buffer.size();
  }
  T front() const override { return this->buffer[this->tail % buffer.size()]; }
  T pop() override {
    auto val = this->front();
    ++this->tail;
    return val;
  }
  void push(const T& val) override {
    this->maybe_log(val);
    this->buffer[this->head % buffer.size()] = val;
    ++this->head;
  }

  ~lock_free_queue() { this->check_leftover(); }
};

template <typename T>
class locked_queue : public base_queue<T> {
  size_t depth;
  mutable std::mutex mtx;
  std::deque<T> buffer;

 public:
  // constructors
  locked_queue(size_t depth, const std::string& name)
      : base_queue<T>(name), depth(depth) {}

  // basic queue operations
  bool empty() const override {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.empty();
  }
  bool full() const override {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.size() >= this->depth;
  }
  T front() const override {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->buffer.front();
  }
  T pop() override {
    std::unique_lock<std::mutex> lock(this->mtx);
    auto val = this->buffer.front();
    this->buffer.pop_front();
    return val;
  }
  void push(const T& val) override {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->maybe_log(val);
    this->buffer.push_back(val);
  }

  ~locked_queue() { this->check_leftover(); }
};

// Implementation of `base_queue` that is a wrapper of `fpga::Stream`
template <typename T>
class frt_queue : public base_queue<T> {
 public:
  fpga::Stream<T> stream;

  explicit frt_queue(int64_t depth, const std::string& name)
      : base_queue<T>(name), stream(depth) {}
  bool empty() const override {
    if (this->stream.empty()) {
      // Yield to the OS to allow the simulation to produce data.
      sleep(0);
      return true;
    } else {
      return false;
    }
  }
  bool full() const override {
    if (this->stream.full()) {
      // Yield to the OS to allow the simulation to consume data.
      sleep(0);
      return true;
    } else {
      return false;
    }
  }
  void push(const T& val) override { stream.push(val); }
  T pop() override { return stream.pop(); };
  T front() const override { return stream.front(); }
};

template <typename T>
std::shared_ptr<base_queue<T>> make_queue(bool is_frt, uint64_t depth,
                                          const std::string& name) {
  if (is_frt) {
    VLOG(1) << "channel '" << name << "' created as an FRT stream";
    return std::make_shared<frt_queue<T>>(depth, name);
  } else if (depth == ::tapa::kStreamInfiniteDepth) {
    // It's too expensive to make the lock-free queue have infinite depth.
    VLOG(1) << "channel '" << name << "' created as a locked queue";
    return std::make_shared<locked_queue<T>>(depth, name);
  } else {
    VLOG(1) << "channel '" << name << "' created as a lock-free queue";
    return std::make_shared<lock_free_queue<T>>(depth, name);
  }
}

// A shared pointer of a queue that can be shared among multiple tasks.
// The pointer cannot be copied so that one task can create the queue and
// the other can wait for the queue to be created.
template <typename T>
class shared_queue {
 public:
  shared_queue() : queue_ptr(nullptr) {}
  shared_queue(const shared_queue&) = delete;
  shared_queue(shared_queue&&) = delete;

  // basic queue operations
  bool empty() const { return queue_ptr->empty(); }
  bool full() const { return queue_ptr->full(); }
  T front() const { return queue_ptr->front(); }
  T pop() { return queue_ptr->pop(); }
  void push(const T& val) { queue_ptr->push(val); }

  // check if the queue is initialized or being initialized
  bool is_initialized() { return bool(queue_ptr); }
  bool is_handshaking() {
    std::lock_guard<std::mutex> lock(handshake_mtx);
    return task_id > 0;
  }

  // return the raw pointer for the queue
  base_queue<T>* get() { return queue_ptr.get(); }

  // initialize the queue without handshaking two tasks, such as in a leaf task
  // in this case, frt is always false.
  void initialize_queue_without_handshake(uint64_t depth = 0,
                                          const std::string& name = "") {
    std::lock_guard<std::mutex> lock(handshake_mtx);
    CHECK(task_id == 0) << "handshake is already in progress";
    initialize_queue(false, depth, name);
  }

  // handshaking two tasks to initialize the queue and decide whether it is an
  // FRT queue or not
  void initialize_queue_by_handshake(bool is_frt = false, uint64_t depth = 0,
                                     const std::string& name = "") {
    // ensure that the function is sequentially called to avoid complications
    std::lock_guard<std::mutex> lock(handshake_mtx);
    task_id++;

    if (task_id == 1) {
      // the first task simply returns and allows the second task to create
      // the queue, unless it is already known as an FRT queue
      if (is_frt) initialize_queue(is_frt, depth, name);
    } else if (task_id == 2) {
      // the second task creates the queue if the first task hasn't done it yet
      if (!queue_ptr) initialize_queue(is_frt, depth, name);
    } else {
      LOG(FATAL) << "more than two tasks are trying to connect to a channel";
    }
  }

 private:
  void initialize_queue(bool is_frt = false, uint64_t depth = 0,
                        const std::string& name = "") {
    CHECK(!is_initialized()) << "queue is already initialized";
    queue_ptr = make_queue<T>(is_frt, depth, name);
  }

  std::shared_ptr<base_queue<T>> queue_ptr;
  std::mutex handshake_mtx;
  int task_id = 0;
};

// shared pointer of a queue
template <typename T>
class basic_stream {
 public:
  // debug helpers
  const std::string& get_name() const { return name; }
  void set_name(const std::string& name) { this->name = name; }

  // not protected since we'll use std::vector<basic_stream<T>>
  basic_stream()
      : name(""),
        depth(0),
        simulation_depth(0),
        queue(std::make_shared<shared_queue<elem_t<T>>>()) {}
  basic_stream(const std::string& name, int depth, int simulation_depth)
      : name(name),
        depth(depth),
        simulation_depth(simulation_depth),
        queue(std::make_shared<shared_queue<elem_t<T>>>()) {}

  basic_stream(const basic_stream&) = default;
  basic_stream(basic_stream&&) = default;
  basic_stream& operator=(const basic_stream&) = default;
  basic_stream& operator=(basic_stream&&) = delete;  // -Wvirtual-move-assign

 protected:
  std::string name;
  int depth;
  int simulation_depth;

  std::shared_ptr<shared_queue<elem_t<T>>> ensure_queue() {
    // if the queue is already initialized, return it
    if (queue->is_initialized()) return queue;

    // if the queue is handshaking, wait for it to be initialized
    if (queue->is_handshaking()) {
      while (!queue->is_initialized())
        internal::yield("channel '" + name + "' is handshaking");
      return queue;
    }

    // if reaching this point, the handshake is not performed and the queue is
    // not initialized. we initialize it without handshaking. this happens when
    // the queue is created in a leaf task.
    queue->initialize_queue_without_handshake(simulation_depth, name);
    return queue;
  }

 private:
  template <typename Arg>
  friend Arg access_stream(Arg arg, bool sequential);
  template <typename Arg>
  friend void access_stream(fpga::Instance&, int&, Arg arg);

  void initialize_queue_by_handshake(bool is_frt = false) {
    queue->initialize_queue_by_handshake(is_frt, simulation_depth, name);
  }

  void frt_set_arg(fpga::Instance& instance, int& idx) {
    auto ptr = dynamic_cast<frt_queue<elem_t<T>>*>(ensure_queue()->get());
    CHECK(ptr != nullptr) << "channel '" << get_name()
                          << "' is not an FRT stream, please report a bug";
    instance.SetArg(idx++, ptr->stream);
  }

  // Child class must access `queue` using `ensure_queue()`.
  std::shared_ptr<shared_queue<elem_t<T>>> queue;
};

// shared pointer of multiple queues
template <typename T>
class basic_streams {
 protected:
  struct metadata_t {
    metadata_t(const std::string& name, int pos) : name(name), pos(pos) {}
    std::vector<basic_stream<T>> refs;  // references to the original streams
    const std::string name;             // name of the original streams
    const int pos;                      // position in the original streams
  };

  basic_streams(const std::shared_ptr<metadata_t>& ptr) : ptr({ptr}) {}
  basic_streams(const basic_streams&) = default;
  basic_streams(basic_streams&&) = default;
  basic_streams& operator=(const basic_streams&) = default;
  basic_streams& operator=(basic_streams&&) = delete;  // -Wvirtual-move-assign

  basic_stream<T> operator[](int pos) const {
    CHECK_NOTNULL(ptr.get());
    CHECK_GE(pos, 0);
    CHECK_LT(pos, ptr->refs.size());
    return ptr->refs[pos];
  }

  std::string get_slice_name(int length) {
    return ptr->name + "[" + std::to_string(ptr->pos) + ":" +
           std::to_string(ptr->pos + length) + ")";
  }

  std::shared_ptr<metadata_t> ptr;
};

// stream without a bound depth; can be default-constructed by a derived class
template <typename T>
class unbound_stream : public istream<T>, public ostream<T> {
 protected:
  unbound_stream() : basic_stream<T>() {}
};

// streams without a bound depth; can be default-constructed by a derived class
template <typename T, int S>
class unbound_streams : public istreams<T, S>, public ostreams<T, S> {
 protected:
  unbound_streams() : basic_streams<T>(nullptr) {}
};

template <typename T, typename = void>
struct has_ostream_overload : std::false_type {};

template <typename T>
struct has_ostream_overload<
    T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};

template <typename T>
std::ostream& operator<<(std::ostream& os, const elem_t<T>& elem) {
  if (elem.eot) {
    // For EoT, create an empty line.
  } else if constexpr (has_ostream_overload<T>::value) {
    // For types that overload `operator<<`, use that.
    os << elem.val;
  } else {
    // For types that do not overload `operator<<`, dump bytes in dex.
    auto bytes = bit_cast<std::array<char, sizeof(elem.val)>>(elem.val);
    os << "0x" << std::hex;
    for (int byte : bytes) {
      os << std::setfill('0') << std::setw(2) << byte;
    }
  }
  return os;
}

}  // namespace internal

/// Provides consumer-side operations to a @c tapa::stream where it is used as
/// an @a input.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T>
class istream : virtual public internal::basic_stream<T> {
 public:
  /// Tests whether the stream is empty.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the stream is empty.
  bool empty() {
    bool is_empty = this->ensure_queue()->empty();
    if (is_empty) {
      internal::yield("channel '" + this->get_name() + "' is empty");
    }
    return is_empty;
  }

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @param[out] is_eot Uninitialized if the stream is empty. Otherwise,
  ///                    updated to indicate whether the next token is EoT.
  /// @return            Whether @c is_eot is updated.
  bool try_eot(bool& is_eot) {
    if (!empty()) {
      is_eot = this->ensure_queue()->front().eot;
      return true;
    }
    return false;
  }

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @param[out] is_success Whether the next token is available.
  /// @return                Whether the next token is available and is EoT.
  bool eot(bool& is_success) {
    bool eot = false;
    is_success = try_eot(eot);
    return eot;
  }

  /// Tests whether the next token is EoT.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the next token is available and is EoT.
  bool eot(std::nullptr_t) {
    bool eot = false;
    try_eot(eot);
    return eot;
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
  bool try_peek(T& value) {
    if (!empty()) {
      auto elem = this->ensure_queue()->front();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' peeked when closed";
      }
      value = elem.val;
      return true;
    }
    return false;
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
  T peek(bool& is_success) {
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
  T peek(std::nullptr_t) {
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
  T peek(bool& is_success, bool& is_eot) {
    if (!empty()) {
      auto elem = this->ensure_queue()->front();
      is_success = true;
      is_eot = elem.eot;
      return elem.val;
    }
    is_success = false;
    is_eot = false;
    return {};
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
    if (!empty()) {
      auto elem = this->ensure_queue()->pop();
      if (elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name() << "' read when closed";
      }
      value = elem.val;
      return true;
    }
    return false;
  }

  /// Reads the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @return The value of the next token.
  T read() {
    T val;
    while (!try_read(val)) {
    }
    return val;
  }

  /// Reads the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// The next token must not be EoT.
  ///
  /// @param[out] value The value of the next token.
  /// @return           @c *this.
  istream& operator>>(T& value) {
    value = read();
    return *this;
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
    T val;
    is_success = try_read(val);
    return val;
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
    T val;
    try_read(val);
    return val;
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
    if (!empty()) {
      auto elem = this->ensure_queue()->pop();
      if (!elem.eot) {
        LOG(FATAL) << "channel '" << this->get_name()
                   << "' opened when not closed";
      }
      return true;
    }
    return false;
  }

  /// Consumes an EoT token.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// The next token must be EoT.
  void open() {
    while (!try_open()) {
    }
  }

 protected:
  // allow derived class to omit initialization
  istream() : internal::basic_stream<T>() {}

 private:
  // allow istreams and streams to return istream
  template <typename U, uint64_t S>
  friend class istreams;
  template <typename U, uint64_t S, uint64_t N, uint64_t SimulationDepth>
  friend class streams;
  istream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
};

/// Provides producer-side operations to a @c tapa::stream where it is used as
/// an @a output.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T>
class ostream : virtual public internal::basic_stream<T> {
 public:
  /// Tests whether the stream is full.
  ///
  /// This is a @a non-blocking and @a non-destructive operation.
  ///
  /// @return Whether the stream is full.
  bool full() {
    bool is_full = this->ensure_queue()->full();
    if (is_full) {
      internal::yield("channel '" + this->get_name() + "' is full");
    }
    return is_full;
  }

  /// Writes @c value to the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// @param[in] value The value to write.
  /// @return          Whether @c value has been written successfully.
  bool try_write(const T& value) {
    if (!full()) {
      this->ensure_queue()->push({value, false});
      return true;
    }
    return false;
  }

  /// Writes @c value to the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// @param[in] value The value to write.
  void write(const T& value) {
    while (!try_write(value)) {
    }
  }

  /// Writes @c value to the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  ///
  /// @param[in] value The value to write.
  /// @return          @c *this.
  ostream& operator<<(const T& value) {
    write(value);
    return *this;
  }

  /// Produces an EoT token to the stream.
  ///
  /// This is a @a non-blocking and @a destructive operation.
  ///
  /// @return Whether the EoT token has been written successfully.
  bool try_close() {
    if (!full()) {
      this->ensure_queue()->push({{}, true});
      return true;
    }
    return false;
  }

  /// Produces an EoT token to the stream.
  ///
  /// This is a @a blocking and @a destructive operation.
  void close() {
    while (!try_close()) {
    }
  }

 protected:
  // allow derived class to omit initialization
  ostream() : internal::basic_stream<T>() {}

 private:
  // allow ostreams and streams to return ostream
  template <typename U, uint64_t S>
  friend class ostreams;
  template <typename U, uint64_t S, uint64_t N, uint64_t SimulationDepth>
  friend class streams;
  ostream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
};

/// Defines a communication channel between two task instances.
template <typename T, uint64_t N = kStreamDefaultDepth,
          uint64_t SimulationDepth = N>
class stream : public internal::unbound_stream<T> {
 public:
  /// Depth of the communication channel.
  constexpr static int depth = N;

  /// Constructs a @c tapa::stream.
  stream() : internal::basic_stream<T>("", N, SimulationDepth) {}

  /// Constructs a @c tapa::stream with the given name for debugging.
  ///
  /// @param[in] name Name of the communication channel (for debugging only).
  template <size_t S>
  stream(const char (&name)[S])
      : internal::basic_stream<T>(name, N, SimulationDepth) {}

 private:
  template <typename U, uint64_t friend_length, uint64_t friend_depth,
            uint64_t friend_simulation_depth>
  friend class streams;
  stream(const internal::basic_stream<T>& base)
      : internal::basic_stream<T>(base) {}
};

/// Provides consumer-side operations to an array of @c tapa::stream where they
/// are used as @a inputs.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T, uint64_t S>
class istreams : virtual public internal::basic_streams<T> {
 public:
  /// Length of the @c tapa::stream array.
  constexpr static int length = S;

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return    @c tapa::istream referenced in the array.
  istream<T> operator[](int pos) const {
    return internal::basic_streams<T>::operator[](pos);
  }

 protected:
  // allow derived class to omit initialization
  istreams() : internal::basic_streams<T>(nullptr) {}

  // allow streams of any length to return istreams
  template <typename U, uint64_t friend_length, uint64_t friend_depth,
            uint64_t friend_simulation_depth>
  friend class streams;

  // allow istreams of any length to return istreams
  template <typename U, uint64_t friend_length>
  friend class istreams;

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int access_pos_ = 0;

  istream<T> access() {
    CHECK_LT(access_pos_, this->ptr->refs.size())
        << "istream slice '" << this->get_slice_name(length)
        << "' accessed for " << access_pos_ + 1
        << " times but it only contains " << this->ptr->refs.size()
        << " channels";
    return this->ptr->refs[access_pos_++];
  }
  template <uint64_t length>
  istreams<T, length> access() {
    CHECK_NOTNULL(this->ptr.get());
    istreams<T, length> result;
    result.ptr =
        std::make_shared<typename internal::basic_streams<T>::metadata_t>(
            this->ptr->name, this->ptr->pos);
    result.ptr->refs.reserve(length);
    for (int i = 0; i < length; ++i) {
      result.ptr->refs.emplace_back(access());
    }
    return result;
  }
};

/// Provides producer-side operations to an array of @c tapa::stream where they
/// are used as @a outputs.
///
/// This class should only be used in task function parameters and should never
/// be instatiated directly.
template <typename T, uint64_t S>
class ostreams : virtual public internal::basic_streams<T> {
 public:
  /// Length of the @c tapa::stream array.
  constexpr static int length = S;

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return @c tapa::ostream referenced in the array.
  ostream<T> operator[](int pos) const {
    return internal::basic_streams<T>::operator[](pos);
  }

 protected:
  // allow derived class to omit initialization
  ostreams() : internal::basic_streams<T>(nullptr) {}

  // allow streams of any length to return ostreams
  template <typename U, uint64_t friend_length, uint64_t friend_depth,
            uint64_t friend_simulation_depth>
  friend class streams;

  // allow ostreams of any length to return istreams
  template <typename U, uint64_t friend_length>
  friend class ostreams;

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int access_pos_ = 0;

  ostream<T> access() {
    CHECK_LT(access_pos_, this->ptr->refs.size())
        << "ostream slice '" << this->get_slice_name(length)
        << "' accessed for " << access_pos_ + 1
        << " times but it only contains " << this->ptr->refs.size()
        << " channels";
    return this->ptr->refs[access_pos_++];
  }
  template <uint64_t length>
  ostreams<T, length> access() {
    CHECK_NOTNULL(this->ptr.get());
    ostreams<T, length> result;
    result.ptr =
        std::make_shared<typename internal::basic_streams<T>::metadata_t>(
            this->ptr->name, this->ptr->pos);
    result.ptr->refs.reserve(length);
    for (int i = 0; i < length; ++i) {
      result.ptr->refs.emplace_back(access());
    }
    return result;
  }
};

/// Defines an array of @c tapa::stream.
template <typename T, uint64_t S, uint64_t N = kStreamDefaultDepth,
          uint64_t SimulationDepth = N>
class streams : public internal::unbound_streams<T, S> {
 public:
  /// Count of @c tapa::stream in the array.
  constexpr static int length = S;

  /// Depth of each @c tapa::stream in the array.
  constexpr static int depth = N;

  /// Constructs a @c tapa::streams array.
  streams()
      : internal::basic_streams<T>(
            std::make_shared<typename internal::basic_streams<T>::metadata_t>(
                "", 0)) {
    for (int i = 0; i < S; ++i) {
      this->ptr->refs.emplace_back("", N, SimulationDepth);
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
            std::make_shared<typename internal::basic_streams<T>::metadata_t>(
                name, 0)) {
    for (int i = 0; i < S; ++i) {
      this->ptr->refs.emplace_back(
          std::string(name) + "[" + std::to_string(i) + "]", N,
          SimulationDepth);
    }
  }

  /// References a @c tapa::stream in the array.
  ///
  /// @param pos Position of the array reference.
  /// @return @c tapa::stream referenced in the array.
  stream<T, N> operator[](int pos) const {
    return internal::basic_streams<T>::operator[](pos);
  };

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int istream_access_pos_ = 0;
  int ostream_access_pos_ = 0;

  istream<T> access_as_istream() {
    CHECK_LT(istream_access_pos_, this->ptr->refs.size())
        << "channels '" << this->ptr->name << "' accessed as istream for "
        << istream_access_pos_ + 1 << " times but it only contains "
        << this->ptr->refs.size() << " channels";
    return this->ptr->refs[istream_access_pos_++];
  }
  ostream<T> access_as_ostream() {
    CHECK_LT(ostream_access_pos_, this->ptr->refs.size())
        << "channels '" << this->ptr->name << "' accessed as ostream for "
        << ostream_access_pos_ + 1 << " times but it only contains "
        << this->ptr->refs.size() << " channels";
    return this->ptr->refs[ostream_access_pos_++];
  }
  template <uint64_t length>
  istreams<T, length> access_as_istreams() {
    CHECK_NOTNULL(this->ptr.get());
    istreams<T, length> result;
    result.ptr =
        std::make_shared<typename internal::basic_streams<T>::metadata_t>(
            this->ptr->name, istream_access_pos_);
    result.ptr->refs.reserve(length);
    for (int i = 0; i < length; ++i) {
      result.ptr->refs.emplace_back(access_as_istream());
    }
    return result;
  }
  template <uint64_t length>
  ostreams<T, length> access_as_ostreams() {
    CHECK_NOTNULL(this->ptr.get());
    ostreams<T, length> result;
    result.ptr =
        std::make_shared<typename internal::basic_streams<T>::metadata_t>(
            this->ptr->name, ostream_access_pos_);
    result.ptr->refs.reserve(length);
    for (int i = 0; i < length; ++i) {
      result.ptr->refs.emplace_back(access_as_ostream());
    }
    return result;
  }
};

namespace internal {

// Helper functions for accessing stream(s), friended by `basic_stream`.
template <typename T>
T access_stream(T arg, bool sequential) {
  if (!sequential) arg.initialize_queue_by_handshake(/*is_frt=*/false);
  return arg;
}
template <typename T>
void access_stream(fpga::Instance& instance, int& idx, T arg) {
  arg.initialize_queue_by_handshake(/*is_frt=*/true);
  arg.frt_set_arg(instance, idx);
}
template <typename T>
T access_streams(T arg, bool sequential) {
  for (int i = 0; i < T::length; ++i) {
    access_stream(arg[i], sequential);
  }
  return arg;
}
template <typename T>
void access_streams(fpga::Instance& instance, int& idx, T arg) {
  for (int i = 0; i < T::length; ++i) {
    access_stream(instance, idx, arg[i]);
  }
}

#define TAPA_DEFINE_DEVICE_ACCESSOR(io, arg_ref) /***************************/ \
  /* param = i/ostream, arg = stream */                                        \
  template <typename T, uint64_t N, typename U>                                \
  struct accessor<io##stream<T>&, stream<U, N> arg_ref> {                      \
    static io##stream<T> access(stream<U, N> arg_ref arg, bool sequential) {   \
      return access_stream(arg, sequential);                                   \
    }                                                                          \
    static void access(fpga::Instance& instance, int& idx,                     \
                       stream<U, N> arg_ref arg) {                             \
      return access_stream(instance, idx, arg);                                \
    }                                                                          \
  };

// streams are accessed as stream without reference
TAPA_DEFINE_DEVICE_ACCESSOR(i, )
TAPA_DEFINE_DEVICE_ACCESSOR(i, &)
TAPA_DEFINE_DEVICE_ACCESSOR(o, )
TAPA_DEFINE_DEVICE_ACCESSOR(o, &)
TAPA_DEFINE_DEVICE_ACCESSOR(unbound_, )
TAPA_DEFINE_DEVICE_ACCESSOR(unbound_, &)

#undef TAPA_DEFINE_DEVICE_ACCESSOR

#define TAPA_DEFINE_ACCESSER(io, reference) /********************************/ \
  /* param = i/ostream, arg = streams */                                       \
  template <typename T, uint64_t length, uint64_t depth>                       \
  struct accessor<io##stream<T> reference, streams<T, length, depth>&> {       \
    static io##stream<T> access(streams<T, length, depth>& arg,                \
                                bool sequential) {                             \
      return access_stream(arg.access_as_##io##stream(), sequential);          \
    }                                                                          \
    static void access(fpga::Instance& instance, int& idx,                     \
                       streams<T, length, depth>& arg) {                       \
      return access_stream(instance, idx, arg.access_as_##io##stream());       \
    }                                                                          \
  };                                                                           \
                                                                               \
  /* param = i/ostream, arg = i/ostreams */                                    \
  template <typename T, uint64_t length>                                       \
  struct accessor<io##stream<T> reference, io##streams<T, length>&> {          \
    static io##stream<T> access(io##streams<T, length>& arg, bool) {           \
      return arg.access();                                                     \
    }                                                                          \
  };                                                                           \
                                                                               \
  /* param = i/ostreams, arg = streams */                                      \
  template <typename T, uint64_t param_length, uint64_t arg_length,            \
            uint64_t depth>                                                    \
  struct accessor<io##streams<T, param_length> reference,                      \
                  streams<T, arg_length, depth>&> {                            \
    static io##streams<T, param_length> access(                                \
        streams<T, arg_length, depth>& arg, bool sequential) {                 \
      return access_streams(                                                   \
          arg.template access_as_##io##streams<param_length>(), sequential);   \
    }                                                                          \
    static void access(fpga::Instance& instance, int& idx,                     \
                       streams<T, arg_length, depth>& arg) {                   \
      return access_streams(                                                   \
          instance, idx,                                                       \
          arg.template access_as_##io##streams<param_length>());               \
    }                                                                          \
  };                                                                           \
                                                                               \
  /* param = i/ostreams, arg = i/ostreams */                                   \
  template <typename T, uint64_t param_length, uint64_t arg_length>            \
  struct accessor<io##streams<T, param_length> reference,                      \
                  io##streams<T, arg_length>&> {                               \
    static io##streams<T, param_length> access(                                \
        io##streams<T, arg_length>& arg, bool) {                               \
      return arg.template access<param_length>();                              \
    }                                                                          \
  };

TAPA_DEFINE_ACCESSER(i, )
TAPA_DEFINE_ACCESSER(i, &)
TAPA_DEFINE_ACCESSER(i, &&)
TAPA_DEFINE_ACCESSER(o, )
TAPA_DEFINE_ACCESSER(o, &)
TAPA_DEFINE_ACCESSER(o, &&)

#undef TAPA_DEFINE_ACCESSER

}  // namespace internal

}  // namespace tapa

#endif  // TAPA_HOST_STREAM_H_
