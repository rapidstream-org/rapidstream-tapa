Integration with Vitis
---------------------------


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
