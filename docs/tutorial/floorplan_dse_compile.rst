Run TAPA Projects With Floorplan DSE
====================================

.. note::

   In this tutorial, we explore how to compile a tapa project with floorplan design space exploration (DSE) and add pipeline to the project.

Example: Compile Autosa with Floorplan DSE
------------------------------------------

The following example demonstrates how to compile a TAPA project with floorplan DSE.
You can use the ``tapa generate-floorplan`` command to run the floorplan DSE and get floorplan
solutions.

.. code-block:: bash

    tapa generate-floorplan \
        -f tapa/src/kernel_kernel.cpp \
        -t kernel0 \
        --device-config device_config.json \
        --floorplan-config floorplan_config.json \
        --clock-period 3.00 \
        --part-num xcu55c-fsvh2892-2L-e

After generating the floorplan solutions, you can compile the project with a specific
floorplan solution with the ``tapa compile`` command by specifying the ``--floorplan-path`` option.
The application will be reorganized according to the floorplan solution, and pipeline
will be added. An xo file and floorplan constrain xdc file will be generated.

.. code-block:: bash

    tapa compile \
        -f tapa/src/kernel_kernel.cpp \
        -t kernel0 \
        --device-config device_config.json \
        --floorplan-path floorplan_0.json \
        --clock-period 3.00 \
        --part-num xcu55c-fsvh2892-2L-e \
        --pipeline-config pipeline_config.json

Alternatively, you can use the ``tapa compile-with-floorplan-dse`` command to compile the project
with floorplan DSE directly. This command will automatically run the floorplan DSE, compile,
and add pipeline to the project for each floorplan solution generated.

.. code-block:: bash

    tapa compile-with-floorplan-dse \
        -f tapa/src/kernel_kernel.cpp \
        -t kernel0 \
        --device-config device_config.json \
        --floorplan-config floorplan_config.json \
        --clock-period 3.00 \
        --part-num xcu55c-fsvh2892-2L-e \
        --pipeline-config pipeline_config.json

The ``--floorplan-config`` option specifies the floorplan DSE configuration file.
An example of a floorplan DSE configuration file is as follows:

.. code-block:: json

    {
        "max_seconds": 1000,
        "dse_range_min": 0.7,
        "dse_range_max": 0.88,
        "partition_strategy": "flat",
        "cell_pre_assignments": {},
        "cpp_arg_pre_assignments": {
            "a": "SLOT_X1Y0:SLOT_X1Y0",
            "b_0": "SLOT_X2Y0:SLOT_X2Y0",
            "b_1": "SLOT_X2Y0:SLOT_X2Y0",
            "c_.*": "SLOT_X2Y0:SLOT_X2Y0"
        },
        "sys_port_pre_assignments": {
            "ap_clk": "SLOT_X2Y0:SLOT_X2Y0",
            "ap_rst_n": "SLOT_X2Y0:SLOT_X2Y0",
            "interrupt": "SLOT_X2Y0:SLOT_X2Y0",
            "s_axi_control_.*": "SLOT_X2Y1:SLOT_X2Y1"
        },
        "grouping_constraints": [],
        "reserved_slot_to_cells": {},
        "partition_schedule": [],
        "slot_to_rtype_to_min_limit": {
            "SLOT_X0Y2:SLOT_X0Y2": {
            "LUT": 0.85
            }
        },
        "slot_to_rtype_to_max_limit": {},
        "ignore_narrow_edge_threshold": 1
    }

The ``cpp_arg_pre_assignments`` field specifies the pre-assignments of the C++ top function
arguments to the slots. If the top port is an array, you can either specify each element
individually or use a regex pattern to match the elements. ``sys_port_pre_assignments`` specifies
the pre-assignments of the verilog system ports to the slots.
