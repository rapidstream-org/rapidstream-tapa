// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "tapa/base/util.h"

namespace tapa {

template <typename To, typename From>
inline typename std::enable_if<sizeof(To) == sizeof(From), To>::type  //
bit_cast(From from) noexcept;

template <typename T>
T reg(T x);

}  // namespace tapa

// FIXME: TAPA stubs should be target-agnotic
#undef TAPA_WHILE_NOT_EOT
#define TAPA_WHILE_NOT_EOT(fifo)                                \
  for (bool tapa_##fifo##_valid;                                \
       !fifo.eot(tapa_##fifo##_valid) || !tapa_##fifo##_valid;) \
  _Pragma("HLS pipeline II = 1") if (tapa_##fifo##_valid)

#undef TAPA_WHILE_NEITHER_EOT
#define TAPA_WHILE_NEITHER_EOT(fifo1, fifo2)                          \
  for (bool tapa_##fifo1##_valid, tapa_##fifo2##_valid;               \
       (!fifo1.eot(tapa_##fifo1##_valid) || !tapa_##fifo1##_valid) && \
       (!fifo2.eot(tapa_##fifo2##_valid) || !tapa_##fifo2##_valid);)  \
  _Pragma("HLS pipeline II = 1") if (tapa_##fifo1##_valid &&          \
                                     tapa_##fifo2##_valid)

#undef TAPA_WHILE_NONE_EOT
#define TAPA_WHILE_NONE_EOT(fifo1, fifo2, fifo3)                              \
  for (bool tapa_##fifo1##_valid, tapa_##fifo2##_valid, tapa_##fifo3##_valid; \
       (!fifo1.eot(tapa_##fifo1##_valid) || !tapa_##fifo1##_valid) &&         \
       (!fifo2.eot(tapa_##fifo2##_valid) || !tapa_##fifo2##_valid) &&         \
       (!fifo3.eot(tapa_##fifo3##_valid) || !tapa_##fifo3##_valid);)          \
  _Pragma("HLS pipeline II = 1") if (tapa_##fifo1##_valid &&                  \
                                     tapa_##fifo2##_valid &&                  \
                                     tapa_##fifo3##_valid)
