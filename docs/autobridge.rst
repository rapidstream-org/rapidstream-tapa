
Floorplanning with `AutoBridge <https://github.com/Licheng-Guo/AutoBridge>`_
----------------------------------------------------------------------------------

This section covers the usage of AutoBridge in TAPA.

AutoBridge is an automated floorplanning and pipelining tool for HLS designs.
By leveraging the high-level design hierarchies,
it produces coarse-grained floorplans so that SLR crossing do not bottleneck the
clock frequency of an HLS kernel.
This is natively integrated with TAPA.

When running ``tapac``, specify the output constraint file:

.. code-block:: shell
  :emphasize-lines: 5

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --constraint constraint.tcl

Since the AXI interfaces are generally not very flexible,
it'll be good if the ``m_axi``
`connectivity configuration file <https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/connectivity-Options>`_
is also specified:

.. code-block:: shell
  :emphasize-lines: 6

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --constraint constraint.tcl \
    --connectivity connectivity.ini

By default, AutoBridge uses the resource estimation from HLS report.
This can be fairly inaccurate and effect the QoR.
TAPA can be configured to use RTL synthesis result for each task instance.

.. code-block:: shell
  :emphasize-lines: 7

  tapac -o vadd.$platform.hw.xo vadd.cpp \
    --platform $platform \
    --top VecAdd \
    --work-dir vadd.$platform.hw.xo.tapa \
    --constraint constraint.tcl \
    --connectivity connectivity.ini \
    --enable-synth-util

.. note::

  Tasks are synthesized in parallel for the post-synthesis area report.

.. note::

  Currently, AutoBridge can only handle the top-level hierarchy.
  Lower-level hierarchies are not visible to AutoBridge and are treated as a
  whole.
  You may need to take this into consideration when designing the kernel.
