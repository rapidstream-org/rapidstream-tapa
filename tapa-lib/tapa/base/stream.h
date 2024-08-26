// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_BASE_STREAM_H_
#define TAPA_BASE_STREAM_H_

namespace tapa {

inline constexpr int kStreamDefaultDepth = 2;

namespace internal {

template <typename T>
struct elem_t {
  T val;
  bool eot;
};

}  // namespace internal

}  // namespace tapa

#endif  // TAPA_BASE_STREAM_H_
