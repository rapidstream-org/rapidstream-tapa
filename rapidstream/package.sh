#!/bin/bash

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

set -e

function patch {
  pushd "$1"
  git clean --force -- . :^rapidstream/
  git checkout --force HEAD -- . :^rapidstream/
  sed -i -e's/"tapa"/"rpd."/g' \
    fpga-runtime/src/frt/devices/shared_memory_queue.cpp
  local old_file
  for old_file in $(git ls-tree -r --name-only HEAD); do
    [[ "${old_file}" == rapidstream/* ]] && continue
    sed -i \
      -e"s/tapa/rapid/g" \
      -e"s/Tapa/Rapid/g" \
      -e"s/TAPA/RAPID/g" \
      "${old_file}"
    if [[ "${old_file}" == src/*.h ]]; then
      g++ -fpreprocessed -dD -E -P "${old_file}" | sponge "${old_file}"
    fi
    local new_file="${old_file}"
    new_file="${new_file//tapa/rapid}"
    new_file="${new_file//TAPA/RAPID}"
    new_file="${new_file//Tapa/Rapid}"
    [[ "${new_file}" == "${old_file}" ]] && continue
    mkdir --parents "$(dirname "${new_file}")"
    mv --force --no-target-directory "${old_file}" "${new_file}"
    rmdir --ignore-fail-on-non-empty --parents "$(dirname "${old_file}")"
  done
  popd
}

function build_binary {
  pushd "$1"
  act -j build
  popd
}

function main {
  patch "${0%/*}"/..
  build_binary "${0%/*}"/..
}

main "$@"
