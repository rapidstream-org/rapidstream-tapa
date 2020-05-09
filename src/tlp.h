#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <glog/logging.h>

#include "tlp/mmap.h"
#include "tlp/stream.h"
#include "tlp/synthesizable/traits.h"
#include "tlp/synthesizable/util.h"
namespace tlp {

struct task {
  int current_step{0};
  std::map<int, std::list<push_type>> coroutines;
  std::unordered_map<push_type*, pull_type*> handle_table;

  task() {}
  task(task&&) = default;
  task(const task&) = delete;
  ~task() { wait_for(current_step); }

  task& operator=(task&&) = default;
  task& operator=(const task&) = delete;

  // proceed until step completes
  void wait_for(int step);

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
    auto& list = this->coroutines[step];
    int idx = list.size();
    list.emplace_back(make_stack_allocator(), [&, idx](pull_type& source) {
      // sink must have a stable pointer
      auto& sink = *std::next(coroutines[step].begin(), idx);
      handle_table[&sink] = current_handle = &source;
      f(args...);
    });
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
    for (uint64_t i = 0; i < length; ++i) {
      this->invoke<step>(f, access(args, i)...);
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
