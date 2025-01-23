// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cstdint>
#include <iomanip>

#include <tapa.h>

using Elem = tapa::vec_t<float, 16>;
constexpr int kBankCount = 4;

constexpr uint64_t kRandom = 1 << 0;
constexpr uint64_t kRead = 1 << 1;
constexpr uint64_t kWrite = 1 << 2;

void Bandwidth(tapa::mmaps<Elem, kBankCount> chan, uint64_t n, uint64_t flags);
