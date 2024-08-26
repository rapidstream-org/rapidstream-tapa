// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_TAPA_H_
#define TAPA_TAPA_H_

#if !defined(TAPA_TARGET_)

#include "tapa/host/tapa.h"  // IWYU pragma: export

#elif TAPA_TARGET_ == XILINX_HLS

#include "tapa/xilinx/hls/tapa.h"  // IWYU pragma: export

#endif

#include "tapa/traits.h"  // IWYU pragma: export

#endif  // TAPA_TAPA_H_
