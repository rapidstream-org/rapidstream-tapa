Example 2
=====================================

This tutorial covers more corner cases not mentioned in Example 1.

Dataflow in a loop
-----------------------

- Currently TAPA does not support dataflow in a loop. The top function should only include:

  - Stream definitions
  - Task invokations

- If your original Vitis HLS code uses the dataflow-in-a-loop style, you may push the loop into the tasks.

.. code-block:: cpp

  // before
  # pragma HLS dataflow
  for (int i = 0; i < n; i++) { // this loop is invalid
    load_input(...);
    compute_add(...);
    // ...
  }

  void load_input(
    // ...
  ) {
    foo(); bar();
  }

.. code-block:: cpp

  // after

  // for (int i = 0; i < n; i++) {
  tapa::task()
  .invoke(load_input, in1, in1_stream, size, n)
  .invoke(load_input, in2, in2_stream, size, n)
  .invoke(compute_add, in1_stream, in2_stream, out_stream, size, n)
  .invoke(store_result, out, out_stream, size, n)
  ;
  // }

  void load_input(
    // ...
  ) {
    for (int iter = 0; iter < n; iter++) { // move the loop here
      foo(); bar();
    }
  }


Computation in the top function
---------------------------------------

- TAPA does not support computation in the top function because we want to strictly decouple communication and computation.
- If your original Vitis HLS code blends computation into the dataflow region, you could push them into specific tasks.
- In this example, ``vSize = size / NUM_WORDS;`` is actually invalid for TAPA, although it may seem trivial.

.. code-block:: cpp

  // before
  vSize = size / NUM_WORDS;

  # pragma HLS dataflow
  load_input(in1, in1_stream, vSize);
  load_input(in2, in2_stream, vSize);
  compute_add(in1_stream, in2_stream, out_stream, vSize);
  store_result(out, out_stream, vSize);

.. code-block:: cpp

  // after
  // Additional argument "n" for each task
  tapa::task()
  .invoke(load_input, in1, in1_stream, size)
  .invoke(load_input, in2, in2_stream, size)
  .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  .invoke(store_result, out, out_stream, size)
  ;

  void load_input(
    tapa::mmap<uint32_t> in,
    hls::ostream<hls::vector<uint32_t>& inStream,
    int Size,
    int n,
  ) {
    Size /= NUM_WORDS; // move the computation here
    for (int i = 0; i < Size; i++) {
    #pragma HLS pipeline II=1
      inStream.write(in[i]);
    }
  }


Final Look
------------

.. code-block:: cpp

  #include <tapa.h>
  #include "assert.h"

  #define MEMORY_DWIDTH 512
  #define SIZEOF_WORD 4
  #define NUM_WORDS ((MEMORY_DWIDTH) / (8 * SIZEOF_WORD))

  #define DATA_SIZE 4096

  void load_input(
      tapa::mmap<uint32_t> in,
      hls::ostream<hls::vector<uint32_t>& inStream,
      int Size,
      int n,
  ) {
    Size /= DATA_SIZE;
    for (int iter = 0; iter < n; iter++) {
      for (int i = 0; i < Size; i++) {
      #pragma HLS pipeline II=1
        inStream.write(in[i]);
      }
    }
  }

  void compute_add(
      tapa::istream<uint32_t>& in1_stream,
      tapa::istream<uint32_t>& in2_stream,
      tapa::ostream<uint32_t>& out_stream,
      int Size
      int n,
  ) {
    Size /= DATA_SIZE;
    for (int iter = 0; iter < n; iter++) {
      for (int i = 0; i < Size; i++) {
      #pragma HLS pipeline II=1
        out_stream.write(in1_stream.read() + in2_stream.read());
      }
    }
  }

  void store_result(
      tapa::mmap<uint32_t> out,
      tapa::istream<hls::vector<uint32_t>& out_stream,
      int Size
      int n,
  ) {
    Size /= DATA_SIZE;
    for (int iter = 0; iter < n; iter++) {
      for (int i = 0; i < Size; i++) {
      #pragma HLS pipeline II=1
        out[i] = out_stream.read();
      }
    }
  }

  void vadd(
      tapa::mmap<uint32_t> in1,
      tapa::mmap<uint32_t> in2,
      tapa::mmap<uint32_t> out,
      int size
      int n,
  ) {

    constexpr int FIFO_DEPTH = 2;
    tapa::stream<uint32_t, FIFO_DEPTH> in1_stream("input_stream_1");
    tapa::stream<uint32_t, FIFO_DEPTH> in2_stream("input_stream_2");
    tapa::stream<uint32_t, FIFO_DEPTH> out_stream("output_stream");

    tapa::task()
    .invoke(load_input, in1, in1_stream, size, n)
    .invoke(load_input, in2, in2_stream, size, n)
    .invoke(compute_add, in1_stream, in2_stream, out_stream, size, n)
    .invoke(store_result, out, out_stream, size, n)
    ;
  }
