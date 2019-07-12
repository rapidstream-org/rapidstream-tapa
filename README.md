# Task-Level Parallelization for HLS

## Feature Synopsis

+ [x] software simulation (not cycle-accurate)
+ [ ] Xilinx HLS backend
+ [ ] HeteroCL frontend

## Application Synopsis

| App      | Properties | Details               | # Streams | # Tasks | # Steps |
| -------- | ---------- | --------------------- | --------- | ------- | ------- |
| `jacobi` | static     | 5-point stencil       | 23        | 17      | 1       |
| `vadd`   | static     | naïve vector addition | 3         | 4       | 1       |

## Getting Started

### Prerequisite

+ CMake 3.13+
+ A C++11 compiler

<details><summary>How to install CMake 3.13+ on Ubuntu 16.04+ and CentOS 7?</summary>

Ubuntu 16.04+

```bash
sudo apt install python-pip
sudo -H python -m pip install cmake
```

CentOS 7

```bash
sudo yum install python-pip
sudo python -m pip install cmake
```

</details>

### Run tests

```bash
mkdir build
cd build
cmake ..
make
make test
```
