#!/bin/bash
# Requires: clang-format, yapf, isort

set -e

cd "${0%/*}"

which clang-format
find \( \
  -path './backend/clang' -or \
  -path './backend/python/tapa/assets/clang' -or \
  -path './regression' -or \
  -path '*/build/*' \
  \) -prune -or \( \
  -iname '*.h' -or -iname '*.cpp' \
  \) -print0 | xargs --null clang-format -i --verbose

tmp="$(mktemp --suffix=tapa-formatter)"
trap 'rm -f "${tmp}"' EXIT

find \( \
  -path './backend/python/tapa/verilog/axi_xbar.py' -or \
  -path './regression' -or \
  -path '*/build/*' \
  \) -prune -or \( \
  -iname '*.py' \
  \) -print0 >"${tmp}"
which yapf
xargs --null yapf --in-place --verbose <"${tmp}"
which isort
xargs --null isort <"${tmp}"
