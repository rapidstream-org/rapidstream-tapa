#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

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

thread_local uint64_t last_signal_timestamp = 0;
const uint64_t kSignalThreshold = 500000000;
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

template <typename T, uint64_t N = 0>
class stream;

template <typename T>
class stream<T, 0> {
 public:
  // debug helpers

  virtual const char* get_name() const = 0;
  virtual uint64_t get_depth() const = 0;

  // consumer const operations

  // whether stream is empty
  virtual bool empty() const = 0;
  // whether stream has ended
  virtual bool eos() const = 0;
  // non-blocking non-destructive read
  virtual bool try_peek(T&) const = 0;
  // blocking non-destructive read
  virtual T peek() const = 0;
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

template <typename T, uint64_t N>
class stream : public stream<T, 0> {
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

  // public attributes

  const std::string name;
  constexpr static uint64_t depth{N};
  const char* get_name() const override { return name.c_str(); }
  uint64_t get_depth() const override { return depth; }

  // consumer const operations

  // whether stream is empty
  bool empty() const override { return head == tail; }
  // whether stream has ended
  bool eos() const override {
    while (empty()) {
      yield("eos");
    }
    return access(tail).eos;
  }
  // non-blocking non-destructive read
  bool try_peek(T& val) const override {
    if (!empty()) {
      auto& elem = access(tail);
      VLOG(2) << "stream '" << name << "' peeked beyond end";
      val = elem.val;
      return true;
    }
    return false;
  }
  // blocking non-destructive read
  T peek() const override {
    T val;
    while (!try_peek(val)) {
      yield("peek");
    }
    return val;
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
      VLOG(2) << "stream '" << name << "' read beyond end";
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

 private:
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

std::function<void(int)> signal_handler_func;
void signal_handler_wrapper(int signal) { signal_handler_func(signal); }

struct task {
  int current_step{0};
  std::unordered_map<int, std::vector<std::thread>> threads{};
  std::thread::id main_thread_id;

  task() : main_thread_id{std::this_thread::get_id()} {
    signal_handler_func = [this](int signal) { signal_handler(signal); };
    signal(SIGINT, signal_handler_wrapper);
  }
  task(task&&) = default;
  task(const task&) = delete;
  ~task() { wait_for(current_step); }

  task& operator=(task&&) = default;
  task& operator=(const task&) = delete;

  void signal_handler(int signal) {
    if (std::this_thread::get_id() == main_thread_id) {
      uint64_t signal_timestamp = get_time_ns();
      if (last_signal_timestamp != 0 &&
          signal_timestamp - last_signal_timestamp < kSignalThreshold) {
        LOG(INFO) << "caught SIGINT twice in " << kSignalThreshold / 1000000
                  << " ms; exit";
        exit(EXIT_FAILURE);
      }
      LOG(INFO) << "caught SIGINT";
      last_signal_timestamp = signal_timestamp;
      for (auto& t : threads[current_step]) {
        if (t.joinable()) {
          pthread_kill(t.native_handle(), signal);
        }
      }
    } else {
      last_signal_timestamp = get_time_ns();
    }
  }

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

#endif  // TASK_LEVEL_PARALLELIZATION_H_
