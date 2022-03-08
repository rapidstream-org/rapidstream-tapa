#!/bin/sh
set -e

sudo git clone \
  https://github.com/Xilinx/HLS_arbitrary_Precision_Types.git \
  /usr/local/src/ap_int
sudo ln -sf /usr/local/src/ap_int/include/* /usr/local/include/
