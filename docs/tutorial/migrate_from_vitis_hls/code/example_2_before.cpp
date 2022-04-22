
#include <hls_vector.h>
#include <hls_stream.h>
#include "assert.h"

#define MEMORY_DWIDTH 512
#define SIZEOF_WORD 4
#define NUM_WORDS ((MEMORY_DWIDTH) / (8 * SIZEOF_WORD))

#define DATA_SIZE 4096

void load_input(
    hls::vector<uint32_t, NUM_WORDS>* in,
    hls::stream<hls::vector<uint32_t, NUM_WORDS> >& inStream,
    int Size
) {
  for (int i = 0; i < Size; i++) {
  #pragma HLS pipeline II=1
    inStream << in[i];
  }
}

void compute_add(
    hls::stream<hls::vector<uint32_t, NUM_WORDS> >& in1_stream,
    hls::stream<hls::vector<uint32_t, NUM_WORDS> >& in2_stream,
    hls::stream<hls::vector<uint32_t, NUM_WORDS> >& out_stream,
    int Size
) {
  for (int i = 0; i < Size; i++) {
  #pragma HLS pipeline II=1
    out_stream << (in1_stream.read() + in2_stream.read());
  }
}

void store_result(
    hls::vector<uint32_t, NUM_WORDS>* out,
    hls::istream<hls::vector<uint32_t>& out_stream,
    int Size
) {
  for (int i = 0; i < Size; i++) {
  #pragma HLS pipeline II=1
    out[i] = out_stream.read();
  }
}

extern "C" {

void vadd(
    tapa::mmap<uint32_t> in1,
    tapa::mmap<uint32_t> in2,
    tapa::mmap<uint32_t> out,
    int size,
    int n,
) {
  #pragma HLS INTERFACE m_axi port = in1 bundle = gmem0
  #pragma HLS INTERFACE m_axi port = in2 bundle = gmem1
  #pragma HLS INTERFACE m_axi port = out bundle = gmem0

  hls::stream<hls::vector<uint32_t, NUM_WORDS> > in1_stream("input_stream_1");
  hls::stream<hls::vector<uint32_t, NUM_WORDS> > in2_stream("input_stream_2");
  hls::stream<hls::vector<uint32_t, NUM_WORDS> > out_stream("output_stream");

  #pragma HLS dataflow

  vSize = size / NUM_WORDS;
  for (int i = 0; i < n; i++) {
    load_input(in1, in1_stream, vSize);
    load_input(in2, in2_stream, vSize);
    compute_add(in1_stream, in2_stream, out_stream, vSize);
    store_result(out, out_stream, vSize);
  }
}

}
