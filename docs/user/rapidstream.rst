Optimization with RapidStream
=============================

.. note::

   This part guides you through the basic usage of RapidStream for optimizing
   TAPA FPGA dataflow accelerators. It assumes you have :ref:`installed TAPA
   and RapidStream <user/installation:One-Step Installation>` and have
   generated a TAPA design into an XO file. If you haven't, please refer to
   the :ref:`Getting Started <user/getting_started:Getting Started>` guide.

We'll cover the steps of generating a virtual device, configuring the
floorplan process, and running the partition-and-pipeline optimization to
create an optimized TAPA design. We'll also explore how to customize the
target device and control the floorplan process for design space exploration.

Getting Started
---------------

To begin optimizing your TAPA design with RapidStream, follow these steps:

Step 1: Generate a Virtual Device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, create a virtual device representing your target FPGA. Here's an
example using a pre-defined U55C Vitis device:

.. code-block:: python

   from rapidstream import get_u55c_vitis_device_factory

   # Set the Vitis platform name
   factory = get_u55c_vitis_device_factory("xilinx_u55c_gen3x16_xdma_3_202210_1")
   # Generate the virtual device in JSON format
   factory.generate_virtual_device("u55c_device.json")

.. note::

   To create a virtual device for your own FPGA, you can customize the
   ``DeviceFactory`` object. We'll cover this in more detail later.

Save this code to a file named ``gen_device.py`` and run it using the
``rapidstream`` executable:

.. code-block:: bash

   rapidstream gen_device.py

.. note::

   Please install RapidStream and a valid license before running the
   ``rapidstream`` executable.

Step 2: Configure the Floorplan Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Next, create a configuration file for the floorplan process. Here's a
simple example that assigns all ports to ``SLOT_X0Y0``:

.. code-block:: python

   from rapidstream import FloorplanConfig

   config = FloorplanConfig(
       port_pre_assignments={".*": "SLOT_X0Y0:SLOT_X0Y0"},
   )
   config.save_to_file("floorplan_config.json")

Save this code to a file named ``gen_floorplan_config.py`` and run it:

.. code-block:: bash

   rapidstream gen_floorplan_config.py

Step 3: Run Partition-and-Pipeline Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Finally, use the ``rapidstream-tapaopt`` executable to perform the
partition-and-pipeline optimization:

.. code-block:: bash

   rapidstream-tapaopt \
       --work-dir ./build \
       --tapa-xo-path [path-to-xo-file] \
       --device-config u55c_device.json \
       --autobridge-config floorplan_config.json

This command generates a new XO file with the optimized design.

Customizing the Target Device
-----------------------------

You can create a custom ``VirtualDevice`` for your specific FPGA. Here's how
to model a U55C FPGA as a 3x2 grid:

Step 1: Determine the Grid Size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~

You need to set the pblock range for each slot using Vivado's format. Each
line should start with either ``-add`` or ``-remove``. Here's an example:

.. code:: python

    for x in range(2):
        for y in range(3):
            pblock = f"-add CLOCKREGION_X{x*4}Y{y*4}:CLOCKREGION_X{x*4+3}Y{y*4+3}"
            df.set_slot_pblock(x, y, [pblock])

Step 3: Extract Slot Resources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Generate a JSON file to save the device configuration:

.. code:: python

    df.factory.generate_virtual_device("u55c_device.json")

This line creates a JSON file named ``u55c_device.json`` that contains the
configuration details for the virtual device. You can use this file in the
floorplan process as an argument to the ``--device-config`` option.

Controlling the Floorplan
-------------------------

Constrain IO Locations
~~~~~~~~~~~~~~~~~~~~~~

You need to tell RapidStream which slot each port should connect to. You do
this by giving it a dictionary through the ``port_pre_assignments`` setting
of the ``FloorplanConfig`` object. Here's an example:

.. code:: python

    from rapidstream import FloorplanConfig

    config = FloorplanConfig(
        port_pre_assignments={".*": "SLOT_X0Y0:SLOT_X0Y0"},
    )
    config.save_to_file("floorplan_config.json")

.. note::

    You can use regular expression patterns to match port names. For example,
    the above code assigns all ports (``".*"``) to slot ``SLOT_X0Y0``.

Constrain Cell Locations
~~~~~~~~~~~~~~~~~~~~~~~~

At the same time, you can use the ``cell_pre_assignments`` setting to put
specific parts of your design in specific slots. Its usage is similar to
``port_pre_assignments`` except that it applies to cells instead of ports,
and the pattern is matched against the cell's hierarchical name.

Design Space Exploration
------------------------

TAPA offers flexibility in pipeline design between tasks, creating a large
design space for optimization. Here are key parameters you can adjust:

Grid Size
~~~~~~~~~

RapidStream models an FPGA device as a grid of slots, assigning each task
module to one slot. This spreads logic evenly across the device to reduce
local congestion and introduces pipelines between slots to avoid global
critical paths. Consider these factors when choosing grid size:

1. **Runtime**: RapidStream uses integer linear programming (ILP) to map
   tasks to slots. Runtime increases with the number of slots exponentially.
   You can choose between two partition methods:

- **Flat**: Better QoR but slower for complex designs with many tasks and
  connections. This is the default method.
- **Multi-level**: Faster but may provide less optimal partition results.
  You may choose this method using the ``partition_strategy`` parameter in
  the ``FloorplanConfig`` object, setting it to ``multi-level``.

.. note::

   Different partition methods may lead to different optimization results
   and runtime. If the ILP solver takes too long, you can set a maximum
   runtime with the ``max_seconds`` parameter. This restricts the optimization
   results to those found within the time limit.

2. **Fragmentation**: When there are too many small slots, the floorplan
   process might fail because the space is too divided. For example, if you
   try to fit three equal-sized tasks into a 2x1 grid, one slot will have too
   much space while the other won't have enough. RapidStream won't split
   tasks into smaller parts. So, users should:

- **Avoid Small Slots**: Don't set the slot size too small.
- **Avoid Large Tasks**: Large tasks reduce floorplan flexibility. Large
  tasks make it harder to arrange things on the floorplan.

.. note::

   The grids should be large enough to accommodate all tasks and avoid
   fragmentation.

3. **Effectiveness**: If the slots are too big, the floorplan might not guide
   the placer well enough. For example, if we treat a whole SLR (Super Logic
   Region) as one slot, there could still be a lot of crowding in certain
   areas within that SLR. This means the placer might not have enough detailed
   information to spread out the components evenly.

.. note::

   The grids should be fine-grained enough to guide the placer to spread out
   the components evenly. A trade-off point should be found in the middle.

Slot Usage Limit
~~~~~~~~~~~~~~~~

RapidStream ensures each slot's resource utilization stays below a set limit.
Adjusting this limit affects the final implementation:

- **Lower Utilization**: More spread out design, less local congestion, more
  global wires.
- **Higher Utilization**: More concentrated design, more local congestion,
  fewer global wires.

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/5.png?raw=true
    :width: 100 %

You can control the range with the ``dse_range_min`` and ``dse_range_max``
parameters in the ``FloorplanConfig`` object. RapidStream's design space
exploration (DSE) algorithm will generate multiple floorplan schemes within
the range.

.. note::

   Set the range to a reasonable value to avoid too many or too few
   floorplan schemes in suboptimal ranges.

Pre-Existing Resource Usage
~~~~~~~~~~~~~~~~~~~~~~~~~~~

If certain resources are already in use by external components outside the
TAPA design, adjust the virtual device accordingly to reserve these
resources. This can be accomplished using the ``set_slot_area`` or
``reduce_slot_area`` API to fine-tune resource usage for each slot.

For example, when using a TAPA design with the Vitis system, it typically
instantiates various controllers (such as DDR and HBM) and other system
components, connecting them to the TAPA design. These system components are
implemented in the user dynamic region alongside the TAPA design. To avoid
potential congestion issues and ensure accurate resource utilization
estimates, it's crucial to reserve appropriate resources for these components
in the virtual device. Failing to do so may result in actual slot utilization
exceeding expectations, potentially leading to local congestion issues.

.. note::

   ``set_slot_area`` and ``reduce_slot_area`` can be used to reserve resources
   for external components.

Inter-Slot Routing
~~~~~~~~~~~~~~~~~~

RapidStream determines the optimal path for inter-slot stream connections by
selecting appropriate intermediate slots. This process aims to balance wire
usage across all slot boundaries. For instance, when connecting slot X0Y0 to
X1Y1, RapidStream chooses between routing through X0Y1 or X1Y0 based on
available wire capacity.

RapidStream prioritizes less congested paths. If the capacity between X0Y0
and X1Y0 is 10,000 wires, while X0Y0 to X0Y1 is only 500, RapidStream is
likely to route through X1Y0. To address congestion issues, users can adjust
wire capacity using the ``set_slot_capacity`` API, potentially guiding
RapidStream to select alternative routes. RapidStream will automatically
generate U-shaped detours to help alleviate congestion in direct paths.

RapidStream typically inserts two flip-flops (FFs) per slot crossing for
pipelining. However, in cases of high FF usage, this approach may cause
additional resource congestion. Users can opt for a single FF per crossing by
using the ``--single-reg`` option in the ``rapidstream-tapaopt`` command.

.. note::

   ``set_slot_capacity`` can be used for rerouting stream connections. And
   ``--single-reg`` can be used to reduce FF usage.
