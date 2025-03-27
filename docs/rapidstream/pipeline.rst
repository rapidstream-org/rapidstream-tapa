Controlling the Pipelining
==========================

Reproducing the Pipelining Result of Previous Runs
--------------------------------------------------

Similar to floorplanning, the pipelining result is also non-deterministic.
RapidStream generates a pipeline checkpoint ``pipeline.json`` for each solution
under the ``<--work-dir>/dse/solution_*`` directory. You can copy the
dictionary in it, e.g.,

.. code-block:: json

   {
      "1000_fifo_aBvec_12_reset": [
         [0, 2], [0, 1], [0, 0]
      ],
      "1001_fifo_aBvec_13_reset": [
         [0, 2], [0, 1], [0, 0]
      ],
      "...": "..."
   }

and paste it into the pipeline configuration python script or json file,
as the value of the ``nets_pre_assigned`` attribute, e.g.,

.. code-block:: json

   {
      "...": "...",
      "nets_pre_assigned": {
         "1000_fifo_aBvec_12_reset": [
            [0, 2], [0, 1], [0, 0]
         ],
         "1001_fifo_aBvec_13_reset": [
            [0, 2], [0, 1], [0, 0]
         ],
         "...": "..."
      },
      "...": "..."
   }

Then, you can start the next run by giving ``--pipeline-config`` the
updated pipeline configuration ``pipeline_config.json`` above.
