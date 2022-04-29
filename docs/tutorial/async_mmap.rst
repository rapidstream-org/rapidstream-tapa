.. _introduction-to-async-mmap:

Flexible Memory Access
===========================================

``async_mmap`` is TAPA's flexible interface to access external memory through
the AXI protocol.
It exposes the five AXI channels (AR, R, AW, W, B) in C++.
In this way,
users have maximal control over how they want to access the external memory.
The ``async_mmap`` feature has several advantages:

- Richer expressiveness and more flexible memory access pattern.

- Runtime burst detection.

- Smaller area overhead.

A successful case is
`the SPLAG accelerator <https://github.com/UCLA-VAST/splag>`_
for single-source shortest path (SSSP),
which achieves up to a 4.9× speedup
over state-of-the-art SSSP accelerators,
up to a 2.6× speedup over 32-thread CPU running at 4.4 GHz,
and up to a 0.9× speedup over an A100 GPU
(that has 4.1× power budget and 3.4× HBM bandwidth).

That being said,
``async_mmap`` is relatively more difficult to use when compared to the
conventional approach (abstract external memory as a simple array)
because of the extra expressiveness.
This tutorial shows examples of how to bring out the most potential of
``async_mmap``.

The Programming Model of ``async_mmap``
---------------------------------------

The following code shows the definition of ``async_mmap``:

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


``async_mmap`` abstracts an external memory as an interface consisting of 5 channels/streams/FIFOs, as shown in the following figure.

.. image:: https://user-images.githubusercontent.com/32432619/162324279-93f2dd34-73a6-4fa5-a4df-afd032b94b80.png
  :width: 100 %

- On the read side, if we send one address to the ``read_addr`` channel,
  the data of type ``T`` stored in that address will appear later in the
  ``read_data`` channel.

- If we send multiple addresses to the ``read_addr`` channel,
  the corresponding data (i.e., the read responses) will appear in order
  in the ``read_data`` channel.

- On the write side, if we (1) send an address to the ``write_addr`` channel,
  and (2) send the corresponding data to the ``write_data`` channel,
  then the data will be written into the associated address.

- If there are multiple outstanding write requests, they will be committed in order.

- The ``write_resp`` channel will receive data that represent how many write transactions have succeeded.

Basic Usage of ``async_mmap``
-----------------------------

``async_mmap`` is a special implementation of ``mmap`` that should be used
only as formal parameters in lower-level tasks [#]_.
``async_mmap`` can be constructed from ``mmap``,
and we could pass an ``mmap`` argument to an ``async_mmap`` parameter.
Due to certain limitations from the Vitis HLS compiler,
``async_mmap`` must be passed by reference, i.e., with ``&``.
In contrast, ``mmap`` must be passed by value, i.e., without ``&``.

.. [#] Lower-level tasks in TAPA are C++ functions that are called direct
  from ``tapa::task::invoke`` and do not instantiate any children tasks or
  streams itself.

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
-----------------------

``mmap`` (which uses Vitis HLS ``#pragma HLS interface m_axi`` under the hood)
are synchronous memory interfaces that heavily rely on memory bursts.
Without memory bursts, the access pattern looks like the following:

.. figure:: ../figures/tapa-sync-mmap-no-burst.drawio.svg
  :width: 100 %

  Synchronous off-chip memory accesses without burst.

An obvious problem is that the long memory latency
(`typically 100 ~ 200 ns <https://arxiv.org/abs/2010.06075>`_)
can result in very low memory throughput.
To solve this problem, memory bursts have been used extensively,
which allows the kernel to receive many pieces of data using a single memory
request:

.. figure:: ../figures/tapa-sync-mmap-burst.drawio.svg
  :width: 100 %

  Synchronous off-chip memory accesses with burst.

However,
memory bursts are only available when the memory access pattern is consecutive.
To solve the problem, TAPA ``async_mmap`` takes a different approach,
which is to issue multiple outstanding requests at the same time:

.. figure:: ../figures/tapa-async-mmap.drawio.svg
  :width: 100 %

  Asynchronous off-chip memory accesses.

Multi-outstanding asynchronous requests are much more efficient than
single-outstanding synchronous requests,
but for sequential access patterns,
accessing memory in large bursts is still significantly more efficient
than in small individual transactions on the external memory.
For example,
reading 4 KB of data in one AXI transaction is much faster than 512 smaller
8-byte AXI transactions.
Existing HLS tools (e.g., Vitis HLS) generally rely on static analysis to
infer bursts,
which may generate unpredictable and limited hardware.

Instead, TAPA infers burst transactions at runtime.
User only needs to issue individual read/write transactions,
and TAPA provides optimized modules to combine and merge sequential transactions
into burst transactions at runtime.

.. figure:: ../figures/tapa-async-mmap-burst.drawio.svg
  :width: 100 %

  Asynchronous off-chip memory accesses with runtime burst detection.

With asynchronous memory interfaces and runtime burst detection,
``async_mmap`` makes it possible to achieve high memory throughput for both
sequential and random memory accesses.

Smaller Area Overhead
---------------------

When interacting with the AXI interface, Vitis HLS will buffer the entire burst transactions using on-chip memories. For a 512-bit AXI interface, the AXI buffers generated by Vitis HLS costs 15 BRAM_18K each for the read channel and the write channel. This becomes a huge problem for HBM devices, where the bottom SLR is packed with 32 HBM channels, and the AXI buffers along takes away >900 BRAM_18K from the bottom SLR.

In our settings, the read responses will be directly passed to the user logic through a stream interface, thus the AXI interface has much smaller area.

The following table shows quantitative results from
`a microbenchmark <https://escholarship.org/uc/item/404825zp>`_:

=============================== =========  ==== ==== ==== ==== ===
Memory Interface                Clock/MHz  LUT  FF   BRAM URAM DSP
=============================== =========  ==== ==== ==== ==== ===
``#pragma HLS interface m_axi``       300  1189 3740   15    0   0
``async_mmap``                        300  1466  162    0    0   0
=============================== =========  ==== ==== ==== ==== ===



Sharing External Memory Interfaces
---------------------------------------

This section covers the usage of shared memory-mapped interfaces.

Vitis HLS does not allow sharing of memory-mapped interfaces among dataflow
modules.
TAPA gives a programmer the flexibility to do this.
This can be very useful when the number of memory-mapped interfaces is limited.
For example, the shared vector add example shipped with TAPA puts the inputs
``a`` and ``b`` in the same memory-mapped interface.
By referencing the same ``mmap<float>`` twice, the two ``Mmap2Stream`` task
instances can both access the same AXI instance.

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

.. note::

  The programmer needs to make sure of memory consistency among
  shared memory-mapped interfaces, for example,
  by accessing different memory locations in different task instances.

.. tip::

  Under the hood,
  TAPA instantiates an AXI interconnect and use a dedicated AXI thread
  for each port so that requests from different ports are not ordered
  with respect to each other.
  This can help reduce potential deadlocks at the cost of more resource usage.


Example 1: Multi-Outstanding Random Memory Accesses
---------------------------------------------------

This example shows how to implement efficient random memory accesses using TAPA.
The key point is to allow multiple outstanding memory operations.
Even though random memory access cannot be merged into bursts,
it is still more effective to allow multiple outstanding transactions.
In the following example,
the ``issue_read_addr`` task will keep issuing read requests as long as the AXI
interface is ready to accept,
while the ``receive_read_resp`` task is only responsible for receiving and
process the responses.

.. code-block:: cpp

  void issue_read_addr(
    tapa::async_mmap<data_t>& mem,
    int n) {
    addr_t random_addr[N];
    for (int i = 0; i < n; ) {
      #pragma HLS pipeline II=1
      if (!mem.read_addr.full()) {
        mem.read_addr.try_write(random_addr[i]);
        i++;
      }
    }
  }

  void receive_read_resp(
    tapa::async_mmap<data_t>& mem,
    int n) {
    for (int i = 0; i < n; ) {
      #pragma HLS pipeline II=1
      if (!mem.read_data.empty()) {
        data_t d = mem.read_data.read();
        i++;
        // ...
      }
    }
  }

  void top(tapa::mmap<data_t> mem, int n) {

    tapa::task()
      // ...
      .invoke(issue_read_addr, mem, n)
      .invoke(receive_read_resp, mem, n)
      // ...
      ;
  }


This simple design is actually very hard or infeasible to implement in Vitis
HLS.
Consider the following Vitis HLS counterpart.
The generated hardware will issue one read request,
then **wait for its response** before issuing another read request,
so there will be only 1 outstanding transactions.

.. code-block:: cpp

  // Inferior Vitis HLS code
  for (int i = 0; i < n; i++) {
    #pragma HLS pipeline II=1
    data_t d = mem[ random_addr[i] ];
    // ... process d
  }


Example 2: Sequential Read from ``async_mmap`` into an Array
------------------------------------------------------------

- Since the outbound ``read_addr`` channel and the inbound ``read_data`` channel are separate, we use two iterator variables ``i_req`` and ``i_resp`` to track the progress of each channel.

- When the number of responses ``i_resp`` match the target ``count``,
  the loop will terminate.

- In each loop iteration, we send a new read request if:

  - The number of read requests ``i_req`` is less than the total request
    ``count``.

  - The ``read_addr`` channel of the async_mmap ``mem`` is not full.

  - We increment ``i_req`` if we successfully issue a read request.

- In each loop iteration, we check if the ``read_data`` channel has data.

  - If so, we get the data from the ``read_data`` channel and stores into an array.

  - We increment ``i_resp`` when we receive a new read response.

- Note that issuing read addresses and receiving read responses must all be
  non-blocking so that they could function in parallel.

.. code-block:: cpp

  template <typename mmap_t, typename addr_t, typename data_t>
  void async_mmap_read_to_array(
      tapa::async_mmap<mmap_t>& mem,
      data_t* array,
      addr_t base_addr,
      unsigned int count,
      unsigned int stride) {
    for (int i_req = 0, i_resp = 0; i_resp < count;) {
      #pragma HLS pipeline II=1

      if (i_req < count &&
          mem.read_addr.try_write(base_addr + i_req * stride)) {
          ++i_req;
      }
      if (!mem.read_data.empty()) {
          array[i_resp] = mem.read_data.read(nullptr);
          ++i_resp;
      }
    }
  }


Example 3: Sequential Write into ``async_mmap`` from a FIFO
-----------------------------------------------------------

Compared to Example 2, this example is slightly more complicated because we are reading from a stream. Therefore, we need to additionally check if the stream/FIFO is empty before executing an operation.

Note that in this example, we don't actually need the data from the ``write_resp`` channel. Still, we need to dump the data from ``write_resp``, otherwise the FIFO will become full and block further write operations.

.. code-block:: cpp

  template <typename mmap_t, typename stream_t, typename addr_t, typename count_t, typename stride_t>
  void async_mmap_write_from_fifo(
      tapa::async_mmap<mmap_t>& mem,
      tapa::istream<stream_t>& fifo,
      addr_t base_addr,
      count_t count,
      stride_t stride) {
  #pragma HLS inline

    for(int i_req = 0, i_resp = 0; i_resp < count;) {
      #pragma HLS pipeline II=1

      // issue write requests
      if (i_req < count &&
          !fifo.empty() &&
          !mem.write_addr.full() &&
          !mem.write_data.full()) {
        mem.write_addr.try_write(base_addr + i_req * stride);
        mem.write_data.try_write(fifo.read(nullptr));
        ++i_req;
      }

      // receive acks of write success
      if (!mem.write_resp.empty()) {
        i_resp += unsigned(mem.write_resp.read(nullptr)) + 1;
      }
    }
  }


Example 4: Simultaneous Read and Write to ``async_mmap``
--------------------------------------------------------

This example reads from the external memory, increment the data by 1,
then write to the same device in a fully pipelined fashion.
This is also a pattern that can hardly be described when abstracting the memory
as an array.
A naive implementation like ``mem[i] = foo(mem[i])`` in Vitis HLS will result in
a low-performance implementation where there will only be one outstanding transaction (similar to the situation in Example 1).

.. code-block:: cpp

  void Copy(tapa::async_mmap<Elem>& mem, uint64_t n, uint64_t flags) {
    Elem elem;

    for (int64_t i_rd_req = 0, i_rd_resp = 0, i_wr_req = 0, i_wr_resp = 0;
         i_rd_resp < n || i_wr_resp < n;) {
      #pragma HLS pipeline II=1
      bool can_read = !mem.read_data.empty();
      bool can_write = !mem.write_addr.full() && !mem.write_data.full();

      int64_t read_addr = i_rd_req;
      int64_t write_addr = i_wr_req;

      if (i_rd_req < n && mem.read_addr.try_write(read_addr)) {
        ++i_rd_req;
      }

      if (can_read && can_write) {
        mem.read_data.try_read(elem);
        mem.write_addr.write(write_addr);
        mem.write_data.write(elem + 1);

        ++i_rd_resp;
        ++i_wr_req;
      }

      if (!mem.write_resp.empty()) {
        i_wr_resp += mem.write_resp.read(nullptr) + 1;
      }
    }
  }
