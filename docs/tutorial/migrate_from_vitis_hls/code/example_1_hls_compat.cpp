// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <hls_vector.h>
#include <tapa.h>
#include <tapa/host/compat.h>
#include "assert.h"

#define MEMORY_DWIDTH 512
#define SIZEOF_WORD 4
#define NUM_WORDS ((MEMORY_DWIDTH) / (8 * SIZEOF_WORD))

#define DATA_SIZE 4096

void load_input(
    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in,
    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>&
        inStream,
    int Size) {
  for (int i = 0; i < Size; i++) {
#pragma HLS pipeline II = 1
    inStream << in[i];
  }
}

void compute_add(
    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>&
        in1_stream,
    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>&
        in2_stream,
    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>&
        out_stream,
    int Size) {
  for (int i = 0; i < Size; i++) {
#pragma HLS pipeline II = 1
    out_stream << (in1_stream.read() + in2_stream.read());
  }
}

void store_result(
    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> out,
    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>&
        out_stream,
    int Size) {
  for (int i = 0; i < Size; i++) {
#pragma HLS pipeline II = 1
    out[i] = out_stream.read();
  }
}

extern "C" {

void vadd(tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in1,
          tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in2,
          tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> out, int size) {
  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream(
      "input_stream_1");
  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream(
      "input_stream_2");
  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream(
      "output_stream");

  tapa::hls_compat::task()
      .invoke(load_input, in1, in1_stream, size)
      .invoke(load_input, in2, in2_stream, size)
      .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
      .invoke(store_result, out, out_stream, size);
}
}
