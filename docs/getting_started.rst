
We will cover the basic usages of TAPA in this section.
If you are migrating from Vitis HLS,
:ref:`another tutorial <tutorial/migrate_from_vitis_hls:migrate from vitis hls>`
presents two nice examples.

Before you start, make sure you have
:ref:`installed TAPA <installation:Quick Installation>` properly.

Hello World: Vector Add
:::::::::::::::::::::::

Let's start with the following TAPA C++ source code:

.. literalinclude:: ../tests/apps/vadd/vadd.cpp
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



Run Software Simulation
:::::::::::::::::::::::

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

.. code-block:: bash

  git clone https://github.com/UCLA-VAST/tapa
  cd tapa/apps/vadd
  g++ -o vadd -O2 vadd.cpp vadd-host.cpp -ltapa -lfrt -lglog -lgflags -lOpenCL
  ./vadd

You should be able to see something like:

.. code-block:: text

  elapsed time: 0.334634 s
  PASS!

By default, the host program will add two :math:`2^{20}`-element vectors.
This can be changed by supplying an argument to the command line:

.. code-block:: bash

  ./vadd 1000

This time the elapsed time should be much shorter:

.. code-block:: text

  elapsed time: 0.0126179 s
  PASS!

The above runs software simulation of the program,
which helps you quickly verify the correctness.



Synthesize into RTL
:::::::::::::::::::::::


The next step is to synthesize it.
The first step would be to run high-level synthesize (HLS):

.. code-block:: bash

  platform=xilinx_u250_xdma_201830_2  # replace with your target platform
  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa

If you do not intend to use the Vitis backend, you could use ``--part-num``
instead. In this case, you must provide the target clock period as well.

.. code-block:: bash

  partnum=xcu250-figd2104-2L-e  # replace with your target partnum
  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --part-num $partnum \
    --clock-period 3.33 \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa

This will take a couple of minutes.
HLS reports will be available in the working directory
``vadd.$platform.hw.xo.tapa/report``.


Run Hardware Simulation with Vitis
::::::::::::::::::::::::::::::::::::

To generate bitstream for hardware simulation:

.. code-block:: bash

  v++ -o vadd.$platform.hw_emu.xclbin \
    --link \
    --target hw_emu \
    --kernel VecAdd \
    --platform $platform \
    vadd.$platform.hw.xo

This would take 5--10 minutes.

TAPA will automatically generate a script called
``vadd.$platform.hw_generate_bitstream.sh`` that includes the command to invoke
v++.


To run hardware simulation with the generated ``hw_emu`` bitstream:

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw_emu.xclbin 1000

It will take about half a minute until it says

.. code-block:: text

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

  .. code-block:: text

    [XRT] WARNING: unaligned host pointer '0x55ca5dc41af0' detected, this leads to extra memcpy


  You can use ``std::vector<T, tapa::aligned_allocator<T>>`` instead of
  ``std::vector`` to allocate memory with aligned addresses
  and get rid of this extra copy.


Run Hardware Simulation with TAPA Simulator
:::::::::::::::::::::::::::::::::::::::::::::

Simply replace the ``vadd.$platform.hw_emu.xclbin`` by the previously generated
``xo`` object. See this :ref:`tutorial <tutorial/fast_cosim:TAPA RTL
Simulation>` for more details.

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw.xo 1000


Generate Bitstream
::::::::::::::::::::::::::::::::::::

When we synthesize the C++ program into RTL, TAPA will also generate a script
named ``${top_name}_generate_bitstream.sh`` along with the ``.xo`` file. To
generate bitstream, simply run this script. This would take several hours.

.. code-block:: bash

  bash VecAdd_generate_bitstream.sh

What the script does is to invoke the Vitis ``v++`` compiler that takes in the
``.xo`` object and generate the hardware bitstream.

Execute on an FPGA
:::::::::::::::::::::::::

To run the hardware accelerator with the generated ``hw`` bitstream:

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw.xclbin

The output messages are similar to hardware simulation but on-board execution
runs much faster (note that we are running the full :math:`2^{20}`-element
vectors and the elapsed time includes the FPGA reconfiguration time,
if necessary).

.. code-block:: text

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
