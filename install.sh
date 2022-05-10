#!/bin/bash
set -e

function install-tapa-for-ubuntu() {
  local codename="$1"

  if ! which sudo >/dev/null; then
    apt-get update
    apt-get install -y sudo
  fi

  sudo apt-get update
  sudo apt-get install -y \
    apt-transport-https \
    ca-certificates \
    gnupg \
    python3 \
    python3-pip \
    wget

  wget -O- https://about.blaok.me/fpga-runtime/frt.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.frt.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.frt.gpg.tmp /usr/share/keyrings/frt.gpg
  sudo tee /etc/apt/sources.list.d/frt.list <<EOF
deb [arch=amd64 signed-by=/usr/share/keyrings/frt.gpg] https://about.blaok.me/fpga-runtime ${codename} main
EOF
  sudo apt-key del A3DB8B24636B33EE || true

  wget -O- https://about.blaok.me/tapa/tapa.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.tapa.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.tapa.gpg.tmp /usr/share/keyrings/tapa.gpg
  sudo tee /etc/apt/sources.list.d/tapa.list <<EOF
deb [arch=amd64 signed-by=/usr/share/keyrings/tapa.gpg] https://about.blaok.me/tapa ${codename} main
EOF
  sudo apt-key del 4F0F08E885049E30 || true

  sudo apt-get update
  sudo apt-get install -y xrt --no-install-recommends || true
  sudo apt-get install -y tapa
}

function install-tapa-for-centos() {
  local version="$1"

  if ! type sudo >/dev/null 2>/dev/null; then
    yum install -y sudo
  fi

  if ! yum list installed epel-release >/dev/null 2>/dev/null; then
    sudo yum install -y --setopt=skip_missing_names_on_install=False \
      "https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm"
  fi

  sudo yum install -y --setopt=skip_missing_names_on_install=False \
    "https://github.com/Blaok/fpga-runtime/releases/latest/download/frt-devel.centos.${version}.x86_64.rpm" \
    "https://github.com/Blaok/tapa/releases/latest/download/tapa.centos.${version}.x86_64.rpm" \
    python3-pip
}

source /etc/os-release

case "${ID}.${VERSION_ID}" in
ubuntu.18.04 | ubuntu.20.04)
  install-tapa-for-ubuntu "${UBUNTU_CODENAME}"
  ;;
centos.7)
  install-tapa-for-centos "${VERSION_ID}"
  ;;
*)
  echo "unsupported os" >&2
  exit 1
  ;;
esac

python3 -m pip install --upgrade --user setuptools wheel
python3 -m pip install --upgrade --user tapa

# check if PATH is correctly set
if ! which tapac >/dev/null; then
  echo "*** Please add \"~/.local/bin\" to PATH ***" >&2
fi
