// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.
//
// Compatible layer for TAPA to behave like vendor-specific tools.
// This file must be included after other headers as it uses TAPA's types,
// depending on the target device.

#pragma once

namespace tapa {
namespace hls {

// HLS stream behaves like an infinite-depth stream in simulation.
template <typename T, uint64_t N = kStreamDefaultDepth>
using stream = ::tapa::stream<T, N, kStreamInfiniteDepth>;
template <typename T, uint64_t N = kStreamDefaultDepth>
using streams = ::tapa::streams<T, N, kStreamInfiniteDepth>;

}  // namespace hls
}  // namespace tapa
