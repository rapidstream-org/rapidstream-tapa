#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <atomic>
#include <future>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <signal.h>
#include <time.h>

#include <glog/logging.h>

#include "tlp/mmap.h"
#include "tlp/stream.h"
#include "tlp/synthesizable/traits.h"
#include "tlp/synthesizable/util.h"

namespace tlp {

extern mutex thread_name_mtx;
extern std::unordered_map<std::thread::id, std::string> thread_name_table;
extern std::atomic_bool sleeping;

struct task {
  int current_step{0};
  std::unordered_map<int, std::vector<std::thread>> threads{};
  std::thread::id main_thread_id;

  task();
  task(task&&) = default;
  task(const task&) = delete;
  ~task() {
    wait_for(current_step);
    for (auto& pair : threads) {
      for (auto& thread : pair.second) {
        if (thread.joinable()) {
          // use an atomic flag to make sure thread is no longer doing anything
          sleeping = false;
          // make thread sleep forever to avoid accessing freed memory
          pthread_kill(thread.native_handle(), SIGUSR1);
          // wait until thread is sleeping
          while (!sleeping) std::this_thread::yield();
          // let OS do the clean-up when the whole program terminates
          thread.detach();
        }
      }
    }
  }

  task& operator=(task&&) = default;
  task& operator=(const task&) = delete;

  void signal_handler(int signal);

  void wait_for(int step) {
    for (auto& t : threads[step]) {
      if (t.joinable()) t.join();
    }
  }

  template <int step, typename Function, typename... Args>
  task& invoke(Function&& f, Args&&... args) {
    return invoke<step>(f, "", args...);
  }

  template <int step, typename Function, typename... Args, size_t S>
  task& invoke(Function&& f, const char (&name)[S], Args&&... args) {
    // wait until current_step >= step
    for (; current_step < step; ++current_step) {
      wait_for(current_step);
    }
    threads[step].push_back(std::thread(f, std::ref(args)...));
    if (name[0] != '\0') {
      thread_name_mtx.lock();
      thread_name_table[threads[step].rbegin()->get_id()] = name;
      thread_name_mtx.unlock();
    }
    return *this;
  }

  // invoke task vector without a name
  template <int step, uint64_t length, typename Function, typename... Args>
  task& invoke(Function&& f, Args&&... args) {
    return invoke<step, length>(f, "", args...);
  }

  // invoke task vector with a name
  template <int step, uint64_t length, typename Function, typename... Args,
            size_t S>
  task& invoke(Function&& f, const char (&name)[S], Args&&... args) {
    // wait until current_step >= step
    for (; current_step < step; ++current_step) {
      wait_for(current_step);
    }
    for (uint64_t i = 0; i < length; ++i) {
      threads[step].push_back(std::thread(f, std::ref(access(args, i))...));
      if (name[0] != '\0') {
        thread_name_mtx.lock();
        thread_name_table[threads[step].rbegin()->get_id()] =
            std::string(name) + "[" + std::to_string(i) + "]";
        thread_name_mtx.unlock();
      }
    }
    return *this;
  }

 private:
  // scalar
  template <typename T>
  static T& access(T& arg, uint64_t idx) {
    return arg;
  }

  // access streams in vector invoke
  template <typename T, uint64_t length, uint64_t depth>
  static stream<T, depth>& access(streams<T, length, depth>& arg,
                                  uint64_t idx) {
    return arg[idx];
  }

  // access async_mmaps in vector invoke
  template <typename T, uint64_t length>
  static async_mmap<T>& access(async_mmaps<T, length>& arg, uint64_t idx) {
    return arg[idx];
  }
};

template <typename T, uint64_t N>
struct vec_t {
  T data[N];
  template <typename U>
  operator vec_t<U, N>() {
    vec_t<U, N> result;
    for (uint64_t i = 0; i < N; ++i) {
      result.data[i] = static_cast<U>(data[i]);
    }
    return result;
  }
  // T& operator[](uint64_t idx) { return data[idx]; }
  void set(uint64_t idx, T val) { data[idx] = val; }
  T get(uint64_t idx) const { return data[idx]; }
  T operator[](uint64_t idx) const { return get(idx); }
  static constexpr uint64_t length = N;
  static constexpr uint64_t bytes = length * sizeof(T);
  static constexpr uint64_t bits = bytes * CHAR_BIT;
};

template <typename T, uint64_t N>
inline std::ostream& operator<<(std::ostream& os, const vec_t<T, N>& obj) {
  os << "{";
  for (uint64_t i = 0; i < N; ++i) {
    if (i > 0) os << ", ";
    os << "[" << i << "]: " << obj[i];
  }
  return os << "}";
}

template <typename T>
struct aligned_allocator {
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  T* allocate(size_t count) {
    constexpr std::size_t N = 4096;
    auto round_up = [](std::size_t n) -> std::size_t {
      return ((n - 1) / N + 1) * N;
    };
    void* ptr = aligned_alloc(N, round_up(count * sizeof(T)));
    if (ptr == nullptr) throw std::bad_alloc();
    return reinterpret_cast<T*>(ptr);
  }
  void deallocate(T* ptr, std::size_t count) { free(ptr); }
};

}  // namespace tlp

#endif  // TASK_LEVEL_PARALLELIZATION_H_
