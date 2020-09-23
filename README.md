# Extending High-Level Synthesis for Task-Parallel Programs

## Feature Synopsis

+ Convenient kernel communication APIs
+ Simple host-kernel interface
+ Universal software simulation w/ coroutines
+ Hierarchical code generator w/ Xilinx HLS backend

## Getting Started

### Prerequisites

+ Ubuntu 16.04+
  + Coroutine-based simulator only works on Ubuntu 18.04+

### Install from Binary

```bash
./install.sh
```

### Install from Source

#### Build Prerequisites

+ CMake 3.13+
+ A C++ 11 compiler (e.g. `g++-9`)
+ Python 3
  + [`haoda`](https://github.com/Blaok/haoda), `pyverilog`
+ Google glog library (`libgoogle-glog-dev`)
+ Clang 8 and its headers (`clang-8`, `libclang-8-dev`)
+ Boost coroutine library (`libboost-coroutine-dev`)
+ Icarus Verilog (`iverilog`)
+ [FPGA Runtime](https://github.com/UCLA-VAST/fpga-runtime)

#### Build `tapacc`

```bash
mkdir build
cd build
cmake ..
make
make test
cd ..
sudo ln -s backend/python/tapac /usr/local/bin/
sudo ln -s build/backend/tapacc /usr/local/bin/
```

## Known Issues

+ Template functions cannot be tasks
+ Vivado HLS include paths (e.g., `/opt/Xilinx/Vivado/2019.2/include`) must not
    be specified in `tapac --cflags`;
  + Workaround is to `export CPATH=/opt/Xilinx/Vivado/2019.2/include`
