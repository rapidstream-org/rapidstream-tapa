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

#include "tapa/traits.h"  // IWYU pragma: export
