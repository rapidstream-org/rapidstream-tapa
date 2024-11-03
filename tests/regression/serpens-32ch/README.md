<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

<img src="https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128" width="64px" alt="RapidStream Logo" />

# Demo: Serpens, general-purpose sparse matrix-vector multiplication

## Feature

32 HBM channels, tailored to 256-bit each + 2 512-bit DDR controllers. All memory ports used.

## Source

This design source is collected from [Serpens](https://github.com/UCLA-VAST/Serpens).

## How to Run the Regression

To reproduce the result, please simply run:
```
cd rapidstream
bash run_tapaopt.sh
```

To run from the very beginning,
1. the [serpens32_256.xo](./rapidstream/serpens32_256.xo) is generated from [./tapa](./tapa/).
   * run `bash run_tapa.sh` to generate the Vitis XO file.
2. Make sure your `--tapa-xo-path` in [run_tapaopt.sh](./rapidstream/run_tapaopt.sh) is set to the newly generated Vitis XO file (if applicable).
3. Generate the device configuration json [u280_device.json](./rapidstream/u280_device.json) by running [gen_device.py](./rapidstream/gen_device.py) with RapidStream.
4. run `bash run_tapaopt.sh`.
