#include <tapa.h>

using tapa::detach;
using tapa::istream;
using tapa::istreams;
using tapa::join;
using tapa::mmap;
using tapa::ostream;
using tapa::ostreams;
using tapa::streams;
using tapa::task;
using tapa::vec_t;

using pkt_t = uint64_t;
constexpr int kN = 8;  // kN x kN network

void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
               ostream<pkt_t>& pkt_out_q0, ostream<pkt_t>& pkt_out_q1) {
  uint8_t priority = 0;
  for (bool valid_0, valid_1;;) {
#pragma HLS pipeline II = 1
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
    bool written_0 = write_0 && pkt_out_q0.try_write(write_0_0 ? pkt_0 : pkt_1);
    bool written_1 = write_1 && pkt_out_q1.try_write(write_1_1 ? pkt_1 : pkt_0);

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

void Stage(int b, istreams<pkt_t, 8>& in_q, ostreams<pkt_t, 8>& out_q) {
  task()
      .invoke<detach>(Switch2x2, b, in_q[0], in_q[4], out_q[0], out_q[1])
      .invoke<detach>(Switch2x2, b, in_q[1], in_q[5], out_q[2], out_q[3])
      .invoke<detach>(Switch2x2, b, in_q[2], in_q[6], out_q[4], out_q[5])
      .invoke<detach>(Switch2x2, b, in_q[3], in_q[7], out_q[6], out_q[7]);
}

void Produce(mmap<vec_t<pkt_t, kN>> mmap_in, uint64_t n,
             ostreams<pkt_t, kN>& out_q) {
produce:
  for (uint64_t i = 0; i < n; ++i) {
#pragma HLS pipeline II = 1
    auto buf = mmap_in[i];
    for (int j = 0; j < kN; ++j) {
      out_q[j].write(buf[j]);
    }
  }
}

void Consume(mmap<vec_t<pkt_t, kN>> mmap_out, uint64_t n,
             istreams<pkt_t, kN>& in_q) {
consume:
  for (uint64_t i = 0; i < n; ++i) {
#pragma HLS pipeline II = 1
    vec_t<pkt_t, kN> buf;
    for (int j = 0; j < kN; ++j) {
      buf.set(j, in_q[j].read());
    }
    mmap_out[i] = buf;
  }
}

void Network(mmap<vec_t<pkt_t, kN>> mmap_in, mmap<vec_t<pkt_t, kN>> mmap_out,
             uint64_t n) {
  streams<pkt_t, kN, 4096> q0("q0");
  streams<pkt_t, kN, 4096> q1("q1");
  streams<pkt_t, kN, 4096> q2("q2");
  streams<pkt_t, kN, 4096> q3("q3");

  task()
      .invoke<join>(Produce, mmap_in, n, q0)
      .invoke<join>(Stage, 2, q0, q1)
      .invoke<join>(Stage, 1, q1, q2)
      .invoke<join>(Stage, 0, q2, q3)
      .invoke<join>(Consume, mmap_out, n, q3);
}
