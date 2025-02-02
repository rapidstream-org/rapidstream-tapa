#!/bin/bash

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Tests that TAPA fast cosim can resume from generated work directory.

set -ex

"$@" --xosim_work_dir="${TEST_TMPDIR}" --xosim_setup_only
vivado -mode batch -source "${TEST_TMPDIR}"/output/run/run_cosim.tcl
"$@" --xosim_work_dir="${TEST_TMPDIR}" --xosim_resume_from_post_sim
