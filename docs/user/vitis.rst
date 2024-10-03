Hardware Implementation
=======================

.. note::

   TAPA compiles tasks into ``v++`` compatible XO files, which can be used to
   generate hardware bitstreams for Xilinx FPGAs.

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
