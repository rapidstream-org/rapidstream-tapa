Migrate from Vitis HLS
======================

.. note::

  This tutorial assumes that you are familiar with Vitis HLS and have some
  experience with TAPA. We introduce the TAPA coding style and show how to
  migrate from Vitis HLS to TAPA.

Example 1: Basics with VecAdd
-----------------------------

In this tutorial, we'll walk through the process of converting a `vector
addition example`_ from Vitis HLS to TAPA. We'll cover the key changes needed
to make your code TAPA-compatible.

.. _vector addition example: https://github.com/Xilinx/Vitis_Accel_Examples/blob/6e994917db398446a11d0403b9088bcc251dd2da/hello_world/src/vadd.cpp

Step 1: Update the Includes
^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, we need to replace the HLS-specific header with TAPA's header.

.. code-block:: diff

   #include <hls_vector.h>
  -#include <hls_stream.h>
  +#include <tapa.h>
   #include "assert.h"

Other HLS headers like ``ap_int.h`` and ``hls_vector.h`` are still supported.
They can be included as usual.

Step 2: Update the Top Function
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Next, we'll modify the top function to use TAPA's memory-mapped interface.

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

In this step, we replace pointer parameters with ``tapa::mmap<T>``, which is
passed by value. We also remove HLS interface pragmas as they're no longer
needed.

Step 3: Update Stream Definitions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Now, let's update the stream definitions to use TAPA's stream type.

.. code-block:: diff

  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  +  tapa::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");

In this step, we replace ``hls::stream`` with ``tapa::stream``. The default
depth for TAPA streams is 2, matching Vitis HLS behavior. However, TAPA's
software simulation enforces the depth, allowing you to catch potential issues
early.

.. note::

  For setting a different depth, use ``tapa::stream<DATA_TYPE, FIFO_DEPTH>``.
  For stream arrays, use ``tapa::streams<DATA_TYPE, ARRAY_SIZE, FIFO_DEPTH>``.

Step 4: Update Task Invocations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Finally, we'll update how tasks are invoked using TAPA's task API.

.. code-block:: diff

  -  #pragma HLS dataflow
  -  load_input(in1, in1_stream, size);
  -  load_input(in2, in2_stream, size);
  -  compute_add(in1_stream, in2_stream, out_stream, size);
  -  store_result(out, out_stream, size);
  +  tapa::task()
  +    .invoke(load_input, in1, in1_stream, size)
  +    .invoke(load_input, in2, in2_stream, size)
  +    .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  +    .invoke(store_result, out, out_stream, size)
  +  ;

In this step, we remove the ``#pragma HLS dataflow`` directive as TAPA always
generates dataflow designs. We replace the function calls with ``tapa::task()``
and ``.invoke()``, chaining the invocations together and adding a semicolon only
the end.

Step 5: Update Task Definitions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Lastly, update the task function signatures to use TAPA's stream types.

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

Compared to Vitis HLS, TAPA requires stream arguments to be directional. We use
``tapa::istream`` for input streams and ``tapa::ostream`` for output streams,
in place of ``hls::stream``.

.. note::

   There is no need to specify the stream depth here, and the streams are
   passed by reference.

Similarly, replace pointers to external memory with ``tapa::mmap<DATA_TYPE>``:

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

.. note::

   The ``tapa::mmap<DATA_TYPE>`` is passed by value, without ``*`` or ``&``.

.. note::

   The code reads from to ``out_stream``, so it is actually a
   ``tapa::istream``; likewise, ``in_stream`` is actually a ``tapa::ostream``.
   Don't be confused by the stream names.

Final Code for Example 1
^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: migrate_from_vitis_hls/code/example_1_after.cpp
  :language: cpp

.. note::

   By updating the includes, top function, stream definitions, task invocations,
   and task definitions, we've successfully migrated the vector addition example
   from Vitis HLS to TAPA.

Example 2: Complex Scenarios
----------------------------

In this tutorial, we'll explore more advanced migration scenarios, focusing on
dataflow in loops and computation in the top function, which are not covered
in :ref:`Example 1 <tutorial/migrate_from_vitis_hls:example 1: Basics with VecAdd>`
and require additional attention.

Scenario 1: Dataflow in a Loop
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TAPA enforces a strict separation between communication structures and
computing units. This means that the "dataflow-in-a-loop" coding style in
Vitis HLS is not directly supported in TAPA.

.. warning::

   The compiler will enforce that a task that instantiates other tasks should
   only include stream definitions and task invocations.

In the following example, the dataflow region is defined within a loop to be
executed for multiple iterations in Vitis HLS. However, this is not allowed in
TAPA, as it will hinder quality of results by introducing additional logic.

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

For the code to be compatible with TAPA, we push the loop into the tasks:

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

We remove the outer loop from the top function and move the loop into each
task function. While this restriction may seem bothering, it ensures a good
timing quality of the generated hardware.

Scenario 2: Computation in the Top Function
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TAPA aims to strictly separate communication and computation for performance
and quality of results. Therefore, computation should not be performed in the
top function.

In the following example, the top function performs computation in a dataflow
region. This is not allowed in TAPA, and the computation should be pushed into
child tasks:

.. code-block:: cpp

  // before
  size /= NUM_WORDS;

  #pragma HLS dataflow
  load_input(in1, in1_stream, size);
  load_input(in2, in2_stream, size);
  compute_add(in1_stream, in2_stream, out_stream, size);
  store_result(out, out_stream, size);

While ``size /= NUM_WORDS`` seems trivial, it is not allowed in TAPA, as it
in fact introduces computation in the top function. We need to move the
computation into the child tasks:

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

TAPA requires the top function focused on task invocation and communication
structure setup.

Final Code for Example 2
^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: migrate_from_vitis_hls/code/example_2_after.cpp
  :language: cpp

.. note::

   TAPA requires a strict separation between communication structures and
   computing units. By pushing the loop and the computation into child tasks,
   TAPA ensures a good timing quality of the generated hardware.

Example 3: HLS-Compat Helpers
-----------------------------

HLS-Compat Helpers are designed to bridge the gap between Vitis HLS and TAPA,
allowing for incremental migration and verification. These helpers provide
HLS-compatible behavior while using TAPA coding style.

.. warning::

   This helper is only intended for software simulation and **is not
   synthesizable**. It is designed as the migration from existing HLS code
   to TAPA can take some efforts. The HLS-Compat helpers provide a way to
   incrementally migrate and verify the correctness of the code using software
   simulation.

We start from the HLS code of
`Example 1 <tutorial/migrate_from_vitis_hls:example 1: Basics with VecAdd>`
to demonstrate the usage of HLS-Compat helpers.

Step 1: Include the Compat Header
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To use the HLS-compat helpers, in addition to ``tapa.h``, also include
``tapa/host/compat.h``.

.. code-block:: diff

   #include <hls_vector.h>
  -#include <hls_stream.h>
  +#include <tapa.h>
  +#include <tapa/host/compat.h
   #include "assert.h"

Step 2: Use Infinite-Depth Streams
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In Vitis HLS's software simulation, ``hls::stream`` has infinite depth. While
it helps to simplify the development, it does not match the hardware behavior.
TAPA takes a different approach by enforcing a fixed depth for streams in the
simulation as the hardware does. This is usually desired as it helps to catch
potential issues early.

However, during development, it can be useful to have infinite-depth streams
for migration and verification.
The HLS-Compat helpers provide ``tapa::hls_compat::stream`` which behaves like
``hls::stream`` in software simulation.

.. code-block:: diff

  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  -  hls::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");
  +  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> in1_stream("input_stream_1");
  +  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> in2_stream("input_stream_2");
  +  tapa::hls_compat::stream<hls::vector<uint32_t, NUM_WORDS>> out_stream("output_stream");

Step 3: Use Direction-Agnostic Stream
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

HLS uses ``hls::stream&`` for both stream input and output; TAPA, however,
requires ``tapa::istream&`` for input streams and ``tapa::ostream&`` for
output streams. ``tapa::hls_compat::stream_interface&`` is the HLS-compat
equivalent of ``tapa::istream&`` and ``tapa::ostream&`` that exposes APIs
from both in software simulation.

.. code-block:: diff

  void compute_add(
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& in1_stream,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& in2_stream,
  -    hls::stream<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
  +    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>& in1_stream,
  +    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>& in2_stream,
  +    tapa::hls_compat::stream_interface<hls::vector<uint32_t, NUM_WORDS>>& out_stream,
       int size
   ) {

Step 4: Sequentially Scheduling Tasks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

HLS schedules dataflow tasks sequentially for software simulation.
TAPA's ``tapa::task()``, however, schedules them in parallel by default
to accelerate simulation and mimic the hardware behavior.
``tapa::hls_compat::task()`` is the HLS-compat equivalent of ``tapa::task()``
that schedules tasks sequentially in the order of invocations.

.. code-block:: diff

  -  load_input(in1, in1_stream, size);
  -  load_input(in2, in2_stream, size);
  -  compute_add(in1_stream, in2_stream, out_stream, size);
  -  store_result(out, out_stream, size);
  +  tapa::hls_compat::task()
  +  .invoke(load_input, in1, in1_stream, size)
  +  .invoke(load_input, in2, in2_stream, size)
  +  .invoke(compute_add, in1_stream, in2_stream, out_stream, size)
  +  .invoke(store_result, out, out_stream, size)
  +  ;

.. warning::

   Remember that this helper is only for software simulation and is not
   synthesizable. If sequential scheduling is desired in hardware, use
   ``tapa::task()`` and pass a token between tasks to signal the completion,
   and enforce the order of execution.

HLS-Compat Version of Example 1
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: migrate_from_vitis_hls/code/example_1_hls_compat.cpp
  :language: cpp

.. warning::

  ``tapa::hls_compat`` APIs are software simulation only and are NOT
  synthesizable.
  One must finish the migration before synthesis, including to remove
  ``#include <tapa/host/compat.h>`` and replace any ``tapa::hls_compat`` API
  with their synthesizable equivalent.
