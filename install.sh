#!/bin/bash
set -e

if ! which sudo >/dev/null; then
  apt update
  apt install -y sudo
fi

sudo apt update
sudo apt install -y gnupg wget

wget -O - https://about.blaok.me/tlp/tlp.gpg.key | sudo apt-key add -
wget -O - https://about.blaok.me/fpga-runtime/frt.gpg.key | sudo apt-key add -

codename="$(grep --perl --only '(?<=UBUNTU_CODENAME=).+' /etc/os-release)"

# get pip
if test "${codename}" = "xenial"; then
  sudo apt update
  sudo apt install software-properties-common
  sudo add-apt-repository ppa:deadsnakes/ppa
  sudo apt update
  sudo apt install -y python3.6 python3-pip
  pip="python3.6 -m pip"
else
  sudo apt update
  sudo apt install -y python3 python3-pip
  pip="python3 -m pip"
fi

sudo tee /etc/apt/sources.list.d/tlp.list <<EOF
deb [arch=amd64] https://about.blaok.me/tlp ${codename} main
EOF
sudo tee /etc/apt/sources.list.d/frt.list <<EOF
deb [arch=amd64] https://about.blaok.me/fpga-runtime ${codename} main
EOF

sudo apt update
sudo apt install -y hlstlp
${pip} install --upgrade setuptools
${pip} install --user --upgrade tlpc
