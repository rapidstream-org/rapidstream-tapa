// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_BUFFER_H_
#define FPGA_RUNTIME_BUFFER_H_

#include <cstddef>

#include <type_traits>

#include <glog/logging.h>

#include "frt/tag.h"

namespace fpga {
namespace internal {

template <typename T, Tag tag>
class Buffer {
 public:
  Buffer(T* ptr, size_t n) : ptr_(ptr), n_(n) {}
  T* Get() const { return ptr_; }
  size_t Size() const { return n_; }
  size_t SizeInBytes() const { return n_ * sizeof(T); }

  template <typename U>
  Buffer<U, tag> Reinterpret() const {
    static_assert(std::is_standard_layout<T>::value,
                  "T must have standard layout");
    static_assert(std::is_standard_layout<U>::value,
                  "U must have standard layout");

    if constexpr (sizeof(U) > sizeof(T)) {
      constexpr auto N = sizeof(U) / sizeof(T);
      CHECK_EQ(sizeof(U) % sizeof(T), 0)
          << "sizeof(U) must be a multiple of sizeof(T) when Buffer<T> is "
             "reinterpreted as Buffer<U> (i.e., `Reinterpret<U>()`); got "
             "sizeof(U) = "
          << sizeof(U) << ", sizeof(T) = " << sizeof(T);
      CHECK_EQ(Size() % N, 0)
          << "size of Buffer<T> must be a multiple of N (= "
             "sizeof(U)/sizeof(T)) when reinterpreted as Buffer<U> (i.e., "
             "`Reinterpret<U>()`); got size = "
          << Size() << ", N = " << sizeof(U) << " / " << sizeof(T) << " = " << N
          << ", but " << Size() << " % " << N << " != 0";
    } else if constexpr (sizeof(U) < sizeof(T)) {
      CHECK_EQ(sizeof(T) % sizeof(U), 0)
          << "sizeof(T) must be a multiple of sizeof(U) when Buffer<T> is "
             "reinterpreted as Buffer<U> (i.e., `Reinterpret<U>()`); got "
             "sizeof(T) = "
          << sizeof(T) << ", sizeof(U) = " << sizeof(U);
    }
    CHECK_EQ(reinterpret_cast<size_t>(Get()) % alignof(U), 0)
        << "data of Buffer<T> must be " << alignof(U)
        << "-byte aligned when reinterpreted as Buffer<U> (i.e., "
           "`Reinterpret<U>()`) because alignof(U) = "
        << alignof(U);
    return Buffer<U, tag>(reinterpret_cast<U*>(Get()),
                          Size() * sizeof(T) / sizeof(U));
  }

 private:
  T* const ptr_;
  const size_t n_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_BUFFER_H_
