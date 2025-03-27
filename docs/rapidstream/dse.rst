Design Space Exploration
========================

Manual Exploration
------------------

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
setting the ``pp_scheme`` attribute to ``single`` in a pipeline configuration
json file, and pass the file to RapidStream using the ``--pipeline-config``
option in the ``rapidstream-tapaopt`` command. ``pp_scheme`` can also be set
to ``single_h_double_v`` to use a single FF for horizontal crossings and two
FFs for vertical crossings. Its default value is ``double``.

.. note::

   ``set_slot_capacity`` can be used for rerouting stream connections. And
   ``pp_scheme`` can be used to control FF usage for inter-slot pipelining.

Automated Exploration
---------------------

Besides the optimization suggestions for designers, we offer an integrated exploration flow by analyzing a group of metrics of the implemented solutions, and exerting a series of actions to tune the configurations of partitioning, floorplanning and pipelining.

Metrics
~~~~~~~

The floorplanning and pipelining in RapidStream automatically dump some metrics.

Floorplan Result
""""""""""""""""

When floorplanning finishes for a solution, RapidStream generates a report ``metric_floorplan.json`` under the metric directory, ``<--work-dir>/dse/solution_*/metrics/``. A typical report contains the following attributes.

.. code-block:: json

   {
   "area_limit": 0.8,
   "overall_usage": {
      "BRAM_18K": 0.5766,
      "DSP": 0.3169,
      "FF": 0.2154,
      "LUT": 0.5322,
      "URAM": 0.5333
   },
   "slots": [
      {
         "total_area": {
            "bram_18k": 384,
            "dsp": 720,
            "ff": 175040,
            "lut": 77520,
            "uram": 64
         },
         "used_area": {
            "bram_18k": 256,
            "dsp": 195,
            "ff": 32680,
            "lut": 58030,
            "uram": 32
         },
         "utilization": {
            "BRAM_18K": 0.6667,
            "DSP": 0.2708,
            "FF": 0.1867,
            "LUT": 0.7486,
            "URAM": 0.5
         },
         "x": 0,
         "y": 0
      },
      {
         "...": "..."
      }
   ]
   }

The ``area_limit`` is the global upper limit of resource usage applied on all slots. Multiple solutions could apply different global upper limits (even not identical with user's input configuration) because of the DSE mechanism or the floorplanner's multiple trials. The ``overall_usage`` shows the resource usage across the whole device. Then, the ``slots`` shows the usage of all slots in a list.

Pipeline Result
"""""""""""""""

When pipelining finishes for a solution, RapidStream generates the pipelining report in ``metric_pipeline.json`` under the metric directory. An example of pipelining result over 3x3 slots is as follows.

.. code-block:: json

   {
      "slot_pair_to_crossing": {
         "((0,0),(0,1))": 4081,
         "((0,0),(1,0))": 6912,
         "((0,1),(0,2))": 3692,
         "((0,1),(1,1))": 6279,
         "((0,2),(1,2))": 4163,
         "((1,0),(1,1))": 4079,
         "((1,0),(2,0))": 6534,
         "((1,1),(1,2))": 3936,
         "((1,1),(2,1))": 7617,
         "((1,2),(2,2))": 5108,
         "((2,0),(2,1))": 5048,
         "((2,1),(2,2))": 5130
      }
   }

The ``"((0,0),(0,1))": 4081`` means the number of slot crossings between ``SLOT_X0Y0`` and ``SLOT_X0Y1`` is 4081.
