Hardware Implementation
=======================

.. note::

   TAPA compiles tasks into ``v++`` compatible XO files, which can be used to
   generate hardware bitstreams for Xilinx FPGAs. The same host executable can
   be used for software simulation, hardware simulation, and on-board
   execution.

Host-Kernel Interface
---------------------

TAPA significantly streamlines FPGA programming compared to Xilinx Vitis. While
Vitis offer high-level OpenCL-based host-kernel interfaces, they still require
extensive host API calls. For instance, Vitis Accel Examples'
`"Hello World" <https://github.com/Xilinx/Vitis_Accel_Examples/blob/21bb0cf788ace593c6075accff7f7783588ae8b4/hello_world/src/host.cpp#L58-L115>`_
program uses 57 lines of code just to invoke the accelerator. Additionally,
different simulation and execution environments demand separate setups.

In contrast, TAPA simplifies this process to a single ``tapa::invoke`` function
call, automatically handling environmental configurations. Developers only need
to modify the bitstream argument for different targets, greatly reducing
complexity and enhancing productivity in FPGA development:

.. code-block:: cpp

   tapa::invoke(
     vadd,                                     // Top-level kernel function.
     bitstream,                                // Path to bitstream. If empty, run software simulation.
     tapa::read_only_mmap<unsigned int>(in1),  // Kernel argument 0, read-only array.
     tapa::read_only_mmap<unsigned int>(in2),  // Kernel argument 1, read-only array.
     tapa::write_only_mmap<int>(out_r),        // Kernel argument 2, write-only array.
     size);                                    // Kernel argument 3, scalar.

This code is used for hardware execution when the second argument is the path
to the ``.xclbin`` bitstream file compiled with ``v++``.

Generate Bitstream
------------------

To generate the Xilinx hardware binary (xclbin) for on-board execution:

.. code-block:: bash

   v++ -o vadd.$platform.hw.xclbin \
     --link \
     --target hw \
     --kernel VecAdd \
     --platform $platform \
     vadd.$platform.hw.xo

.. note::

   Replace ``$platform`` with the target platform (e.g.,
   ``xilinx_u280_xdma_201920_3``).

This would take several hours and generate a binary for the specified platform
as ``vadd.$platform.hw.xclbin``. This is the binary that will be loaded onto
the FPGA for execution.

.. tip::

   You should refer to the Xilinx documentation for more information on
   the ``v++`` command.

Execute on an FPGA
------------------

The same host executable for software simulation and hardware simulation
(``vadd``) can be used to run the hardware accelerator on an FPGA:

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw.xclbin

The output messages are similar to those you'd see in hardware simulation.
However, on-board execution is significantly faster. It's worth noting that:

- We are running the full vectors, each containing :math:`2^{20}` elements.
- The elapsed time shown includes the time needed for FPGA reconfiguration.

.. code-block:: text

  INFO: Found platform: Xilinx
  INFO: Found device: xilinx_u280_xdma_201920_3
  INFO: Found device: xilinx_u250_xdma_201830_2
  INFO: Using xilinx_u250_xdma_201830_2
  DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 0
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
  DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 0
  DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 1
  [XRT] WARNING: unaligned host pointer '0x7f258d90a010' detected, this leads to extra memcpy
  DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 1
  DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::ReadOnlyBuffer<T>) [with T = float]' called with index = 2
  [XRT] WARNING: unaligned host pointer '0x7f258d509010' detected, this leads to extra memcpy
  DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::ReadOnlyBuffer<T>) [with T = float]' called with index = 2
  DEBUG: Function 'void fpga::Instance::SetArg(int, T&&) [with T = long unsigned int]' called with index = 3
  elapsed time: 7.48926 s
  PASS!
