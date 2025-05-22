#!/bin/bash

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Tests if the given XO file has the expected pragmas.

set -e

function expect-pragma() {
  local -r pragma="$1"
  [[ "$2" = in ]]
  local -r file="$3"
  if grep -q "^[[:blank:]]*// pragma ${pragma}[[:blank:]]*$" "${file}"; then
    return 0
  else
    cat "${file}"
    return 1
  fi
}

declare -r src_dir="xo/ip_repo/tapa_xrtl_VecAddShared_1_0/src"

rm -rf xo/
unzip -q "$1" -d xo/

echo "All pragmas:"
grep '// pragma' "${src_dir}" -RH

expect-pragma 'RS clk port=ap_clk' in "${src_dir}"/Load_fsm.v
expect-pragma 'RS rst port=ap_rst_n active=low' in "${src_dir}"/Load_fsm.v
expect-pragma 'RS ap-ctrl ap_start=ap_start ap_done=ap_done ap_idle=ap_idle ap_ready=ap_ready scalar=(srcs_offset|n)' in "${src_dir}"/Load_fsm.v
expect-pragma 'RS ap-ctrl ap_start=Mmap2Stream_0__ap_start ap_done=Mmap2Stream_0__ap_done ap_idle=Mmap2Stream_0__ap_idle ap_ready=Mmap2Stream_0__ap_ready scalar=Mmap2Stream_0___\.\*' in "${src_dir}"/Load_fsm.v
