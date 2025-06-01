// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.
//
// Compatible layer for TAPA to behave like vendor-specific tools

#pragma once

#include <cstdint>

#include "tapa.h"

namespace tapa {
namespace hls {

// HLS stream behaves like an infinite-depth stream in simulation.
template <typename T, uint64_t N = kStreamDefaultDepth>
using stream = ::tapa::stream<T, N, kStreamInfiniteDepth>;

}  // namespace hls
}  // namespace tapa
