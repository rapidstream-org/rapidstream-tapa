#ifndef TAPA_LOGGING_H_
#define TAPA_LOGGING_H_

#if !defined(TAPA_TARGET_)

#include "tapa/host/logging.h"

#elif TAPA_TARGET_ == XILINX_HLS

#include "tapa/xilinx/hls/logging.h"

#endif

#endif  // TAPA_LOGGING_H_
