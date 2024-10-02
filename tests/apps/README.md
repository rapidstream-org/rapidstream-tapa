<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

## General

This directory contains multiple small example TAPA designs. We provide a reference `.ini` file for each design that includes the binding to physical HBM/DDR ports.

For large and complex designs, refer to the `tests/regression` directory.


## Running TAPA application with rapidstream optimization
In each app, an `run_rs.sh` file is provided to demonstrate the workflow to csynth TAPA
applications to XO files, optimize the design with RapidStream, and cosimulate the
design with TAPA.

The example script of generating rapidstream configuration files can be found
at `rapidstream-tapa/tests/rs_templetes/gen_config.py`.

The example of script of TAPA and rapidstream commands can be found at `/home/Ed-5100/rapidstream-tapa/tests/rs_templetes/run_rs.sh`

To run the examples, install both rapidstream and TAPA, and follow the instruction below.
```bash
cd rapidstream-tapa/tests/apps/vadd
source ./run_rs.sh
```

The steps of installing TAPA and rapidstream can be found at `https://tapa.readthedocs.io/en/main/user/installation.html`
