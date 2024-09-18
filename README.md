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

The RapidStream TAPA framework enables users to design high-frequency FPGA dataflow accelerators. The framework has two key components:

- The TAPA frontend provides API for expressing task-parallel accelerators in C++ and compiles
  the design into Verilog RTL.

- The RapidStream backend optimizes the generated RTL for high frequency and partitions the
  design for parallel placement and routing.

The framework originated from the [VAST Lab](https://vast.cs.ucla.edu/) at UCLA. Now it is maintained by RapidStream Design Automation, Inc.

![TAPA Framework](https://user-images.githubusercontent.com/32432619/157972074-12fe5f32-4cd0-492e-b47a-06c23ea9c283.png)


## Installation

The RapidStream-TAPA toolchain can be installed with a single command. After the installation completes successfully, restart your terminal session, or run ``source ~/.profile``.
You can upgrade your installation at any time by repeating the installation command.

To install TAPA, you need to have the following dependencies:

- Ubuntu 18.04 or later, or Debian 10 or later:
    - g++
    - iverilog
    - ocl-icd-libopencl1 (if you plan to run simulations and on-board tests)
    - unzip (for simulation using xo files)
- Red Hat Enterprise Linux 9 or later, AlmaLinux/Rocky Linux 9 or later, or Amazon Linux 2023:
    - gcc-c++
    - iverilog built from source
    - libxcrypt-compat
    - ocl-icd (if you plan to run simulations and on-board tests)
    - unzip (for simulation using xo files)
- Fedora 34 or later, up to Fedora 38:
    - gcc-c++
    - iverilog
    - libxcrypt-compat
    - ocl-icd (if you plan to run simulations and on-board tests)
    - unzip (for simulation using xo files)
- Vitis HLS 2022.1 or later

You can install the dependencies by running the following commands:

```bash
# for Ubuntu and Debian
sudo apt-get install g++ iverilog ocl-icd-libopencl1

# or for Fedora, RHEL, and Amazon Linux
# sudo yum install gcc-c++ iverilog libxcrypt-compat ocl-icd

# Install TAPA
sh -c "$(curl -fsSL tapa.rapidstream.sh)"

# Install RapidStream
sh -c "$(curl -fsSL rapidstream.sh)"
```

For more details, please refer to the [documentation](https://tapa.readthedocs.io/en/main/).


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
