#ifndef TLP_STREAM_H_
#define TLP_STREAM_H_

#include <cstddef>

#include <array>
#include <atomic>
#include <stdexcept>
#include <string>

#include "tlp/coroutine.h"

namespace tlp {

template <typename T>
struct elem_t {
  T val;
  bool eos;
};

class stream_base {
 public:
  // debug helpers

  virtual const char* get_name() const = 0;
  virtual uint64_t get_depth() const = 0;
};

template <typename T>
class istream : virtual public stream_base {
 public:
  // consumer const operations

  // whether stream is empty
  virtual bool empty() const = 0;
  // whether stream has ended
  virtual bool try_eos(bool&) const = 0;
  virtual bool eos(bool&) const = 0;
  virtual bool eos(std::nullptr_t) const = 0;
  // non-blocking non-destructive read
  virtual bool try_peek(T&) const = 0;
  // alternative non-blocking non-destructive read
  virtual T peek(bool&) const = 0;
  virtual T peek(std::nullptr_t) const = 0;
  // alternative non-blocking non-destructive read with eos
  virtual T peek(bool&, bool&) const = 0;

  // consumer non-const operations

  // non-blocking destructive read
  virtual bool try_read(T&) = 0;
  // blocking destructive read
  virtual T read() = 0;
  // alterative non-blocking destructive read
  virtual T read(bool&) = 0;
  virtual T read(std::nullptr_t) = 0;
  // non-blocking destructive read with default value
  virtual T read(bool*, const T&) = 0;
  // non-blocking open
  virtual bool try_open() = 0;
  // blocking open
  virtual void open() = 0;
};

template <typename T>
class ostream : virtual public stream_base {
 public:
  // producer const operations

  // whether stream is full
  virtual bool full() const = 0;

  // producer non-const operations

  // non-blocking write
  virtual bool try_write(const T&) = 0;
  // blocking write
  virtual void write(const T&) = 0;
  // non-blocking close
  virtual bool try_close() = 0;
  // blocking close
  virtual void close() = 0;
};

template <typename T, uint64_t N = 1>
class stream : public istream<T>, public ostream<T> {
 public:
  // constructors
  stream() : tail{0}, head{0} {}
  template <size_t S>
  stream(const char (&name)[S]) : name{name}, tail{0}, head{0} {}
  // deleted copy constructor
  stream(const stream&) = delete;
  // default move constructor
  stream(stream&&) = default;
  // deleted copy assignment operator
  stream& operator=(const stream&) = delete;
  // default move assignment operator
  stream& operator=(stream&&) = default;

  ~stream() {
    // do not call virtual function empty() in destructor
    if (head != tail) {
      LOG(WARNING) << "stream '" << name
                   << "' destructed with leftovers; hardware behavior may be "
                      "unexpected in consecutive invocations";
    }
  }

  // must call APIs from tlp::istream or tlp::ostream; calling from tlp::stream
  // directly is not allowed
 private:
  template <typename U, uint64_t S, uint64_t M>
  friend class streams;

  std::string name;
  constexpr static uint64_t depth{N};
  const char* get_name() const override { return name.c_str(); }
  uint64_t get_depth() const override { return depth; }

  // consumer const operations

  // whether stream is empty
  bool empty() const override {
    bool is_empty = head == tail;
    if (is_empty) internal::yield();
    return is_empty;
  }
  // whether stream has ended
  bool try_eos(bool& eos) const override {
    if (!empty()) {
      eos = access(tail).eos;
      return true;
    }
    return false;
  }
  bool eos(bool& succeeded) const override {
    bool eos = false;
    succeeded = try_eos(eos);
    return eos;
  }
  bool eos(std::nullptr_t) const override {
    bool eos = false;
    try_eos(eos);
    return eos;
  }
  // non-blocking non-destructive read
  bool try_peek(T& val) const override {
    if (!empty()) {
      auto& elem = access(tail);
      if (elem.eos) {
        throw std::runtime_error("stream '" + name + "' peeked when closed");
      }
      val = elem.val;
      return true;
    }
    return false;
  }
  // alternative non-blocking non-destructive read
  T peek(bool& succeeded) const override {
    T val;
    succeeded = try_peek(val);
    return val;
  }
  T peek(std::nullptr_t) const override {
    T val;
    try_peek(val);
    return val;
  }
  // peek val and eos at the same time
  T peek(bool& succeeded, bool& is_eos) const override {
    if (!empty()) {
      auto& elem = access(tail);
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
  bool try_read(T& val) override {
    if (!empty()) {
      auto elem = access(tail);
      ++tail;
      if (elem.eos) {
        throw std::runtime_error("stream '" + name + "' read when closed");
      }
      val = elem.val;
      return true;
    }
    return false;
  }
  // blocking destructive read
  T read() override {
    T val;
    while (!try_read(val)) {
    }
    return val;
  }
  // alterative non-blocking destructive read
  T read(bool& succeeded) override {
    T val;
    succeeded = try_read(val);
    return val;
  }
  T read(std::nullptr_t) override {
    T val;
    try_read(val);
    return val;
  }
  // non-blocking destructive read with default value
  T read(bool* succeeded_ret, const T& default_val) override {
    T val;
    bool succeeded = try_read(val);
    if (succeeded_ret != nullptr) {
      *succeeded_ret = succeeded;
    }
    return succeeded ? val : default_val;
  }

  // non-blocking destructive open
  bool try_open() override {
    if (!empty()) {
      auto elem = access(tail);
      ++tail;
      if (!elem.eos) {
        throw std::runtime_error("stream '" + name +
                                 "' opened when not closed");
      }
      return true;
    }
    return false;
  }
  // blocking destructive open
  void open() override {
    while (!try_open()) {
    }
  }

  // producer const operations

  // whether stream is full
  bool full() const override {
    bool is_full = head - tail == depth;
    if (is_full) internal::yield();
    return is_full;
  }

  // producer non-const operations

  // non-blocking write
  bool try_write(const T& val) override {
    if (!full()) {
      access(head) = {val, false};
      ++head;
      return true;
    }
    return false;
  }
  // blocking write
  void write(const T& val) override {
    while (!try_write(val)) {
    }
  }
  // non-blocking close
  bool try_close() override {
    if (!full()) {
      access(head) = {{}, true};
      ++head;
      return true;
    }
    return false;
  }
  // blocking close
  void close() override {
    while (!try_close()) {
    }
  }

  // producer writes to head and consumer reads from tail
  // okay to keep incrementing because it'll take > 100 yr to overflow uint64_t
  std::atomic_uint64_t tail;
  std::atomic_uint64_t head;

  // buffer operations

  // elem_t<T>* buffer;
  std::array<elem_t<T>, depth + 1> buffer;
  elem_t<T>& access(uint64_t idx) { return buffer[idx % buffer.size()]; }
  const elem_t<T>& access(uint64_t idx) const {
    return buffer[idx % buffer.size()];
  }
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
class streams : public istreams<T, S>, public ostreams<T, S> {
  stream<T, N> streams_[S];

 public:
  // constructors
  streams() {}
  template <size_t SS>
  streams(const char (&name)[SS]) {
    for (int i = 0; i < S; ++i) {
      streams_[i].name = std::string(name) + "[" + std::to_string(i) + "]";
    }
  }
  // deleted copy constructor
  streams(const streams&) = delete;
  // default move constructor
  streams(streams&&) = default;
  // deleted copy assignment operator
  streams& operator=(const streams&) = delete;
  // default move assignment operator
  streams& operator=(streams&&) = default;

  stream<T, N>& operator[](int idx) { return streams_[idx]; };
};

}  // namespace tlp

#endif  // TLP_STREAM_H_
