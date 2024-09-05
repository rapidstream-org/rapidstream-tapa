<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

<img src="https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128" width="64px" alt="RapidStream Logo" />

# RapidStream TAPA

[![Development Branches](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/dev-branches.yml/badge.svg)](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/dev-branches.yml)
[![Staging Build](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/staging-build.yml/badge.svg)](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/staging-build.yml)
[![Documentation](https://readthedocs.org/projects/tapa/badge/?version=latest)](https://tapa.readthedocs.io/en/latest/?badge=latest)

TAPA is a dataflow HLS framework that features fast compilation, expressive programming model and generates high-frequency FPGA accelerators.
Now it is maintained by RapidStream Design Automation, Inc.

![TAPA Framework](https://user-images.githubusercontent.com/32432619/157972074-12fe5f32-4cd0-492e-b47a-06c23ea9c283.png)


## Installation

We are planning something big for the next release! For now, the documentation may be out-dated.
You may install our latest preview version by following the instructions below.

To install TAPA, you need to have the following dependencies:

- Ubuntu 18.04 or later, or Debian 10 or later
- g++
- iverilog
- ocl-icd-libopencl1 (if you plan to run simulations and on-board tests)

You can install the dependencies by running the following command:

```bash
sudo apt-get install g++ iverilog ocl-icd-libopencl1
```

Then, you can install TAPA as a regular user by running the following command:

```bash
sh -c "$(curl -fsSL tapa.rapidstream.sh)"
```

To compile the host code for simulation and on-board tests, you may compile your host code linking with the TAPA library:

```bash
g++ -std=c++17 \
    [your code here] \
    -I ~/.rapidstream-tapa/usr/include \
    -I[path to Vitis HLS header files] \
    -Wl,-rpath,$(readlink -f ~/.rapidstream-tapa/usr/lib) \
    -L ~/.rapidstream-tapa/usr/lib \
    -ltapa -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs
```

For example:

```bash
g++ -std=c++17 \
    tests/apps/vadd/vadd-host.cpp tests/apps/vadd/vadd.cpp \
    -I ~/.rapidstream-tapa/usr/include \
    -I /opt/tools/xilinx/Vitis_HLS/2024.1/include \
    -Wl,-rpath,$(readlink -f ~/.rapidstream-tapa/usr/lib) \
    -L ~/.rapidstream-tapa/usr/lib \
    -ltapa -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs
```

For generating the RTL code, you can use the `tapa` command. For example:

```bash
# source the Vitis HLS settings
source /opt/tools/xilinx/Vitis_HLS/2024.1/settings64.sh
tapa compile \
    -f tests/apps/bandwidth/bandwidth.cpp \
    --cflags -Itests/apps/bandwidth/ \
    -t Bandwidth \
    --clock-period 3 \
    --part-num xcu250-figd2104-2L-e
```

We are working on the documentation and the stable release. Stay tuned!

## High-Frequency

- TAPA explicitly decouples communication and computation for better QoR.
- TAPA integrates the [RapidStream Composer](https://rapidstream-da.com/) to optimize the RTL generation process.
- TAPA achieves **2×** higher the frequency on average compared to Vivado. [<sup>1</sup>](https://doi.org/10.1145/3431920.3439289)

## Speed

- TAPA compiles **7×** faster than Vitis HLS. [<sup>2</sup>](https://doi.org/10.1109/fccm51124.2021.00032)
- TAPA provides **3×** faster software simulation than Vitis HLS.[<sup>2</sup>](https://doi.org/10.1109/fccm51124.2021.00032)
- TAPA provides **8×** faster RTL simulation than Vitis.
- TAPA is integrating [RapidStream Composer](https://rapidstream-da.com/) that
  is up to **10×** faster than Vivado.

## Expressiveness

- TAPA extends the Vitis HLS syntax for richer expressiveness at the C++ level.
- TAPA provides dedicated APIs for arbitrary external memory access patterns.
- TAPA allows users to explicitly specify parallelism.
- In addition to static burst analysis, TAPA supports runtime burst detectuion by transparently merging small memory transactions into large bursts.

## HBM-Specific Optimizations

- TAPA significantly reduce the area overhead of HBM interface IPs compared to Vitis HLS.
- TAPA includes an automated design space exploration tool to balance the resource pressure and the wire pressure for HBM FPGAs.
- TAPA automatically select the physical channel for each top-level argument of your accelerator.

## Successful Cases

- [Serpens](https://arxiv.org/abs/2111.12555), DAC'22, achieves 270 MHz on the Xilinx Alveo U280 HBM board when using 24 HBM channels. The Vitis HLS baseline failed in routing.
- [Sextans](https://dl.acm.org/doi/pdf/10.1145/3490422.3502357), FPGA'22, achieves 260 MHz on the Xilinx Alveo U250 board when using 4 DDR channels. The Vivado baseline achieves only 189 MHz.
- [SPLAG](https://github.com/UCLA-VAST/splag), FPGA'22,
  achieves up to a 4.9× speedup over state-of-the-art FPGA accelerators,
  up to a 2.6× speedup over 32-thread CPU running at 4.4 GHz,
  and up to a 0.9× speedup over an A100 GPU
  (that has 4.1× power budget and 3.4× HBM bandwidth).
- [AutoSA Systolic-Array Compiler](https://github.com/UCLA-VAST/AutoSA),
  FPGA'21:
  ![AutoSA Frequency Figure](https://user-images.githubusercontent.com/32432619/157976148-594e98bc-2658-4ebc-ae0d-3d2a347d1854.png)
- [KNN](https://github.com/SFU-HiAccel/CHIP-KNN), FPT'20, achieves 252 MHz on the Xilinx Alveo U280 board. The Vivado baseline achieves only 165 MHz.

## Getting Started

+ [Documentation](https://tapa.readthedocs.io/en/release/)
+ [Installation](https://tapa.readthedocs.io/en/release/installation.html)
+ [Hello World](https://tapa.readthedocs.io/en/release/getting_started.html)
+ [API Reference](https://tapa.readthedocs.io/en/release/api.html)

## TAPA Publications

+ Yuze Chi, Licheng Guo, Jason Lau, Young-kyu Choi, Jie Wang, Jason Cong.
  [Extending High-Level Synthesis for Task-Parallel Programs](https://doi.org/10.1109/fccm51124.2021.00032).
  In FCCM, 2021.
  [[PDF]](https://about.blaok.me/pub/fccm21-tapa.pdf)
  [[Code]](https://github.com/UCLA-VAST/tapa)
  [[Slides]](https://about.blaok.me/pub/fccm21-tapa.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fccm21-tapa.mp4)
+ Licheng Guo, Yuze Chi, Jie Wang, Jason Lau, Weikang Qiao, Ecenur Ustun, Zhiru Zhang, Jason Cong.
  [AutoBridge: Coupling Coarse-Grained Floorplanning and Pipelining for High-Frequency HLS Design on Multi-Die FPGAs](https://doi.org/10.1145/3431920.3439289).
  In FPGA, 2021. (**Best Paper Award**)
  [[PDF]](https://about.blaok.me/pub/fpga21-autobridge.pdf)
  [[Code]](https://github.com/Licheng-Guo/AutoBridge)
  [[Slides]](https://about.blaok.me/pub/fpga21-autobridge.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fpga21-autobridge.mp4)

## Licensing

RapidStream TAPA is an open source project managed by RapidStream Design
Automation, Inc., who is authorized by the contributors to license the software
under the MIT license.

## Contributor License Agreement (CLA)

By contributing to this open-source repository, you agree to the RapidStream
Contributor License Agreement.

Under this agreement, you grant to RapidStream Design Automation, Inc. and to
recipients of software distributed by RapidStream a perpetual, worldwide,
non-exclusive, no-charge, royalty-free, irrevocable copyright license to
reproduce, prepare derivative works of, publicly display, publicly perform,
sublicense, and distribute your contributions and such derivative works.
You also grant to RapidStream Design Automation, Inc. and to recipients of
software distributed by RapidStream a perpetual, worldwide, non-exclusive,
no-charge, royalty-free, irrevocable patent license to make, have made, use,
offer to sell, sell, import, and otherwise transfer the work,

Please note that this is a summary of the licensing terms, and the full text of
the [MIT license](https://github.com/rapidstream-org/rapidstream-tapa/blob/main/LICENSE)
and the [RapidStream Contributor License Agreement](https://github.com/rapidstream-org/rapidstream-tapa/blob/main/CLA.md)
should be consulted for detailed legal information.

---

Copyright (c) 2024 RapidStream Design Automation, Inc.
<br/> Copyright (c) 2020 Blaok.
<br/> All rights reserved.
