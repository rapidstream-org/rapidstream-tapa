RTL Simulation
==============

.. note::

  TAPA offers two approaches for RTL simulation: a fast lightweight simulation
  and the full Vitis cosimulation. This section covers both methods and
  provides guidance on when to use each.

Fast Lightweight Simulation
---------------------------

TAPA-generated xo can be used for cosim with Vitis, but this process is slow
due to complex simulation models for external IPs. Even for a basic vector-add
application, setup takes over 10 minutes, while actual simulation only takes
seconds.

TAPA's lightweight simulation methodology addresses the long setup time of
Vitis cosimulation by using simplified simulation models. This approach allows
for rapid iteration and debugging of basic functionality:

- Quick setup time (a few seconds).
- Uses basic objects to mimic external components (e.g., plain buffer with
  AXI interface for DRAM).
- Shares the same interfaces as Vitis counterparts.
- Ideal for catching logic errors in user code.

.. image:: https://user-images.githubusercontent.com/32432619/164995378-a5d1ea4b-a673-42ef-9f9d-4e0dcc9ce527.png
  :width: 100 %

.. note::

   While less accurate internally, after fixing basic functional bugs with
   TAPA's fast cosim, users can run Vitis cosim for a more realistic
   simulation if needed.

Basic Usage
^^^^^^^^^^^

To run the fast simulation, pass the path to the generated xo file as the
``--bitstream`` argument:

.. code-block:: bash

   ./vadd --bitstream VecAdd.xo 1000

Viewing Waveforms
^^^^^^^^^^^^^^^^^

Two options are available for waveform analysis:

- ``-xosim_work_dir <dir>``: Saves intermediate data and files.
- ``-xosim_save_waveform``: Saves waveform to a .wdb file in the work
  directory. You must also specify ``-xosim_work_dir`` to use this option.

Debugging Frozen Simulations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the simulation becomes unresponsive:

1. Use ``-xosim_work_dir`` to save intermediate files
2. Abort the simulation with Ctrl-C
3. Locate ``[work-dir]/output/run/run_cosim.tcl``
4. Run in Vivado GUI: ``vivado -mode gui -source run_cosim.tcl``

This allows real-time observation and waveform analysis.

.. warning::

   Cross-channel access for HBM is not currently supported in the fast
   cosimulation. Each AXI interface can only access one HBM channel.

Vitis Hardware Simulation
-------------------------

For more accurate simulations that closely model real hardware behavior,
Vitis cosimulation can be used.

Generating Bitstream
^^^^^^^^^^^^^^^^^^^^

To generate the Xilinx emulation binary (xclbin) for hardware simulation:

.. code-block:: bash

   v++ -o vadd.$platform.hw_emu.xclbin \
     --link \
     --target hw_emu \
     --kernel VecAdd \
     --platform $platform \
     vadd.$platform.hw.xo

.. note::

   Replace ``$platform`` with the target platform (e.g.,
   ``xilinx_u280_xdma_201920_3``).

This process typically takes 5-10 minutes and generates a binary for the
specified platform as ``vadd.$platform.hw_emu.xclbin``.

Running Hardware Simulation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

To run the hardware simulation with the generated binary:

.. code-block:: bash

   ./vadd --bitstream=vadd.$platform.hw_emu.xclbin 1000

The output will be similar to the following:

.. code-block::

   INFO: Loading vadd.xilinx_u250_xdma_201830_2.hw_emu.xclbin
   INFO: Found platform: Xilinx
   INFO: Found device: xilinx_u250_xdma_201830_2
   INFO: Using xilinx_u250_xdma_201830_2
   INFO: [HW-EMU 01] Hardware emulation runs simulation underneath. Using a large data set will result in long simulation times. It is recommended that a small dataset is used for faster execution. The flow uses approximate models for DDR memory and interconnect and hence the performance data generated is approximate.
   DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 0
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
   DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 0
   DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 1
   [XRT] WARNING: unaligned host pointer '0x55ca5dc42aa0' detected, this leads to extra memcpy
   DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::WriteOnlyBuffer<T>) [with T = const float]' called with index = 1
   DEBUG: Function 'void fpga::Instance::AllocBuf(int, fpga::ReadOnlyBuffer<T>) [with T = float]' called with index = 2
   [XRT] WARNING: unaligned host pointer '0x55ca5dc43a50' detected, this leads to extra memcpy
   DEBUG: Function 'void fpga::Instance::SetArg(int, fpga::ReadOnlyBuffer<T>) [with T = float]' called with index = 2
   DEBUG: Function 'void fpga::Instance::SetArg(int, T&&) [with T = long unsigned int]' called with index = 3
   INFO: [HW-EMU 06-0] Waiting for the simulator process to exit
   INFO: [HW-EMU 06-1] All the simulator processes exited successfully
   elapsed time: 31.0901 s
   PASS!

.. tip::

   In the example above, we use ``std::vector`` for memory-mapped variables.
   However, this approach doesn't align the variables to page boundaries,
   which leads to two problems:

   1. An extra memory copy is required for host-kernel communication.
   2. XRT (Xilinx Runtime) issues a warning message:

   .. code-block:: text

      [XRT] WARNING: unaligned host pointer '0x55ca5dc41af0' detected, this leads to extra memcpy

   To resolve these issues and eliminate the extra copy, you can use a
   specialized vector with aligned memory allocation:

   .. code-block:: cpp

      std::vector<T, tapa::aligned_allocator<T>>

.. tip::

   You may add option parsing code to your host program's main function to
   allow users to specify the bitstream file at runtime:

   .. code-block:: c++

      #include <gflags/gflags.h>
      DEFINE_string(bitstream, "", "path to bitstream file");

      int main(int argc, char* argv[]) {
        gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);
        // ...
        tapa::invoke(/*...*/, FLAGS_bitstream, /*...*/);

Choosing the Right Approach
---------------------------

- Use TAPA's fast simulation for quick iterations and basic functional
  debugging.
- Use Vitis cosimulation for more realistic simulations, especially when
  accurate timing or bandwidth information is needed.
- Both approaches use the same host program, allowing easy switching between
  simulation methods and on-board execution.
