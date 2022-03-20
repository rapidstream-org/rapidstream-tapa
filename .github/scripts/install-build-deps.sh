#!/bin/sh
set -e

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libboost-coroutine-dev \
  libboost-stacktrace-dev \
  libgflags-dev \
  libgoogle-glog-dev \
  python3-pip

sudo apt-get autoremove -y
sudo -H python3 -m pip install --upgrade pip==20.3.4
sudo -H python3 -m pip install cmake
wget -O - https://raw.githubusercontent.com/Blaok/fpga-runtime/master/install.sh | bash
