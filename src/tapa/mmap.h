#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <cstddef>

#ifndef __SYNTHESIS__

#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <frt.h>

#include "tapa/coroutine.h"

#endif  // __SYNTHESIS__

#include "tapa/stream.h"
#include "tapa/vec.h"

namespace tapa {

namespace internal {

#ifdef __SYNTHESIS__

template <typename T>
struct async_mmap {
  using addr_t = int64_t;
  using resp_t = uint8_t;

  tapa::ostream<addr_t> read_addr;
  tapa::istream<T> read_data;
  tapa::ostream<addr_t> write_addr;
  tapa::ostream<T> write_data;
  tapa::istream<resp_t> write_resp;
};

#else  // __SYNTHESIS__

template <typename Param, typename Arg>
struct accessor;

#endif  // __SYNTHESIS__

}  // namespace internal

#ifndef __SYNTHESIS__
template <typename T>
class async_mmap;
#endif  // __SYNTHESIS__

/// Defines a view of a piece of consecutive memory with synchronous random
/// accesses.
template <typename T>
#ifdef __SYNTHESIS__
using mmap = T*;
#else   // __SYNTHESIS__
class mmap {
 public:
  /// Constructs a @c tapa::mmap with unknown size.
  ///
  /// @param ptr Pointer to the start of the mapped memory.
  explicit mmap(T* ptr) : ptr_{ptr}, size_{0} {}

  /// Constructs a @c tapa::mmap with the given @c size.
  ///
  /// @param ptr  Pointer to the start of the mapped memory.
  /// @param size Size of the mapped memory (in unit of element count).
  mmap(T* ptr, uint64_t size) : ptr_{ptr}, size_{size} {}

  /// Constructs a @c tapa::mmap from the given @c container.
  ///
  /// @param container Container holding a @c tapa::mmap. Must implement
  ///                  @c data() and @c size().
  template <typename Container>
  explicit mmap(Container& container)
      : ptr_{container.data()}, size_{container.size()} {}

  /// Implicitly casts to a regular pointer.
  ///
  /// @c tapa::mmap should be used just like a pointer in the kernel.
  operator T*() { return ptr_; }

  /// Increments the start of the mapped memory.
  ///
  /// @return The incremented @c tapa::mmap.
  mmap& operator++() {
    ++ptr_;
    return *this;
  }

  /// Decrements the start of the mapped memory.
  ///
  /// @return The decremented @c tapa::mmap.
  mmap& operator--() {
    --ptr_;
    return *this;
  }

  /// Increments the start of the mapped memory.
  ///
  /// @return The @c tapa::mmap before incrementation.
  mmap operator++(int) { return mmap(ptr_++); }

  /// Decrements the start of the mapped memory.
  ///
  /// @return The @c tapa::mmap before decrementation.
  mmap operator--(int) { return mmap(ptr_--); }

  /// Retrieves the start of the mapped memory.
  ///
  /// This should be used on the host only.
  ///
  /// @return The start of the mapped memory.
  T* get() const { return ptr_; }

  /// Retrieves the size of the mapped memory.
  ///
  /// This should be used on the host only.
  ///
  /// @return The size of the mapped memory (in unit of element count).
  uint64_t size() const { return size_; }

  /// Reinterprets the element type of the mapped memory as
  /// <tt>tapa::vec_t<T, N></tt>.
  ///
  /// This should be used on the host only.
  /// The size of mapped memory must be a multiple of @c N.
  ///
  /// @tparam N  Vector length of the new element type.
  /// @return @c tapa::mmap of the same piece of memory but of type
  ///         <tt>tapa::vec_t<T, N></tt>.
  template <uint64_t N>
  mmap<vec_t<T, N>> vectorized() const {
    CHECK_EQ(size_ % N, 0) << "size must be a multiple of N";
    return mmap<vec_t<T, N>>(reinterpret_cast<vec_t<T, N>*>(ptr_), size_ / N);
  }

  /// Reinterprets the element type of the mapped memory as @c U.
  ///
  /// This should be used on the host only.
  /// Both @c T and @c U must have standard layout.
  /// The host memory pointer must be properly aligned.
  /// The size of mapped memory must be a multiple of @c sizeof(U)/sizeof(T).
  ///
  /// @tparam U  The new element type.
  /// @return @c tapa::mmap<U> of the same piece of memory.
  template <typename U>
  mmap<U> reinterpret() const {
    static_assert(std::is_standard_layout<T>::value,
                  "T must have standard layout");
    static_assert(std::is_standard_layout<U>::value,
                  "U must have standard layout");

    if (sizeof(U) > sizeof(T)) {
      constexpr auto N = sizeof(U) / sizeof(T);
      CHECK_EQ(size() % N, 0) << "size must be a multiple of N = " << N;
    }
    CHECK_EQ(reinterpret_cast<size_t>(get()) % alignof(U), 0)
        << "pointer must be " << alignof(U) << "-byte aligned";
    return mmap<U>(reinterpret_cast<U*>(get()), size() * sizeof(T) / sizeof(U));
  }

 protected:
  T* ptr_;
  uint64_t size_;
};
#endif  // __SYNTHESIS__

/// Defines a view of a piece of consecutive memory with asynchronous random
/// accesses.
template <typename T>
#ifdef __SYNTHESIS__
// HLS doesn't like non-reference instance of hls::stream in the arguments.
using async_mmap = internal::async_mmap<T>&;
#else   // __SYNTHESIS__
class async_mmap : public mmap<T> {
 public:
  /// Type of the addresses.
  using addr_t = int64_t;

  /// Type of the write responses.
  using resp_t = uint8_t;

 private:
  using super = mmap<T>;

  tapa::stream<addr_t, 64> read_addr_q_{"read_addr"};
  tapa::stream<T, 64> read_data_q_{"read_data"};
  tapa::stream<addr_t, 64> write_addr_q_{"write_addr"};
  tapa::stream<T, 64> write_data_q_{"write_data"};
  tapa::stream<resp_t, 64> write_resp_q_{"write_resp"};

  // Only convert when scheduled.
  async_mmap(const super& mem)
      : super(mem),
        read_addr(read_addr_q_),
        read_data(read_data_q_),
        write_addr(write_addr_q_),
        write_data(write_data_q_),
        write_resp(write_resp_q_) {}

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

 public:
  /// Provides access to the <i>read address</i> channel.
  ///
  /// Each value written to this channel triggers an asynchronous memory read
  /// request. Consecutive requests may be coalesced into a long burst request.
  tapa::ostream<addr_t> read_addr;

  /// Provides access to the read data channel.
  ///
  /// Each value read from this channel represents the data retrieved from the
  /// underlying memory system.
  tapa::istream<T> read_data;

  /// Provides access to the write address channel.
  ///
  /// Each value written to this channel triggers an asynchronous memory write
  /// request. Consecutive requests may be coalesced into a long burst request.
  tapa::ostream<addr_t> write_addr;

  /// Provides access to the write data channel.
  ///
  /// Each value written to this channel supplies data to the memory write
  /// request.
  tapa::ostream<T> write_data;

  /// Provides access to the write response channel.
  ///
  /// Each value read from this channel represents the data count acknowledged
  /// by the underlying memory system.
  tapa::istream<resp_t> write_resp;

  void operator()() {
    int16_t write_count = 0;
    for (;;) {
      if (!read_addr_q_.empty() && !read_data_q_.full()) {
        const auto addr = read_addr_q_.read();
        CHECK_GE(addr, 0);
        if (addr != 0) {
          CHECK_LT(addr, this->size_);
        }
        read_data_q_.write(this->ptr_[addr]);
      }
      if (write_count != 256 && !write_addr_q_.empty() &&
          !write_data_q_.empty()) {
        const auto addr = write_addr_q_.read();
        CHECK_GE(addr, 0);
        if (addr != 0) {
          CHECK_LT(addr, this->size_);
        }
        this->ptr_[addr] = write_data_q_.read();
        ++write_count;
      } else if (write_count > 0 &&
                 this->write_resp_q_.try_write(resp_t(write_count - 1))) {
        CHECK_LE(write_count, 256);
        write_count = 0;
      }
    }
  }

  static async_mmap schedule(super mem) {
    // a copy of async_mem is stored in std::function<void()>
    async_mmap async_mem(mem);
    internal::schedule(/*detach=*/true, async_mem);
    return async_mem;
  }
};
#endif  // __SYNTHESIS__

/// Defines an array of @c tapa::mmap.
template <typename T, uint64_t S>
#ifdef __SYNTHESIS__
class mmaps;
#else  // __SYNTHESIS__
class mmaps {
 protected:
  std::vector<mmap<T>> mmaps_;

 public:
  /// Constructs a @c tapa::mmap array from the given @c pointers and @c sizes.
  ///
  /// @param ptrs  Pointers to the start of the array of mapped memory.
  /// @param sizes Sizes of each mapped memory (in unit of element count).
  template <typename PtrContainer, typename SizeContainer>
  mmaps(const PtrContainer& pointers, const SizeContainer& sizes) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(pointers[i], sizes[i]);
    }
  }

  /// Constructs a @c tapa::mmap array from the given @c container.
  ///
  /// @param container Container holding an array of @c tapa::mmap. @c container
  ///                  must implement @c operator[] that returns a container
  ///                  suitable for constructing a @c tapa::mmap.
  template <typename Container>
  explicit mmaps(Container& container) {
    for (uint64_t i = 0; i < S; ++i) {
      mmaps_.emplace_back(container[i]);
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

  /// References a @c tapa::mmap in the array.
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

  /// Reinterprets the element type of each mapped memory as
  /// <tt>tapa::vec_t<T, N></tt>.
  ///
  /// This should be used on the host only.
  /// The size of each mapped memory must be a multiple of @c N.
  ///
  /// @tparam N  Vector length of the new element type.
  /// @return @c tapa::mmap of the same pieces of memory but of type
  ///         <tt>tapa::vec_t<T, N></tt>.
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

  /// Reinterprets the element type of each mapped memory as @c U.
  ///
  /// This should be used on the host only.
  /// Both @c T and @c U must have standard layout.
  /// The host memory pointers must be properly aligned.
  /// The size of each mapped memory must be a multiple of
  /// @c sizeof(U)/sizeof(T).
  ///
  /// @tparam U  The new element type.
  /// @return <tt>tapa::mmaps<U, S></tt> of the same pieces of memory.
  template <typename U>
  mmaps<U, S> reinterpret() const {
    static_assert(std::is_standard_layout<T>::value,
                  "T must have standard layout");
    static_assert(std::is_standard_layout<U>::value,
                  "U must have standard layout");

    std::array<U*, S> ptrs;
    std::array<uint64_t, S> sizes;
    for (uint64_t i = 0; i < S; ++i) {
      if (sizeof(U) > sizeof(T)) {
        constexpr auto N = sizeof(U) / sizeof(T);
        CHECK_EQ(mmaps_[i].size() % N, 0)
            << "size[" << i << "] must be a multiple of N = " << N;
      }
      CHECK_EQ(reinterpret_cast<size_t>(mmaps_[i].get()) % alignof(U), 0)
          << "pointer[" << i << "] must be " << alignof(U) << "-byte aligned";
      ptrs[i] = reinterpret_cast<U*>(mmaps_[i].get());
      sizes[i] = mmaps_[i].size() * sizeof(T) / sizeof(U);
    }
    return mmaps<U, S>(ptrs, sizes);
  }

 private:
  template <typename Param, typename Arg>
  friend struct internal::accessor;

  int access_pos_ = 0;

  mmap<T> access() {
    LOG_IF(WARNING, access_pos_ >= S)
        << "invocation #" << access_pos_ << " accesses mmaps["
        << access_pos_ % S << "]";
    return mmaps_[access_pos_++ % S];
  }
};

// Host-only mmap types that must have correct size.
#define TAPA_DEFINE_MMAP(tag)                          \
  template <typename T>                                \
  class tag##_mmap : public mmap<T> {                  \
    tag##_mmap(T* ptr) : mmap<T>(ptr) {}               \
                                                       \
   public:                                             \
    using mmap<T>::mmap;                               \
    tag##_mmap(const mmap<T>& base) : mmap<T>(base) {} \
                                                       \
    template <uint64_t N>                              \
    tag##_mmap<vec_t<T, N>> vectorized() const {       \
      return mmap<T>::template vectorized<N>();        \
    }                                                  \
    template <typename U>                              \
    tag##_mmap<U> reinterpret() const {                \
      return mmap<T>::template reinterpret<U>();       \
    }                                                  \
  }
TAPA_DEFINE_MMAP(placeholder);
TAPA_DEFINE_MMAP(read_only);
TAPA_DEFINE_MMAP(write_only);
TAPA_DEFINE_MMAP(read_write);
#undef TAPA_DEFINE_MMAP
#define TAPA_DEFINE_MMAPS(tag)                                        \
  template <typename T, uint64_t S>                                   \
  class tag##_mmaps : public mmaps<T, S> {                            \
    tag##_mmaps(const std::array<T*, S>& ptrs) : mmaps<T, S>(ptrs){}; \
                                                                      \
   public:                                                            \
    using mmaps<T, S>::mmaps;                                         \
    tag##_mmaps(const mmaps<T, S>& base) : mmaps<T, S>(base) {}       \
                                                                      \
    template <uint64_t N>                                             \
    tag##_mmaps<vec_t<T, N>, S> vectorized() const {                  \
      return mmaps<T, S>::template vectorized<N>();                   \
    }                                                                 \
    template <typename U>                                             \
    tag##_mmaps<U, S> reinterpret() const {                           \
      return mmaps<T, S>::template reinterpret<U>();                  \
    }                                                                 \
  }
TAPA_DEFINE_MMAPS(placeholder);
TAPA_DEFINE_MMAPS(read_only);
TAPA_DEFINE_MMAPS(write_only);
TAPA_DEFINE_MMAPS(read_write);
#undef TAPA_DEFINE_MMAPS

namespace internal {

template <typename T>
struct accessor<async_mmap<T>, mmap<T>&> {
  [[deprecated("please use async_mmap<T>& in formal parameters")]]  //
  static async_mmap<T>
  access(mmap<T>& arg) {
    return async_mmap<T>::schedule(arg);
  }
};

template <typename T>
struct accessor<async_mmap<T>&, mmap<T>&> {
  static async_mmap<T> access(mmap<T>& arg) {
    return async_mmap<T>::schedule(arg);
  }
};

template <typename T, uint64_t S>
struct accessor<mmap<T>, mmaps<T, S>&> {
  static mmap<T> access(mmaps<T, S>& arg) { return arg.access(); }
};

template <typename T, uint64_t S>
struct accessor<async_mmap<T>, mmaps<T, S>&> {
  [[deprecated("please use async_mmap<T>& in formal parameters")]]  //
  static async_mmap<T>
  access(mmaps<T, S>& arg) {
    return async_mmap<T>::schedule(arg.access());
  }
};

template <typename T, uint64_t S>
struct accessor<async_mmap<T>&, mmaps<T, S>&> {
  static async_mmap<T> access(mmaps<T, S>& arg) {
    return async_mmap<T>::schedule(arg.access());
  }
};

#define TAPA_DEFINE_ACCESSER(tag, frt_tag)                     \
  template <typename T>                                        \
  struct accessor<mmap<T>, tag##_mmap<T>> {                    \
    static mmap<T> access(tag##_mmap<T> arg) { return arg; }   \
    static void access(fpga::Instance& instance, int& idx,     \
                       tag##_mmap<T> arg) {                    \
      auto buf = fpga::frt_tag(arg.get(), arg.size());         \
      instance.SetArg(idx++, buf);                             \
    }                                                          \
  };                                                           \
  template <typename T, uint64_t S>                            \
  struct accessor<mmaps<T, S>, tag##_mmaps<T, S>> {            \
    static void access(fpga::Instance& instance, int& idx,     \
                       tag##_mmaps<T, S> arg) {                \
      for (uint64_t i = 0; i < S; ++i) {                       \
        auto buf = fpga::frt_tag(arg[i].get(), arg[i].size()); \
        instance.SetArg(idx++, buf);                           \
      }                                                        \
    }                                                          \
  }
TAPA_DEFINE_ACCESSER(placeholder, Placeholder);
// read/write are with respect to the kernel in tapa but host in frt
TAPA_DEFINE_ACCESSER(read_only, WriteOnly);
TAPA_DEFINE_ACCESSER(write_only, ReadOnly);
TAPA_DEFINE_ACCESSER(read_write, ReadWrite);
#undef TAPA_DEFINE_ACCESSER
template <typename T>
struct accessor<mmap<T>, mmap<T>> {
  static_assert(!std::is_same<T, T>::value,
                "must use one of "
                "placeholder_mmap/read_only_mmap/write_only_mmap/"
                "read_write_mmap in tapa::invoke");
};
template <typename T, int64_t S>
struct accessor<mmaps<T, S>, mmaps<T, S>> {
  static_assert(!std::is_same<T, T>::value,
                "must use one of "
                "placeholder_mmaps/read_only_mmaps/write_only_mmaps/"
                "read_write_mmaps in tapa::invoke");
};

}  // namespace internal

#endif  // __SYNTHESIS__

}  // namespace tapa

#endif  // TAPA_MMAP_H_
