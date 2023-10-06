# Docker for TAPA

This directory contains a Dockerfile and instructions to develop applications
using TAPA or even develop TAPA itself inside a Docker container.

## Build Image

First, choose an Ubuntu version for the base image.
The default is Ubuntu 22.04 (`jammy`).
Ubuntu 20.04 (`focal`) is also supported.

Then, download [Xilinx packages](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo.html)
for your selected Ubuntu version.
Extract any compressed files and put all `.deb` files under a directory.
For example:

```console
foo@bar:~$ ls /path/to/xilinx/packages
xilinx-u250-gen3x16-xdma-4.1-202210-1-dev_1-3512975_all.deb
xilinx-u280-gen3x16-xdma-1-202211-1-dev_1-3585755_all.deb
xrt_202320.2.16.204_20.04-amd64-xrt.deb
```

Build the docker image:

```sh
docker build . \
  --tag tapa \
  --build-context packages=/path/to/xilinx/packages \
  --build-arg UBUNTU_VERSION=jammy \
  --build-arg UID="$(id -u)" \
  --build-arg GID="$(id -g)" \

```

This will create an image (tag name `tapa`) that has the downloaded packages and
all necessary libraries required to run TAPA (including relevant Xilinx tools).

## Launch Container

First, figure out where your Xilinx tools are installed.
The following example assume the tools are installed under `/path/to/xilinx`.

Launch a container named `tapa`,
but change the tool version and paths accordingly:

```sh
docker run \
  --detach \
  --env XILINX_HLS=/path/to/xilinx/Vitis_HLS/2023.2 \
  --env XILINX_VITIS=/path/to/xilinx/Vitis/2023.2 \
  --env XILINX_VIVADO=/path/to/xilinx/Vivado/2023.2 \
  --env XILINX_XRT=/opt/xilinx/xrt \
  --init \
  --name tapa \
  --volume /path/to/xilinx:/path/to/Xilinx:ro \
  --volume /path/to/tapa:/home/ubuntu/tapa \
  tapa \
  sleep infinity
```

This will map your Xilinx tool installation into the container,
and your local copy of TAPA repository into the container as well.
Note that due to `--detach`,
the above command will return immediately without entering the container;
to get a shell session inside the container, do the following:

```sh
docker exec -it tapa bash
```

You'll then get a shell session in the container where you can develop as usual.
For example, type (or copy-and-paste) the following to build TAPA from source
(warning: that takes time due to LLVM) and run cosim for the vadd example:

```sh
mkdir -p build
cd build
cmake -GNinja ..
source ${XILINX_HLS}/settings64.sh
ninja vadd-cosim
```

> [!NOTE]
> Docker containers are volatile.
> Any changes out of the `tapa` directory will be lost when your container is
> `docker rm`'ed.
> To persist changes, map more files or directories to the container when you
> create it (`docker run`).
> For example, `--volume ${HOME}:/home/ubuntu` will map your home directory
> inside the container.
