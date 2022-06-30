# TARO

TARO is an automated framework for applying free-running optimization on FPGA HLS applications. Free-running optimization simplifies the tasks' control logic by regulating tasks' operation with streams of input and output data.

- TARO stands for **T**APA **A**uto**R**un **O**ptimizer

- More details on free-running kernels : Xilinx's [UG1393](https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/Free-Running-Kernel) document

## Installation

Please follow the instructions for [TAPA installation](https://tapa.readthedocs.io/en/release/installation.html). After step 4 (Install TAPA), please type the following to install tarocc:
```
sudo ln -sf "${PWD}"/tarocc /usr/local/bin/
```

## Examples

Examples can be found in taro-apps/ directory

## Usage example (MM)

Given an input kernel file mm.cpp, please type:
```
tarocc mm.cpp -top=mm -base-file=mm-base.cpp -opt-file=mm-opt.cpp 
```

This will generate an optimized kernel file mm-opt.cpp. Then you can run HW co-simulation by:
```
mkdir build
cd build
cmake ..
make
make mm-cosim
```
Or you can generate a bitstream with the command: make mm-hw.
