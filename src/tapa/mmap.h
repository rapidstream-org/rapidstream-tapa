#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <cstddef>

#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "tapa/coroutine.h"

namespace tapa {

template <typename T>
class async_mmap;
template <typename T>
class mmap {
 public:
  mmap(T* ptr) : ptr_{ptr}, size_{0} {}
  mmap(T* ptr, uint64_t size) : ptr_{ptr}, size_{size} {}
  template <typename Allocator>
  mmap(std::vector<typename std::remove_const<T>::type, Allocator>& vec)
      : ptr_{vec.data()}, size_{vec.size()} {}
  operator T*() { return ptr_; }

  // Dereference.
  T& operator[](std::size_t idx) {
    CHECK_GE(idx, 0);
    CHECK_LT(idx, size_);
    return ptr_[idx];
  }
  const T& operator[](std::size_t idx) const {
    CHECK_GE(idx, 0);
    CHECK_LT(idx, size_);
    return ptr_[idx];
  }
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
  std::ptrdiff_t operator-(mmap<T> ptr) { return ptr_ - ptr; }

  // Convert to async_mmap.
  tapa::async_mmap<T> async_mmap() { return tapa::async_mmap<T>(*this); }

  // Host-only APIs.
  T* get() { return ptr_; }
  uint64_t size() { return size_; }

 protected:
  T* ptr_;
  uint64_t size_;
};

template <typename T>
class async_mmap : public mmap<T> {
  using super = mmap<T>;

 public:
  async_mmap(super&& mem) : super(mem) {}
  async_mmap(const super& mem) : super(mem) {}
  async_mmap(T* ptr) : super(ptr) {}
  async_mmap(T* ptr, uint64_t size) : super(ptr, size) {}
  template <typename Allocator>
  async_mmap(std::vector<typename std::remove_const<T>::type, Allocator>& vec)
      : super(vec) {}

  // Read operations.
  void read_addr_write(uint64_t addr) {
    CHECK_GE(addr, 0);
    CHECK_LT(addr, super::size_);
    read_addr_q_.push(addr);
  }
  bool read_addr_try_write(uint64_t addr) {
    read_addr_write(addr);
    return true;
  }
  bool read_data_empty() {
    bool is_empty = read_addr_q_.empty();
    if (is_empty) internal::yield();
    return is_empty;
  }
  bool read_data_try_read(T& resp) {
    if (read_data_empty()) {
      return false;
    }
    resp = super::ptr_[read_addr_q_.front()];
    read_addr_q_.pop();
    return true;
  };

  // Write operations.
  void write_addr_write(uint64_t addr) {
    CHECK_GE(addr, 0);
    CHECK_LT(addr, super::size_);
    if (write_data_q_.empty()) {
      write_addr_q_.push(addr);
    } else {
      super::ptr_[addr] = write_data_q_.front();
      write_data_q_.pop();
    }
  }
  bool write_addr_try_write(uint64_t addr) {
    write_addr_write(addr);
    return true;
  }
  void write_data_write(const T& data) {
    if (write_addr_q_.empty()) {
      write_data_q_.push(data);
    } else {
      super::ptr_[write_addr_q_.front()] = data;
      write_addr_q_.pop();
    }
  }
  bool write_data_try_write(const T& data) {
    write_data_write(data);
    return true;
  }

  // Waits until no operations are pending or on-going.
  void fence() {}

 private:
  // Must be operated via the read/write addr/data stream APIs.
  operator T*() { return super::ptr_; }

  // Dereference not permitted.
  T& operator[](std::size_t idx) { return super::ptr_[idx]; }
  const T& operator[](std::size_t idx) const { return super::ptr_[idx]; }
  T& operator*() { return *super::ptr_; }
  const T& operator*() const { return *super::ptr_; }

  // Increment / decrement not permitted.
  T& operator++() { return *++super::ptr_; }
  T& operator--() { return *--super::ptr_; }
  T operator++(int) { return *super::ptr_++; }
  T operator--(int) { return *super::ptr_--; }

  // Arithmetic not permitted.
  async_mmap<T> operator+(std::ptrdiff_t diff) { return super::ptr_ + diff; }
  async_mmap<T> operator-(std::ptrdiff_t diff) { return super::ptr_ - diff; }
  std::ptrdiff_t operator-(async_mmap<T> ptr) { return super::ptr_ - ptr; }

  // The software simulator queues pending operations.
  std::queue<uint64_t> read_addr_q_;
  std::queue<uint64_t> write_addr_q_;
  std::queue<T> write_data_q_;
};

template <typename T, uint64_t S>
class async_mmaps {
  std::vector<async_mmap<T>> mmaps_;

 public:
  // constructors
  async_mmaps(const std::array<T*, S>& ptrs) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(ptrs[i]);
    }
  }
  async_mmaps(const std::array<T*, S>& ptrs,
              const std::array<uint64_t, S>& sizes) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(ptrs[i], sizes[i]);
    }
  }
  template <typename Allocator>
  async_mmaps(
      std::vector<typename std::remove_const<T>::type, Allocator> (&vecs)[S]) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(vecs[i]);
    }
  }
  template <typename Allocator>
  async_mmaps(
      std::array<std::vector<typename std::remove_const<T>::type, Allocator>,
                 S>& vecs) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(vecs[i]);
    }
  }

  // defaultd copy constructor
  async_mmaps(const async_mmaps&) = default;
  // default move constructor
  async_mmaps(async_mmaps&&) = default;
  // defaultd copy assignment operator
  async_mmaps& operator=(const async_mmaps&) = default;
  // default move assignment operator
  async_mmaps& operator=(async_mmaps&&) = default;

  async_mmap<T>& operator[](int idx) { return mmaps_[idx]; };
};

template <typename T, uint64_t N>
struct vec_t;

template <typename T, uint64_t N, typename Allocator>
inline mmap<vec_t<T, N>> make_vec_mmap(
    std::vector<typename std::remove_const<T>::type, Allocator>& vec) {
  if (vec.size() % N != 0) {
    throw std::runtime_error("vector must be aligned to make mmap vec");
  }
  return mmap<vec_t<T, N>>(reinterpret_cast<vec_t<T, N>*>(vec.data()),
                           vec.size() / N);
}

template <typename T, uint64_t N, typename Allocator>
inline async_mmap<vec_t<T, N>> make_vec_async_mmap(
    std::vector<typename std::remove_const<T>::type, Allocator>& vec) {
  if (vec.size() % N != 0) {
    throw std::runtime_error("vector must be aligned to make async_mmap vec");
  }
  return async_mmap<vec_t<T, N>>(reinterpret_cast<vec_t<T, N>*>(vec.data()),
                                 vec.size() / N);
}

template <typename T, uint64_t N, uint64_t S, typename Allocator>
inline async_mmaps<vec_t<T, N>, S> make_vec_async_mmaps(
    std::array<std::vector<typename std::remove_const<T>::type, Allocator>, S>&
        vec) {
  std::array<vec_t<T, N>*, S> ptrs;
  std::array<uint64_t, S> sizes;
  for (int i = 0; i < S; ++i) {
    ptrs[i] = reinterpret_cast<vec_t<T, N>*>(vec[i].data());
    sizes[i] = vec[i].size() / N;
    if (vec[i].size() % N != 0) {
      throw std::runtime_error("vector must be aligned to make async_mmap vec");
    }
  }
  return async_mmaps<vec_t<T, N>, S>(ptrs, sizes);
}

}  // namespace tapa

#endif  // TAPA_MMAP_H_
