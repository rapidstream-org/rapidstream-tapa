# Task-Level Parallelization for HLS

## Feature Synopsis

+ [x] software simulation (not cycle-accurate)
+ [ ] Xilinx HLS backend
+ [ ] HeteroCL frontend

## Application Synopsis

| App      | Properties       | Details               | # Streams | # Tasks | # Steps |
| -------- | ---------------- | --------------------- | --------- | ------- | ------- |
| `cannon` | static,feedback  | Cannon's algorithm    | 8         | 7       | 3       |
| `graph`  | dynamic,feedback | connected components  | 6         | 3       | 1       |
| `jacobi` | static           | 5-point stencil       | 23        | 17      | 1       |
| `vadd`   | static           | naïve vector addition | 3         | 4       | 1       |

## Getting Started

### Prerequisite

+ CMake 3.13+
+ A C++11 compiler
+ Google glog library

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

<details><summary>How to install Google glog library on Ubuntu and CentOS?</summary>

Ubuntu

```bash
sudo apt install libgoogle-glog-dev
```

CentOS

```bash
sudo yum install glog-devel
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
