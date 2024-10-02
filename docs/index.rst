RapidStream TAPA Documentation
##############################

Welcome to the RapidStream TAPA documentation. RapidStream TAPA is a powerful
framework for designing high-frequency FPGA dataflow accelerators. It combines
a robust C++ API for expressing task-parallel designs with advanced
optimization techniques from `RapidStream`_ to deliver exceptional design
performance and productivity.

.. _RapidStream: https://rapidstream-da.com

This documentation is divided into two main sections:
:ref:`User Documentation` for those using TAPA to develop FPGA
accelerators, and :ref:`Developer Documentation` for those contributing
to or extending the TAPA framework itself.

.. _User Documentation:

.. toctree::
  :caption: User Documentation
  :maxdepth: 2

  user/intro
  user/installation
  user/getting_started

.. toctree::
  :maxdepth: 1

  user/cheatsheet

.. toctree::
  :caption: Getting Started with RapidStream

  rapidstream

.. toctree::
  :caption: Integration with Vitis

  vitis

.. toctree::
  :caption: Tutorial: Speed

  tutorial/fast_cosim

.. toctree::
  :caption: Tutorial: Expressiveness

  tutorial
  tutorial/async_mmap

.. toctree::
  :caption: Tutorial: Design Tips

  tutorial/migrate_from_vitis_hls
  tutorial/debug_tips

.. toctree::
  :caption: Example Designs

  example

.. toctree::
  :caption: More about TAPA

  overview/vs-state-of-the-art

.. toctree::
  :caption: Reference

  api
  overview/output_file_organization.rst
  overview/known-issues

.. _Developer Documentation:

Developer Documentation
=======================
