// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

using tapa::detach;
using tapa::istream;
using tapa::istreams;
using tapa::mmap;
using tapa::ostreams;
using tapa::streams;
using tapa::task;
using tapa::vec_t;

using pkt_t = uint64_t;
constexpr int kN = 8;  // kN x kN network

constexpr int kStageCount = 3;  // log2(kN)

void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
               ostreams<pkt_t, 2>& pkt_out_q) {
  uint8_t priority = 0;

  b = kStageCount - 1 - b;

  [[tapa::pipeline(1)]] for (bool valid_0, valid_1;;) {
#pragma HLS latency max = 0
    auto pkt_0 = pkt_in_q0.peek(valid_0);
    auto pkt_1 = pkt_in_q1.peek(valid_1);
    bool fwd_0_0 = valid_0 && (pkt_0 & (1 << b)) == 0;
    bool fwd_0_1 = valid_0 && (pkt_0 & (1 << b)) != 0;
    bool fwd_1_0 = valid_1 && (pkt_1 & (1 << b)) == 0;
    bool fwd_1_1 = valid_1 && (pkt_1 & (1 << b)) != 0;

    bool conflict =
        valid_0 && valid_1 && fwd_0_0 == fwd_1_0 && fwd_0_1 == fwd_1_1;
    bool prioritize_1 = priority & 1;

    bool read_0 = !((!fwd_0_0 && !fwd_0_1) || (prioritize_1 && conflict));
    bool read_1 = !((!fwd_1_0 && !fwd_1_1) || (!prioritize_1 && conflict));
    bool write_0 = fwd_0_0 || fwd_1_0;
    bool write_1 = fwd_1_1 || fwd_0_1;
    bool write_0_0 = fwd_0_0 && (!fwd_1_0 || !prioritize_1);
    bool write_1_1 = fwd_1_1 && (!fwd_0_1 || prioritize_1);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, check for conflict
    const bool written_0 =
        write_0 && pkt_out_q[0].try_write(write_0_0 ? pkt_0 : pkt_1);
    const bool written_1 =
        write_1 && pkt_out_q[1].try_write(write_1_1 ? pkt_1 : pkt_0);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, round robin priority of both ins
    if (read_0 && (write_0_0 ? written_0 : written_1)) {
      pkt_in_q0.read(nullptr);
    }
    if (read_1 && (write_1_1 ? written_1 : written_0)) {
      pkt_in_q1.read(nullptr);
    }

    if (conflict) ++priority;
  }
}

void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
  task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
}

void Stage(int b, istreams<pkt_t, kN>& in_q, ostreams<pkt_t, kN> out_q) {
  task().invoke<detach>(InnerStage, b, in_q, in_q, out_q);
}

void Produce(mmap<vec_t<pkt_t, kN>> mmap_in, uint64_t n,
             ostreams<pkt_t, kN>& out_q) {
produce:
  [[tapa::pipeline(1)]] for (uint64_t i = 0; i < n; ++i) {
    auto buf = mmap_in[i];
    for (int j = 0; j < kN; ++j) {
      out_q[j].write(buf[j]);
    }
  }
}

void Consume(mmap<vec_t<pkt_t, kN>> mmap_out, uint64_t n,
             istreams<pkt_t, kN> in_q) {
consume:
  [[tapa::pipeline(1)]] for (uint64_t i = 0; i < n; ++i) {
    vec_t<pkt_t, kN> buf;
    for (int j = 0; j < kN; ++j) {
      buf.set(j, in_q[j].read());
      CHECK_EQ(buf[j] % kN, j);
    }
    mmap_out[i] = buf;
  }
}

void Network(mmap<vec_t<pkt_t, kN>> mmap_in, mmap<vec_t<pkt_t, kN>> mmap_out,
             uint64_t n) {
  streams<pkt_t, kN*(kStageCount + 1), 4096> qs("qs");

  task()
      .invoke(Produce, mmap_in, n, qs)
      .invoke<tapa::join, kStageCount>(Stage, tapa::seq(), qs, qs)
      .invoke(Consume, mmap_out, n, qs);
}
