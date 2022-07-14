#!/bin/bash
# Requires iverilog and ifne.

set -e

cd "${0%/*}"/../..

which iverilog
find \( \
  -path './regression' -or \
  -path '*/build' \
  \) -prune -or \( \
  -iname '*.v' -or -iname '*.sv' \
  \) -execdir iverilog -y. -tnull -Wall '{}' ';' |&
  tee /dev/stderr |
  ifne false
