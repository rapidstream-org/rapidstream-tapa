#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <cstdarg>
#include <cstdint>

#include <array>
#include <atomic>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include <signal.h>
#include <time.h>

#include <glog/logging.h>

namespace tlp {

extern thread_local uint64_t last_signal_timestamp;
constexpr uint64_t kSignalThreshold = 500000000;
inline uint64_t get_time_ns() {
  timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return static_cast<uint64_t>(tp.tv_sec) * 1000000000 + tp.tv_nsec;
}

template <typename T>
struct elem_t {
  bool eos;
  T val;
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
  // non-blocking non-destructive read
  virtual bool try_peek(T&) const = 0;
  // alternative non-blocking non-destructive read
  virtual T peek(bool&) const = 0;

  // consumer non-const operations

  // non-blocking destructive read
  virtual bool try_read(T&) = 0;
  // blocking destructive read
  virtual T read() = 0;
  // alterative non-blocking destructive read
  virtual T read(bool&) = 0;
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
  // constructor
  stream(const std::string& name = "") : name{name}, tail{0}, head{0} {}
  // deleted copy constructor
  stream(const stream&) = delete;
  // default move constructor
  stream(stream&&) = default;
  // deleted copy assignment operator
  stream& operator=(const stream&) = delete;
  // default move assignment operator
  stream& operator=(stream&&) = default;

  // must call APIs from tlp::istream or tlp::ostream; calling from tlp::stream
  // directly is not allowed
 private:
  const std::string name;
  constexpr static uint64_t depth{N};
  const char* get_name() const override { return name.c_str(); }
  uint64_t get_depth() const override { return depth; }

  // consumer const operations

  // whether stream is empty
  bool empty() const override { return head == tail; }
  // whether stream has ended
  bool try_eos(bool& eos) const override {
    if (!empty()) {
      eos = access(tail).eos;
      return true;
    }
    return false;
  }
  bool eos(bool& succeeded) const override {
    bool eos;
    succeeded = try_eos(eos);
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
      yield("read");
    }
    return val;
  }
  // alterative non-blocking destructive read
  T read(bool& succeeded) override {
    T val;
    succeeded = try_read(val);
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
    T val;
    while (!try_open()) {
      yield("open");
    }
  }

  // producer const operations

  // whether stream is full
  bool full() const override { return head - tail == depth; }

  // producer non-const operations

  // non-blocking write
  bool try_write(const T& val) override {
    if (!full()) {
      access(head) = {false, val};
      ++head;
      return true;
    }
    return false;
  }
  // blocking write
  void write(const T& val) override {
    while (!try_write(val)) {
      yield("write");
    }
  }
  // non-blocking close
  bool try_close() override {
    if (!full()) {
      access(head) = {true, {}};
      ++head;
      return true;
    }
    return false;
  }
  // blocking close
  void close() override {
    while (!try_close()) {
      yield("close");
    }
  }

  // producer writes to head and consumer reads from tail
  // okay to keep incrementing because it'll take > 100 yr to overflow uint64_t

  std::atomic<uint64_t> tail;
  std::atomic<uint64_t> head;

  // buffer operations

  // elem_t<T>* buffer;
  std::array<elem_t<T>, depth + 1> buffer;
  elem_t<T>& access(uint64_t idx) { return buffer[idx % buffer.size()]; }
  const elem_t<T>& access(uint64_t idx) const {
    return buffer[idx % buffer.size()];
  }

  void yield(const std::string& func) const {
    if (last_signal_timestamp != 0) {
      // print stalling message if within 500 ms of signal
      if (get_time_ns() - last_signal_timestamp < kSignalThreshold) {
        LOG(INFO) << "stalling for: " << (name.empty() ? "" : name + ".")
                  << func << "()";
      }
      last_signal_timestamp = 0;
    }
    std::this_thread::yield();
  }
};

template <typename T>
class mmap {
 public:
  mmap(T* ptr) : ptr_{ptr}, size_{0} {}
  mmap(T* ptr, uint64_t size) : ptr_{ptr}, size_{size} {}
  mmap(std::vector<typename std::remove_const<T>::type>& vec)
      : ptr_{vec.data()}, size_{vec.size()} {}
  operator T*() { return ptr_; }

  // Dereference.
  T& operator[](std::size_t idx) { return ptr_[idx]; }
  const T& operator[](std::size_t idx) const { return ptr_[idx]; }
  T& operator*() { return *ptr_; }
  const T& operator*() const { return *ptr_; }

  // Increment / decrement.
  T& operator++() { return *++ptr_; }
  T& operator--() { return *--ptr_; }
  T operator++(int) { return *ptr_++; }
  T operator--(int) { return *ptr_--; }

  // Arithmetic.
  mmap<T> operator+(std::ptrdiff_t diff) { return ptr_ + diff; }
  mmap<T> operator-(std::ptrdiff_t diff) { return ptr_ - diff; }
  mmap<T> operator-(mmap<T> ptr) { return ptr_ - ptr; }

  // Host-only APIs.
  T* get() { return ptr_; }
  uint64_t size() { return size_; }

 private:
  T* ptr_;
  uint64_t size_;
};

struct task {
  int current_step{0};
  std::unordered_map<int, std::vector<std::thread>> threads{};
  std::thread::id main_thread_id;

  task();
  task(task&&) = default;
  task(const task&) = delete;
  ~task() { wait_for(current_step); }

  task& operator=(task&&) = default;
  task& operator=(const task&) = delete;

  void signal_handler(int signal);

  void wait_for(int step) {
    for (auto& t : threads[step]) {
      t.join();
    }
  }

  template <int step, typename Function, typename... Args>
  task& invoke(Function&& f, Args&&... args) {
    // wait until current_step >= step
    for (; current_step < step; ++current_step) {
      wait_for(current_step);
    }
    threads[step].push_back(std::thread(f, std::ref(args)...));
    return *this;
  }
};

}  // namespace tlp

#define TLP_WHILE_NOT_EOS(fifo)                               \
  for (bool tlp_##fifo##_valid;                               \
       !fifo.eos(tlp_##fifo##_valid) || !tlp_##fifo##_valid;) \
    if (tlp_##fifo##_valid)

#define TLP_WHILE_NEITHER_EOS(fifo1, fifo2)                         \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid;               \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) && \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid);)  \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid)

#define TLP_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                            \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid, tlp_##fifo3##_valid; \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) &&        \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid) &&        \
       (!fifo3.eos(tlp_##fifo3##_valid) || !tlp_##fifo3##_valid);)         \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid && tlp_##fifo3##_valid)

#endif  // TASK_LEVEL_PARALLELIZATION_H_
