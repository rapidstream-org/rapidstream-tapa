

Basic Usage
----------------

- Step 1: Generate a virtual device representing your target FPGA.

Here we use a pre-defined U55C Vitis device as an example. Later we will show how to customize the device.

.. code:: python

    from rapidstream import get_u55c_vitis_device_factory

    factory = get_u55c_vitis_device_factory(VITIS_PLATFORM)
    factory.generate_virtual_device("u55c_device.json")

Run the following scripts with the ``rapidstream`` executable.

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

Run the following scripts with the ``rapidstream`` executable.

.. code:: bash

    rapidstream gen_floorplan_config.py


- Step 3: Run the partition-and-pipeline optimization.

Finally, invoke the ``rapidstream-tapaopt`` executable to perform the partition-and-pipeline
optimization. The following command will generate a new XO file with the optimized design.

.. code:: bash

    rapidstream-tapaopt \
        --work-dir ./build \
        --tapa-xo-path [path-to-xo-file] \
        --device-config u55c_device.json \
        --autobridge-config floorplan_config.json \


Customize Target Device
-------------------------

You can easily customize a ``VirtualDevice`` for your own device.

- Step 1: determine the grid size.

RapidStream treats a device as a grid of slots. The floorplan process assigns each task module
to one of the slots, while balancing the resource utilization across all slots and minimizing
the number of inter-slot connections.

In this example, we use the ``DeviceFactory`` utility to model the U55C FPGA as a 3x2 grid.
We choose this size because U55C has 3 SLRs and we typically partition one SLR into two slots.

.. code:: python

    from rapidstream import DeviceFactory

    df = DeviceFactory(row=3, col=2, part_num="xcu55c-fsvh2892-2L-e")


You need to specify the pblock range for each slot using Vivado convention. Each line
must starts with either ``-add`` or ``-remove``. For example:

.. code:: python

    for x in range(2):
        for y in range(3):
            pblock = f"-add CLOCKREGION_X{x*4}Y{y*4}:CLOCKREGION_X{x*4+3}Y{y*4+3}"
            df.set_slot_pblock(x, y, [pblock])

We provide an utility to automatically extract the number of resources in each slot. This utility
invokes Vivado to generate the pblocks and read out all physical resources in each slot. You could also
manuualy input or adjust those information through the ``set_slot_area`` API and ``reduce_slot_area`` API.

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
-------------------------------

- **Constrain IO locations**.

You must specify which slot each port should connect to by passing a dict to the ``port_pre_assignments``
parameter. You can use regular expressions to match port names. For example, the following configuration file assigns all ports to ``SLOT_X0Y0``.

.. code:: python

    from rapidstream import FloorplanConfig

    config = FloorplanConfig(
        port_pre_assignments={".*": "SLOT_X0Y0:SLOT_X0Y0"},
    )
    config.save_to_file("floorplan_config.json")

In the meanwhile, you can also use the ``cell_pre_assignments`` parameter to assign specific cells to specific slots.
You need to provide a dict that maps hierarchical name of the target cells to desired locations.



Design Space Exploration with RapidStream
------------------------------------------

The pipeline flexibility between tasks brings a large design space for the partition-and-pipeline optimization. Following are some
key parameters that you can control to explore the design space.

- **Grid Size**.

RapidStream models an FPGA device by a grid of slots and assigns each task module to one slot. The intuition behind this is to
force the logic to spread evenly across the device to reduce local congestion; while introducing pipelines between slots to avoid
global critical paths. Thus, the number of slots in the grid is a key parameter that affects the design space. Here are some key
considerations when choosing the grid size:

    - Scalability of the floorplan process. Currently RapidStream employs ILP to map tasks to slots and balance the resource
      utilization across slots. The runtime of the ILP solver grows exponentially with the number of slots. By default, RapidStream
      attempts to directly map tasks to slots in one ILP run . If the runtime is too long,
      you can switch to an interactive partition strategy, which first maps tasks to a smaller number of super-slots and then
      hierarchically maps tasks to slots within each super-slot. RapidStream provides two partition methods: ``flat`` and ``multi-level``. You can select which method to use
      through the ``partition_strategy`` parameter in the ``FloorplanConfig`` object. You can also set the maximal number of
      seconds for the optimization process with the ``max_seconds`` parameter.

        - The ``flat`` method gets better QoR but may be slower for designs with many tasks and connections.
        - The ``multi-level`` method is faster but may get less-optimal partition results.

    - Fragmentation issues. When there are too many small slots, the floorplan process may fail to find a feasible solution due to
      fragmentation. For example, considering mapping a design with three tasks of equal size to a 2x1 grid. Either of the two slots
      will be underutilized while the other will be overutilized. RapidStream will not break the tasks into smaller pieces. Thus,
      users should

        - Not set the slot size to be too small
        - Avoid writing large tasks that will reduce the flexibility of the floorplan process.

    - Effectiveness. If the slots are too large, then the floorplan might not provide enough guidance to the placer. For example, if we
      treat a whole SLR as one slot, chances are there will still be high local congestion within the SLR.


- **Usage Limit of Each Slot**.

When mapping tasks to slots, RapidStream will count the resource utilization of each slot and ensure that the utilization is below a
given usage limit. For example, a typical rule of thumb is to keep the utilization below 70%. Different usage limits can significantly
affect the floorplan and thus the final implementation.

    - When design is more spread out, the slot utilization is lower but the number of inter-slot
      wires is higher. This leads to less local congestions but more global wires.

    - When design is more concentrated, the slot utilization is higher but the number of inter-slot
      wires is lower. This leads to more local congestions but less global wires.

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/5.png?raw=true
    :width: 100 %

You can control the range with the ``dse_range_min`` and ``dse_range_max`` parameters in the ``FloorplanConfig`` object.
RapidStream will generate multiple floorplan schemes within the range.


- **Compensate for Pre-Existing Resource Usage**.

There might be cases where some resources in the slots are already used by other system components outside the TAPA design. In this case,
you can adjust the virtual device to reserve certain amount of resources for the pre-existing components. You can use the ``set_slot_area``
or ``reduce_slot_area`` API to adjust the resource usage of each slot.

A typical example is to use the TAPA design with the Vitis system. The Vitis system will instantiate DDR controllers, HBM controllers and
other system components and connect them with the TAPA design. Those system components will be implemented in the user dynamic region as
well, so it is necessary to reserve some resources for them in the virtual device. Otherwise the actual slot utilization will be higher than
expected and may lead to congestion issues.

- **Inter-Slot Routing**.

For each inter-slot stream connection, RapidStream will determine which intermediate slots the connection should pass through. For example,
consider a stream connection from slot X0Y0 to slot X1Y1, the stream could either pass through slot X0Y1 or slot X1Y0. The choice of the
intermediate slot will affect the final implementation.

RapidStream performs such slot-level routing with the goal to balance the wire usage across all slot boundaries. For example, if the virtual
device specifies that there are 10000 wires between slot X0Y0 and slot X1Y0 but only 500 wires between slot X0Y0 and slot X0Y1, chances are
RapidStream will choose to route the stream through slot X1Y0.

If you observe that there are congestions happening in certain slot boundary, you can try to reduce the specified wire capacity in the
virtual device using the ``set_slot_capacity`` API, which will guide RapidStream to choose a different routing path.
In many times, we can use a U-shape detour to alleviate congestions in the direct path.

Another consideration is how many pipeline registers to insert per slot crossing. The default behavior is to insert two FFs per slot crossing.
However, there may be cases where the FF usage is already very high and it backfires to insert too many registers. In this situation, you could
also change the policy to 1 FF per crossing by using the ``--single-reg`` option of ``rapidstream-tapaopt``.
