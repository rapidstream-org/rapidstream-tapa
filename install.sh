#!/bin/bash
set -e

function install-tapa-ubuntu() {
  local -r codename="$1"
  local -r clang_version="$2"

  if ! which sudo >/dev/null; then
    DEBIAN_FRONTEND=noninteractive apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y sudo
  fi

  sudo DEBIAN_FRONTEND=noninteractive apt-get update
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    apt-transport-https \
    ca-certificates \
    gnupg \
    python3 \
    python3-pip \
    wget

  wget -O- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.llvm.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.llvm.gpg.tmp /usr/share/keyrings/llvm.gpg
  sudo tee /etc/apt/sources.list.d/llvm-${clang_version}.list <<EOF
deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/${codename}/ llvm-toolchain-${codename}-${clang_version} main
EOF

  wget -O- https://about.blaok.me/fpga-runtime/frt.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.frt.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.frt.gpg.tmp /usr/share/keyrings/frt.gpg
  sudo tee /etc/apt/sources.list.d/frt.list <<EOF
deb [arch=amd64 signed-by=/usr/share/keyrings/frt.gpg] https://about.blaok.me/fpga-runtime ${codename} main
EOF
  sudo apt-key del A3DB8B24636B33EE || true

  if [[ -z "${TAPA_DEB}" ]]; then
    wget -O- https://about.blaok.me/tapa/tapa.gpg.key | gpg --dearmor |
      sudo tee /usr/share/keyrings/.tapa.gpg.tmp >/dev/null
    sudo mv /usr/share/keyrings/.tapa.gpg.tmp /usr/share/keyrings/tapa.gpg
    sudo tee /etc/apt/sources.list.d/tapa.list <<EOF
deb [arch=amd64 signed-by=/usr/share/keyrings/tapa.gpg] https://about.blaok.me/tapa ${codename} main
EOF
    sudo apt-key del 4F0F08E885049E30 || true
  fi

  sudo DEBIAN_FRONTEND=noninteractive apt-get update
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y xrt --no-install-recommends || true
  if [[ -z "${TAPA_DEB}" ]]; then
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y tapa
  else
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -f "${TAPA_DEB}"
  fi
}

function install-tapa-python() {
  python3 -m pip install --upgrade --user setuptools wheel
  if [[ -z "${TAPA_WHL}" ]]; then
    python3 -m pip install --upgrade --user tapa
  else
    python3 -m pip install --upgrade --user "${TAPA_WHL}"
  fi

  # check if PATH is correctly set
  if ! which tapac >/dev/null; then
    echo "*** Please add \"~/.local/bin\" to PATH ***" >&2
  fi
}

source /etc/os-release

case "${ID}.${VERSION_ID}" in
ubuntu.20.04 | ubuntu.22.04)
  install-tapa-ubuntu "${UBUNTU_CODENAME}" 17
  ;;
*)
  echo "unsupported os" >&2
  exit 1
  ;;
esac

install-tapa-python
