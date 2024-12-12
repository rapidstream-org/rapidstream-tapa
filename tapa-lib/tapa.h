// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#if !defined(TAPA_TARGET_DEVICE_)

#include "tapa/host/tapa.h"  // IWYU pragma: export

#elif defined(TAPA_TARGET_XILINX_HLS_)

#include "tapa/xilinx/hls/tapa.h"  // IWYU pragma: export

#elif defined(TAPA_TARGET_STUB_)

#include "tapa/stub/tapa.h"  // IWYU pragma: export

#endif

// Add compatibility layers for TAPA to behave like vendor-specific tools.
// Compatible layer are meant to behave like a specific vendor, such as
// Vitis HLS's hls::stream, where the simulation depth is infinite.
// It is compilable (synthesizable) for the specific vendor, but we also
// strive to make it compilable for other vendors.
#include "tapa/compat.h"  // IWYU pragma: export
