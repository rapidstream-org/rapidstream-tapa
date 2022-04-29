
Floorplanning with `AutoBridge <https://github.com/Licheng-Guo/AutoBridge>`_
----------------------------------------------------------------------------------

This section covers the usage of AutoBridge in TAPA.

AutoBridge is an automated floorplanning and pipelining tool for HLS designs.
By leveraging the high-level design hierarchies,
it produces coarse-grained floorplans so that SLR crossing do not bottleneck the
clock frequency of an HLS kernel.
This is natively integrated with TAPA.

Supported Devices
::::::::::::::::::::::::::::

Currently AutoBridge supports the Xilinx Alveo U250 and U280 FPGAs. Specifically, the tool has been tested on Vitis version ``xilinx_u250_xdma_201830_2`` and ``xilinx_u280_xdma_201920_3``.

- Feel free to contact us for support of other devices.
- Note that the methodology works on large devices (like UltraScale+).

Basic Usage
::::::::::::

When running ``tapac``, add the ``--enable-floorplan`` option and specify the output constraint file with the ``--floorplan-output`` option.

.. code-block:: shell
  :emphasize-lines: 5,6

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --enable-floorplan \
    --floorplan-output constraint.tcl

AutoBridge needs to know how the AXI interfaces bind to the physical ports, thus the user need to provide a
`connectivity configuration file <https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/connectivity-Options>`_ through the ``--connectivity`` option:

.. code-block:: shell
  :emphasize-lines: 7

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --enable-floorplan \
    --floorplan-output constraint.tcl \
    --connectivity connectivity.ini

.. note::

  Be careful with the connectivity file. An inferior port binding will cause unnecessary routing congestion. Especially for U280 users, since all the 32 HBM channels are placed on the bottom SLR, you want to balance the usage of channel 0-15 and channel 16-21.

Here is an example of the connectivity file. The syntax is the same as required by the Vitis workflow:

.. code-block:: shell

  [connectivity]
  sp=VecAdd_1.a:DDR[0]
  sp=VecAdd_1.b:DDR[1]
  sp=VecAdd_1.c:DDR[2]

- Note that if you invoke tapac from the command line, you must append an ``_1`` suffix to the top name. On the other hand, you must not append this suffix if you use the cmake flow.


By default, AutoBridge uses the resource estimation from HLS report.
This can be fairly inaccurate (especially for LUT and FF) and affect the QoR.
TAPA can be configured to use RTL synthesis result for each task instance by the ``--enable-synth-util`` option.
This option instructs TAPA to run logic synthesis of each task to get a more accurate resource usage, thus improving the quality of the floorplanning results.

.. code-block:: shell
  :emphasize-lines: 8

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --enable-floorplan \
    --floorplan-output constraint.tcl \
    --connectivity connectivity.ini \
    --enable-synth-util

.. note::

  Tasks are synthesized in parallel for the post-synthesis area report. Still, this step could takes a while as RTL synthesis is much slower than C synthesis. By default, tapac will invoke 8 parallel Vivado processes to synthesize each task. You could allow more processes through the ``--max-parallel-synth-jobs`` option, though you should be careful to not run out of memory.


If an external memory port is only read from or only write to, you should mark it through the ``--read-only-args`` and ``--write-only-args`` options. They will help improve the floorplan quality. Those options accept regular expressions representing multiple ports. In the example, ports whose names match the pattern ``hbm_[0-3]`` will be marked as read-only, while ports ``my_out_port`` and ``another_out_port`` will be marked as write-only.

.. code-block:: shell
  :emphasize-lines: 9,10,11

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --enable-floorplan \
    --floorplan-output constraint.tcl \
    --connectivity connectivity.ini \
    --enable-synth-util \
    --read-only-args "hbm_[0-3]" \
    --write-only-args "my_out_port" \
    --write-only-args "another_out_port" \

Visualize the Impact of AutoBridge
::::::::::::::::::::::::::::::::::::::

This figure visualizes the difference in the final bitstream with or without AutoBridge.

.. image:: https://user-images.githubusercontent.com/32432619/165637029-9595b37b-6323-463b-a206-aa73ad7c1519.png
  :width: 50 %

It  shows a CNN accelerator implemented on
the Xilinx U250 FPGA. It interacts with three DDR controllers, as
marked in grey, pink, and yellow blocks in the figure. In the original
implementation result, the whole design is packed close together
within die 2 and die 3. To demonstrate our proposed idea, we first
manually floorplan the design to distribute the logic in four dies
and to avoid overlapping the user logic with DDR controllers. Additionally, we pipeline the FIFO channels connecting modules in
different dies as demonstrated in the figure. The manual approach
improves the final frequency by 53%, from 216 MHz to 329 MHz.

Basic Idea of AutoBridge
::::::::::::::::::::::::::

The key idea of AutoBridge is that it utilizes the pipelining flexibility of data flow designs.

.. image:: https://user-images.githubusercontent.com/32432619/165635895-6955f6e2-3517-4dad-9f54-4203e997eb8a.png
  :width: 75 %

- To relieve local congestion, AutoBridge tries to spread the logic evenly across the entire device.
- To resolve global critical paths, AutoBridge adds additional pipelines to the latency-insensitive interfaces between tasks.


.. image:: https://user-images.githubusercontent.com/32432619/165636025-a85940ac-70f9-4a2d-8376-c1a96510e449.png
  :width: 75 %


Skip Repetitive HLS Synthesis
::::::::::::::::::::::::::::::::::::::

The following sections discuss how to manipulate the floorplanning process and you may need to run floorplanning multiple times on the same design. To avoid re-running HLS compilation of the tasks, you could specify which steps of TAPA you want to activate. By default, TAPA will execute all steps.

.. code-block:: shell

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \

This is equivalent to:

.. code-block:: shell
  :emphasize-lines: 5,6,7,8,9,10

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --run-tapacc \
    --run-hls \
    --generate-task-rtl \
    --run-floorplanning \
    --generate-top-rtl \
    --pack-xo \

If you want to re-run floorplanning only, you could only specify the last four steps:

.. code-block:: shell
  :emphasize-lines: 5,6,7,8

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --generate-task-rtl \
    --run-floorplanning \
    --generate-top-rtl \
    --pack-xo \

Manipulate the Floorplan Parameters
::::::::::::::::::::::::::::::::::::::::

The floorplanning process is about the tradeoff between two factors:

- Area limit: the maximal percentage of occupied resources in each slot
- SLR crossing limit: the maximal number of wires crossing an SLR boundary

Given a specific area limit and an SLR crossing limit, AutoBridge assigns each task to one slot such that minimize the total wire length.
By default, AutoBridge will empirically select an area limit and an SLR crossing limit within an acceptable range.

You could manipulate AutoBridge through the following options:

- ``--min-area-limit`` and ``--max-area-limit`` specify the range of acceptable area limit that AutoBridge chooses from.
- ``--min-slr-width-limit`` and ``--max-slr-width-limit`` specify the range of acceptable SLR crossing limit that AutoBridge chooses from.
- ``--floorplan-opt-priority`` controls the priority between the two factors. By default, AutoBridge takes priority in selecting a smaller area limit (``AREA_PRIORITIZED``). You could change it by providing the ``SLR_CROSSING_PRIORITIZED`` option.

Manual Floorplanning
:::::::::::::::::::::::

You could enforce the tool to assign certain task instances to specified slots. To do this, use the ``--floorplan-pre-assignments`` option to provide a json file that includes the manual floorplanning.

- A common use case is to reproduce the floorplan step of a previous run. In each time, AutoBridge will generate a ``floorplan-region-to-instances.json`` that records the entire floorplan results. If you simply provide this file from a previous run, then you are essentially reproducing the previous run.

- This step requires you to know some lower level information of the device.

- Here is an example json file. ``CR_X0Y0_To_CR_X3Y3`` stands for "the rectangle region determined by CLOCKREGION_X0Y0 and CLOCKREGION_X3Y3". Assume that the task ``foo`` is invoked twice, this json assigns the first invoked instance ``TASK_VERTEX_foo_1`` to ``CR_X0Y0_To_CR_X3Y3`` and the second invoked instance to ``CR_X4Y8_To_CR_X7Y11``.

.. code-block:: json

  {
    "CR_X0Y0_To_CR_X3Y3": [
      "TASK_VERTEX_foo_1",
      "TASK_VERTEX_bar"
    ],
    "CR_X4Y8_To_CR_X7Y11": [
      "TASK_VERTEX_foo_2",
      "TASK_VERTEX_taz"
    ]
  }

Scalability
::::::::::::

AutoBridge has been tested on designs from <20 tasks to designs with >1000 tasks. Here are some tips if you have more than hundreds of tasks:

- Install the Gurobi solver. Gurobi is free for academia, and the registration process takes less than 5 minutes. See instructions :ref:`here <installation:install gurobi (recommended)>`.

- By default, AutoBridge searches for optimal solutions globally. However, if the process could not finish within a reasonable time, try adding the ``--floorplan-strategy`` option with ``QUICK_FLOORPLANNING``. Empirically, designs with hundreds of tasks may need to run in this strategy, but the situation varies a lot based on how the tasks are connected.

- By default, each solving process is allowed for 600 seconds. You could adjust the threshold by the ``--max-search-time`` option.


Visualize Your Design topology
:::::::::::::::::::::::::::::::::::::

AutoBridge will generate a ``.dot`` representation of how your tasks and streams are connected. You could generate a figure from it online, for example at `GraphvizOnline <https://dreampuf.github.io/GraphvizOnline/>`_. Here is an example of the ``vadd`` application.

.. image:: https://user-images.githubusercontent.com/32432619/166062922-f69bf372-a15c-4d8c-ac11-0fa4f43a3a7e.png
  :width: 80 %

Blue boxes represent external memory ports. The number beside each edge represents its width.


Tips to Improve Frequency
:::::::::::::::::::::::::::::::

- A general rule of thumb is to write smaller tasks, which allows more flexibility in the floorplanning process.

- Smaller tasks also has less *control broadcast*. In general, Vitis HLS will generate a centralized controller for the entire task. If your task is too large, the controller will have a high fanout that will cause routing congestion. Checkout our `DAC 2020 <https://cadlab.cs.ucla.edu/beta/cadlab/sites/default/files/publications/dac20-hls-timing.pdf>`_ paper that studies this problem. The pipeline optimization technique has been realized in Vitis HLS as the ``frp`` pipeline style (e.g., ``#pragma HLS pipeline II=1 style=frp``), which trades area for less control signal fanout.

- Modify the AutoBridge parameter and generate multiple bitstream at the same time. When it comes to the tradeoff between area limit and wire number, it is hard to judge which design point is better, so we may need to run multiple points on the pareto-optimal curve.

- Currently, AutoBridge can only handle the top-level hierarchy. Lower-level hierarchies are not visible to AutoBridge and are treated as a
  whole. You may need to take this into consideration when designing the kernel. This limitation is expected to be addressed soon.

- AutoBridge will generate a ``.dot`` file that helps you visualize the topology of your design.
