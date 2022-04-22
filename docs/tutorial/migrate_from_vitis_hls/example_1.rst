Example 1
=====================================

This tutorial shows how to convert the `vector-add`_ example from Vitis HLS to TAPA.

.. _vector-add: https://github.com/Xilinx/Vitis_Accel_Examples/blob/master/hello_world/src/vadd.cpp


Update the includes
-----------------------

- Replace ``hls_stream.h`` by ``tapa.h``.
- No changes to other HLS headers such as ``ap_int.h``, etc.

.. code-block:: cpp

  // before
  #include <hls_stream.h>

.. code-block:: cpp

  // after
  #include <tapa.h>


Update the top function
-------------------------

- Update the top function header.
- Replace the pointer arguments by ``tapa::mmap<T>``. Note that ``tapa::mmap<T>`` is not passed by pointer.
- We no longer need to write ``pragma HLS interface``

.. code-block:: cpp

  // before
  void vadd(
          hls::vector<uint32_t, NUM_WORDS>* in1,
          hls::vector<uint32_t, NUM_WORDS>* in2,
          hls::vector<uint32_t, NUM_WORDS>* out,
          int size) {
  #pragma HLS INTERFACE m_axi port = in1 bundle = gmem
  #pragma HLS INTERFACE m_axi port = in2 bundle = gmem1
  #pragma HLS INTERFACE m_axi port = out bundle = gmem0

.. code-block:: cpp

  // after
  void vadd(
          tapa::mmap<uint32_t> in1,
          tapa::mmap<uint32_t> in2,
          tapa::mmap<uint32_t> out,
          int size) {

- Update the stream definitions.
- Replace ``hls::stream<T>`` by ``tapa::stream<DATA_TYPE, FIFO_DEPTH>``.
- If there are stream arrays, we should use ``tapa::streams<DATA_TYPE, ARRAY_SIZE, FIFO_DEPTH>``. Refer to other examples for details.

.. code-block:: cpp

  // before
  hls::stream<hls::vector<uint32_t, NUM_WORDS> > in1_stream("input_stream_1");
  hls::stream<hls::vector<uint32_t, NUM_WORDS> > in2_stream("input_stream_2");
  hls::stream<hls::vector<uint32_t, NUM_WORDS> > out_stream("output_stream");


.. code-block:: cpp

  // after
  constexpr int FIFO_DEPTH = 2;
  tapa::stream<uint32_t, FIFO_DEPTH> in1_stream("input_stream_1");
  tapa::stream<uint32_t, FIFO_DEPTH> in2_stream("input_stream_2");
  tapa::stream<uint32_t, FIFO_DEPTH> out_stream("output_stream");


- Update the task invocations.
- No need for ``#pragma HLS dataflow``
- Use the ``tapa::task().invoke()`` API.

  - The first argument is the task function name; the remaining arguments are the variables passed to the task.
- Use a chain of ``.invoke()`` to call all tasks. Note that we only append ``;`` to the very end.

.. code-block:: cpp

  // before
  #pragma HLS dataflow
  load_input(in1, in1_stream, Size);
  load_input(in2, in2_stream, Size);
  compute_add(in1_stream, in2_stream, out_stream, Size);
  store_result(out, out_stream, Size);

.. code-block:: cpp

  // after
  tapa::task()
  .invoke(load_input, in1, in1_stream, Size)
  .invoke(load_input, in2, in2_stream, Size)
  .invoke(compute_add, in1_stream, in2_stream, out_stream, Size)
  .invoke(store_result, out, out_stream, Size)
  ;


Update Task Definitions
---------------------------

- Update stream arguments.
- Replace ``hls::stream<>`` by ``tapa::istream<DATA_TYPE>`` or ``tapa::ostream<DATA_TYPE>``.

  - Note that we distinguish whether an stream argument is an *input* stream or an *output* stream.
  - No need to specify the stream depth here.

- Don't forget to pass streams as reference ``&``.

.. code-block:: cpp

    // before
    void compute_add(
        hls::stream<hls::vector<uint32_t, NUM_WORDS> >& in1_stream,
        hls::stream<hls::vector<uint32_t, NUM_WORDS> >& in2_stream,
        hls::stream<hls::vector<uint32_t, NUM_WORDS> >& out_stream,
        int Size) {

.. code-block:: cpp

    // after
    void compute_add(
        tapa::istream<uint32_t>& in1_stream,
        tapa::istream<uint32_t>& in2_stream,
        tapa::ostream<uint32_t>& out_stream,
        int Size) {


- Update external memory arguments.
- Replace pointers by ``tapa::mmap<DATA_TYPE>``. Note that ``tapa::mmap<DATA_TYPE>`` is not passed by pointer.
- The code reads from to ``out_stream`` so it is actually an ``tapa::istream``; likewise ``in_stream`` is actually an ``tapa::ostream``. Don't be confused by the stream names.

.. code-block:: cpp

  // before
  void store_result(
      hls::vector<uint32_t, NUM_WORDS>* out,
      hls::stream<hls::vector<uint32_t, NUM_WORDS> >& out_stream,
      int Size) {
    // ...
  }
  void load_input(
      hls::vector<uint32_t, NUM_WORDS>* in,
      hls::stream<hls::vector<uint32_t, NUM_WORDS> >& inStream,
      int Size) {
    // ...
  }

.. code-block:: cpp

  // after
  void store_result(
      tapa::mmap<uint32_t> out,
      tapa::istream<uint32_t>& out_stream,
      int Size) {
    // ...
  }
  void load_input(
      tapa::mmap<uint32_t> in,
      tapa::ostream<uint32_t>& inStream,
      int Size) {
    // ...
  }


- Update the stream APIs.
- Most APIs of ``tapa::stream<>`` is compatible with ``hls::stream<>``.
- In this example, the ``<<`` and ``>>`` overload of ``hls::stream<>`` is not supported.

.. code-block:: cpp

  // before
  for (int i = 0; i < Size; i++) {
  #pragma HLS pipeline II=1
      inStream << in[i];
  }

.. code-block:: cpp

  // after
  for (int i = 0; i < Size; i++) {
  #pragma HLS pipeline II=1
      inStream.write(in[i]);
  }

Final Look
-----------------

.. code-block:: cpp

  #include <hls_vector.h>
  #include <tapa.h>
  #include "assert.h"

  #define MEMORY_DWIDTH 512
  #define SIZEOF_WORD 4
  #define NUM_WORDS ((MEMORY_DWIDTH) / (8 * SIZEOF_WORD))

  #define DATA_SIZE 4096

  void load_input(
      tapa::mmap<uint32_t> in,
      tapa::ostream<hls::vector<uint32_t>& inStream,
      int Size
  ) {
    for (int i = 0; i < Size; i++) {
    #pragma HLS pipeline II=1
      inStream.write(in[i]);
    }
  }

  void compute_add(
      tapa::istream<uint32_t>& in1_stream,
      tapa::istream<uint32_t>& in2_stream,
      tapa::ostream<uint32_t>& out_stream,
      int Size
  ) {
    for (int i = 0; i < Size; i++) {
    #pragma HLS pipeline II=1
      out_stream.write(in1_stream.read() + in2_stream.read());
    }
  }

  void store_result(
      tapa::mmap<uint32_t> out,
      tapa::istream<hls::vector<uint32_t>& out_stream,
      int Size
  ) {
    for (int i = 0; i < Size; i++) {
    #pragma HLS pipeline II=1
      out[i] = out_stream.read();
    }
  }

  void vadd(
      tapa::mmap<uint32_t> in1,
      tapa::mmap<uint32_t> in2,
      tapa::mmap<uint32_t> out,
      int size
  ) {

    constexpr int FIFO_DEPTH = 2;
    tapa::stream<uint32_t, FIFO_DEPTH> in1_stream("input_stream_1");
    tapa::stream<uint32_t, FIFO_DEPTH> in2_stream("input_stream_2");
    tapa::stream<uint32_t, FIFO_DEPTH> out_stream("output_stream");

    tapa::task()
    .invoke(load_input, in1, in1_stream, Size)
    .invoke(load_input, in2, in2_stream, Size)
    .invoke(compute_add, in1_stream, in2_stream, out_stream, Size)
    .invoke(store_result, out, out_stream, Size)
    ;
  }
