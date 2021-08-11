#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <cstddef>
#include <cstring>

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <glog/logging.h>

#include "tapa/coroutine.h"

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
    this->buffer.resize(depth + 1);
  }

  // debug helpers
  uint64_t get_depth() const { return this->buffer.size() - 1; }

  // basic queue operations
  bool empty() const override { return this->head == this->tail; }
  bool full() const { return this->head - this->tail == this->buffer.size(); }
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

template <typename T>
struct elem_t {
  T val;
  bool eos;
};

template <typename T>
class base {
 public:
  // debug helpers
  const std::string& get_name() const { return this->ptr->get_name(); }
  void set_name(const std::string& name) { ptr->set_name(name); }
  uint64_t get_depth() const { return this->ptr->get_depth(); }

 protected:
  base(std::shared_ptr<queue<elem_t<T>>> ptr) : ptr(ptr) {}

  std::shared_ptr<queue<elem_t<T>>> ptr;
};

template <typename T>
class stream : public istream<T>, public ostream<T> {
 protected:
  stream() : base<T>(nullptr) {}
};

template <typename T, int S>
class streams : public istreams<T, S>, public ostreams<T, S> {};

}  // namespace internal

template <typename T>
class istream : virtual public internal::base<T> {
 public:
  // default copy constructor
  istream(const istream&) = default;
  // default copy assignment operator
  istream& operator=(const istream&) = default;

  // consumer const operations

  // whether stream is empty
  bool empty() const {
    bool is_empty = this->ptr->empty();
    if (is_empty) {
      internal::yield("channel '" + this->get_name() + "' is empty");
    }
    return is_empty;
  }
  // whether stream has ended
  bool try_eos(bool& eos) const {
    if (!empty()) {
      eos = this->ptr->front().eos;
      return true;
    }
    return false;
  }
  bool eos(bool& succeeded) const {
    bool eos = false;
    succeeded = try_eos(eos);
    return eos;
  }
  bool eos(std::nullptr_t) const {
    bool eos = false;
    try_eos(eos);
    return eos;
  }
  // non-blocking non-destructive read
  bool try_peek(T& val) const {
    if (!empty()) {
      auto& elem = this->ptr->front();
      if (elem.eos) {
        LOG(FATAL) << "channel '" << this->get_name() << "' peeked when closed";
      }
      val = elem.val;
      return true;
    }
    return false;
  }
  // alternative non-blocking non-destructive read
  T peek(bool& succeeded) const {
    T val;
    memset(&val, 0xcc, sizeof(val));
    succeeded = try_peek(val);
    return val;
  }
  T peek(std::nullptr_t) const {
    T val;
    memset(&val, 0xcc, sizeof(val));
    try_peek(val);
    return val;
  }
  // peek val and eos at the same time
  T peek(bool& succeeded, bool& is_eos) const {
    if (!empty()) {
      auto& elem = this->ptr->front();
      succeeded = true;
      is_eos = elem.eos;
      return elem.val;
    }
    succeeded = false;
    is_eos = false;
    return {};
  }

  // consumer non-const operations

  // non-blocking destructive read
  bool try_read(T& val) {
    if (!empty()) {
      auto elem = this->ptr->pop();
      if (elem.eos) {
        LOG(FATAL) << "channel '" << this->get_name() << "' read when closed";
      }
      val = elem.val;
      return true;
    }
    return false;
  }
  // blocking destructive read
  T read() {
    T val;
    while (!try_read(val)) {
    }
    return val;
  }
  // alterative non-blocking destructive read
  T read(bool& succeeded) {
    T val;
    memset(&val, 0xcc, sizeof(val));
    succeeded = try_read(val);
    return val;
  }
  T read(std::nullptr_t) {
    T val;
    memset(&val, 0xcc, sizeof(val));
    try_read(val);
    return val;
  }
  // non-blocking destructive read with default value
  T read(bool* succeeded_ret, const T& default_val) {
    T val;
    bool succeeded = try_read(val);
    if (succeeded_ret != nullptr) {
      *succeeded_ret = succeeded;
    }
    return succeeded ? val : default_val;
  }

  // non-blocking destructive open
  bool try_open() {
    if (!empty()) {
      auto elem = this->ptr->pop();
      if (!elem.eos) {
        LOG(FATAL) << "channel '" << this->get_name()
                   << "' opened when not closed";
      }
      return true;
    }
    return false;
  }
  // blocking destructive open
  void open() {
    while (!try_open()) {
    }
  }

 protected:
  istream() : internal::base<T>(nullptr) {}
};

template <typename T>
class ostream : virtual public internal::base<T> {
 public:
  // default copy constructor
  ostream(const ostream&) = default;
  // deleted move constructor
  ostream& operator=(const ostream&) = default;

  // producer const operations

  // whether stream is full
  bool full() const {
    bool is_full = this->ptr->full();
    if (is_full) {
      internal::yield("channel '" + this->get_name() + "' is full");
    }
    return is_full;
  }

  // producer non-const operations

  // non-blocking write
  bool try_write(const T& val) {
    if (!full()) {
      this->ptr->push({val, false});
      return true;
    }
    return false;
  }
  // blocking write
  void write(const T& val) {
    while (!try_write(val)) {
    }
  }
  // non-blocking close
  bool try_close() {
    if (!full()) {
      this->ptr->push({{}, true});
      return true;
    }
    return false;
  }
  // blocking close
  void close() {
    while (!try_close()) {
    }
  }

 protected:
  ostream() : internal::base<T>(nullptr) {}
};

template <typename T, uint64_t N = 1>
class stream : public internal::stream<T> {
 public:
  stream()
      : internal::base<T>(
            std::make_shared<internal::queue<internal::elem_t<T>>>(N)) {}
  template <size_t S>
  stream(const char (&name)[S])
      : internal::base<T>(
            std::make_shared<internal::queue<internal::elem_t<T>>>(N, name)) {}

  constexpr static uint64_t depth{N};
};

template <typename T, uint64_t S>
class istreams {
 public:
  virtual istream<T>& operator[](int idx) = 0;
};

template <typename T, uint64_t S>
class ostreams {
 public:
  virtual ostream<T>& operator[](int idx) = 0;
};

template <typename T, uint64_t S, uint64_t N = 1>
class streams : public internal::streams<T, S> {
  stream<T, N> streams_[S];

 public:
  // constructors
  streams() {}
  template <size_t SS>
  streams(const char (&name)[SS]) {
    for (int i = 0; i < S; ++i) {
      streams_[i].set_name(std::string(name) + "[" + std::to_string(i) + "]");
    }
  }
  // deleted copy constructor
  streams(const streams&) = default;
  // default move constructor
  streams(streams&&) = default;
  // deleted copy assignment operator
  streams& operator=(const streams&) = default;
  // default move assignment operator
  streams& operator=(streams&&) = default;

  stream<T, N>& operator[](int idx) override { return streams_[idx]; };
};

}  // namespace tapa

#endif  // TAPA_STREAM_H_
