# Extending High-Level Synthesis for Task-Parallel Programs

## Feature Synopsis

+ [x] software simulation (not cycle-accurate)
+ [x] Xilinx HLS backend
+ [ ] HeteroCL frontend

## Application Synopsis

| App         | Properties       | Details                           | # Streams | # Tasks | # Steps |
| ----------- | ---------------- | --------------------------------- | --------- | ------- | ------- |
| `bandwidth` | static           | bandwidth test using `async_mmap` | 4         | 4       | 1       |
| `cannon`    | static,feedback  | Cannon's algorithm                | 20        | 7       | 1       |
| `graph`     | dynamic,feedback | connected components              | 6         | 3       | 1       |
| `jacobi`    | static           | 5-point stencil                   | 23        | 17      | 1       |
| `vadd`      | static           | naïve vector addition             | 3         | 4       | 1       |

## Getting Started

### Prerequisites

+ Ubuntu 16.04+

### Install from Binary

```bash
./install.sh
```

### Install from Source

#### Build Prerequisites

+ CMake 3.13+
+ A C++11 compiler
+ Google glog library
+ Clang headers

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
