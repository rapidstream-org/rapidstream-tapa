// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

using Elem = tapa::vec_t<float, 16>;
constexpr int kBankCount = 1;

constexpr uint64_t kRead = 1 << 1;
constexpr uint64_t kWrite = 1 << 2;
