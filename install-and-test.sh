#!/bin/bash
set -e
base_dir="$(realpath "$(dirname "$0")")"

"${base_dir}/install.sh"
pip install --user --upgrade cmake
sudo apt install -y moreutils xilinx-u250-xdma-dev

parallel=$(which parallel.moreutils || true)
if test -z "${parallel}"; then
  parallel=$(which parallel)
fi

# configure sequentially, build in parallel
for app in bandwidth cannon graph jacobi vadd; do
  build_dir="/tmp/build-${app}"
  cmake -S "${base_dir}/apps/${app}" -B ${build_dir}
  cmd="${cmd}'cmake --build ${build_dir} --target ${app}-cosim' "
done

eval exec "${parallel}" -- ${cmd}
