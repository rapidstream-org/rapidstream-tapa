// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cassert>
#include <cstdint>

#include <tapa.h>

// p x p PEs
const int p = 2;

// Handles kN x kN matrices maximum.
const int kN = 32;  // Use fixed value for efficient hardware generation.

void Cannon(tapa::mmap<const float> a_vec, tapa::mmap<const float> b_vec,
            tapa::mmap<float> c_vec, uint64_t n);
