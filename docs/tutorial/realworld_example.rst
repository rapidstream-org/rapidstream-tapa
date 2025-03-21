Real-World Examples
----------------------

The TAPA repo also includes a set of large scale designs under the
``tests/regression`` directory. This directory is under active development and
we are adding more sophiticated TAPA designs here.

- ``autosa`` folder contains a set of designs that are generated by the AutoSA
  compiler. ``mm/10x13`` is a matrix multiplication systolic array of 10x13
  dimension, taking 90% of LUTs on the Alveo U55C FPGA.

- ``callipepla`` is a conjugate gradient solver using 26 of 32 HBM channels
  originally published in `FPGA'23
  <https://dl.acm.org/doi/pdf/10.1145/3543622.3573182>`_.

- ``cnn`` and ``lu_decomposition`` are both systolic arrays (of different
  shape) originally published in `FPGA'21
  <https://dl.acm.org/doi/pdf/10.1145/3431920.3439292>`_.

- ``hbm-bandwidth`` can be used to profile the HBM bandwidth. It reads from and
  write back to all 32 channels of HBM. It is a good demonstration of the
  expressiveness and the optimized area of ``async_mmap``.

- ``hbm-bandwidth-1-ch`` only reads from and writes to one HBM channel.

- ``serpens`` is a sparse matrix-vector multiplication published in `DAC'22
  <https://arxiv.org/pdf/2111.12555.pdf>`_. We provide different versions that
  are of the same architecture but different parallelism (number of HBM
  channels instantiated).

- ``spmm`` is a sparse matrix-matrix multiplication published in `FPGA'22
  <https://dl.acm.org/doi/pdf/10.1145/3490422.3502357>`_.

- ``spmv-hisparse-mmap`` is another sparse matrix-vector multiplication
  published in `HiSparse
  <https://www.csl.cornell.edu/~zhiruz/pdfs/spmv-fpga2022.pdf>`_ at FPGA'22.

- ``knn`` is a K-nearest-neighbor accelerator originally published in `FPT'20
  <http://www.sfu.ca/~zhenman/files/C19-FPT2020-CHIP-KNN.pdf>`_.

- ``page_rank`` is an accelerator for the Page-Rank algorithm that is included
  in `FCCM'21 <https://about.blaok.me/pub/fccm21-tapa.pdf>`_.


One-click TAPA + RapidStream Compilation
:::::::::::::::::::::::::::::::::::::::::

The following designs under the ``tests/regression`` directory contain a script
``rapidstream/run_rs.sh`` that can be used to compile the design with TAPA and
RapidStream.

- ``autosa/mm/10x13/u55c``
- ``autosa/mm/10x13/u250``
- ``callipepla``
- ``serpens-32ch``
- ``spmm/sextans-u55c-3x3floorplan``

The script executes the following steps:

1. Generate the configuration (.json) files for RapidStream (<5 min):

- ``device_config.json``: device information including the resource within and
  between slots.
- ``floorplan_config.json``: containing the floorplan solving time limit, the
  range of floorplan resource budget to be searched, the floorplan (partition)
  strategy, pre-assignments of cells/ports, min resource limit on slots, etc.
- ``impl_config.json``: Vitis/Vivado implementation configuration including
  the target clock period, the parallelism for number of Vivado runs and jobs
  within each run, etc. The default parallelism is set to 1 for safe memory
  usage, you can increase it to speed up the implementation based on your
  compute resource. Empirically, the implementation of a large-scale design
  for Alveo/Versal devices requires at least 64GB memory for each run. For
  example, if you have 256GB memory, you can set ``--max-workers`` to 4. As for
  the ``--max-synth-jobs``, it controls the `--vivado.synth.jobs
  <https://docs.amd.com/r/2024.1-English/
  ug1393-vitis-application-acceleration/vivado-Options>`_ parameter
  of the v++ command.

2. Synthesize TAPA code to RTL and pack it to a kernel object (.xo) (<5 min).

3. RapidStream consumes the kernel object and the configuration files to
   generate the floorplan & pipeline and run Vitis implementation (hours).
   The following parameters controls the RapidStream behavior:

- ``--skip-preprocess``: skip the preprocess step before floorplanning and
  reuse the old post-preprocess RapidStream IR in the ``--work-dir``.
- ``--skip-partition``: skip the partitioning/floorplanning step before
  pipeline generation and reuse the old post-partitioning/floorplanning
  RapidStream IR in the ``--work-dir``.
- ``--skip-add-pipeline``: skip the pipeline generation step before exporting
  and reuse the old post-pipelining RapidStream IR in ``--work-dir``.
- ``--skip-export``: skip the export step before implementation and reuse the
  previously exported solutions in the ``--work-dir``.
- ``--run-impl``: run Vitis implementation on existing exported solutions.
- ``--extract-metrics``: extract the quality metrics from the implemented
  (post-route) design checkpoint (.dcp).
- ``--setup-single-slot-eval``: enable the single-slot evaluation flow.

4. Report the maximum achieved frequency (<1 min).
