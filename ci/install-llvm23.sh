#!/usr/bin/env bash
set -euo pipefail

# Shared by .github/workflows/ci.yml and docker.yaml. Keep the LLVM/CUDA
# package set here so local CI reproduction and GitHub Actions stay aligned.
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends ca-certificates curl gnupg

install -d -m 0755 /etc/apt/trusted.gpg.d
curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
  | gpg --dearmor --yes -o /etc/apt/trusted.gpg.d/llvm.gpg
echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-23 main" \
  > /etc/apt/sources.list.d/llvm-23.list

curl -fsSL \
  https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb \
  -o /tmp/cuda-keyring.deb
dpkg -i /tmp/cuda-keyring.deb
rm -f /tmp/cuda-keyring.deb

apt-get update
apt-get install -y --no-install-recommends \
  cmake \
  llvm-23-dev \
  libmlir-23-dev \
  mlir-23-tools \
  llvm-23-tools \
  libpolly-23-dev \
  clang-23 \
  lld-23 \
  ninja-build \
  ccache \
  libgtest-dev \
  zlib1g-dev \
  libzstd-dev \
  libcurl4-openssl-dev \
  libedit-dev \
  python3-pip \
  cuda-nvcc-12-6 \
  cuda-cudart-dev-12-6 \
  cuda-driver-dev-12-6

# Ubuntu 24.04 does not ship python3-lit. Install the upstream lit package,
# which provides the `lit` executable used by the CTest regression target.
python3 -m pip install --break-system-packages lit
