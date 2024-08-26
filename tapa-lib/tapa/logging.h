// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_LOGGING_H_
#define TAPA_LOGGING_H_

#if !defined(TAPA_TARGET_)

#include "tapa/host/logging.h"

#elif TAPA_TARGET_ == XILINX_HLS

#include "tapa/xilinx/hls/logging.h"

#endif

#endif  // TAPA_LOGGING_H_
