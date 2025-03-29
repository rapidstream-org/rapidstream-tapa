<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

<img src="https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128" width="64px" alt="RapidStream Logo" />


# RapidStream TAPA

[![Build and Publish Stable Version](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/publish-stable.yml/badge.svg?branch=main)](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/publish-stable.yml)
[![Staging Build](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/staging-build.yml/badge.svg)](https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/staging-build.yml)
[![Documentation](https://readthedocs.org/projects/tapa/badge/?version=latest)](https://tapa.readthedocs.io/en/latest/?badge=latest)

RapidStream TAPA is a powerful framework for designing high-frequency FPGA
dataflow accelerators. It combines a **powerful C++ API** for expressing
task-parallel designs with **advanced optimization techniques from
[RapidStream](https://rapidstream-da.com/)** to deliver exceptional design
performance and productivity.

- **High-Frequency Performance**: Achieve 2× higher frequency on average
  compared to Vivado[<sup>1</sup>](https://doi.org/10.1145/3431920.3439289).
- **Rapid Development**: 7× faster compilation and 3× faster software
  simulation than Vitis HLS[<sup>2</sup>](https://doi.org/10.1109/fccm51124.2021.00032).
- **Expressive API**: Rich C++ syntax with dedicated APIs for complex memory
  access patterns and explicit parallelism.
- **HBM Optimizations**: Automated design space exploration and physical
  optimizations for HBM FPGAs.


## Quick Start

### Prerequisites

- Ubuntu 18.04+, Debian 10+, RHEL 9+, Fedora 34+, or Amazon Linux 2023
- Vitis HLS 2022.1 or later

### Installation

```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt-get install g++ ocl-icd-libopencl1

# Install RapidStream TAPA
# You may rerun this command to update to the latest version.
sh -c "$(curl -fsSL tapa.rapidstream.sh)"

# Optional: Install RapidStream
sh -c "$(curl -fsSL rapidstream.sh)"
```

### Compilation

```bash
git clone https://github.com/rapidstream-org/rapidstream-tapa
cd rapidstream-tapa/tests/apps/vadd

# Software simulation
tapa g++ vadd.cpp vadd-host.cpp -o vadd
./vadd

# Hardware compilation and emulation
tapa compile \
   --top VecAdd \
   --part-num xcu250-figd2104-2L-e \
   --clock-period 3.33 \
   -f vadd.cpp \
   -o vecadd.xo
./vadd --bitstream=vecadd.xo 1000
```

### Optimize with RapidStream

```bash
rapidstream-tapaopt \
  --work-dir ./build \
  --tapa-xo-path vecadd.xo \
  --device-config u55c_device.json \
  --floorplan-config floorplan_config.json
```

For detailed instructions, see our [User Guide](https://tapa.readthedocs.io/en/main/).


## Documentation

- [Quick Reference](https://tapa.readthedocs.io/en/main/user/cheatsheet.html).
- [User Guide](https://tapa.readthedocs.io/en/main/).
- [Installation Guide](https://tapa.readthedocs.io/en/main/user/installation.html).
- [Getting Started](https://tapa.readthedocs.io/en/main/user/getting_started.html).
- [API Reference](https://tapa.readthedocs.io/en/main/api.html).


## Success Stories

- [Serpens](https://dl.acm.org/doi/10.1145/3489517.3530420) (DAC'22): 270 MHz
  on Xilinx Alveo U280 HBM board with 24 HBM channels, while the Vitis HLS
  baseline failed in routing.
- [Sextans](https://dl.acm.org/doi/pdf/10.1145/3490422.3502357) (FPGA'22):
  260 MHz on Xilinx Alveo U250 board with 4 DDR channels, while the Vivado
  baseline achieves only 189 MHz.
- [SPLAG](https://github.com/UCLA-VAST/splag) (FPGA'22): Up to 4.9× speedup
  over state-of-the-art FPGA accelerators, up to 2.6× speedup over 32-thread
  CPU running at 4.4 GHz, and up to 0.9× speedup over an A100 GPU.
- [AutoSA Systolic-Array Compiler](https://github.com/UCLA-VAST/AutoSA)
  (FPGA'21): Significant frequency improvements over the Vitis HLS baseline.
- [KNN](https://github.com/SFU-HiAccel/CHIP-KNN) (FPT'20): 252 MHz on Xilinx
  Alveo U280 board, compared to 165 MHz with the Vivado baseline.


## Licensing

RapidStream TAPA is open-source software licensed under the MIT license.
For full license details, please refer to the
[LICENSE](https://github.com/rapidstream-org/rapidstream-tapa/blob/main/LICENSE)
file. By contributing to this project, you agree to the RapidStream Contributor
License Agreement. For more information, please read the full
[CLA](https://github.com/rapidstream-org/rapidstream-tapa/blob/main/CLA.md).


## Publications

1. Licheng Guo, Yuze Chi, Jie Wang, Jason Lau, Weikang Qiao, Ecenur Ustun, Zhiru Zhang, Jason Cong.
   [AutoBridge: Coupling coarse-grained floorplanning and pipelining for high-frequency HLS design on multi-die FPGAs](https://doi.org/10.1145/3431920.3439289).
   FPGA, 2021. (Best Paper Award)
2. Yuze Chi, Licheng Guo, Jason Lau, Young-kyu Choi, Jie Wang, Jason Cong.
   [Extending high-level synthesis for task-Parallel programs](https://doi.org/10.1109/fccm51124.2021.00032).
   FCCM, 2021.
3. Young-kyu Choi, Yuze Chi, Jason Lau, Jason Cong.
   [TARO: Automatic optimization for free-running kernels in FPGA high-level synthesis](https://doi.org/10.1109/TCAD.2022.3216544).
   TCAD, 2022.
4. Licheng Guo, Pongstorn Maidee, Yun Zhou, Chris Lavin, Eddie Hung, Wuxi Li, Jason Lau, Weikang Qiao, Yuze Chi, Linghao Song, Yuanlong Xiao, Alireza Kaviani, Zhiru Zhang, Jason Cong.
   [RapidStream 2.0: Automated parallel implementation of latency insensitive FPGA designs through partial reconfiguration](https://doi.org/10.1145/3593025).
   TRETS, 2023.
5. Licheng Guo, Yuze Chi, Jason Lau, Linghao Song, Xingyu Tian, Moazin Khatti, Weikang Qiao, Jie Wang, Ecenur Ustun, Zhenman Fang, Zhiru Zhang, Jason Cong.
   [TAPA: A scalable task-parallel dataflow programming framework for modern FPGAs with co-optimization of HLS and physical design](https://doi.org/10.1145/3609335).
   TRETS, 2023.


---

Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.<br/>
Copyright (c) 2020 Yuze Chi and contributors.<br/>
