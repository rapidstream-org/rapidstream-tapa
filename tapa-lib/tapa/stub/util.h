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
