ARG UBUNTU_VERSION=jammy
FROM ubuntu:${UBUNTU_VERSION} AS base

RUN <<EOF
  set -e
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    autoconf \
    build-essential \
    ca-certificates \
    curl \
    gcc-multilib \
    git \
    gnupg \
    htop \
    libc6-dev-i386 \
    libfreetype6 \
    libicu-dev \
    libncurses5 \
    libtinfo-dev \
    libx11-6 \
    libxext6 \
    libxi6 \
    libxrender1 \
    libxtst6 \
    locales \
    ninja-build \
    python3-pip \
    sudo \
    tzdata \
    unzip \
    vim \
    wget \
    x11-apps \
    zlib1g-dev \
    zsh \

EOF

RUN --mount=from=packages,target=/tmp/packages <<EOF
  set -e
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    /tmp/packages/*.deb
EOF

ARG UID=1000
ARG GID=1000
RUN <<EOF
  set -e
  locale-gen en_US.UTF-8
  groupadd ubuntu --gid ${GID}
  useradd --create-home ubuntu --uid ${UID} --gid ${GID}
  echo 'ubuntu ALL=(ALL) NOPASSWD: ALL' >>/etc/sudoers
EOF
USER ubuntu
WORKDIR /home/ubuntu
ENV PATH="/home/ubuntu/.local/bin:${PATH}"

FROM base AS tapa
ARG CLANG_VERSION=17
ARG UBUNTU_VERSION
RUN <<EOF
  set -e
  wget -O- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.llvm.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.llvm.gpg.tmp /usr/share/keyrings/llvm.gpg
  echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/${UBUNTU_VERSION} llvm-toolchain-${UBUNTU_VERSION}-${CLANG_VERSION} main" |
    sudo tee /etc/apt/sources.list.d/llvm-${CLANG_VERSION}.list
  wget -O- https://about.blaok.me/fpga-runtime/frt.gpg.key | gpg --dearmor |
    sudo tee /usr/share/keyrings/.frt.gpg.tmp >/dev/null
  sudo mv /usr/share/keyrings/.frt.gpg.tmp /usr/share/keyrings/frt.gpg
  echo "deb [arch=amd64 signed-by=/usr/share/keyrings/frt.gpg] https://about.blaok.me/fpga-runtime ${UBUNTU_VERSION} main" |
    sudo tee /etc/apt/sources.list.d/frt.list
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    clang-${CLANG_VERSION} \
    g++ \
    iverilog \
    libboost-coroutine-dev \
    libboost-stacktrace-dev \
    libc++-${CLANG_VERSION}-dev \
    libfrt-dev \
    libgflags-dev \
    libgoogle-glog-dev \
    python3-pip \
    python3-venv \
    rsync \

  sudo python3 -m pip install --no-cache-dir \
    cmake \

  sudo python3 -m pip install --no-cache-dir \
    absl-py \
    autobridge>=0.0.20220512.dev1 \
    click>=7.1.2 \
    coloredlogs>=9.3 \
    haoda>=0.0.20220507.dev1 \
    jinja2 \
    pyverilog>=1.2.0 \
    pyyaml>=5.1 \
    tapa-fast-cosim>=0.0.20220514.dev1 \
    toposort \

EOF
