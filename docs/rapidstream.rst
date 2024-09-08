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


Control the Floorplan Process
================================
