# Overview

[![CI](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml/badge.svg)](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml)
[![install](https://github.com/Blaok/tapa/actions/workflows/install.yml/badge.svg)](https://github.com/Blaok/tapa/actions/workflows/install.yml)
[![Documentation Status](https://readthedocs.org/projects/tapa/badge/?version=latest)](https://tapa.readthedocs.io/en/latest/?badge=latest)

TAPA is an extension to high-level synthesis (HLS) C++
for task-parallel programs.

## Feature Synopsis

### Programmability Improvement

+ [Convenient kernel communication APIs](#kernel-communication-interface)
+ [Simple host-kernel interface](#host-kernel-interface)
+ [Universal software simulation w/ coroutines](#software-simulation)
+ [Hierarchical code generator w/ Xilinx HLS backend](#rtl-code-generation)

### Beyond Programmability

+ [Asynchronous/concurrent memory accesses](#memory-system)
+ [Flexible detached (autorun/free-running) tasks](#detached-tasks)
+ [Resource-efficient FIFO templates](#hardware-fifo)
+ [Improved timing closure](#timing-closure) (via [AutoBridge][autobridge])

## TAPA vs State-of-the-Art

In this project, we target PCIe-based FPGA accelerators that are commonly available in data centers (e.g., [Amazon EC2 F1][aws-f1], [Intel PAC D5005][d5005], [Xilinx Alveo U250][u250]).
In additional to the traditional register-transfer level (RTL) programming approach, these accelerators can also be programmed natively using vendor high-level synthesis (HLS) tools, e.g., [Intel FPGA SDK for OpenCL][iocl] and [Xilinx Vitis][xhls].
Both tools support task-parallel programs in HLS.

[Intel FPGA SDK for OpenCL][iocl] supports multiple OpenCL kernels running in parallel.
The kernels communicate with each other via `channel`.
Blocking/non-blocking read/write are supported.
Both the kernel instances and the communication channels are global variables, which has several implications:

+ Hierarchical designs must be flattened;
+ Sharing code with channel accesses among kernels is difficult;
+ Accessed channels are not clearly visible for each kernel;
+ Functionally identical kernels must be high-level–synthesized separately.

The [number of kernels is limited to 256](https://www.intel.com/content/www/us/en/programmable/documentation/mwh1391807965224.html#ewa1400860540321) due to limitations in software simulation.

[Xilinx Vitis][xhls] supports two different task-parallel granularities.
Fine-grained task-parallelism is supported via `#pragma HLS dataflow`.
In a dataflow region, C++ function calls are interpreted as if they run in parallel.
The communication interface for each C++ function is `hls::stream<T>&`, which generates `ap_fifo` RTL interface with simple ready-valid handshake.
Blocking/non-blocking read/write are supported.
The communication channels are instantiated using the same `hls::stream<T>` C++ class in the dataflow region.
Hierarchical dataflow regions are possible.
However, functionally identical kernels are still high-level–synthesized separately.

Coarse-grained task-parallelism is supported by linking multiple OpenCL kernels together (“[k2k streaming](https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/streamingconnections.html)”).
Each OpenCL kernel itself may leverage `#pragma HLS dataflow`, thus enabling task-parallelism at a higher-level of granularity.
Inter–OpenCL kernel communication uses a more-complicated and less-general `hls::stream<ap_axiu<width, 0, 0, 0>>&` construct, which generates AXI4-Stream (`axis`) RTL interface.
In addition to blocking/non-blocking read/write operations, `axis` also provides a `last` bit that can implement transactions.
However, leveraging such task-parallelism requires additional information beyond the source code at link time, which complicates the overall workflow.

Compared with these state of the art, TAPA provides a more flexible and productive development experience.
In many cases, TAPA can even improve the quality of result (QoR).
Details are as follows.

### Kernel Communication Interface

In addition to blocking/non-blocking read/write operations, TAPA also supports *peeking* and *transactions*.
Peeking is defined as reading a token from a channel without removing the token from the channel.
This makes it possible to conditionally read tokens based on the content of tokens.
Transactions are supported by providing a special end-of-transaction (`eot`) token for each channel.
Such a token can be used to indicate the completion of a transaction without additional coding constructs.

TAPA uses separate `tapa::istream<T>&` and `tapa::ostream<T>&` classes for communication interfaces.
This makes the communication directions of each interface visually obvious in the source code.
IDEs will also only suggest available functions for code completion (e.g., `read` won't show up for `tapa::ostream<T>&`).

### Host-Kernel Interface

Both Intel FPGA SDK for OpenCL and Xilinx Vitis provide high-level host-kernel interface as specified in the OpenCL standard.
While programmers are relived from writing their own operating system drivers, they still have to write lengthy OpenCL host API calls.
For example, the [hello world example](https://github.com/Xilinx/Vitis_Accel_Examples/blob/21bb0cf788ace593c6075accff7f7783588ae8b4/hello_world/src/host.cpp#L58-L115) in Vitis Accel Examples spent 57 lines of code just to call the accelerator from the host.
Worse, running software simulation, hardware simulation, and on-board execution requires different [environment variable setup](https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/runemulation1.html#ariaid-title5) in addition to different bitstream specifications.

TAPA cuts this down to a single function call to `tapa::invoke` and takes care of environmental variable setup.
Programmers only need to change the bitstream specification for different targets.

```cpp
tapa::invoke(
  vadd,                                     // Top-level kernel function.
  bitstream,                                // Path to bitstream. If empty, run software simulation.
  tapa::read_only_mmap<unsigned int>(in1),  // Kernel argument 0, read-only array.
  tapa::read_only_mmap<unsigned int>(in2),  // Kernel argument 1, read-only array.
  tapa::write_only_mmap<int>(out_r),        // Kernel argument 2, write-only array.
  size);                                    // Kernel argument 3, scalar.
```

### Software Simulation

C++ functions in a `#pragma HLS dataflow` region is called sequentially for software simulation.
This works correctly only for programs that do not have cyclic communication patterns (e.g., an image-processing pipeline).

A common approach that fixes the correctness issue is to launch a thread for each task (e.g., OpenCL kernel).
This approach however, has limited scalability due to contention caused by preemptive threads managed by the operating system.

TAPA takes a coroutine-based approach to solve the scalability problem.
Coroutines are managed by user space programs and do not have the contention issue.
Moreover, [context switch between coroutines are much faster than threads](https://www.boost.org/doc/libs/1_76_0/libs/coroutine/doc/html/coroutine/performance.html).

### RTL Code Generation

Although Vitis HLS can reuse HLS results for different instances of the same C++ function, it is not supported in a `#pragma HLS dataflow` region.
Both Intel FPGA SDK for OpenCL and Xilinx Vitis HLS run HLS for each instance of tasks as if they are different for task-parallel programs.
This can lead to long compilation time for programs with many identical task instances, e.g., as in [SODA][soda] and [AutoSA][autosa].
To work around this problem, [SODA][soda] implements an RTL backend that calls Vivado HLS for each task and then create the top-level RTL code by itself.
TAPA generalizes this approach and makes it available to all task-parallel programs.

### Memory System

Vitis HLS provides DRAM access via AXI4 memory-mapped (`m_axi`) interfaces.
However, in a `#pragma HLS dataflow` regions, task instances may not access the same `m_axi` interface.

TAPA provides `tapa::mmap` that has the same functionality as Vitis HLS's `m_axi` interfaces.
TAPA additionally provides asynchronous accesses to memory-mapped arguments, via separate address/data channels exposed as `tapa::i/ostream`.
This makes it possible to achieve a high random access throughput.

TAPA also makes it possible to access the same memory-mapped interface from multiple task instances.
This is implemented by instantiating AXI interconnect where an AXI interface is shared.
Note that *with great power comes great responsibility*.
The AXI interconnect do not provide built-in locking mechanisms.
Programmers must take care of correctness with concurrent reads and writes.

### Detached Tasks

Controlling tasks as OpenCL kernels can be expensive, yet often times each task instance is small and do not need runtime management.
Intel FPGA SDK for OpenCL supports “autorun” kernels, which are started as soon as the accelerator starts and is restarted when it stops.
However, [they cannot have any arguments other than the global communication channels](https://www.intel.com/content/www/us/en/programmable/documentation/mwh1391807965224.html#ewa1456413600674).
Similarly, Xilinx Vitis supports [“free-running” OpenCL kernels](https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/streamingconnections.html#ariaid-title5).
While the state of these kernels do not have to be maintained at runtime and thus resources are saved, the degree of freedom is also greatly limited.
`#pragma HLS dataflow` do not support “autorun” or “free-running” instances, therefore all task instances must be properly terminated.
This can lead to unnecessary complexity to propagate termination signals.

TAPA makes it possible to “detach” a task instance once instantiated.
By default, a TAPA task is not considered finished until all its children instances finish.
Detach task instances resembles the concept of `std::thread::detach()`, and the parent task will not wait for it to finish.
This makes it possible to do necessary initialization before entering an infinite loop in detached tasks, thus having both resource efficiency and programming flexibility.

### Hardware FIFO

Vitis HLS can generate two different implementations for the communication channels, using shift-registers (SLR) and block RAMs (BRAM).
However, it seems to unnecessarily prefer BRAM in many cases (e.g., when the FIFO is wide but shallow).
TAPA uses a larger threshold that reduces BRAM usage.
Moreover, when URAM is more efficient (width ≥ 36 and depth ≥ 4096), it is used in place of BRAM.

### Timing Closure

TAPA natively supports [AutoBridge][autobridge].
By specifying a filename for the output TCL constraints, TAPA can generate coarse-grained floorplans that were often necessary to achieve good timing closure.

## Related Publications

+ Yuze Chi, Licheng Guo, Jason Lau, Young-kyu Choi, Jie Wang, Jason Cong.
  [Extending High-Level Synthesis for Task-Parallel Programs](https://doi.org/10.1109/fccm51124.2021.00032).
  In FCCM, 2021.
  [[PDF]](https://about.blaok.me/pub/fccm21-tapa.pdf)
  [[Code]][tapa]
  [[Slides]](https://about.blaok.me/pub/fccm21-tapa.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fccm21-tapa.mp4)
+ Licheng Guo, Yuze Chi, Jie Wang, Jason Lau, Weikang Qiao, Ecenur Ustun, Zhiru Zhang, Jason Cong.
  [AutoBridge: Coupling Coarse-Grained Floorplanning and Pipelining for High-Frequency HLS Design on Multi-Die FPGAs](https://doi.org/10.1145/3431920.3439289).
  In FPGA, 2021. (Best Paper Award)
  [[PDF]](https://about.blaok.me/pub/fpga21-autobridge.pdf)
  [[Code]][autobridge]
  [[Slides]](https://about.blaok.me/pub/fpga21-autobridge.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fpga21-autobridge.mp4)

## Getting Started

+ [Installation](docs/installation.rst)
+ [Tutorial](docs/tutorial.rst)
+ [API Reference](docs/api.rst)

## Known Issues

+ Template functions cannot be tasks
+ Vitis HLS include paths (e.g., `/opt/Xilinx/Vitis_HLS/2020.2/include`) must not be specified in `tapac --cflags`;
  + Workaround is to use `CPATH` environment variable (e.g., `export CPATH=/opt/Xilinx/Vitis_HLS/2020.2/include`)

[tapa]: https://github.com/UCLA-VAST/tapa/
[autobridge]: https://github.com/Licheng-Guo/AutoBridge/
[aws-f1]: https://aws.amazon.com/ec2/instance-types/f1/
[d5005]: https://www.intel.com/content/www/us/en/programmable/products/boards_and_kits/dev-kits/altera/intel-fpga-pac-d5005/overview.html
[u250]: https://www.xilinx.com/products/boards-and-kits/alveo/u250.html
[iocl]: https://www.intel.com/content/www/us/en/software/programmable/sdk-for-opencl/overview.html
[xhls]: https://www.xilinx.com/products/design-tools/vitis.html
[soda]: https://github.com/UCLA-VAST/soda/
[autosa]: https://github.com/UCLA-VAST/AutoSA/
