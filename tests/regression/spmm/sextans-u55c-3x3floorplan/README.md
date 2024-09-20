<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

<img src="https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128" width="64px" alt="RapidStream Logo" />

# Demo: Sextans, general-purpose Sparse-Matrix Dense-Matrix Multiplication

## Source

This design source is collected from [Sextans](https://github.com/linghaosong/Sextans).

## How to Run

To reproduce the result, please simply run:
```
cd rapidstream
bash run_tapaopt.sh
```

To run from the very beginning,
1. the [Sextans1613.xo](./rapidstream/Sextans1613.xo) is generated from [./tapa](./tapa/).
   * Please check the [README](./tapa/README.md) to understand the update in [run_tapa.sh](./tapa/run_tapa.sh).
   * run `bash run_tapa.sh` to generate the Vitis XO file.
2. Make sure your `--tapa-xo-path` in [run_tapaopt.sh](./rapidstream/run_tapaopt.sh) is set to the newly generated Vitis XO file (if applicable).
3. Generate the device configuration json [u55c_device.json](./rapidstream/u55c_device.json) by running [gen_device.py](./rapidstream/gen_device.py) with RapidStream.
4. run `bash run_tapaopt.sh`.
