.. _introduction-to-async-mmap:
Async Memory-Mapped I/O
=======================

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

- Since the outbound ``read_addr`` channel and the inbound ``read_data``
  channel are separate, we use two iterator variables ``i_req`` and ``i_resp``
  to track the progress of each channel.

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

Compared to Example 2, this example is slightly more complicated because we are
reading from a stream. Therefore, we need to additionally check if the
stream/FIFO is empty before executing an operation.

Note that in this example, we don't actually need the data from the
``write_resp`` channel. Still, we need to dump the data from ``write_resp``,
otherwise the FIFO will become full and block further write operations.

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
a low-performance implementation where there will only be one outstanding
transaction (similar to the situation in Example 1).

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
