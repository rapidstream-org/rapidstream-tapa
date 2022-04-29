Refer to the TAPA Github repo for the full code.

Mini Examples
-----------------

Vector Add
:::::::::::::::::


.. literalinclude:: ../apps/vadd/vadd.cpp
   :language: cpp


Vector Add with Shared Memory Interface
::::::::::::::::::::::::::::::::::::::::::::::::


.. literalinclude:: ../apps/shared-vadd/vadd.cpp
   :language: cpp


Vector Add with Multiple Task Hierarchy
::::::::::::::::::::::::::::::::::::::::::::::::

.. literalinclude:: ../apps/nested-vadd/vadd.cpp
   :language: cpp


Bandwidth Test (using ``async_mmap``)
:::::::::::::::::::::::::::::::::::::::::::

.. literalinclude:: ../apps/bandwidth/bandwidth.cpp
   :language: cpp


Network
:::::::::::

.. literalinclude:: ../apps/network/network.cpp
   :language: cpp

Graph
:::::::::::

.. literalinclude:: ../apps/graph/graph.cpp
   :language: cpp

Cannon
:::::::::::

.. literalinclude:: ../apps/cannon/cannon.cpp
   :language: cpp

Jacobi
:::::::::::


.. literalinclude:: ../apps/jacobi/jacobi.cpp
   :language: cpp



Real-World Examples
----------------------

The TAPA repo also includes a set of large scale designs under the ``regression`` directory. This directory is in active development and we are adding more sophiticated TAPA designs here.

- cnn: an 16x13 systolic array, originally published in FPGA'21
- lu_decompose: an 32x32 triangular systolic array, originally published in FPGA'21
- spmm: sparse matrix-matrix multiplication, originally published in FPGA'22
- knn: K-nearest neighbor, originally published in FPL'20
