<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

<img src="https://imagedelivery.net/AU8IzMTGgpVmEBfwPILIgw/1b565657-df33-41f9-f29e-0d539743e700/128" width="64px" alt="RapidStream Logo" />

# Floorplan 3x3 Runs

## Brief Intro

This configuration achieved ~280MHz in several solutions during runs. It mainly partitions the left half of U55C FPGA vertically into 2x3 grid, thus an overall 3x3 grid. This is for balancing the die-crossing utilization along the 16 horizontally distributed bundles -- corresponding to 16 laguna columns at each side of a die boundary.
