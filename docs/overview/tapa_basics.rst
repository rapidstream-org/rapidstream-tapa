TAPA Basics
=================

This section introduces the basic concepts of a TAPA program and the major
steps of the TAPA compilation process


TAPA Program
------------------

TAPA dataflow programs decouple communication and computation. A TAPA program
has two types of building blocks: ``streams`` and ``tasks``.

- A ``stream`` is essentially a FIFO
- A ``task`` consumes input data from some ``streams``, perform computation,
  then produces output data to some other ``streams``.
- All ``tasks`` execute in parallel and communicate with each other through
  ``streams``.


.. image:: https://user-images.githubusercontent.com/32432619/167315788-4f4c7241-d7bb-454d-80d2-94a3eae505a5.png
  :width: 100 %


TAPA Compilation Process
----------------------------

The TAPA compilation process ultimately involves two steps.

- First, TAPA extracts each ``task`` and synthesize it independently using an
  existing HLS compilers (e.g., Vitis HLS).

- Second, TAPA generates optimized logic to stitch the RTL of each task
  together into the final accelerator.

.. image:: https://user-images.githubusercontent.com/32432619/167316153-926db74c-add0-4a8e-aa88-fedaa2d3d669.png
  :width: 100 %


Magic of TAPA
--------------------

The core idea of TAPA is to synthesize each ``task`` using existing HLS tools
and generate our own customized RTL to compose the ``tasks``. This allows us to
stand on the shoulder of existing HLS tools, take advantage of them and go
higher.

- **Speed**. Since we explicitly decouples communication and computation, and
  then synthesize each task in parallel, we gain significant speedup compared
  to conventional HLS tools. In addition, we design both software and hardware
  simulator specifically optimized for our programming model and thus beat
  universal simulation tools.

- **High Frequency**. We add layout-aware optimization when generating glue
  logic to compose the individual tasks. Specifically, we pre-determine the
  approximate placement location of each task, and properly pipeline the
  communication structures between tasks based on their locations.

- **Expressiveness**. Since we generate all RTL for communication structures,
  we are free to define additional communication APIs. So far, we have
  implemented a set of highly flexbile APIs for stream interaction and external
  memory access, which are unavailable in other HLS tools.
