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

ARG CLANG_VERSION=17
RUN <<EOF
  set -e
  wget -O- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor |
    tee /usr/share/keyrings/.llvm.gpg.tmp >/dev/null
  . /etc/os-release
  mv /usr/share/keyrings/.llvm.gpg.tmp /usr/share/keyrings/llvm.gpg
  echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/${VERSION_CODENAME} llvm-toolchain-${VERSION_CODENAME}-${CLANG_VERSION} main" |
    tee /etc/apt/sources.list.d/llvm-${CLANG_VERSION}.list
EOF

ARG UID=1000
ARG GID=1000
RUN <<EOF
  set -e
  locale-gen en_US.UTF-8
  groupadd rapidstream --gid ${GID}
  useradd --create-home rapidstream --uid ${UID} --gid ${GID}
  echo 'rapidstream ALL=(ALL) NOPASSWD: ALL' >>/etc/sudoers
EOF
USER rapidstream
WORKDIR /home/rapidstream
ENV PATH="/home/rapidstream/.local/bin:${PATH}"

RUN --mount=from=packages,target=/tmp/packages <<EOF
  set -e
  sudo apt update
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    --no-install-recommends /tmp/packages/*.deb
  python3 -m pip install /tmp/packages/*.whl
EOF
