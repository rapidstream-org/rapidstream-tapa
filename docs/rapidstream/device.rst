Customizing the Target Device
=============================

You can create a custom ``VirtualDevice`` for your specific FPGA. Here's how
to model a U55C FPGA as a 3x2 grid:

Step 1: Determine the Grid Size
-------------------------------

RapidStream views each device as a grid of slots. During the floorplan
process, task modules are assigned to these slots. The goal is to balance
resource use across all slots while keeping connections between slots to a
minimum. This approach helps optimize the device's layout and performance.

We use the ``DeviceFactory`` utility to represent the U55C FPGA as a 3x2 grid
in this example. We chose this size because the U55C has three Super Logic
Regions (SLRs), and we usually split one SLR into two slots.

.. code-block:: python

   from rapidstream import DeviceFactory
   df = DeviceFactory(row=3, col=2, part_num="xcu55c-fsvh2892-2L-e")

Step 2: Set Slot Pblocks
------------------------

You need to set the pblock range for each slot using Vivado's format. Each
line should start with either ``-add`` or ``-remove``. Here's an example:

.. code:: python

    for x in range(2):
        for y in range(3):
            pblock = f"-add CLOCKREGION_X{x*4}Y{y*4}:CLOCKREGION_X{x*4+3}Y{y*4+3}"
            df.set_slot_pblock(x, y, [pblock])

Step 3: Extract Slot Resources
------------------------------

We offer a tool to automatically count the resources in each slot. RapidStream
uses Vivado to create pblocks and gather information about all physical
resources in each slot. You can also manually enter or change this information
using the ``set_slot_area`` and ``reduce_slot_area`` functions.

To use the automatic tool, you can run this command:

.. code:: python

    df.extract_slot_resources()

This will get the resource information for all slots in your design and save it
in the device factory object.

Step 4: Set Inter-Slot Capacity
-------------------------------

You need to set the number of wires capacity between each pair of slots.

.. code:: python

    # Set North/South SLR crossing capacity
    for x in range(2):
        df.set_slot_capacity(x, 0, north=11520)
        df.set_slot_capacity(x, 1, north=11520)

        df.set_slot_capacity(x, 1, south=11520)
        df.set_slot_capacity(x, 2, south=11520)

    # Set East/West wire limits
    for y in range(2):
        df.set_slot_capacity(0, y, east=40320)
        df.set_slot_capacity(1, y, west=40320)

    df.set_slot_capacity(0, 2, east=41178)
    df.set_slot_capacity(1, 2, west=41178)

This code sets the wire limits between slots in different directions (north,
south, east, west). The numbers represent how many wires RapidStream is
allowed to connect between each pair of slots.

Step 5: Generate Virtual Device
-------------------------------

Generate a JSON file to save the device configuration:

.. code:: python

    df.factory.generate_virtual_device("u55c_device.json")

This line creates a JSON file named ``u55c_device.json`` that contains the
configuration details for the virtual device. You can use this file in the
floorplan process as an argument to the ``--device-config`` option.
