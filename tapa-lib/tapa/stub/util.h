// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "tapa/base/util.h"

// FIXME: TAPA should provide its equivalence
#ifndef ap_wait
void ap_wait(void);
#endif

#ifndef ap_wait_n
void ap_wait_n(int);
#endif

namespace tapa {

template <typename To, typename From>
inline typename std::enable_if<sizeof(To) == sizeof(From), To>::type  //
bit_cast(From from) noexcept;

template <typename T, size_t Depth = 1>
T reg(T x);

}  // namespace tapa
