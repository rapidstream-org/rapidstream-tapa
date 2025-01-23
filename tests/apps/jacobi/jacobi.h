// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <tapa.h>

void Jacobi(tapa::mmap<float> bank_0_t0, tapa::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num);
