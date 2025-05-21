// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_HOST_MMAP_H_
#define TAPA_HOST_MMAP_H_

#include <cstddef>
#include <cstdint>

#include <type_traits>
#include <vector>

#include <frt.h>

#include "tapa/host/coroutine.h"
#include "tapa/host/stream.h"
#include "tapa/host/vec.h"

namespace tapa {

namespace internal {

template <typename Param, typename Arg>
struct accessor;

}  // namespace internal

template <typename T>
class async_mmap;

/// Defines a view of a piece of consecutive memory with synchronous random
/// accesses.
template <typename T>
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
  T* data() const { return ptr_; }

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
    CHECK_EQ(size() % N, 0)
        << "size of mmap<T> must be a multiple of N when vectorized as a "
           "mmap<vec_t<T, N>> (i.e., `vectorized<N>()`); got size = "
        << size() << ", N = " << N << ", but " << size() << " % " << N
        << " != 0";
    return mmap<vec_t<T, N>>(reinterpret_cast<vec_t<T, N>*>(get()), size() / N);
  }

  /// Reinterprets the element type of the mapped memory as @c U.
  ///
  /// This should be used on the host only.
  /// Both @c T and @c U must have standard layout.
  /// The host memory pointer must be properly aligned.
  /// If @c sizeof(U) > @c sizeof(T), the size of mapped memory must be a
  /// multiple of @c sizeof(U)/sizeof(T) (which must be an integer itself).
  /// If @c sizeof(U) < @c sizeof(T), @c sizeof(T) must be a multiple of
  /// @c sizeof(U).
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
      CHECK_EQ(sizeof(U) % sizeof(T), 0)
          << "sizeof(U) must be a multiple of sizeof(T) when mmap<T> is "
             "reinterpreted as mmap<U> (i.e., `reinterpret<U>()`); got "
             "sizeof(U) = "
          << sizeof(U) << ", sizeof(T) = " << sizeof(T);
      CHECK_EQ(size() % N, 0)
          << "size of mmap<T> must be a multiple of N (= sizeof(U)/sizeof(T)) "
             "when reinterpreted as mmap<U> (i.e., `reinterpret<U>()`); got "
             "size = "
          << size() << ", N = " << sizeof(U) << " / " << sizeof(T) << " = " << N
          << ", but " << size() << " % " << N << " != 0";
    } else if (sizeof(U) < sizeof(T)) {
      CHECK_EQ(sizeof(T) % sizeof(U), 0)
          << "sizeof(T) must be a multiple of sizeof(U) when mmap<T> is "
             "reinterpreted as mmap<U> (i.e., `reinterpret<U>()`); got "
             "sizeof(T) = "
          << sizeof(T) << ", sizeof(U) = " << sizeof(U);
    }
    CHECK_EQ(reinterpret_cast<std::size_t>(get()) % alignof(U), 0)
        << "data of mmap<T> must be " << alignof(U)
        << "-byte aligned when reinterpreted as mmap<U> (i.e., "
           "`reinterpret<U>()`) because alignof(U) = "
        << alignof(U);
    return mmap<U>(reinterpret_cast<U*>(get()), size() * sizeof(T) / sizeof(U));
  }

 protected:
  T* ptr_;
  uint64_t size_;
};

template <typename T>
class immap : public mmap<T> {
 public:
  // Inherit all constructors from mmap
  using mmap<T>::mmap;
};

template <typename T>
class ommap : public mmap<T> {
 public:
  // Inherit all constructors from mmap
  using mmap<T>::mmap;
};

/// Defines a view of a piece of consecutive memory with asynchronous random
/// accesses.
template <typename T>
class async_mmap : public mmap<T> {
 public:
  /// Type of the addresses.
  using addr_t = int64_t;

  /// Type of the write responses.
  using resp_t = uint8_t;

 private:
  using super = mmap<T>;

  stream<addr_t, 64> read_addr_q_{"read_addr"};
  stream<T, 64> read_data_q_{"read_data"};
  stream<addr_t, 64> write_addr_q_{"write_addr"};
  stream<T, 64> write_data_q_{"write_data"};
  stream<resp_t, 64> write_resp_q_{"write_resp"};

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
  ostream<addr_t> read_addr;

  /// Provides access to the read data channel.
  ///
  /// Each value read from this channel represents the data retrieved from the
  /// underlying memory system.
  istream<T> read_data;

  /// Provides access to the write address channel.
  ///
  /// Each value written to this channel triggers an asynchronous memory write
  /// request. Consecutive requests may be coalesced into a long burst request.
  ostream<addr_t> write_addr;

  /// Provides access to the write data channel.
  ///
  /// Each value written to this channel supplies data to the memory write
  /// request.
  ostream<T> write_data;

  /// Provides access to the write response channel.
  ///
  /// Each value read from this channel represents the data count acknowledged
  /// by the underlying memory system.
  istream<resp_t> write_resp;

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
    using i_addr_t = istream<addr_t>&;
    using i_data_t = istream<T>&;
    using o_addr_t = ostream<addr_t>&;
    using o_data_t = ostream<T>&;
    using i_resp_t = istream<resp_t>&;
    using o_resp_t = ostream<resp_t>&;
    using s_addr_t = stream<addr_t, 64>&;
    using s_data_t = stream<T, 64>&;
    using s_resp_t = stream<resp_t, 64>&;
    using internal::accessor;

    // a copy of async_mem is stored in std::function<void()>
    async_mmap async_mem(mem);
    // access the streams for the scheduled async i/o task
    accessor<i_addr_t, s_addr_t>::access(async_mem.read_addr_q_, false);
    accessor<o_data_t, s_data_t>::access(async_mem.read_data_q_, false);
    accessor<i_addr_t, s_addr_t>::access(async_mem.write_addr_q_, false);
    accessor<i_data_t, s_data_t>::access(async_mem.write_data_q_, false);
    accessor<o_resp_t, s_resp_t>::access(async_mem.write_resp_q_, false);
    internal::schedule(/*detach=*/true, async_mem);
    // access the streams for the user task
    accessor<o_addr_t, s_addr_t>::access(async_mem.read_addr_q_, false);
    accessor<i_data_t, s_data_t>::access(async_mem.read_data_q_, false);
    accessor<o_addr_t, s_addr_t>::access(async_mem.write_addr_q_, false);
    accessor<o_data_t, s_data_t>::access(async_mem.write_data_q_, false);
    accessor<i_resp_t, s_resp_t>::access(async_mem.write_resp_q_, false);
    return async_mem;
  }
};

/// Defines an array of @c tapa::mmap.
template <typename T, uint64_t S>
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
          << "size of mmap<T> must be a multiple of N when vectorized as a "
             "mmap<vec_t<T, N>> (i.e., `vectorized<N>()`); got size = "
          << mmaps_[i].size() << ", N = " << N << ", but " << mmaps_[i].size()
          << " % " << N << " != 0";
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
  /// If @c sizeof(U) > @c sizeof(T) , the size of each mapped memory must be a
  /// multiple of @c sizeof(U)/sizeof(T) (which must be an integer itself).
  /// If @c sizeof(U) < @c sizeof(T) , @c sizeof(T) must be a multiple of
  /// @c sizeof(N).
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
        CHECK_EQ(sizeof(U) % sizeof(T), 0)
            << "sizeof(U) must be a multiple of sizeof(T) when mmap<T> is "
               "reinterpreted as mmap<U> (i.e., `reinterpret<U>()`); got "
               "sizeof(U) = "
            << sizeof(U) << ", sizeof(T) = " << sizeof(T);
        constexpr auto N = sizeof(U) / sizeof(T);
        CHECK_EQ(mmaps_[i].size() % N, 0)
            << "size of mmap<T> must be a multiple of N (= "
               "sizeof(U)/sizeof(T)) when reinterpreted as mmap<U> (i.e., "
               "`reinterpret<U>()`); got size = "
            << mmaps_[i].size() << ", N = " << sizeof(U) << " / " << sizeof(T)
            << " = " << N << ", but " << mmaps_[i].size() << " % " << N
            << " != 0";
      } else if (sizeof(U) < sizeof(T)) {
        CHECK_EQ(sizeof(T) % sizeof(U), 0)
            << "sizeof(T) must be a multiple of sizeof(U) when mmap<T> is "
               "reinterpreted as mmap<U> (i.e., `reinterpret<U>()`); got "
               "sizeof(T) = "
            << sizeof(T) << ", sizeof(U) = " << sizeof(U);
      }
      CHECK_EQ(reinterpret_cast<std::size_t>(mmaps_[i].get()) % alignof(U), 0)
          << "data of mmap<T> must be " << alignof(U)
          << "-byte aligned when reinterpreted as mmap<U> (i.e., "
             "`reinterpret<U>()`) because alignof(U) = "
          << alignof(U);
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

template <typename T, int chan_count, int64_t chan_size>
class hmap : public mmap<T> {
 private:
  using super = mmap<T>;

 public:
  hmap(const super& mem) : super(mem) {
    CHECK_EQ(chan_size * chan_count, this->size())
        << "hmap<T, " << chan_count << ", " << chan_size
        << "> must have size = " << chan_size * chan_count << ", got "
        << this->size();
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

// Host-only immap types that must have correct size.
#define TAPA_DEFINE_IMMAP(tag)                            \
  template <typename T>                                   \
  class tag##_immap : public immap<T> {                   \
    tag##_immap(T* ptr) : immap<T>(ptr) {}                \
                                                          \
   public:                                                \
    using immap<T>::immap;                                \
    tag##_immap(const immap<T>& base) : immap<T>(base) {} \
                                                          \
    template <uint64_t N>                                 \
    tag##_immap<vec_t<T, N>> vectorized() const {         \
      return immap<T>::template vectorized<N>();          \
    }                                                     \
    template <typename U>                                 \
    tag##_immap<U> reinterpret() const {                  \
      return immap<T>::template reinterpret<U>();         \
    }                                                     \
  }
TAPA_DEFINE_IMMAP(placeholder);
TAPA_DEFINE_IMMAP(read_only);
TAPA_DEFINE_IMMAP(write_only);
TAPA_DEFINE_IMMAP(read_write);
#undef TAPA_DEFINE_IMMAP

// Host-only ommap types that must have correct size.
#define TAPA_DEFINE_OMMAP(tag)                            \
  template <typename T>                                   \
  class tag##_ommap : public ommap<T> {                   \
    tag##_ommap(T* ptr) : ommap<T>(ptr) {}                \
                                                          \
   public:                                                \
    using ommap<T>::ommap;                                \
    tag##_ommap(const ommap<T>& base) : ommap<T>(base) {} \
                                                          \
    template <uint64_t N>                                 \
    tag##_ommap<vec_t<T, N>> vectorized() const {         \
      return ommap<T>::template vectorized<N>();          \
    }                                                     \
    template <typename U>                                 \
    tag##_ommap<U> reinterpret() const {                  \
      return ommap<T>::template reinterpret<U>();         \
    }                                                     \
  }
TAPA_DEFINE_OMMAP(placeholder);
TAPA_DEFINE_OMMAP(read_only);
TAPA_DEFINE_OMMAP(write_only);
TAPA_DEFINE_OMMAP(read_write);
#undef TAPA_DEFINE_OMMAP

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
  access(mmap<T>& arg, bool) {
    LOG_FIRST_N(ERROR, 1) << "please use async_mmap<T>& in formal parameters";
    return async_mmap<T>::schedule(arg);
  }
};

template <typename T>
struct accessor<async_mmap<T>&, mmap<T>&> {
  static async_mmap<T> access(mmap<T>& arg, bool) {
    return async_mmap<T>::schedule(arg);
  }
};

template <typename T, uint64_t S>
struct accessor<mmap<T>, mmaps<T, S>&> {
  static mmap<T> access(mmaps<T, S>& arg, bool) { return arg.access(); }
};

template <typename T, uint64_t S>
struct accessor<async_mmap<T>, mmaps<T, S>&> {
  [[deprecated("please use async_mmap<T>& in formal parameters")]]  //
  static async_mmap<T>
  access(mmaps<T, S>& arg, bool) {
    LOG_FIRST_N(ERROR, 1) << "please use async_mmap<T>& in formal parameters";
    return async_mmap<T>::schedule(arg.access());
  }
};

template <typename T, uint64_t S>
struct accessor<async_mmap<T>&, mmaps<T, S>&> {
  static async_mmap<T> access(mmaps<T, S>& arg, bool) {
    return async_mmap<T>::schedule(arg.access());
  }
};

#define TAPA_DEFINE_ACCESSER(tag, tag_ref, frt_tag)                       \
  template <typename T>                                                   \
  struct accessor<mmap<T>, tag##mmap<T> tag_ref> {                        \
    static mmap<T> access(tag##mmap<T> tag_ref arg, bool) { return arg; } \
    static void access(fpga::Instance& instance, int& idx,                \
                       tag##mmap<T> tag_ref arg) {                        \
      auto buf = fpga::frt_tag(arg.get(), arg.size());                    \
      instance.SetArg(idx++, buf);                                        \
    }                                                                     \
  };                                                                      \
  template <typename T, uint64_t S>                                       \
  struct accessor<mmaps<T, S>, tag##mmaps<T, S> tag_ref> {                \
    static void access(fpga::Instance& instance, int& idx,                \
                       tag##mmaps<T, S> tag_ref arg) {                    \
      for (uint64_t i = 0; i < S; ++i) {                                  \
        auto buf = fpga::frt_tag(arg[i].get(), arg[i].size());            \
        instance.SetArg(idx++, buf);                                      \
      }                                                                   \
    }                                                                     \
  };                                                                      \
  template <typename T, int chan_count, int64_t chan_size>                \
  struct accessor<hmap<T, chan_count, chan_size>, tag##mmap<T tag_ref>> { \
    static void access(fpga::Instance& instance, int& idx,                \
                       tag##mmap<T> tag_ref arg) {                        \
      for (int i = 0; i < chan_count; ++i) {                              \
        auto buf = fpga::frt_tag(&arg[i * chan_size], chan_size);         \
        instance.SetArg(idx++, buf);                                      \
      }                                                                   \
    }                                                                     \
  }
TAPA_DEFINE_ACCESSER(placeholder_, , Placeholder);
// read/write are with respect to the kernel in tapa but host in frt
TAPA_DEFINE_ACCESSER(read_only_, , WriteOnly);
TAPA_DEFINE_ACCESSER(write_only_, , ReadOnly);
TAPA_DEFINE_ACCESSER(read_write_, , ReadWrite);
// For tapa::task.invoke(), the mmap is passed as reference to the accessor,
// so accessor<mmap<T>, mmap<T>&> is used. This is used for simulation and
// the mmap is passed as read-write buffer.
TAPA_DEFINE_ACCESSER(, &, ReadWrite);
#undef TAPA_DEFINE_ACCESSER

// If the user uses mmap/mmaps directly in tapa::invoke, it should be an error.
//
// This errors when mmap/mmaps are directly passed by value in the argument
// list, which is the case in tapa::invoke(), where mmap<T> parameter has
// an argument of mmap<T> instead of mmap<T> &.
//
// This does not apply to tapa::task().invoke() because the later always passes
// by reference, so that stream can be passed into istream &, using accessors.
// Therefore, mmaps arguments also have types of mmap &, instead of mmap.
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

}  // namespace tapa

// Generalized window_readincr for AIE
template <typename T>
T window_readincr(tapa::immap<const T>& mem) {
  T res = *mem;
  mem++;
  return res;
}

// Generalized window_writeincr for AIE
template <typename T>
void window_writeincr(tapa::ommap<T>& mem, const T& val) {
  *mem = val;
  mem++;
}

#endif  // TAPA_HOST_MMAP_H_
