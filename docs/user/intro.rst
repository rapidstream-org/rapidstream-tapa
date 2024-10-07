Introduction
============

What is RapidStream TAPA?
-------------------------

RapidStream TAPA (\ **Ta**\ sk-\ **Pa**\ rallel) is an end-to-end framework
designed for creating high-frequency FPGA dataflow accelerators. It combines
a powerful C++ API with advanced optimization techniques from `RapidStream`_
to deliver high design performance and productivity.

.. _RapidStream: https://rapidstream-da.com

RapidStream TAPA enables developers to express complex, task-parallel FPGA
designs using familiar and standard ``g++``-compilable C++ syntax while
leveraging FPGA-specific optimizations, aiming to bridge the gap between
high-level design description and efficient hardware implementation.

The framework consists of two key components:

- `TAPA Compiler`_ provides a powerful C++ API for expressing
  task-parallel accelerators and compiles the design into Verilog RTL.
- `RapidStream`_ optimizes the generated RTL for high frequency and partitions
  the design for parallel placement and routing.

While the open-source `TAPA compiler`_ can be used standalone as an HLS tool,
integrating it with `RapidStream`_ maximizes the achievable frequency of TAPA
designs, maximizing the performance of the resulting FPGA accelerators.

.. _TAPA Compiler: https://github.com/rapidstream-org/rapidstream-tapa

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/1.png?raw=true
  :width: 100 %

TAPA Programming Model
----------------------

TAPA compiler introduces an HLS programming model by focusing on task-parallel
dataflow designs. In this model, parallel HLS ``tasks`` communicate with each
other through ``streams``.

- **Tasks** are independent units of computation that execute concurrently,
  defined as C++ functions invoked by the TAPA runtime.
- **Streams** are FIFO-like communication channels connecting tasks, instantiated
  as C++ objects in the TAPA tasks.
- Tasks read from streams, perform computation as defined in the task
  function, and write to other streams.

The TAPA compiler synthesizes these high-level task-parallel descriptions into
standalone, fully-functional Verilog RTL, which can be co-simulated with the
original C++ code using the TAPA runtime, or synthesized into FPGA bitstreams
for deployment with the same TAPA runtime.

.. image:: https://user-images.githubusercontent.com/32432619/167315788-4f4c7241-d7bb-454d-80d2-94a3eae505a5.png
  :width: 100 %

TAPA offers several advantages over other FPGA accelerations solutions like
Intel FPGA SDK for OpenCL and Xilinx Vitis.

Unlike Intel's approach, which limits kernel instances and communication
channels to global variables, TAPA allows for hierarchical designs and
easier code sharing among kernels. TAPA also provides clearer visibility
of accessed channels for each kernel and enables efficient synthesis of
functionally identical kernels.

TAPA overcomes Xilinx Vitis's limitations by supporting both fine-grained
and coarse-grained task parallelism within a unified framework, eliminating
the need for separate OpenCL kernels and complex linking processes. TAPA's
stream interfaces are more consistent and generalizable across different
granularities of parallelism.

With the TAPA programming model, developers can:

- **Express** complex dataflow designs in C++ with a high-level API.
- **Scale** the design by composing tasks and streams.
- **Debug** the design using standard C++ debugging tools and techniques.
- **Accelerate** development with the TAPA compiler's fast compilation time.
- **Optimize** the design for high frequency and resource utilization using
  RapidStream.

.. note::

   These features collectively contribute to TAPA providing a more flexible,
   productive, and higher-quality development experience for FPGA programming
   compared to other solutions.

RapidStream Optimization
------------------------

The RapidStream backend automatically optimizes TAPA designs for high
performance through its partition-and-pipeline optimization:

- **Intelligent Partitioning**: RapidStream automatically floorplans the design
  across the FPGA, achieving balanced resource utilization and reducing local
  congestion.
- **Pipeline Insertion**: RapidStream inserts pipeline registers between tasks
  to maximize the design frequency and mitigate long-wire delays.

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/2.png?raw=true
  :width: 100 %

This dual-pronged approach yields two critical benefits:

1. **Reduced Local Congestion**: By spreading logic across the entire FPGA,
   RapidStream avoids over-utilization hotspots and minimizes local congestion.
2. **Optimized Global Paths**: By efficiently pipelining long-distance
   connections, RapidStream minimizes wire delays and maintains high clock
   frequency.

Success Stories
---------------

Systolic Array Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Without RapidStream, the systolic array design failed to route due to local
congestion. By leveraging RapidStream's partition-and-pipeline optimization,
the design was spread across the FPGA, reducing congestion and achieving a
target clock period of 3 ns (333 MHz).

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/3.png?raw=true
  :width: 50 %

Stencil Design on Xilinx Alveo U280
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For the stencil design on U280, routing was failing due to severe congestion,
as highlighted in the picture as red areas. By intelligently redistributing
the logic and adding pipeline stages to the interconnects using RapidStream,
the design improved from routing failure to 250 MHz.

.. image:: https://github.com/rapidstream-org/doc-figures/blob/main/4.png?raw=true
  :width: 50 %

Summary
-------

RapidStream TAPA is a powerful framework for designing high-frequency FPGA
dataflow accelerators. It provides the following key advantages:

- **Rapid Development**: TAPA compiler accelerates development with fast
  compilation times and familiar C++ syntax. It enables 7× faster compilation
  and 3× faster software simulation than conventional approaches.
- **Expressive API**: TAPA compiler provides rich and modern C++ syntax with
  dedicated constructs for complex memory access patterns and explicit
  parallelism.
- **Scalability**: TAPA compiler scales designs by encapsulating complex
  dataflow patterns into reusable tasks and streams.

RapidStream optimization further enhances the design performance:

- **High-Frequency Performance**: RapidStream optimizes TAPA designs for high
  frequency, achieving 2× higher frequency on average compared to traditional
  HLS tools.
- **HBM Optimizations**: RapidStream optimizes TAPA designs for HBM-based
  FPGAs, automating design space exploration and physical optimizations.

Whether you're working on complex algorithms, data processing pipelines,
or custom accelerators, RapidStream TAPA provides the tools and optimizations
needed to maximize your FPGA's potential.
