RapidStream TAPA Documentation
##############################

Welcome to the `RapidStream TAPA`_ documentation. RapidStream TAPA is a
powerful framework for designing high-frequency FPGA dataflow accelerators.
It combines a robust C++ API for expressing task-parallel designs with
advanced optimization techniques from `RapidStream`_ to deliver exceptional
design performance and productivity.

.. _RapidStream TAPA: https://github.com/rapidstream-org/rapidstream-tapa

.. _RapidStream: https://rapidstream-da.com

This documentation is divided into three main sections:
:ref:`User Documentation` for those using TAPA to develop FPGA accelerators,
:ref:`Tutorials` for hands-on examples, and :ref:`Developer Documentation` for
those contributing to or extending the TAPA framework itself.

.. _User Documentation:

.. toctree::
  :caption: User Documentation
  :maxdepth: 2

  user/intro
  user/installation
  user/getting_started
  user/cosim
  user/vitis
  user/tasks
  user/data
  user/rapidstream

.. toctree::
  :maxdepth: 1

  user/cheatsheet

.. _Tutorials:

.. toctree::
  :caption: Tutorials
  :maxdepth: 2

  tutorial/async_mmap
  tutorial/migrate_from_vitis_hls
  tutorial/debug_tips
  tutorial/mini_example
  tutorial/realworld_example
  tutorial/getting_started_vadd

.. toctree::
  :caption: Reference
  :maxdepth: 3

  ref/api
  ref/output_file_organization.rst

.. _Developer Documentation:

.. toctree::
  :caption: Developer Documentation
  :maxdepth: 2

  dev/build
  dev/development
  dev/contributing
  dev/release

.. _FAQ:

.. toctree::
  :caption: FAQ
  :maxdepth: 1

  user/faq
