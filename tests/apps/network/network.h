// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <tapa.h>

using tapa::detach;
using tapa::istream;
using tapa::istreams;
using tapa::mmap;
using tapa::ostreams;
using tapa::streams;
using tapa::task;
using tapa::vec_t;

constexpr int kN = 8;           // kN x kN network
constexpr int kStageCount = 3;  // log2(kN)

using pkt_t = uint64_t;
using pkt_vec_t = tapa::vec_t<pkt_t, kN>;

void Network(mmap<vec_t<pkt_t, kN>> mmap_in, mmap<vec_t<pkt_t, kN>> mmap_out,
             uint64_t n);
