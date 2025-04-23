// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

constexpr int kModuleCount = 1000;
constexpr int kPortCount = 10;

void Copy(tapa::istreams<float, kPortCount / 2>& in_q,
          tapa::ostreams<float, kPortCount / 2>& out_q, int64_t n) {
  for (int64_t i = 0; i < n; ++i) {
    for (int j = 0; j < kPortCount / 2; ++j) {
      out_q[j].write(in_q[j].read());
    }
  }
}

void Scatter(tapa::istream<float>& in_q,
             tapa::ostreams<float, kPortCount / 2>& out_q, int64_t n) {
  for (int64_t i = 0; i < n; ++i) {
    const float data = in_q.read();
    for (int j = 0; j < kPortCount / 2; ++j) {
      out_q[j].write(data);
    }
  }
}

void Gather(tapa::istreams<float, kPortCount / 2>& in_q,
            tapa::ostream<float>& out_q, int64_t n) {
  for (int64_t i = 0; i < n; ++i) {
    tapa::vec_t<float, kPortCount / 2> data;
    for (int j = 0; j < kPortCount / 2; ++j) {
      data[j] = in_q[j].read();
    }
    out_q.write(tapa::sum(data));
  }
}

void ModuleScalability(tapa::istream<float>& in_q, tapa::ostream<float>& out_q,
                       int64_t n) {
  tapa::streams<float, (kModuleCount + 1) * (kPortCount / 2)> data_q;

  tapa::task()
      .invoke(Scatter, in_q, data_q, n)
      .invoke<tapa::join, kModuleCount>(Copy, data_q, data_q, n)
      .invoke(Gather, data_q, out_q, n);
}
