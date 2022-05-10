# TAPA

[![CI](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml/badge.svg)](https://github.com/UCLA-VAST/tapa/actions/workflows/CI.yml)
[![install](https://github.com/Blaok/tapa/actions/workflows/install.yml/badge.svg)](https://github.com/Blaok/tapa/actions/workflows/install.yml)
[![Documentation Status](https://readthedocs.org/projects/tapa/badge/?version=latest)](https://tapa.readthedocs.io/en/latest/?badge=latest)

TAPA is a dataflow HLS framework that features fast compilation, expressive programming model and generates high-frequency FPGA accelerators.

![TAPA Framework](https://user-images.githubusercontent.com/32432619/157972074-12fe5f32-4cd0-492e-b47a-06c23ea9c283.png)


## High-Frequency

- TAPA explicitly decouples communication and computation for better QoR.

- TAPA integrates the [AutoBridge](https://github.com/Licheng-Guo/AutoBridge) floorplanner to optimize the RTL generation process.

- TAPA achieves **2×** higher the frequency on average compared to Vivado. [<sup>1</sup>](https://doi.org/10.1145/3431920.3439289)


## Speed

- TAPA compiles **7×** faster than Vitis HLS. [<sup>2</sup>](https://doi.org/10.1109/fccm51124.2021.00032)

- TAPA provides **3×** faster software simulation than Vitis HLS.[<sup>2</sup>](https://doi.org/10.1109/fccm51124.2021.00032)

- TAPA provides **8×** faster RTL simulation than Vitis.

- [in-progress] TAPA is integrating [RapidStream](https://github.com/Licheng-Guo/RapidStream) that is up to **10×** faster than Vivado.[<sup>3</sup>](https://vast.cs.ucla.edu/~chiyuze/pub/fpga22-rapidstream.pdf)


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
