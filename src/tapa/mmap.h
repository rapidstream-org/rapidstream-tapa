#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <cstddef>

#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "tapa/coroutine.h"
#include "tapa/synthesizable/vec.h"

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

  // Increment / decrement.
  T& operator++() { return *++ptr_; }
  T& operator--() { return *--ptr_; }
  T operator++(int) { return *ptr_++; }
  T operator--(int) { return *ptr_--; }

  // Convert to async_mmap.
  tapa::async_mmap<T> async_mmap() { return tapa::async_mmap<T>(*this); }

  // Host-only APIs.
  T* get() const { return ptr_; }
  uint64_t size() const { return size_; }
  template <int64_t N>
  mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(size_ % N, 0) << "size must be a multiple of N";
    return mmap<vec_t<T, N>>(reinterpret_cast<vec_t<T, N>*>(ptr_), size_ / N);
  }

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
    if (is_empty) {
      internal::yield("read data channel is empty");
    }
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
class mmaps {
  std::vector<mmap<T>> mmaps_;

 public:
  // constructors
  mmaps(const std::array<T*, S>& ptrs) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(ptrs[i]);
    }
  }
  mmaps(const std::array<T*, S>& ptrs, const std::array<uint64_t, S>& sizes) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(ptrs[i], sizes[i]);
    }
  }
  template <typename Allocator>
  mmaps(
      std::vector<typename std::remove_const<T>::type, Allocator> (&vecs)[S]) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(vecs[i]);
    }
  }
  template <typename Allocator>
  mmaps(std::array<std::vector<typename std::remove_const<T>::type, Allocator>,
                   S>& vecs) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(vecs[i]);
    }
  }

  // defaultd copy constructor
  mmaps(const mmaps&) = default;
  // default move constructor
  mmaps(mmaps&&) = default;
  // defaultd copy assignment operator
  mmaps& operator=(const mmaps&) = default;
  // default move assignment operator
  mmaps& operator=(mmaps&&) = default;

  mmap<T>& operator[](int idx) { return mmaps_[idx]; };

  template <uint64_t offset, uint64_t length>
  mmaps<T, length> slice() {
    static_assert(offset + length < S, "invalid slice");
    mmaps<T, length> result;
    for (uint64_t i = 0; i < length; ++i) {
      result.mmaps_[i] = this->mmaps_[offset + i];
    }
    return result;
  }

  // Host-only APIs.
  template <int64_t N>
  mmaps<vec_t<T, N>, S> vectorized() const {
    std::array<vec_t<T, N>*, S> ptrs;
    std::array<uint64_t, S> sizes;
    for (uint64_t i = 0; i < S; ++i) {
      CHECK_EQ(mmaps_[i].size() % N, 0)
          << "size[" << i << "] must be a multiple of N";
      ptrs[i] = reinterpret_cast<vec_t<T, N>*>(mmaps_[i].get());
      sizes[i] = mmaps_[i].size() / N;
    }
    return mmaps<vec_t<T, N>, S>(ptrs, sizes);
  }
};

// Host-only mmap types that must have correct size.
template <typename T>
class placeholder_mmap : public mmap<T> {
  using mmap<T>::mmap;
  placeholder_mmap(T* ptr) : mmap<T>(ptr) {}

 public:
  template <int64_t N>
  placeholder_mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(this->size_ % N, 0) << "size must be a multiple of N";
    return placeholder_mmap<vec_t<T, N>>(
        reinterpret_cast<vec_t<T, N>*>(this->ptr_), this->size_ / N);
  }
};
template <typename T>
class read_only_mmap : public mmap<T> {
  using mmap<T>::mmap;
  read_only_mmap(T* ptr) : mmap<T>(ptr) {}

 public:
  template <int64_t N>
  read_only_mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(this->size_ % N, 0) << "size must be a multiple of N";
    return read_only_mmap<vec_t<T, N>>(
        reinterpret_cast<vec_t<T, N>*>(this->ptr_), this->size_ / N);
  }
};
template <typename T>
class write_only_mmap : public mmap<T> {
  using mmap<T>::mmap;
  write_only_mmap(T* ptr) : mmap<T>(ptr) {}

 public:
  template <int64_t N>
  write_only_mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(this->size_ % N, 0) << "size must be a multiple of N";
    return write_only_mmap<vec_t<T, N>>(
        reinterpret_cast<vec_t<T, N>*>(this->ptr_), this->size_ / N);
  }
};
template <typename T>
class read_write_mmap : public mmap<T> {
  using mmap<T>::mmap;
  read_write_mmap(T* ptr) : mmap<T>(ptr) {}

 public:
  template <int64_t N>
  read_write_mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(this->size_ % N, 0) << "size must be a multiple of N";
    return read_write_mmap<vec_t<T, N>>(
        reinterpret_cast<vec_t<T, N>*>(this->ptr_), this->size_ / N);
  }
};

}  // namespace tapa

#endif  // TAPA_MMAP_H_
