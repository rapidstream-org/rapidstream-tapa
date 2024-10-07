Efficient Memory Accesses
=========================

.. note::

   In this tutorial, we explore how to implement efficient memory accesses
   leveraging TAPA's asynchronous memory-mapped interface.

Example 1: Multi-Outstanding Random Memory Accesses
---------------------------------------------------

In this example, the key to optimizing performance is allowing multiple
outstanding memory operations. Our goal is to perform random memory read
operations more efficiently by issuing multiple read requests without waiting
for each response.

The Problem with Traditional Approaches
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In traditional HLS designs, like those using Vitis HLS, random memory
accesses often look like this:

.. code-block:: cpp

  // Inferior Vitis HLS code
  for (int i = 0; i < n; i++) {
    #pragma HLS pipeline II=1
    data_t d = mem[random_addr[i]];
    // ... process d
  }

This approach has a significant drawback: it issues one read request, then
waits for its response before issuing another. This results in only one
outstanding transaction at a time, severely limiting performance.

TAPA Parallel Requests
^^^^^^^^^^^^^^^^^^^^^^

TAPA allows us to separate the process of issuing read requests and receiving
responses, enabling multiple outstanding transactions. Here's how we implement
this:

Issue Read Addresses
~~~~~~~~~~~~~~~~~~~~

We create a task to continuously issue read requests. The ``issue_read_addr``
function continually issues read requests as long as the AXI interface is
ready to accept them.

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

Receive Read Responses
~~~~~~~~~~~~~~~~~~~~~~

We create another task to handle incoming read responses. The
``receive_read_resp`` function processes incoming read responses
independently.

.. code-block:: cpp

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

Top-Level Function
~~~~~~~~~~~~~~~~~~

Finally, we use TAPA's task system to invoke both functions concurrently.
By separating these operations, we allow multiple outstanding read
requests, significantly improving performance for random memory accesses.

.. code-block:: cpp

  void top(tapa::mmap<data_t> mem, int n) {

    tapa::task()
      // ...
      .invoke(issue_read_addr, mem, n)
      .invoke(receive_read_resp, mem, n)
      // ...
      ;
  }

.. note::

   This TAPA-based approach enables efficient random memory accesses by
   allowing multiple outstanding transactions. It overcomes the limitations
   of traditional HLS designs, providing a more flexible and performant
   solution for handling random memory operations.

Example 2: Efficient Sequential Reading
---------------------------------------

In this example, we demonstrate how to efficiently read data sequentially
from an ``async_mmap`` into an array using TAPA. This approach allows for
overlapping read requests and responses, maximizing memory bandwidth
utilization.

Implementation
^^^^^^^^^^^^^^

We implement a function that reads data from an ``async_mmap`` into an
array, managing both read requests and responses concurrently. Here's
the implementation:

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

1. **Dual Iterators**:

  - ``i_req``: Tracks the number of read requests issued.
  - ``i_resp``: Tracks the number of responses received.

2. **Loop Condition**:

  - The loop continues until all responses are received (``i_resp < count``).

3. **Issuing Read Requests**:

  - Checks if more requests need to be sent (``i_req < count``).
  - Ensures the read_addr channel isn't full.
  - Calculates the address based on base_addr, current request number, and stride.
  - Increments ``i_req`` upon successful request issuance.

4. **Receiving Read Responses**:

  - Checks if the read_data channel has data.
  - Reads the data into the array at the correct position.
  - Increments ``i_resp`` for each response received.

Key Points
^^^^^^^^^^

- **Non-Blocking Operations**: Both issuing requests and receiving responses
  are non-blocking, allowing them to operate in parallel.
- **Pipelining**: The ``#pragma HLS pipeline II=1`` directive ensures that the
  loop is fully pipelined, attempting to initiate a new iteration every clock
  cycle.
- **Flexible Addressing**: The ``stride`` parameter allows for various access
  patterns (e.g., accessing every other element).

Usage Example
^^^^^^^^^^^^^

Here's how you can use the ``async_mmap_read_to_array`` function in a TAPA
task ``read_data``:

.. code-block:: cpp

  void read_data(tapa::mmap<int> mem) {
    int array[N];
    async_mmap_read_to_array(mem, array, 0x1000, 1000, 1);
  }

This would read 1000 integers from memory starting at address 0x1000, with
a stride of 1 (consecutive elements).

.. tip::

   Despite that TAPA does not support template functions as tasks, you can
   call a template function from a non-template task function.

.. note::

   This TAPA-based approach for sequential reading from ``async_mmap`` into an
   array provides an efficient method to overlap memory requests and responses.
   By managing read requests and responses separately, we can achieve higher
   throughput compared to traditional sequential reading methods.

Example 3: Efficient Sequential Writing
---------------------------------------

This example builds on the concepts from Example 2, focusing on writing data
from a FIFO into an ``async_mmap``. Our goal is to efficiently write data from
a FIFO stream into memory, managing write requests and responses concurrently:

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

The function manages both write requests and responses concurrently. It
ensures the FIFO is not empty and the write channels are not full before
issuing a write. Write responses are read and processed, even though the
data isn't used, to prevent blocking.

.. note::

   This example demonstrates how to efficiently write data from a FIFO to
   memory using TAPA's ``async_mmap``, building on the concepts from the
   previous example while addressing the specific requirements of write
   operations and FIFO input.

Example 4: Pipelined Read-Modify-Write Operations
-------------------------------------------------

In this example, we explore how to perform simultaneous read and write
operations on an ``async_mmap`` using TAPA. This example showcases a common
pattern of reading data, modifying it, and writing it back in a fully
pipelined fashion.

Our goal is to read data from external memory, increment it by 1, and write
it back to the same device, all while maintaining a high-performance,
pipelined implementation.

The Challenge with Traditional HLS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In traditional HLS approaches, a simple implementation like
``mem[i] = foo(mem[i])`` often results in poor performance due to the
sequential nature of read-modify-write operations. This leads to only one
outstanding transaction at a time, and the write operation is blocked until
the read operation and computation are complete.

TAPA Pipelined Operations
^^^^^^^^^^^^^^^^^^^^^^^^^

Here's the implementation that allows for simultaneous read and write
operations:

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
        mem.write_data.write(elem + 1);  // compute

        ++i_rd_resp;
        ++i_wr_req;
      }

      if (!mem.write_resp.empty()) {
        i_wr_resp += mem.write_resp.read(nullptr) + 1;
      }
    }
  }

1. **Request Trackers**:

   - ``i_rd_req``: Tracks read requests issued.
   - ``i_rd_resp``: Tracks read responses received.
   - ``i_wr_req``: Tracks write requests issued.
   - ``i_wr_resp``: Tracks write responses received.

2. **Pipelined Loop**:

   - Continues until all read responses and write responses are processed.
   - The ``#pragma HLS pipeline II=1`` directive ensures full pipelining.

Key Points of the Pipeline
^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Decoupled Operations**: Read requests and write operations are decoupled,
  allowing for overlapping execution.
- **Non-Blocking Checks**: All channel operations use non-blocking checks
  to prevent stalls.

.. note::

   This TAPA-based approach for simultaneous read and write operations
   demonstrates how to achieve high-performance, pipelined execution for
   read-modify-write patterns. By decoupling the different stages of the
   process and using ``async_mmap``'s non-blocking interfaces, we can
   significantly improve throughput compared to traditional sequential
   implementations.
