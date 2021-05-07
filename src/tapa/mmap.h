#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <cstddef>

#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "tapa/coroutine.h"
#include "tapa/stream.h"
#include "tapa/synthesizable/vec.h"

namespace tapa {

template <typename T>
class async_mmap;
template <typename T>
class mmap {
 public:
  mmap(T* ptr) : ptr_{ptr}, size_{0} {}
  mmap(T* ptr, uint64_t size) : ptr_{ptr}, size_{size} {}
  template <typename Allocator> explicit
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
  template <uint64_t N>
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
  using addr_t = int64_t;
  using resp_t = uint8_t;

  tapa::stream<addr_t, 64> read_addr_q_{"read_addr"};
  tapa::stream<T, 64> read_data_q_{"read_data"};
  tapa::stream<addr_t, 64> write_addr_q_{"write_addr"};
  tapa::stream<T, 64> write_data_q_{"write_data"};
  tapa::stream<resp_t, 64> write_resp_q_{"write_resp"};

 public:
  async_mmap(super&& mem) : super(mem) {}
  async_mmap(const super& mem) : super(mem) {
    internal::schedule(/*detach=*/true, [this] {
      worker(this->get(), this->size(), read_addr_q_, read_data_q_,
             write_addr_q_, write_data_q_, write_resp_q_);
    });
  }
  async_mmap(T* ptr) : super(ptr) {}
  async_mmap(T* ptr, uint64_t size) : super(ptr, size) {}
  template <typename Allocator> explicit
  async_mmap(std::vector<typename std::remove_const<T>::type, Allocator>& vec)
      : super(vec) {}

  tapa::ostream<addr_t>& read_addr{read_addr_q_};
  tapa::istream<T>& read_data{read_data_q_};
  tapa::ostream<addr_t>& write_addr{write_addr_q_};
  tapa::ostream<T>& write_data{write_data_q_};
  tapa::istream<resp_t>& write_resp{write_resp_q_};

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

  // Copy of all arguments so that the worker can run after async_mmap is
  // deconstructed.
  static void worker(T* ptr, uint64_t size, decltype(read_addr_q_) read_addr_q,
                     decltype(read_data_q_) read_data_q,
                     decltype(write_addr_q_) write_addr_q,
                     decltype(write_data_q_) write_data_q,
                     decltype(write_resp_q_) write_resp_q) {
    int16_t write_count = 0;
    for (;;) {
      if (!read_addr_q.empty() && !read_data_q.full()) {
        const auto addr = read_addr_q.read();
        CHECK_GE(addr, 0);
        if (addr != 0) {
          CHECK_LT(addr, size);
        }
        read_data_q.write(ptr[addr]);
      }
      if (write_count != 256 && !write_addr_q.empty() &&
          !write_data_q.empty()) {
        const auto addr = write_addr_q.read();
        CHECK_GE(addr, 0);
        if (addr != 0) {
          CHECK_LT(addr, size);
        }
        ptr[addr] = write_data_q.read();
        ++write_count;
      } else if (write_count > 0 &&
                 write_resp_q.try_write(resp_t(write_count - 1))) {
        CHECK_LE(write_count, 256);
        write_count = 0;
      }
    }
  }
};

template <typename T, uint64_t S>
class mmaps {
 protected:
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
  template <typename Allocator> explicit
  mmaps(
      std::vector<typename std::remove_const<T>::type, Allocator> (&vecs)[S]) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(vecs[i]);
    }
  }
  template <typename Allocator> explicit
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
  template <uint64_t N>
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
#define TAPA_DEFINE_MMAP(tag)                                           \
  template <typename T>                                                 \
  class tag##_mmap : public mmap<T> {                                   \
    using mmap<T>::mmap;                                                \
    tag##_mmap(T* ptr) : mmap<T>(ptr) {}                                \
                                                                        \
   public:                                                              \
    template <uint64_t N>                                               \
    tag##_mmap<vec_t<T, N>> vectorized() const {                        \
      CHECK_EQ(this->size_ % N, 0) << "size must be a multiple of N";   \
      return tag##_mmap<vec_t<T, N>>(                                   \
          reinterpret_cast<vec_t<T, N>*>(this->ptr_), this->size_ / N); \
    }                                                                   \
  }
TAPA_DEFINE_MMAP(placeholder);
TAPA_DEFINE_MMAP(read_only);
TAPA_DEFINE_MMAP(write_only);
TAPA_DEFINE_MMAP(read_write);
#undef TAPA_DEFINE_MMAP
#define TAPA_DEFINE_MMAPS(tag)                                           \
  template <typename T, uint64_t S>                                      \
  class tag##_mmaps : public mmaps<T, S> {                               \
    using mmaps<T, S>::mmaps;                                            \
    tag##_mmaps(const std::array<T*, S>& ptrs) : mmaps<T, S>(ptrs){};    \
                                                                         \
   public:                                                               \
    template <uint64_t N>                                                \
    tag##_mmaps<vec_t<T, N>, S> vectorized() const {                     \
      std::array<vec_t<T, N>*, S> ptrs;                                  \
      std::array<uint64_t, S> sizes;                                     \
      for (uint64_t i = 0; i < S; ++i) {                                 \
        CHECK_EQ(this->mmaps_[i].size() % N, 0)                          \
            << "size[" << i << "] must be a multiple of N";              \
        ptrs[i] = reinterpret_cast<vec_t<T, N>*>(this->mmaps_[i].get()); \
        sizes[i] = this->mmaps_[i].size() / N;                           \
      }                                                                  \
      return tag##_mmaps<vec_t<T, N>, S>(ptrs, sizes);                   \
    }                                                                    \
  }
TAPA_DEFINE_MMAPS(placeholder);
TAPA_DEFINE_MMAPS(read_only);
TAPA_DEFINE_MMAPS(write_only);
TAPA_DEFINE_MMAPS(read_write);
#undef TAPA_DEFINE_MMAPS

}  // namespace tapa

#endif  // TAPA_MMAP_H_
