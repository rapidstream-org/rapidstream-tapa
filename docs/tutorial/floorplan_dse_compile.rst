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
will be added.

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
