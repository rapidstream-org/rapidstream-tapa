// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cstdint>

#include <ap_int.h>

struct [[gnu::packed]] input_t {
  unsigned int skip_1 : 1;
  float value;
  uint64_t offset : 5;
  bool skip_2;
};

void VecAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
            tapa::ostream<float>& c, uint64_t n);
