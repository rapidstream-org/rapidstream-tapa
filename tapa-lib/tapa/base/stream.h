// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_BASE_STREAM_H_
#define TAPA_BASE_STREAM_H_

#include <cstddef>
#include <cstdint>

#include <limits>

namespace tapa {

inline constexpr uint64_t kStreamDefaultDepth = 2;
inline constexpr uint64_t kStreamInfiniteDepth =
    std::numeric_limits<uint64_t>::max();

#ifndef TAPA_ACCURATE_FRT_STREAM_DEPTH
inline constexpr uint64_t kFrtUpgradeDepth = 64;
#endif

namespace internal {

template <typename T>
struct elem_t {
  T val;
  bool eot;
};

}  // namespace internal

}  // namespace tapa

#endif  // TAPA_BASE_STREAM_H_
