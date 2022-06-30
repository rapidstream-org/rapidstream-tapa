#!/bin/bash
# Requires: clang-format

set -e

cd "${0%/*}"

find \( \
  -path './backend/clang' -or \
  -path './backend/python/tapa/assets/clang' -or \
  -path './regression' -or \
  -path '*/build/*' \
  \) -prune -or \( \
  -iname '*.h' -or -iname '*.cpp' \
  \) -print0 | xargs --null clang-format -i --verbose
