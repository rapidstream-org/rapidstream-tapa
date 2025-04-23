#!/bin/bash
# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.
set -ex
work_dir="${0%/*}/../../../work.out"
platform="xilinx_u250_gen3x16_xdma_4_1_202210_1"

tapa="$1"
src="$2"

"${tapa}" --work-dir "${work_dir}" \
    analyze \
    --input "${src}" --top ModuleScalability \
    synth --platform "${platform}" \

py-spy \
    record -o "${work_dir}"/profile.svg \
    --idle \
    -- \
    python3 "${tapa}" --work-dir "${work_dir}" \
    synth --platform "${platform}" \
    --skip-hls-based-on-mtime \
