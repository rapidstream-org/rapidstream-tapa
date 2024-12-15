.. Copyright (c) 2024 RapidStream Design Automation, Inc.
   All rights reserved.
   The contributor(s) of this file has/have agreed to the RapidStream Contributor License Agreement.

.. image:: https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128
   :width: 64px
   :alt: RapidStream Logo

TAPA Design
===========

Introduction
------------

RapidStream is fully compatible with `TAPA <https://github.com/rapidstream-org/rapidstream-tapa>`_.
In this recipe, we illustrate how to create a Xilinx objective file (``.xo``) using TAPA, then optimize the ``.xo`` file with RapidStream, and finally utilize the optimized output in the ongoing Vitis development process.

Xilinx Object Files
--------------------

Vitis compiled object ``.xo`` `files <https://docs.amd.com/r/en-US/ug1393-vitis-application-acceleration/Design-Topology>`_ are IP packages used in the AMD Vitis kernel development flow for programming the programmable logic (PL) region of target devices. These files can be `generated from HLS C++ code <https://docs.amd.com/r/en-US/ug1393-vitis-application-acceleration/Developing-PL-Kernels-using-C>`_ using the ``v++`` command, `packed from RTL code <https://docs.amd.com/r/en-US/ug1393-vitis-application-acceleration/RTL-Kernel-Development-Flow>`_, or created using third-party frameworks like `RapidStream TAPA <https://github.com/rapidstream-org/rapidstream-tapa>`_. In this example, we use ``RapidStream TAPA`` to generate the ``VecAdd.xo`` file, but the same flow applies to object files generated through other methods.

Tutorial
--------

You can find the source code for this tutorial on rapidstream-cookbook `repo <https://github.com/rapidstream-org/rapidstream-cookbook/tree/main/getting_started/tapa_aie_source>`_.

Step 1: C++ Simulation
~~~~~~~~~~~~~~~~~~~~~~~

Since our design calls Xilinx Libraries, we need to source the Vitis environment before running the simulation.

.. code-block:: bash

   source <Vitis_install_path>/Vitis/2023.2/settings64.sh

Before generating the ``.xo`` file, we recommend running a C++ simulation to verify the correctness of the design. This step is optional but highly recommended. Run the following command or ``make csim`` to perform C++ simulation:

.. code-block:: bash

   tapa g++ design/main.cpp design/VecAdd.cpp \
   -I /opt/tools/xilinx/Vitis_HLS/2023.2/include \
   -o build/run_u55c.py/main.exe -O2
   ./build/run_u55c.py/main.exe

You should see the following output:

.. code-block:: text

   I20241010 15:14:52.494259 4113880 task.h:66] running software simulation with TAPA library
   kernel time: 0.0197967 s
   PASS!

Step 2: Generate the Xilinx Object File (``.xo``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We use TAPA on top of 2023.2 to generate the ``.xo`` file. Run the following command or run ``make xo``:

.. code-block:: bash

   source <Vitis_install_path>/Vitis/2023.2/settings64.sh
   mkdir -p build/run_u55c.py
   cd build/run_u55c.py && tapa compile \
   --top VecAdd \
   --part-num xcu280-fsvh2892-2L-e \
   --clock-period 3.33 \
   -o VecAdd.xo \
   -f ../../design/VecAdd.cpp \
   2>&1 | tee tapa.log

Step 3 (Optional): Use Vitis ``--link`` to Generate the ``.xclbin`` File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With the ``.xo`` file generated, you can use ``v++ -link`` to generate the ``.xclbin`` file. Run the following command or execute ``make hw``:

.. code-block:: bash

   v++ -l -t hw \
     --platform  xilinx_u280_gen3x16_xdma_1_202211_1 \
     --kernel VecAdd \
     --connectivity.nk VecAdd:1:VecAdd \
     --config design/link_config.ini \
     --temp_dir build \
     -o build/VecAdd.xclbin \
     build/VecAdd.xo

If your machine is equipped with the target FPGA device, you can deploy the optimized design on the FPGA by running the following command:

.. code-block:: bash

   ./app.exe <path_to_vitis_xclbin>

.. warning::

   This step can take hours to complete. We recommend using the RapidStream flow to optimize the ``.xo`` file instead of generating the ``.xclbin`` file if you are familiar with AMD Vitis flow.

Step 4: Define Virtual Device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this tutorial, we use the `Alveo U55C <https://www.amd.com/en/products/accelerators/alveo/u55c/a-u55c-p00g-pq-g.html>`_ as an example. The device is organized into six slots, each containing 16 clock regions of logic. In actual implementations, the available slots are reduced based on the platform specifics, as some resources are reserved for shell logic.


To generate a ``device.json`` file that details the device features, such as slot resources and locations, you can either run the ``run_u55c.py`` script by invoking RapidStream as shown below or simply enter ``make device`` in the terminal.

.. code-block:: bash

   rapidstream run_u55c.py

Step 5: Use RapidStream to Optimize ``.xo`` Design
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The RapidStream flow conducts design space exploration and generates solutions by taking all TAPA-generated ``.xo`` files as the input.
The RapidStream flow for TAPA requires the following key inputs:

- **tapa-xo-path**: The path to the TAPA-generated ``xo`` file (``VecAdd.xo``).
- **device-config**: The virtual device (``device.json``) generated in the previous step by calling RapidStream APIs.
- **floorplan-config**: The configuration file ([floorplan_config.json](design/config/run_u55c.py/floorplan_config.json)) to guide integrated AutoBridge to floorplan the design.
- **implementation-config**: The configuration file ([impl_config.json](design/config/run_u55c.py/impl_config.json)) to guide Vitis to implement the design (e.g., kernel clock, Vitis platform, etc.).
- **connectivity-ini**: The link configuration file ([link_config.ini](design/config/run_u55c.py/link_config.ini)) specifying how the kernel interfaces are connected to the memory controller. This is the same as the Vitis link configuration file.

In ``floorplan_config.json``, we intentionally assign the cell "add_kernel" to "SLOT_X1Y1" by the following configuration. You can also specify the cell assignment using a similar regular expression.

.. code-block:: json

   "cell_pre_assignments": {
       ".*add_kernel.*": "SLOT_X1Y1_TO_SLOT_X1Y1"
   }

We encapsulate the RapidStream command for TAPA as ``rapidstream-tapaopt`` for invoking.
You can run the command below or execute ``make all`` supported by our [Makefile](Makefile).

.. code-block:: bash

   rapidstream-tapaopt --work-dir build/run_u55c.py \
                       --tapa-xo-path ./VecAdd.xo \
                       --device-config build/run_u55c.py/device.json \
                       --run-impl \
                       --floorplan-config ../../design/config/run_u55c.py/ab_config.json \
                       --implementation-config ../../design/config/run_u55c.py/impl_config.json \
                       --connectivity-ini ../../design/config/run_u55c.py/link_config.ini

When finished, you can locate these files using the following command:

.. code-block:: bash

   find ./build/run_u55c.py/ -name *.xo

If everything is successful, you should find an optimized ``.xo`` file in ``build/run_u55c.py/dse/solution_0/updated.xo``.

Since we enabled the ``--run-impl`` option, RapidStream will launch Vitis to generate the ``.xclbin`` file for the optimized ``.xo`` file.
You can find the optimized ``.xclbin`` file by running the following command:

.. code-block:: bash

   find ./build -name *.xclbin.info

Step 6: Check the Real Floorplan Report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since we assigned the cell "add_kernel" to "SLOT_X1Y1" in the configuration file, we can check the real floorplan report by running the following command or ``make check_floorplan``:

.. code-block:: bash

   rapidstream ../../common/util/get_slot.py \
     -i build/run_u55c.py \
     -o build/run_u55c.py

You can open ``build/run_u55c.py/floorplan_solution_<N>.csv`` to check the real floorplan report. You may find more than one CSV file depending on the number of solutions.


Step 7: Check the Group Module Report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RapidStream mandates a clear distinction between communication and computation within user designs.

- In ``Group modules``, users are tasked solely with defining inter-submodule communication. For those familiar with Vivado IP Integrator flow, crafting a Group module mirrors the process of connecting IPs in IPI. RapidStream subsequently integrates appropriate pipeline registers into these Group modules.

- In ``Leaf modules``, users retain the flexibility to implement diverse computational patterns, as RapidStream leaves these Leaf modules unchanged.

To generate a report on group types, execute the commands below or ``make show_groups``:

.. code-block:: bash

   rapidstream ../../common/util/get_group.py \
       -i build/passes/0-imported.json \
       -o build/module_types.csv

The module types for your design can be found in ``build/module_types.csv``.


Step 8: Use Vitis ``--link`` with the Optimized ``.xo`` File (optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With the optimized ``.xo`` file generated, you can use ``v++ -link`` to generate the ``.xclbin`` file. Run the following command or ``make``:

.. code-block:: bash

   v++ -l -t hw \
     --platform xilinx_u280_gen3x16_xdma_1_202211_1 \
     --kernel VecAdd \
     --connectivity.nk VecAdd:1:VecAdd \
     --config design/link_config.ini \
     --temp_dir build/rapidstream \
     -o build/VecAdd_rs_opt.xclbin \
     ./build/dse/candidate_0/exported/VecAdd.xo

To examine the timing results for each design point, use this command:

.. code-block:: bash

   find ./build -name *.xclbin.info

If your machine is equipped with the target FPGA device, you can deploy the optimized design on the FPGA by running the following command:

.. code-block:: bash

   make host
   ./app.exe <path_to_optimized_xclbin>
