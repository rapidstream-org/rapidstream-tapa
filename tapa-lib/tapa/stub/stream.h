// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "tapa/base/stream.h"

#include <cstddef>
#include <cstdint>

namespace tapa {

template <typename T>
class istream {
 public:
  bool empty() const;
  bool try_eot(bool& is_eot) const;
  bool eot(bool& is_success) const;
  bool eot(std::nullptr_t) const;
  bool try_peek(T& value) const;
  T peek(bool& is_success) const;
  T peek(std::nullptr_t) const;
  T peek(bool& is_success, bool& is_eot) const;
  bool try_read(T& value);
  T read();
  istream& operator>>(T& value);
  T read(bool& is_success);
  T read(std::nullptr_t);
  T read(const T& default_value, bool* is_success = nullptr);
  bool try_open();
  void open();
};

template <typename T>
class ostream {
 public:
  bool full() const;
  bool try_write(const T& value);
  void write(const T& value);
  ostream& operator<<(const T& value);
  bool try_close();
  void close();
};

template <typename T, uint64_t N = kStreamDefaultDepth,
          uint64_t SimulationDepth = N>
class stream : public istream<T>, public ostream<T> {
 public:
  constexpr static int depth = N;
  stream();
  template <size_t S>
  stream(const char (&name)[S]);
};

template <typename T, uint64_t S>
class istreams {
 public:
  constexpr static int length = S;
  istream<T> operator[](int pos) const;
};

template <typename T, uint64_t S>
class ostreams {
 public:
  constexpr static int length = S;
  ostream<T> operator[](int pos) const;
};

template <typename T, uint64_t S, uint64_t N = kStreamDefaultDepth,
          uint64_t SimulationDepth = N>
class streams : public istreams<T, S>, public ostreams<T, S> {
 public:
  constexpr static int length = S;
  constexpr static int depth = N;
  streams();
  template <size_t name_length>
  streams(const char (&name)[name_length]);
  stream<T, N> operator[](int pos) const;
};

}  // namespace tapa
