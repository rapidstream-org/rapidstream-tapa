// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cstdint>

#include <ap_int.h>

struct [[gnu::packed]] input_t {
  unsigned int skip_1 : 1;  // Byte 0, Bit 0
  float value;              // Byte 1-4, 32 bits
  uint64_t offset : 5;      // Byte 5, Bits 0-4
  bool skip_2 : 2;          // Byte 5, Bits 5-6
};

static_assert(sizeof(input_t) == 6, "input_t must be 6 bytes");

void StreamAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
               tapa::ostream<float>& c);
