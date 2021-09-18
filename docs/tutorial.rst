Tutorial
========

This tutorial covers the basics for using TAPA to design FPGA accelerators
effectively and efficiently.

Getting Started
---------------

We will cover the basic usages of TAPA in this section.
Before you start, make sure you have
:ref:`installed TAPA <installation:installation>` properly.

Hello World: Vector Add
:::::::::::::::::::::::

This section covers the basic steps to design and run an FPGA accelerator with
TAPA.

Let's start with the following TAPA C++ source code:

.. literalinclude:: ../apps/vadd/vadd.cpp
   :language: cpp

The above code adds two variable-length ``float`` vectors, ``a`` and ``b``,
to produce a new vector ``c`` with the same length.
This is done with four C++ functions:
``Add``, ``Mmap2Stream``, ``Stream2Mmap``, and ``VecAdd``.
The last function *invokes* the first three.
Let's discuss them one-by-one.

The ``Add`` function performs the element-wide addition operations.
It takes 4 arguments:
``tapa::istream<float>& a`` and ``tapa::istream<float>& b`` are
:ref:`input streams <api:istream>`.
``tapa::ostream<float>& c`` is an :ref:`output stream <api:ostream>`.
``uint64_t n`` is a scalar input for the length of the vectors.
This function reads each of ``a`` and ``b`` for ``n`` times, adds them together,
and writes the sum to ``c``.

The ``Mmap2Stream`` function reads an input vector from DRAM and writes it to
a stream.
It takes 3 arguments:
``tapa::mmap<const float> mmap`` is a :ref:`memory-mapped <api:mmap>` interface.
On FPGA, this is usually the on-board DRAM.
``uint64_t n`` is a scalar input for the length of the vectors.
``tapa::ostream<stream>& c`` is an :ref:`output stream <api:ostream>`.

The ``Stream2Mmap`` function is the reverse of ``Mmap2Stream``.

The above three functions each defines a task, which can run *in parallel*.
Tasks that do not instantiation other tasks are called *lower-level* tasks.

The ``VecAdd`` function instantiates the three lower-level tasks and defines
the communication channels between them.
It takes 3 arguments:
3 mmap interfaces for the 3 vectors and one scalar for the vector length.
3 :ref:`communication channels <api:stream>` are defined in ``VecAdd``.
The 3 lower-level tasks are instantiated 4 times in total because there are
2 input vectors, each of which needs its own ``Mmap2Stream``.
The ``VecAdd`` function is an *upper-level* task.
It is also the *top-level* task that defines the interface between the kernel
and the host.
Once the 4 children task instances are instantiated,
they will run in parallel and their parent will wait until all children finish.

Next, let's actually run the program.
We will need a host program to driver the kernel.

.. code-block:: cpp

  void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
              tapa::mmap<float> c_array, uint64_t n);

  int main()
  ...
  vector<float> a(n);
  vector<float> b(n);
  vector<float> c(n);
  ...
  tapa::invoke(VecAdd, FLAGS_bitstream, tapa::read_only_mmap<const float>(a),
               tapa::read_only_mmap<const float>(b),
               tapa::write_only_mmap<float>(c), n);
  ...

The top-level kernel function is called using ``tapa::invoke`` so that
software simulation, hardware simulation, and on-board execution can share the
same piece of code.
``tapa::invoke`` takes a variable number of arguments.
The first argument is the top-level kernel function,
which needs to be declared ahead of time.
The second argument is the path to the desired bitstream.
If this argument is an empty string, software simulation will run.
The rest of the arguments are passed to the top-level kernel function.
In our vector add example,
we allocate 3 ``vector<float>`` for ``a``, ``b``, and ``c``, respectively.
``a`` and ``b`` are passed to ``tapa::invoke`` as ``tapa::read_only_mmap``
because the kernel reads but not writes them.
Similarly, ``c`` is passed as ``tapa::write_only_mmap`` because the kernel
writes but not reads them.
Scalar values like the vector length ``n`` are passed to the kernel directly.

.. note::

  ``tapa::read_only_mmap`` and ``tapa::write_only_mmap`` only specify
  the host-kernel communication behavior.
  They do not control nor check whether the kernel really reads or writes
  a memory-mapped argument.
  Under the hood, ``tapa::read_only_mmap`` just tells the host program that
  this memory space should be transferred to the FPGA before launching the
  kernel, and it does not need to be transferred back to the host when the
  kernel finishes.
  ``tapa::write_only_mmap`` just tells the host program that this memory space
  do not need to be transferred to the FPGA before launching, but it should be
  transferred back to the host when the kernel finishes.
  Two other possible alternatives are ``tapa::read_write_mmap`` and
  ``tapa::placeholder_mmap``.

.. note::

  Scalar values are always read-only to the kernel.

The vector add example is one of the application examples shipped with TAPA.
Note that the apps additionally requires the
`gflags <https://github.com/gflags/gflags>`_ library
(``sudo apt install libgflags-dev``).

.. code-block:: bash

  git clone https://github.com/UCLA-VAST/tapa
  cd tapa/apps/vadd
  g++ -o vadd -O2 vadd.cpp vadd-host.cpp -ltapa -lfrt -lglog -lgflags -lOpenCL
  ./vadd

You should be able to see something like:

.. code-block:: none

  elapsed time: 0.334634 s
  PASS!

By default, the host program will add two :math:`2^{20}`-element vectors.
This can be changed by supplying an argument to the command line:

.. code-block:: bash

  ./vadd 1000

This time the elapsed time should be much shorter:

.. code-block:: none

  elapsed time: 0.0126179 s
  PASS!

The above runs software simulation of the program,
which helps you quickly verify the correctness.
The next step is to synthesize it.
The first step would be to run high-level synthesize (HLS):

.. code-block:: bash

  platform=xilinx_u250_xdma_201830_2  # replace with your target platform
  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa

This will take a couple of minutes.
HLS reports will be available in the working directory
``vadd.$platform.hw.xo.tapa/report``.

To generate bitstream for hardware simulation:

.. code-block:: bash

  v++ -o vadd.$platform.hw_emu.xclbin \
    --link \
    --target hw_emu \
    --kernel VecAdd \
    --platform $platform \
    vadd.$platform.hw.xo

This would take 5--10 minutes.

To run hardware simulation with the generated ``hw_emu`` bitstream:

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw_emu.xclbin 1000

It will take about half a minute until it says

.. code-block:: none

  INFO: Loading vadd.xilinx_u250_xdma_201830_2.hw_emu.xclbin
  INFO: Found platform: Xilinx
  INFO: Found device: xilinx_u250_xdma_201830_2
  INFO: Using xilinx_u250_xdma_201830_2
  INFO: [HW-EMU 01] Hardware emulation runs simulation underneath. Using a large data set will result in long simulation times. It is recommended that a small dataset is used for faster execution. The flow uses approximate models for DDR memory and interconnect and hence the performance data generated is approximate.
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 0
  XRT build version: 2.8.743
  Build hash: 77d5484b5c4daa691a7f78235053fb036829b1e9
  Build date: 2020-11-16 00:19:11
  Git branch: 2020.2
  PID: 233333
  UID: 1000
  [Fri Sep 17 04:38:27 2021 GMT]
  HOST: foo
  EXE: /path/to/tapa/apps/vadd/vadd
  [XRT] WARNING: unaligned host pointer '0x55ca5dc41af0' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 0
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 1
  [XRT] WARNING: unaligned host pointer '0x55ca5dc42aa0' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 1
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::ReadOnlyBuffer<T>) [with T = float]’ called with index = 2
  [XRT] WARNING: unaligned host pointer '0x55ca5dc43a50' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::ReadOnlyBuffer<T>) [with T = float]’ called with index = 2
  DEBUG: Function ‘void fpga::Instance::SetArg(int, T&&) [with T = long unsigned int]’ called with index = 3
  INFO: [HW-EMU 06-0] Waiting for the simulator process to exit
  INFO: [HW-EMU 06-1] All the simulator processes exited successfully
  elapsed time: 31.0901 s
  PASS!

.. tip::

  We are using ``std::vector`` in the above example so the memory-mapped
  variables are not aligned to page boundaries.
  As a result, host-kernel communication requires an extra memory copy
  and XRT issues this warning:

  .. code-block:: none

    [XRT] WARNING: unaligned host pointer '0x55ca5dc41af0' detected, this leads to extra memcpy


  You can use ``std::vector<T, tapa::aligned_allocator<T>>`` instead of
  ``std::vector`` to allocate memory with aligned addresses
  and get rid of this extra copy.

To generate bitstream for on-board execution:

.. code-block:: bash

  v++ -o vadd.$platform.hw.xclbin \
    --link \
    --target hw \
    --kernel VecAdd \
    --platform $platform \
    vadd.$platform.hw.xo

This would take several hours.

To run hardware simulation with the generated ``hw`` bitstream:

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw.xclbin

The output messages are similar to hardware simulation but on-board execution
runs much faster (note that we are running the full :math:`2^{20}`-element
vectors and the elapsed time includes the FPGA reconfiguration time,
if necessary).

.. code-block:: none

  INFO: Found platform: Xilinx
  INFO: Found device: xilinx_u280_xdma_201920_3
  INFO: Found device: xilinx_u250_xdma_201830_2
  INFO: Using xilinx_u250_xdma_201830_2
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 0
  XRT build version: 2.9.317
  Build hash: b0230e59e22351fb957dc46a6e68d7560e5f630c
  Build date: 2021-03-13 05:10:45
  Git branch: 2020.2_PU1
  PID: 23333
  UID: 1000
  [Fri Sep 17 20:01:23 2021 GMT]
  HOST: foo
  EXE: /path/to/tapa/apps/vadd/vadd
  [XRT] WARNING: unaligned host pointer '0x7f258dd0b010' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 0
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 1
  [XRT] WARNING: unaligned host pointer '0x7f258d90a010' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]’ called with index = 1
  DEBUG: Function ‘void fpga::Instance::AllocBuf(int, fpga::ReadOnlyBuffer<T>) [with T = float]’ called with index = 2
  [XRT] WARNING: unaligned host pointer '0x7f258d509010' detected, this leads to extra memcpy
  DEBUG: Function ‘void fpga::Instance::SetArg(int, fpga::ReadOnlyBuffer<T>) [with T = float]’ called with index = 2
  DEBUG: Function ‘void fpga::Instance::SetArg(int, T&&) [with T = long unsigned int]’ called with index = 3
  elapsed time: 7.48926 s
  PASS!

.. note::

  We are using the same host executable for software simulation,
  hardware simulation, and on-board execution.

Using CMake
:::::::::::

This section covers the steps to use CMake to simplify the synthesis and
implementation process.

The commands to run HLS and FPGA implementation are too long to type manually,
aren't they?
You could write your own shell scripts to reuse the commands.
Better, you can leverage CMake to achieve incremental compilation.

.. note::

  GNU or BSD Make can do incremental compilation as well, but it gets really
  messy to define generic build rules and reuse ``Makefile`` itself.

TAPA provides some convenient CMake functions and uses them in the
``CMakeLists.txt`` files for the application examples.
Let's redo the vector add example with CMake.

The first step to use CMake is to install it:

.. code-block:: shell

  pip install cmake --upgrade

.. tip::

  See :ref:`troubleshooting <installation:cmake returns an error>` for a common
  error related to CMake.

CMake separates generated files from the source code.
We should create a ``build`` directory for the generated files
and run ``cmake`` in it:

.. code-block:: shell

  mkdir build
  cd build
  cmake \
    /path/to/tapa/apps/vadd \             # can be relative or absolute path
    -DPLATFORM=xilinx_u250_xdma_201830_2  # replace with your target platform

To generate the host executable and run software simulation:

.. code-block:: shell

  make vadd -j8   # replace with your core count
  ./vadd

To generate the XO file via HLS:

.. code-block:: shell

  make vadd-hw-xo

To generate the hardware emulation bitstream and run hardware emulation:

.. code-block:: shell

  make vadd-cosim

To generate the on-board bitstream and run on-board execution:

.. code-block:: shell

  make vadd-hw

Peeking a Stream
::::::::::::::::

This section covers the usage of non-destructive read (a.k.a. peek) on a stream.

TAPA provides the functionality to read a token from a stream without actually
removing it from the stream.
This can be helpful when the computation operations depend on the content of the
input token, for example, in a switch network.

The network example shipped with TAPA uses the peeking API.
That example implements 3-stage 8×8
`Omega network <https://www.mathcs.emory.edu/~cheung/Courses/355/Syllabus/90-parallel/Omega.html>`_.
The basic component of such a multi-stage switch network is a 2×2 switch box,
which routes an input packet based on one bit in the destination address
(which is part of the packet).
For example, if a packet from ``pkt_in_q0`` has destination ``2 = 0b010``,
and we are interested in bit 1 (0-based),
this packet should be written to ``pkt_out_q[1]``.
If another packet from ``pkt_in_q1`` has destination ``7 = 0b111``,
we will have to choose only one of them to write to ``pkt_out_q[1]`` because
only one token can be written in each clock cycle.
This means we need to make the decision before we remove the token from the
input channel (stream).
In the following code, we first
:ref:`peek <classtapa_1_1istream_1a6df8ab2e1caaaf2e32844b7cc716cf11>`
the input stream.
Then, we use the peeked destinations to determine which input(s) can actually be
consumed.
Finally, we schedule the read operations based on these decisions.

.. code-block:: cpp

  void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
                 ostreams<pkt_t, 2>& pkt_out_q) {
  ...
  for (bool is_valid_0, is_valid_1;;) {
    const auto pkt_0 = pkt_in_q0.peek(valid_0);
    const auto pkt_1 = pkt_in_q1.peek(valid_1);
    ... // decide which input(s) can be consumed
    if (...) pkt_in_q0.read(nullptr);
    if (...) pkt_in_q1.read(nullptr);
  }

End-of-Transaction
::::::::::::::::::

This section covers the usage of end-of-transaction tokens.

`SODA <https://github.com/UCLA-VAST/soda>`_ is a highly parallel
microarchitecture for stencil applications.
It is implemented using the
`dataflow optimization <https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/vitis_hls_optimization_techniques.html#bmx1539734225930>`_.
However, this requires that each kernel is terminated properly.
This can be done by broadcasting the loop trip-count to each kernel function,
but doing so requires an adder in each function.
Since each kernel module can be very small,
the adder can consume a lot of resources.

TAPA provides a less expensive solution is to this problem.
With TAPA, a kernel can send a special token to denote the end of transaction
(``EoT``).
For example, in the jacobi stencil example shipped with TAPA,
the producer, ``Mmap2Stream``, sends an ``EoT`` token by
:ref:`closing <classtapa_1_1ostream_1a10405849fa9a12a02e2fc0d33b305d22>` the
stream.

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

A downstream module,
``Stream2Mmap`` in the example, can then detect the program termination by
:ref:`testing for the EoT token <classtapa_1_1istream_1aa776b6b4c8cfd5a287c6699077152dcd>`.

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

Detached Task
:::::::::::::

This section covers the usage of detached tasks.

Sometimes terminating each kernel function is an overkill.
For example, a task function may be purely data-driven and we don't have to
terminate it on program termination.
In that case,
TAPA allows you to *detach* a task on invocation instead of joining it to
the parent.
This resembles ``std::thread::detach``
in the `C++ STL <https://en.cppreference.com/w/cpp/thread/thread/detach>`_.
The network example shipped with TAPA leverages this feature to simplify design;
the 2×2 switch boxes are instantiated and detached.

.. code-block:: cpp

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

Hierarchical Design
:::::::::::::::::::

This section covers the usage of hierarchical design.

TAPA tasks are recursively defined.
The children tasks instantiated by an upper-level task can itself be a parent of
its children.
The network example shipped with TAPA leverages this feature to simplify design;
the 2×2 switch boxes are instantiated in an inner wrapper stage;
each inner stage is then wrapped in a stage.
The top-level task only instantiates a data producer, a data consumer,
and 3 wrapper stages.

.. code-block:: cpp

  void Network(mmap<vec_t<pkt_t, kN>> mmap_in, mmap<vec_t<pkt_t, kN>> mmap_out,
               uint64_t n) {
    streams<pkt_t, kN, 4096> q0("q0");
    streams<pkt_t, kN, 4096> q1("q1");
    streams<pkt_t, kN, 4096> q2("q2");
    streams<pkt_t, kN, 4096> q3("q3");

    task()
        .invoke(Produce, mmap_in, n, q0)
        .invoke(Stage, 2, q0, q1)
        .invoke(Stage, 1, q1, q2)
        .invoke(Stage, 0, q2, q3)
        .invoke(Consume, mmap_out, n, q3);
  }

Advanced Features
-----------------

Stream/MMAP Array
:::::::::::::::::

This section covers the usage of stream/mmap arrays
(:ref:`api:streams`/:ref:`api:mmaps`).

Often times a singleton ``stream`` or ``mmap`` is insufficient for
parameterized designs.
For example, the network example shipped with TAPA defines a 8×8 switch network.
What if we want to use a 16×16 network? Or 4×4?
Apparently we would like the network size parameterized so that such a
design-space exploration does not take too much manual effort.
This is possible in TAPA through arrays of stream/mmap and batch invocation.

The concepts of arrays of ``tapa::(i/o)stream`` and ``tapa::mmap`` and
batch invocation explain themselves.
The relevant APIs are well documented in the
:ref:`API references <api:the tapa library (libtapa)>`.
How task invocation interacts with the arrays is worth explaining.
Using the network example,
the ``InnerStage`` function instantiates the 2×2 switch boxes ``kN / 2`` times.
Instead of writing ``kN / 2`` ``invoke`` functions,
this can be done using a single ``invoke``.
It will effectively instantiate the same ``Switch2x2`` task ``kN / 2`` times.
Every time a task is instantiated,
an (actual) argument ``streams`` or ``mmaps`` array will be accessed sequentially.
If the (formal) parameter is a singleton ``istream``, ``ostream``, ``mmap``,
``async_mmap``,
only one element in the array will be accessed.
If the (formal) parameter is an array of ``istreams``, ``ostreams``,
or ``mmaps``,
the number of element accessed is determined by
the array length of the (formal) parameter.
Once accessed,
the accessed element will be marked "accessed" and the next time the following
element will be accessed.

Let's go back to the example.
``InnerStage`` instantiates ``Switch2x2`` ``kN / 2`` times.
The first argument is the scalar input ``b``,
which is broadcast to each instance of ``Switch2x2``.
The second argument is an ``istreams<pkt_t, kN / 2>`` array.
The ``kN / 2`` ``Switch2x2`` instances each takes one ``istream<pkt_t>``.
The third argument is accessed similarly.
The fourth argument is an ``ostreams<pkt_t, kN>`` array.
The ``kN / 2`` ``Switch2x2`` instances each takes one ``ostreams<pkt_t, 2>``,
which is effectively two ``ostream<pkt_t>``.

The same argument can be passed to different parameters in the same invocation.
In such a case, the array elements are accessed sequentially
first from left to right in the argument list then repeated many times
if it is a batch invocation.
``Stage`` leverages this feature in the network example.
The first apparence of ``in_q`` accesses the first ``kN / 2`` elements,
and the second accesses the second half.

.. code-block:: cpp

  void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
                 ostreams<pkt_t, 2>& pkt_out_q) {
  }

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

  void Stage(int b, istreams<pkt_t, kN>& in_q, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach>(InnerStage, b, in_q, in_q, out_q);
  }

Asynchronous MMAP Interface
:::::::::::::::::::::::::::

This section covers the usage of asynchronous memory-mapped interfaces.

Vitis HLS assumes a fixed latency for the regular ``tapa::mmap`` interfaces,
which are implemented using AXI memory-mapped interfaces (``m_axi``) in RTL.
If the memory request did not arrive in time,
the kernel will stall.
TAPA provides an alternative by separating the request and response of the
memory-mapped interfaces and making the requests asynchronous.
This is done by exposing 5 stream interfaces,
each of which corresponds to one of the data channels in ``m_axi``.
More details are documented in the :ref:`API references <api:async_mmap>`.

Sharing MMAP Interface
::::::::::::::::::::::

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
