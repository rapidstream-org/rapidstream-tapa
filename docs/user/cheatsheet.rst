Quick Reference
===============

.. note::

   This guide provides a quick reference for using RapidStream TAPA. If you
   are a beginner, start with the
   :ref:`Getting Started <user/getting_started:Getting Started>` guide.

Installation
------------

.. code-block:: bash

  # Install TAPA
  sh -c "$(curl -fsSL tapa.rapidstream.sh)"

  # Optional: Install RapidStream
  sh -c "$(curl -fsSL rapidstream.sh)"

Compilation
-----------

.. code-block:: bash

  # Host compilation
  tapa g++ kernel.cpp host.cpp -o program

  # HLS synthesis
  tapa compile \
    --top TopLevel \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f kernel.cpp \
    -o kernel.xo

Execution
---------

.. code-block:: bash

  # Software simulation
  ./program

  # Hardware simulation
  ./program --bitstream=kernel.xo

  # On-board execution
  ./program --bitstream=kernel.hw.xclbin

Debugging
---------

.. code-block:: bash

  # Log streams
  export TAPA_STREAM_LOG_DIR=/path/to/logs
  ./program

  # View HLS reports
  ls work.out/report/

TAPA Task Structure
-------------------

.. code-block:: cpp

  void TaskName(tapa::istream<T>& in,
                tapa::ostream<T>& out,
                tapa::mmap<T> mem,
                uint64_t scalar) {
    // Task logic
  }

Stream Operations
-----------------

.. code-block:: cpp

  // Read
  T data = stream.read();
  stream >> data;  // Equivalent

  // Write
  stream.write(data);
  stream << data;  // Equivalent

Upper-Level Task
----------------

.. code-block:: cpp

  void TopLevel(tapa::mmap<T> mem, uint64_t scalar) {
    tapa::stream<T> s("stream_name");

    tapa::task()
      .invoke(Task1, s, mem, scalar)
      .invoke(Task2, s, scalar);
  }

Host Invocation
---------------

.. code-block:: cpp

  tapa::invoke(TopLevel,
               bitstream_path,
               tapa::read_only_mmap<const T>(vec),
               tapa::write_only_mmap<T>(out),
               scalar);
