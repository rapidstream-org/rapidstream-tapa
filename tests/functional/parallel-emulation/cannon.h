// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cassert>
#include <cstdint>

#include <tapa.h>

// p x p PEs
constexpr int p = TAPA_CANNON_P;

// Handles kN x kN matrices maximum. Use fixed value for efficient hardware
// generation.
constexpr int kN = p * 16;

void Cannon(tapa::mmap<const float> a_vec, tapa::mmap<const float> b_vec,
            tapa::mmap<float> c_vec, uint64_t n);
