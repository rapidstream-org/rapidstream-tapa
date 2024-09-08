RapidStream
-------------


Basic Usage
==============

- Step 1: Generate a virtual device representing your target FPGA.

Here we use a pre-defined U55C Vitis device as an example. Later we will show how to customize the device.

.. code:: python

    from rapidstream import get_u55c_vitis_device_factory

    factory = get_u55c_vitis_device_factory(VITIS_PLATFORM)
    factory.generate_virtual_device("u55c_device.json")

Run the following scripts with the `rapidstream` executable.

.. code:: bash

    rapidstream gen_device.py

- Step 2: Configurate the floorplan process.

For simplicity, we specify that all ports should connect to `SLOT_X0Y0`. Meanwhile we skip all other
parameters to the default value. Save the contents to a file named `floorplan_config.json`.

.. code:: python

    from rapidstream import FloorplanConfig

    config = FloorplanConfig(
        port_pre_assignments={".*": "SLOT_X0Y0:SLOT_X0Y0"},
    )
    config.save_to_file("floorplan_config.json")

Run the following scripts with the `rapidstream` executable.

.. code:: bash

    rapidstream gen_floorplan_config.py


- Step 3: Run the partition-and-pipeline optimization.

Finally, invoke the `rapidstream-tapaopt` executable to perform the partition-and-pipeline
optimization. The following command will generate a new XO file with the optimized design.

.. code:: bash

    rapidstream-tapaopt \
        --work-dir ./build \
        --tapa-xo-path [path-to-xo-file] \
        --device-config u55c_device.json \
        --autobridge-config floorplan_config.json \


Customize Target Device
========================

You can easily customize a `VirtualDevice` for your own device.

- Step 1: determine the grid size.

RapidStream treats a device as a grid of slots. The floorplan process assigns each task module
to one of the slots, while balancing the resource utilization across all slots and minimizing
the number of inter-slot connections.

In this example, we use the `DeviceFactory` utility to model the U55C FPGA as a 3x2 grid.
We choose this size because U55C has 3 SLRs and we typically partition one SLR into two slots.

.. code:: python

    from rapidstream import DeviceFactory

    df = DeviceFactory(row=3, col=2, part_num="xcu55c-fsvh2892-2L-e")


You need to specify the pblock range for each slot using Vivado convention. Each line
must starts with either "-add" or "-remove". For example:

.. code:: python

    for x in range(2):
        for y in range(3):
            pblock = f"-add CLOCKREGION_X{x*4}Y{y*4}:CLOCKREGION_X{x*4+3}Y{y*4+3}"
            df.set_slot_pblock(x, y, [pblock])

We provid an utility to automatically extract the number of resources in each slot. This utility
invokes Vivado to generate the pblocks and read out all physical resources in each slot. You could also
manuualy input or adjust those information through the `set_slot_area` API and `reduce_slot_area` API.

.. code:: python

    df.extract_slot_resources()


For now, you need to specify the number of physical wires between each pair of slots. We are working on
a utility to automatically extract the wire information from Vivado.

.. code:: python

    # set SLR crossing capacity
    for x in range(2):
        df.set_slot_capacity(x, 0, north=11520)
        df.set_slot_capacity(x, 1, north=11520)

        df.set_slot_capacity(x, 1, south=11520)
        df.set_slot_capacity(x, 2, south=11520)

    # Set W/E capacity
    for y in range(2):
        df.set_slot_capacity(0, y, east=40320)
        df.set_slot_capacity(1, y, west=40320)
    df.set_slot_capacity(0, 2, east=41178)
    df.set_slot_capacity(1, 2, west=41178)

Finally, generate a json file to save the device configuration.

.. code:: python

    df.factory.generate_virtual_device("u55c_device.json")


Control the Floorplan Process
================================
