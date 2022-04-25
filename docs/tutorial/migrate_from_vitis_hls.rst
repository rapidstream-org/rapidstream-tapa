Migrate from Vitis HLS
::::::::::::::::::::::

Example 1
=========

This tutorial shows how to convert the `vector-add`_ example from Vitis HLS to
TAPA.

.. _vector-add: https://github.com/Xilinx/Vitis_Accel_Examples/blob/6e994917db398446a11d0403b9088bcc251dd2da/hello_world/src/vadd.cpp

Update the Includes
-------------------

- Replace ``hls_stream.h`` by ``tapa.h``.
- No changes to other HLS headers such as ``ap_int.h``, etc.

.. code-block:: diff

   #include <hls_vector.h>
  -#include <hls_stream.h>
  +#include <tapa.h>
   #include "assert.h"

Update the Top Function
-----------------------

- Update the top function header.
- Replace the pointer parameters by ``tapa::mmap<T>``.
  Note that ``tapa::mmap<T>`` is passed by value.
- We no longer need to write ``#pragma HLS interface``.

.. code-block:: diff

   void vadd(
  -    hls::vector<uint32_t, NUM_WORDS>* in1,
  -    hls::vector<uint32_t, NUM_WORDS>* in2,
  -    hls::vector<uint32_t, NUM_WORDS>* out,
  +    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in1,
  +    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in2,
  +    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> out,
       int size
   ) {
  -  #pragma HLS INTERFACE m_axi port = in1 bundle = gmem0
  -  #pragma HLS INTERFACE m_axi port = in2 bundle = gmem1
  -  #pragma HLS INTERFACE m_axi port = out bundle = gmem0

- Update the stream definitions.
- Replace ``hls::stream<DATA_TYPE>`` by ``tapa::stream<DATA_TYPE>``.
  This creates a stream with the default depth of 2,
  `as in Vitis HLS <https://xilinx.github.io/Vitis-Tutorials/2021-2/build/html/docs/Hardware_Acceleration/Feature_Tutorials/03-dataflow_debug_and_optimization/fifo_sizing_and_deadlocks.html#deadlock-detection-and-analysis>`_.
  A different depth can be specified with
  ``tapa::stream<DATA_TYPE, FIFO_DEPTH>``.
- If there are stream arrays,
  we should use ``tapa::streams<DATA_TYPE, ARRAY_SIZE, FIFO_DEPTH>``.
  Refer to :ref:`Example 2 <tutorial/migrate_from_vitis_hls:example 2>`
  for details.

.. code-block:: diff

  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");

- Update the task invocations.
- No need for ``#pragma HLS dataflow``.
- Use the ``tapa::task().invoke()`` API.

  - The first argument is the task function;
    the remaining arguments are passed to the task.

- Use a chain of ``.invoke()`` to call all tasks.
  Note that we only append ``;`` to the very end.

.. code-block:: diff

  -  #pragma HLS dataflow
  -  load_input(in1, in1_stream, size);
  -  load_input(in2, in2_stream, size);
  -  compute_add(in1_stream, in2_stream, out_stream, size);
  -  store_result(out, out_stream, size);
  +  tapa::task()
  +  .invoke(load_input, in1, in1_stream, size)
  +  .invoke(load_input, in2, in2_stream, size)
  +  .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  +  .invoke(store_result, out, out_stream, size)
  +  ;

Update Task Definitions
-----------------------

- Update stream arguments.
- Replace ``hls::stream<DATA_TYPE>`` by ``tapa::istream<DATA_TYPE>`` or
  ``tapa::ostream<DATA_TYPE>``.

  - Note that we distinguish whether a stream argument is an *input* stream or
    an *output* stream.
  - No need to specify the stream depth here.

- Don't forget to pass streams by reference (with ``&``).

.. code-block:: diff

  void compute_add(
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& in1_stream,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& in2_stream,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
  +    tapa::istream<hls::vector<uint32_t, NUM_WORDS>>& in1_stream,
  +    tapa::istream<hls::vector<uint32_t, NUM_WORDS>>& in2_stream,
  +    tapa::ostream<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
       int size
   ) {

- Update external memory arguments.
- Replace pointers by ``tapa::mmap<DATA_TYPE>``.
  Note that ``tapa::mmap<DATA_TYPE>`` is passed by value (without ``*`` or
  ``&``).
- The code reads from to ``out_stream``, so it is actually a ``tapa::istream``;
  likewise, ``in_stream`` is actually a ``tapa::ostream``.
  Don't be confused by the stream names.

.. code-block:: diff

   void store_result(
  -    hls::vector<uint32_t, NUM_WORDS>* out,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
  +    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> out,
  +    tapa::istream<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
       int size
   ) {
     // ...
   }

   void load_input(
  -    hls::vector<uint32_t, NUM_WORDS>* in,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& inStream,
  +    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in,
  +    tapa::ostream<hls::vector<uint32_t, NUM_WORDS>>& inStream,
       int size
   ) {
     // ...
   }

- Update the stream APIs if necessary.
- Most APIs of ``tapa::stream`` are compatible with ``hls::stream``.

Final Look of Example 1
-----------------------

.. literalinclude:: migrate_from_vitis_hls/code/example_1_after.cpp
  :language: cpp

Example 2
=========

This tutorial covers more corner cases not mentioned in
:ref:`Example 1 <tutorial/migrate_from_vitis_hls:example 1>`.

Dataflow in a Loop
------------------

- Currently TAPA does not support dataflow in a loop.
  The top function should only include:

  - Stream definitions
  - Task invocations

- If your original Vitis HLS code uses the dataflow-in-a-loop style,
  you may push the loop into the tasks.

.. code-block:: cpp

  // before
  for (int i = 0; i < size; i++) { // this loop is invalid in TAPA
    #pragma HLS dataflow
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

  // for (int i = 0; i < size; i++) {
  tapa::task()
  .invoke(load_input, in1, in1_stream, size)
  .invoke(load_input, in2, in2_stream, size)
  .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  .invoke(store_result, out, out_stream, size)
  ;
  // }

  void load_input(
    // ...
  ) {
    for (int i = 0; i < size; i++) { // move the loop here
      foo(); bar();
    }
  }

Computation in the Top Function
-------------------------------

- TAPA does not support computation in the top function because we want to
  strictly decouple communication and computation.
- If your original Vitis HLS code blends computation into the dataflow region,
  you could push them into specific tasks.
- In this example, ``size /= NUM_WORDS;`` is actually invalid for TAPA,
  although it may seem trivial.

.. code-block:: cpp

  // before
  size /= NUM_WORDS;

  #pragma HLS dataflow
  load_input(in1, in1_stream, size);
  load_input(in2, in2_stream, size);
  compute_add(in1_stream, in2_stream, out_stream, size);
  store_result(out, out_stream, size);

.. code-block:: cpp

  // after
  tapa::task()
  .invoke(load_input, in1, in1_stream, size)
  .invoke(load_input, in2, in2_stream, size)
  .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  .invoke(store_result, out, out_stream, size)
  ;

  void load_input(
    tapa::mmap<hls::vector<uint32_t, NUM_WORDS>> in,
    tapa::ostream<hls::vector<uint32_t, NUM_WORDS>>& inStream,
    int size
  ) {
    size /= NUM_WORDS; // move the computation here
    for (int i = 0; i < size; i++) {
    #pragma HLS pipeline II=1
      inStream << in[i];
    }
  }


Final Look of Example 2
-----------------------

.. literalinclude:: migrate_from_vitis_hls/code/example_2_after.cpp
  :language: cpp
