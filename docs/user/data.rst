Advanced Data Movements
=======================

.. note::

  TAPA offers several features for data movements, extending the capabilities
  of Vitis HLS to enhance the flexibility and expressiveness of your designs.

This section covers various aspects of data movement in TAPA, including stream
operations, memory-mapped interfaces, and asynchronous memory access.

Stream and MMAP Arrays
----------------------

TAPA supports arrays of streams (``istreams``/``ostreams``) and memory-mapped
interfaces (``mmaps``) to facilitate parameterized designs and reduce code
repetition (:ref:`api:streams`/:ref:`api:mmaps`). This feature is particularly
useful for creating flexible, scalable designs.

.. tip::

   A singleton ``stream`` or ``mmap`` is insufficient for parameterized
   designs. For example, the
   `network app <https://github.com/rapidstream-org/rapidstream-tapa/blob/main/tests/apps/network/network.cpp>`_
   shipped with TAPA defines an 8×8 switch network. What if we want to use a
   16×16 network? Or 4×4? TAPA allows parameterization of network size
   through arrays of ``stream``/``mmap`` and batch invocation.

With TAPA, you can define arrays of streams and memory-mapped interfaces and
invoking multiple tasks in parallel using ``invoke<..., n>``, where ``n``
is the number of invocations:

1. For each task instantiation, the ``streams`` or ``mmaps`` array arguments
   are accessed in sequence from the array, distributing the elements across
   multiple invocations.
2. If the formal parameter is a singleton (``istream``, ``ostream``, ``mmap``,
   ``async_mmap``), only one element in the array is accessed.
3. If the formal parameter is an array (``istreams``, ``ostreams``, ``mmaps``),
   the number of elements accessed is determined by the array length of the
   formal parameter.

Example usage from the
`TAPA network app <https://github.com/rapidstream-org/rapidstream-tapa/blob/main/tests/apps/network/network.cpp>`_:

.. code-block:: cpp

  void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
                 ostreams<pkt_t, 2>& pkt_out_q) {
  }

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

In the ``InnerStage`` function:

1. It instantiates the ``Switch2x2`` task ``kN / 2`` times using a single
   ``invoke<..., kN / 2>``.
2. The first argument ``b`` is a scalar input, broadcast to each ``Switch2x2``
   instance.
3. The second argument ``in_q0`` is an ``istreams<pkt_t, kN / 2>`` array. Each
   of the ``Switch2x2`` instances takes one ``istream<pkt_t>``, as
   the formal parameter is a singleton (``istream``).
4. The third argument ``in_q1`` is accessed similarly to ``in_q0``.
5. The fourth argument ``out_q`` is an ``ostreams<pkt_t, kN>`` array. Each
   ``Switch2x2`` instance takes one ``ostreams<pkt_t, 2>``, which is
   effectively two ``ostream<pkt_t>``.

.. note::

   TAPA uses ``istreams``, ``ostreams``, and ``mmaps`` to support arrays of
   streams and memory-mapped interfaces, and they are distributed across
   multiple invocations using ``invoke<..., n>``.

Stream Operations
-----------------

Peeking a Stream
^^^^^^^^^^^^^^^^

TAPA provides non-destructive read (peek) functionality for streams, allowing
you to read a token without removing it. This is useful when computations
depend on the content of input tokens, such as in switch networks.

Example usage from the
`TAPA network app <https://github.com/rapidstream-org/rapidstream-tapa/blob/main/tests/apps/network/network.cpp>`_:

.. code-block:: cpp

  for (bool is_valid_0, is_valid_1;;) {
    const auto pkt_0 = pkt_in_q0.peek(valid_0);
    const auto pkt_1 = pkt_in_q1.peek(valid_1);
    // Make decisions based on peeked values
    if (...) pkt_in_q0.read(nullptr);
    if (...) pkt_in_q1.read(nullptr);
  }

This example demonstrates the use of the peeking API through an implementation
of a 3-stage 8×8
`Omega network <https://www.mathcs.emory.edu/~cheung/Courses/355/Syllabus/90-parallel/Omega.html>`_.
At the core of this multi-stage switch network is a 2×2 switch box, which
routes input packets based on a specific bit in their destination address.
This destination address is embedded within the packet itself.

To illustrate, consider a packet from ``pkt_in_q0`` with a destination of 2
(binary ``010``). If we're focusing on bit 1 (using 0-based indexing), this
packet should be directed to ``pkt_out_q[1]``. Similarly, if a packet from
``pkt_in_q1`` has a destination of 7 (binary ``111``), it would also be
routed to ``pkt_out_q[1]``. However, since only one token can be written per
clock cycle, a decision must be made regarding which packet to prioritize.

This decision-making process needs to occur before removing any tokens from
the input channels (streams). The code accomplishes this by first peeking at
the input stream using :ref:`peek <classtapa_1_1istream_1a6df8ab2e1caaaf2e32844b7cc716cf11>`
to examine the destinations without consuming the data. Based on these peeked
destinations, it determines which inputs can be processed.

.. note::

   ``.peek()`` returns the token's value and validity, but does not consume
   the token from the stream.

End-of-Transaction
^^^^^^^^^^^^^^^^^^

TAPA allows sending special end-of-transaction (EoT) tokens to denote the end
of a data stream. This is particularly useful in dataflow optimizations where
proper kernel termination is required.

For example, `SODA <https://github.com/UCLA-VAST/soda>`_ is a highly parallel
microarchitecture for stencil applications. It is implemented using
`dataflow optimization <https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/vitis_hls_optimization_techniques.html#bmx1539734225930>`_.
However, this approach requires proper termination of each kernel.

Traditionally, this is achieved by broadcasting the loop trip-count to each
kernel function. However, this method necessitates an adder in each function,
which can be resource-intensive, especially for small kernel modules.

TAPA offers a more resource-efficient solution to this problem. It allows
kernels to send a special "End of Transaction" (EoT) token to signify
completion. This approach is demonstrated in the
`jacobi stencil example <https://github.com/rapidstream-org/rapidstream-tapa/blob/main/tests/apps/jacobi/jacobi.cpp>`_
provided with TAPA:

The producer, ``Mmap2Stream``, sends an ``EoT`` token by
:ref:`closing <classtapa_1_1ostream_1a10405849fa9a12a02e2fc0d33b305d22>`
the stream by calling ``stream.close()``:

.. code-block:: cpp

  void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                   tapa::ostream<tapa::vec_t<float, 2>>& stream) {
    [[tapa::pipeline(2)]] for (uint64_t i = 0; i < n; ++i) {
      tapa::vec_t<float, 2> tmp;
      tmp.set(0, mmap[i * 2]);
      tmp.set(1, mmap[i * 2 + 1]);
      stream.write(tmp);
    }
    stream.close();
  }

Downstream modules, such as ``Stream2Mmap`` in this example, can decide on
program termination by checking for the EoT token by checking on the
``eot`` flag returned by ``try_eot()``:

.. code-block:: cpp

  void Stream2Mmap(tapa::istream<tapa::vec_t<float, 2>>& stream,
                  tapa::mmap<float> mmap) {
    [[tapa::pipeline(2)]] for (uint64_t i = 0;;) {
      bool eot;
      if (stream.try_eot(eot)) {
        if (eot) break;
        auto packed = stream.read(nullptr);
        mmap[i * 2] = packed[0];
        mmap[i * 2 + 1] = packed[1];
        ++i;
      }
    }
  }

In summary, the API for EoT tokens in TAPA is as follows:

.. code-block:: cpp

  // Producer
  stream.close();

  // Consumer
  bool eot;
  if (stream.try_eot(eot)) {
    if (eot) break;
    // Process data
  }

.. note::

   TAPA supports the ``close()`` and ``try_eot()`` APIs to close a stream and
   check for the EoT token, respectively.

Asynchronous Memory Access
--------------------------

TAPA's ``async_mmap`` provides a flexible interface for external memory access
through the AXI protocol. It exposes the five AXI channels (``AR``, ``R``,
``AW``, ``W``, ``B``) in C++, giving users maximal control over memory access
patterns.

.. tip::

   ``async_mmap`` provides richer memory access patterns expressiveness than
   the traditional ``mmap`` interface with much smaller area overhead.

.. note::

   ``async_mmap`` supports runtime burst detection to optimize memory access.

Structure and Channels
^^^^^^^^^^^^^^^^^^^^^^

The ``async_mmap`` is defined as follows:

.. code-block:: cpp

  template <typename T>
  struct async_mmap {
    using addr_t = int64_t;
    using resp_t = uint8_t;

    tapa::ostream<addr_t> read_addr;
    tapa::istream<T> read_data;
    tapa::ostream<addr_t> write_addr;
    tapa::ostream<T> write_data;
    tapa::istream<resp_t> write_resp;
  };

This structure abstracts an external memory as an interface consisting of five
streams:

1. ``read_addr``: Output stream for read addresses.
2. ``read_data``: Input stream for read data.
3. ``write_addr``: Output stream for write addresses.
4. ``write_data``: Output stream for write data.
5. ``write_resp``: Input stream for write responses.

The ``async_mmap`` structure is illustrated in the following diagram:

.. image:: https://user-images.githubusercontent.com/32432619/162324279-93f2dd34-73a6-4fa5-a4df-afd032b94b80.png
  :width: 100 %

Usage Model
^^^^^^^^^^^

- Read operations:

  - Send an address to the ``read_addr`` channel.
  - Receive the corresponding data of type ``T`` from the ``read_data``
    channel.
  - Multiple read requests can be issued before receiving responses

- Write operations:

  - Send an address to the ``write_addr`` channel.
  - Send the corresponding data to the ``write_data`` channel.
  - The ``write_resp`` channel will receive data indicating how many write
    transactions have succeeded.

Basic Usage
^^^^^^^^^^^

``async_mmap`` should be used only as formal parameters in leaf-level tasks.
It can be constructed from ``mmap``, and an ``mmap`` argument can be passed to
an ``async_mmap`` parameter.

.. warning::

   ``async_mmap`` should only be used as formal parameters in leaf-level tasks,
   which are C++ functions that are called directly from ``tapa::task::invoke``
   and do not instantiate any children tasks or streams

.. warning::
   Due to certain from the Vitis HLS compiler, ``async_mmap`` must be passed
   by reference, i.e., with ``&``. In contrast, ``mmap`` must be passed by
   value, i.e., without ``&``.

.. code-block:: cpp

  void task1(tapa::async_mmap<data_t>& mem);
  void task2(tapa::      mmap<data_t>  mem);

  // Note the &
  void task1(tapa::async_mmap<data_t>& mem) {
    // ...
    mem.read_addr.write(...);
    mem.read_data.read();
    // ...
  }

  // Note no &
  void task2(tapa::mmap<data_t> mem) {
    // ...
    mem[i] = foo;
    bar = mem[j];
    // ...
  }

  void top(tapa::mmap<data_t> mem1, tapa::mmap<data_t> mem2) {
    tapa::task()
      .invoke(task1, mem1)
      .invoke(task2, mem2)
      ;
  }

Runtime Burst Detection
^^^^^^^^^^^^^^^^^^^^^^^

TAPA infers burst ``async_mmap`` transactions at runtime, allowing for
efficient memory access in both sequential and random access patterns.
Users only need to issue individual read/write transactions, and TAPA
optimizes them into burst transactions when possible.

This approach offers several advantages:

1. More efficient for both sequential and random access patterns.
2. No reliance on static analysis for burst inference.
3. Allows for dynamic, data-dependent access patterns.

.. raw:: html

   <details>
   <summary><a>What are bursts?</a></summary>
   <br/>


``mmap`` (which uses Vitis HLS ``#pragma HLS interface m_axi`` internally)
provides synchronous memory interfaces that heavily rely on memory bursts.
Without memory bursts, the access pattern looks like this:

.. figure:: ../figures/tapa-sync-mmap-no-burst.drawio.svg
  :width: 100 %

  Synchronous off-chip memory accesses without burst.

A significant issue is that long memory latency
(`typically 100 ~ 200 ns <https://arxiv.org/abs/2010.06075>`_)
can result in very low memory throughput. To address this, memory bursts
are widely used, allowing the kernel to receive multiple data pieces using
a single memory request:

.. figure:: ../figures/tapa-sync-mmap-burst.drawio.svg
  :width: 100 %

  Synchronous off-chip memory accesses with burst.

However, memory bursts are only available for consecutive memory access
patterns. To overcome this limitation, TAPA ``async_mmap`` uses a different
approach, issuing multiple outstanding requests simultaneously:

.. figure:: ../figures/tapa-async-mmap.drawio.svg
  :width: 100 %

  Asynchronous off-chip memory accesses.

Multi-outstanding asynchronous requests are much more efficient than
single-outstanding synchronous requests. However, for sequential access
patterns, large burst memory accesses are still significantly more efficient
than small individual transactions on external memory. For instance, reading
4 KB of data in one AXI transaction is much faster than 512 smaller 8-byte
AXI transactions. Current HLS tools (e.g., Vitis HLS) typically use static
analysis to infer bursts, which may result in unpredictable and limited
hardware.

TAPA, instead, infers burst transactions at runtime. Users only need to issue
individual read/write transactions, and TAPA provides optimized modules to
combine and merge sequential transactions into burst transactions dynamically.

.. figure:: ../figures/tapa-async-mmap-burst.drawio.svg
  :width: 100 %

  Asynchronous off-chip memory accesses with runtime burst detection.

.. raw:: html

   </details>
   <br/>

Smaller Area Overhead
^^^^^^^^^^^^^^^^^^^^^

Compared to Vitis HLS, TAPA's ``async_mmap`` implementation results in
significantly smaller area overhead. This is particularly beneficial
for HBM devices with multiple memory channels.

Quantitative results from
`a microbenchmark <https://escholarship.org/uc/item/404825zp>`_:

=============================== =========  ==== ==== ==== ==== ===
Memory Interface                Clock/MHz  LUT  FF   BRAM URAM DSP
=============================== =========  ==== ==== ==== ==== ===
``#pragma HLS interface m_axi``       300  1189 3740   15    0   0
``async_mmap``                        300  1466  162    0    0   0
=============================== =========  ==== ==== ==== ==== ===

As shown, ``async_mmap`` uses significantly fewer BRAM resources and
flip-flops, making it more efficient for designs with multiple memory
interfaces.

.. note::

   By using asynchronous memory interfaces and runtime burst detection,
   ``async_mmap`` enables high memory throughput for both sequential and
   random memory accesses with minimal area overhead.

Sharing Memory Interfaces
-------------------------

TAPA provides the flexibility to share memory-mapped interfaces among
dataflow modules, a feature not available in Vitis HLS. This capability
is particularly useful when the number of memory-mapped interfaces is limited.

Example: Shared Vector Add
^^^^^^^^^^^^^^^^^^^^^^^^^^

The shared vector add example shipped with TAPA demonstrates this capability
by putting the inputs ``a`` and ``b`` in the same memory-mapped interface.

.. code-block:: cpp

  void Mmap2Stream(mmap<float> mmap, int offset, uint64_t n, ostream<float>& stream) {
    for (uint64_t i = 0; i < n; ++i) {
      stream.write(mmap[n * offset + i]);
    }
    stream.close();
  }

  void Load(mmap<float> srcs, uint64_t n, ostream<float>& a, ostream<float>& b) {
    task()
        .invoke(Mmap2Stream, srcs, 0, n, a)
        .invoke(Mmap2Stream, srcs, 1, n, b);
  }

- The same ``mmap<float>`` is referenced twice in the ``Load`` function.
- Two ``Mmap2Stream`` task instances can access the same AXI instance.
- The ``offset`` parameter in ``Mmap2Stream`` allows for accessing different
  parts of the shared memory.

Implementation Details
^^^^^^^^^^^^^^^^^^^^^^

Under the hood, TAPA implements this sharing mechanism as follows:

1. **AXI Interconnect**: TAPA instantiates an AXI interconnect to manage
   access to the shared memory interface.
2. **Dedicated AXI Threads**: Each port using the shared interface gets a
   dedicated AXI thread.
3. **Unordered Requests**: Requests from different ports are not ordered
   with respect to each other. This helps reduce potential deadlocks.

.. warning::

   **Memory Consistency**: The programmer needs to ensure memory consistency
   among shared memory-mapped interfaces. This typically involves accessing
   different memory locations in different task instances.

.. note::

   TAPA allows sharing memory-mapped interfaces among dataflow modules,
   reducing the number of memory interfaces required.
