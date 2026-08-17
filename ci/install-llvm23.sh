#!/usr/bin/env bash
set -euo pipefail

# Shared by .github/workflows/ci.yml, .github/workflows/release.yml, and
# docker.yaml. The default installs the Ubuntu LLVM 23 shared-library toolchain.
# Set TRSC_STATIC_LLVM=1 for the static toolchain. Set
# TRSC_INSTALL_TEST_DEPS=1 when CTest/lit and unit tests will run.
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
base_packages=(
  cmake \
  file \
  clang-23 \
  lld-23 \
  ninja-build \
  zlib1g-dev \
  libzstd-dev \
  libcurl4-openssl-dev \
  libedit-dev \
  cuda-nvcc-12-6 \
  cuda-cudart-dev-12-6 \
  cuda-driver-dev-12-6
)

if [[ "${TRSC_STATIC_LLVM:-0}" != 1 ]]; then
  base_packages+=(
    libpolly-23-dev
    ccache
    libgtest-dev
    python3-pip
  )
fi
if [[ "${TRSC_STATIC_LLVM:-0}" == 1 &&
      "${TRSC_INSTALL_TEST_DEPS:-0}" == 1 ]]; then
  base_packages+=(
    ccache
    libgtest-dev
    python3-pip
  )
fi
if [[ "${TRSC_INSTALL_TIDY_DEPS:-0}" == 1 ]]; then
  base_packages+=(
    clang-tidy-23
    clang-tools-23
  )
fi
base_packages+=(
  llvm-23-dev
  libmlir-23-dev
  llvm-23-tools
  mlir-23-tools
)

apt-get install -y --no-install-recommends "${base_packages[@]}"

if [[ "${TRSC_STATIC_LLVM:-0}" != 1 ]]; then
  # Ubuntu 24.04 does not ship python3-lit. Install the upstream lit package,
  # which provides the `lit` executable used by the CTest regression target.
  python3 -m pip install --break-system-packages lit
  exit 0
fi

if [[ "${TRSC_INSTALL_TEST_DEPS:-0}" == 1 ]]; then
  # Static CI still needs lit for the regression tests.
  python3 -m pip install --break-system-packages lit
fi

STATIC_PREFIX="${TRSC_STATIC_LLVM_PREFIX:-/opt/llvm-23-static}"
STATIC_LIB_DIR="$STATIC_PREFIX/lib"
rm -rf "$STATIC_PREFIX"
install -d -m 0755 "$STATIC_LIB_DIR"

LLVM_ARCHIVE="$STATIC_LIB_DIR/libLLVM23-all.a"
MLIR_ARCHIVE="$STATIC_LIB_DIR/libMLIR23-all.a"
{
  echo "CREATE $LLVM_ARCHIVE"
  for archive in /usr/lib/llvm-23/lib/libLLVM*.a; do
    echo "ADDLIB $archive"
  done
  echo SAVE
  echo END
} | llvm-ar-23 -M
{
  echo "CREATE $MLIR_ARCHIVE"
  for archive in /usr/lib/llvm-23/lib/libMLIR*.a; do
    echo "ADDLIB $archive"
  done
  echo SAVE
  echo END
} | llvm-ar-23 -M

test -s "$LLVM_ARCHIVE"
test -s "$MLIR_ARCHIVE"
if find "$STATIC_LIB_DIR" -maxdepth 1 -type f \
    \( -name 'libLLVM.so*' -o -name 'libMLIR.so*' \) | grep -q .; then
  echo "static LLVM archive bundle unexpectedly contains shared libraries" >&2
  exit 1
fi
