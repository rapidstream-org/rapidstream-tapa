Controlling the Floorplanning
=============================

Constrain IO Locations
----------------------

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
------------------------

At the same time, you can use the ``cell_pre_assignments`` setting to put
specific parts of your design in specific slots. Its usage is similar to
``port_pre_assignments`` except that it applies to cells instead of ports,
and the pattern is matched against the cell's hierarchical name.

Reproducing the Floorplan of Previous Runs
------------------------------------------

Since the ILP-based floorplanning is non-deterministic, you may want to
reproduce your previous floorplan and tune other stages. RapidStream
generates a floorplan checkpoint ``floorplan.json`` for each solution
under the ``<--work-dir>/dse/solution_*`` directory. You can copy the
dictionary in it, e.g.,

.. code-block:: json

   {
      "FloatvAddFloatv_0": "SLOT_X2Y0:SLOT_X2Y0",
      "FloatvAddFloatv_1": "SLOT_X1Y1:SLOT_X1Y1",
      "...": "..."
   }

and paste it into the floorplan configuration python script or json file,
as the value of the ``cell_pre_assignments`` attribute, e.g.,

.. code-block:: json

   {
      "...": "...",
      "cell_pre_assignments": {
         "FloatvAddFloatv_0": "SLOT_X2Y0:SLOT_X2Y0",
         "FloatvAddFloatv_1": "SLOT_X1Y1:SLOT_X1Y1",
         "...": "..."
      },
      "...": "..."
   }

Then, you can start the next run by giving ``--floorplan-config`` the
updated floorplan configuration ``floorplan_config.json`` above.
