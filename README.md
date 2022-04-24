# TAPA

[![CI](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml/badge.svg)](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml)
[![install](https://github.com/Blaok/tapa/actions/workflows/install.yml/badge.svg)](https://github.com/Blaok/tapa/actions/workflows/install.yml)
[![Documentation Status](https://readthedocs.org/projects/tapa/badge/?version=latest)](https://tapa.readthedocs.io/en/latest/?badge=latest)

TAPA is a high-performance fast-compiling HLS framework that is fully compatible with the Xilinx Vitis/Vivado workflow.

<img width="931" alt="image" src="https://user-images.githubusercontent.com/32432619/157972074-12fe5f32-4cd0-492e-b47a-06c23ea9c283.png">

- TAPA takes in a task-parallel dataflow program in C++ written in Vitis HLS syntax and additional TAPA APIs.
  - Compared to Vitis HLS, TAPA supports **more flexible memory access patterns** and **runtime burst detection**.
- TAPA extracts parallel components of the program and invokes Vitis HLS to compile each component simultaneously.
  - TAPA compiles **7X** faster than Vitis HLS. In some cases, TAPA reduces a **10-hour** Vitis compilation to **10-min**.
- TAPA invokes the [AutoBridge](https://github.com/Licheng-Guo/AutoBridge) floorplanner to floorplan the dataflow program and pipeline the global data links accordingly.
  - With AutoBridge, TAPA achieves **2X** higher the frequency on average compared to Vivado.
- TAPA supports a [customized fast C-RTL simulation plug-in](https://github.com/Licheng-Guo/tapa-fast-cosim).
  - **10X** faster than Vitis to setup the simulation.
- TAPA generates an xo object that is fully compatible as the input to the Vitis v++ compiler for bitstream generation.
  - [in-progress] TAPA is integrating the parallel physical implementation tool [RapidStream](https://github.com/Licheng-Guo/RapidStream).

## Successful Cases

- [Serpens](https://arxiv.org/abs/2111.12555), to appear in DAC'22, achieves 270 MHz on the Xilinx Alveo U280 HBM board when using 24 HBM channels. The Vivado baseline failed in routing.
- [Sextans](https://dl.acm.org/doi/pdf/10.1145/3490422.3502357), FPGA'22, achieves 260 MHz on the Xilinx Alveo U250 board when using 4 DDR channels. The Vivado baseline achieves only 189 MHz.

- [AutoSA Systolic-Array Compiler](https://github.com/UCLA-VAST/AutoSA), FPGA'21:
<img width="668" alt="image" src="https://user-images.githubusercontent.com/32432619/157976148-594e98bc-2658-4ebc-ae0d-3d2a347d1854.png">

- [KNN](https://github.com/SFU-HiAccel/CHIP-KNN), FPT'20, achieves 252 MHz on the Xilinx Alveo U280 board. The Vivado baseline achieves only 165 MHz.

## Getting Started

+ [Installation](https://tapa.readthedocs.io/en/latest/installation.html)
+ [Tutorial](https://tapa.readthedocs.io/en/latest/tutorial.html)
+ [API Reference](https://tapa.readthedocs.io/en/latest/api.html)

## Related Publications

+ Yuze Chi, Licheng Guo, Jason Lau, Young-kyu Choi, Jie Wang, Jason Cong.
  [Extending High-Level Synthesis for Task-Parallel Programs](https://doi.org/10.1109/fccm51124.2021.00032).
  In FCCM, 2021.
  [[PDF]](https://about.blaok.me/pub/fccm21-tapa.pdf)
  [[Code]](https://github.com/UCLA-VAST/tapa)
  [[Slides]](https://about.blaok.me/pub/fccm21-tapa.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fccm21-tapa.mp4)
+ Licheng Guo, Yuze Chi, Jie Wang, Jason Lau, Weikang Qiao, Ecenur Ustun, Zhiru Zhang, Jason Cong.
  [AutoBridge: Coupling Coarse-Grained Floorplanning and Pipelining for High-Frequency HLS Design on Multi-Die FPGAs](https://doi.org/10.1145/3431920.3439289).
  In FPGA, 2021. (Best Paper Award)
  [[PDF]](https://about.blaok.me/pub/fpga21-autobridge.pdf)
  [[Code]](https://github.com/Licheng-Guo/AutoBridge)
  [[Slides]](https://about.blaok.me/pub/fpga21-autobridge.slides.pdf)
  [[Video]](https://about.blaok.me/pub/fpga21-autobridge.mp4)
