#ifndef TAPA_TAPA_H_
#define TAPA_TAPA_H_

#if !defined(TAPA_TARGET_)

#include "tapa/host/tapa.h"

#elif TAPA_TARGET_ == XILINX_HLS

#include "tapa/xilinx/hls/tapa.h"

#endif

#endif  // TAPA_TAPA_H_
