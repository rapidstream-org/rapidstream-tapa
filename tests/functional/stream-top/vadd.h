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

// TEST 7: Ensure that ap_int can be used in a stream
typedef ap_int<32> output_ap_int_t;
static_assert(sizeof(output_ap_int_t) == 4, "output_ap_int_t must be 4 bytes");

typedef tapa::vec_t<output_ap_int_t, 2> output_vec_t;
static_assert(sizeof(output_vec_t) == 8, "output_vec_t must be 8 bytes");

#ifndef OUTPUT_TYPE
#define OUTPUT_TYPE output_ap_int_t
#endif

void StreamAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
               tapa::ostream<tapa::vec_t<OUTPUT_TYPE, 2>>& c);
