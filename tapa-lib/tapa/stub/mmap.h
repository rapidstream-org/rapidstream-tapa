// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cstddef>

#include "tapa/base/mmap.h"
#include "tapa/stub/stream.h"
#include "tapa/stub/vec.h"

namespace tapa {

template <typename T>
class mmap {
 public:
  explicit mmap(T* ptr);
  mmap(T* ptr, uint64_t size);

  template <typename Container>
  explicit mmap(Container& container);

  operator T*();
  mmap& operator++();
  mmap& operator--();
  mmap operator++(int);
  mmap operator--(int);
  T* data() const;
  T* get() const;
  uint64_t size() const;

  template <uint64_t N>
  mmap<vec_t<T, N>> vectorized() const;

  template <typename U>
  mmap<U> reinterpret() const;
};

template <typename T>
struct async_mmap : public mmap<T> {
 public:
  using addr_t = int64_t;
  using resp_t = uint8_t;

  ostream<addr_t> read_addr;
  istream<T> read_data;
  ostream<addr_t> write_addr;
  ostream<T> write_data;
  istream<resp_t> write_resp;

  void operator()();
  static async_mmap schedule(mmap<T> mem);
};

template <typename T, uint64_t S>
class mmaps {
 public:
  template <typename PtrContainer, typename SizeContainer>
  mmaps(const PtrContainer& pointers, const SizeContainer& sizes);

  template <typename Container>
  explicit mmaps(Container& container);

  mmaps(const mmaps&) = default;
  mmaps(mmaps&&) = default;
  mmaps& operator=(const mmaps&) = default;
  mmaps& operator=(mmaps&&) = default;

  mmap<T>& operator[](int idx);

  template <uint64_t offset, uint64_t length>
  mmaps<T, length> slice();

  template <uint64_t N>
  mmaps<vec_t<T, N>, S> vectorized();

  template <typename U>
  mmaps<U, S> reinterpret() const;
};

template <typename T, int chan_count, int64_t chan_size>
class hmap : public mmap<T> {
 public:
  hmap(const mmap<T>& mem);
};

}  // namespace tapa
