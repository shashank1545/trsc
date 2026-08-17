# Getting started

## 1. Download the release

Download `trsc-v0.0.1-linux-x86_64.zip` from the GitHub release and extract it:

```bash
unzip trsc-v0.0.1-linux-x86_64.zip
cd trsc-v0.0.1-linux-x86_64
export PATH="$PWD/bin:$PATH"
```

The executable is stripped and contains LLVM 23 and MLIR 23 statically. LLVM
and MLIR packages are not required on the host. Generated programs still need
a C compiler driver, a compatible x86_64 glibc, zlib, and zstd.

Ubuntu 24.04:

```bash
sudo apt update
sudo apt install clang zlib1g libzstd1
```

Debian 12 or newer:

```bash
sudo apt update
sudo apt install clang zlib1g libzstd1
```

Arch Linux:

```bash
sudo pacman -Syu --needed clang zlib zstd
```

Fedora:

```bash
sudo dnf install clang zlib zstd
```

Check the executable:

```bash
trsc --version
trsc --help
ldd bin/trsc
```

The ldd output must not contain `libLLVM`, `libMLIR`, `libz3`,
`libstdc++`, or `libgcc_s`. The release targets glibc-based x86_64 Linux
and requires no newer than `GLIBC_2.35`.

## 2. Build inside the Ubuntu Docker environment

This reproduces the release linkage: Ubuntu LLVM/MLIR static archives are
created inside the container, trsc is built against them, and the installed
tree remains inside the container.

Build the Ubuntu image:

```bash
docker build -f docker.yaml -t trsc:llvm23 .
```

Build and install trsc inside the container:

```bash
docker run --rm --network host \
 -v "$PWD:/src" \
 -w /src \
 trsc:llvm23 \
 bash -euo pipefail -c '
TRSC_STATIC_LLVM=1 \
 TRSC_STATIC_LLVM_PREFIX=/tmp/llvm-23-static \
 bash ci/install-llvm23.sh

    cmake -S /src -B /tmp/trsc-build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DTRSC_BUILD_OPT=OFF \
      -DMLIR_DIR=/usr/lib/llvm-23/lib/cmake/mlir \
      -DLLVM_DIR=/usr/lib/llvm-23/lib/cmake/llvm \
      -DTRSC_LLVM_STATIC_ARCHIVE=/tmp/llvm-23-static/lib/libLLVM23-all.a \
      -DTRSC_MLIR_STATIC_ARCHIVE=/tmp/llvm-23-static/lib/libMLIR23-all.a \
      -DCMAKE_C_COMPILER=clang-23 \
      -DCMAKE_CXX_COMPILER=clang++-23 \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"

    cmake --build /tmp/trsc-build
    cmake --install /tmp/trsc-build --prefix /tmp/trsc-install --strip
    ldd /tmp/trsc-install/bin/trsc
    /tmp/trsc-install/bin/trsc --help

'
```

GPU tests require an NVIDIA driver and Docker GPU support. CPU-only builds and
the non-GPU tests do not require a visible GPU.

## 3. Build with distribution LLVM/MLIR packages

This path is for development. It uses the shared LLVM/MLIR libraries supplied
by the distribution and is not the release build.

The project currently targets LLVM/MLIR 23. The distribution must provide
matching packages. If it provides another major version, use the custom LLVM
path below instead.

Install the common build dependencies and the distribution's LLVM/MLIR
development packages.

Ubuntu:

```bash
sudo apt update
sudo apt install \
  cmake ninja-build clang-23 llvm-23-dev libmlir-23-dev llvm-23-tools \
  zlib1g-dev libzstd-dev libcurl4-openssl-dev libedit-dev
export LLVM_DIR=/usr/lib/llvm-23/lib/cmake/llvm
export MLIR_DIR=/usr/lib/llvm-23/lib/cmake/mlir
export CC=clang-23
export CXX=clang++-23
```

Debian:

```bash
sudo apt update
sudo apt install \
  cmake ninja-build clang llvm-dev libmlir-dev \
  zlib1g-dev libzstd-dev libcurl4-openssl-dev libedit-dev
```

Debian package names and LLVM major versions are commonly suffixed. Set
`LLVM_DIR` and `MLIR_DIR` to the matching installed package directories.

Arch Linux:

```bash
sudo pacman -Syu --needed \
  cmake ninja clang llvm mlir zlib zstd curl libedit
export LLVM_DIR=/usr/lib/cmake/llvm
export MLIR_DIR=/usr/lib/cmake/mlir
export CC=clang
export CXX=clang++
```

Fedora:

```bash
sudo dnf install \
  cmake ninja-build clang llvm llvm-devel mlir mlir-devel \
  zlib-devel libzstd-devel libcurl-devel libedit-devel
export LLVM_DIR=/usr/lib64/cmake/llvm
export MLIR_DIR=/usr/lib64/cmake/mlir
export CC=clang
export CXX=clang++
```

If a distribution uses another CMake package location, find the package files
and use their parent directories:

```bash
find /usr -name LLVMConfig.cmake -o -name MLIRConfig.cmake
```

Configure without static archive overrides:

```bash
cmake -S . -B build-distro -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTRSC_LINK_LLVM_DYLIB=ON \
  -DTRSC_BUILD_OPT=OFF \
  -DMLIR_DIR="$MLIR_DIR" \
  -DLLVM_DIR="$LLVM_DIR" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build-distro
```

Verify that this development build uses shared LLVM/MLIR libraries:

```bash
ldd build-distro/tools/trsc/trsc | grep -E 'libLLVM|libMLIR'
```

The CMake build also requires a CUDA toolkit because trsc builds its CUDA
runtime wrapper. Install the toolkit from the distribution or its supported
vendor repository if CMake reports that CUDAToolkit is missing.

## 4. Build with an existing or custom LLVM/MLIR installation

LLVM_DIR and MLIR_DIR must point to the same LLVM/MLIR installation. For an
existing installation:

```bash
LLVM_ROOT=/path/to/llvm-install

cmake -S . -B build-custom -G Ninja \
 -DCMAKE_BUILD_TYPE=Release \
 -DTRSC_BUILD_OPT=OFF \
 -DMLIR_DIR="$LLVM_ROOT/lib/cmake/mlir" \
  -DLLVM_DIR="$LLVM_ROOT/lib/cmake/llvm" \
 -DCMAKE_C_COMPILER="$LLVM_ROOT/bin/clang" \
  -DCMAKE_CXX_COMPILER="$LLVM_ROOT/bin/clang++" \
 -DTRSC_LINK_LLVM_DYLIB=ON
cmake --build build-custom
```

For a custom static installation, use matching static archive bundles and
replace the shared-link option:

```text
  -DTRSC_LINK_LLVM_DYLIB=OFF \
  -DTRSC_LLVM_STATIC_ARCHIVE="$LLVM_ROOT/lib/libLLVM23-all.a" \
  -DTRSC_MLIR_STATIC_ARCHIVE="$LLVM_ROOT/lib/libMLIR23-all.a"
```

The archive paths must belong to the same LLVM/MLIR build as the CMake
metadata. Do not mix custom archives with unrelated distribution metadata.

To build LLVM/MLIR from source, build and install MLIR plus LLVM's test
utilities (`FileCheck` and `not`) from one LLVM 23 checkout. Run these commands
from the LLVM checkout root. This requires CMake, Ninja, lld, and a CUDA
toolkit.

```bash
git clone --branch <llvm-23-tag> https://github.com/llvm/llvm-project.git
cd llvm-project

cmake -G Ninja -S llvm -B build \
  -DLLVM_USE_LINKER=lld \
  -DLLVM_ENABLE_PROJECTS=mlir \
  -DLLVM_TARGETS_TO_BUILD=Native \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLVM_BUILD_LLVM_DYLIB=OFF \
  -DLLVM_LINK_LLVM_DYLIB=OFF \
  -DMLIR_LINK_MLIR_DYLIB=OFF \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_INSTALL_UTILS=ON \
  -DMLIR_ENABLE_CUDA_RUNNER=ON \
  -DCMAKE_INSTALL_PREFIX=/mlir_install
cmake --build build --target install -j"$(nproc)"
```

Add `-DLLVM_CCACHE_BUILD=ON` if ccache is installed and you want compiler
caching. It is not required for this build.

This source build is large. Use the distribution-package path for development
when its LLVM/MLIR major version matches trsc.

## Version behavior

TRSC_VERSION is a compile-time CMake value, not a runtime option passed to
trsc. CMake turns it into the TRSC_VERSION compiler definition used by the
version-printing code, so the value becomes part of the executable:

```bash
trsc --version
```

Local builds omit the value and report `trsc 0.0.0-dev`. The release workflow
passes the Git tag version so the release executable reports, for example,
`trsc 0.0.1`. Removing that release setting without adding automatic Git-tag
version detection would make the release binary report the development version.

## First compilation

Create `hello.rs`:

```rust
fn main() {
    println!("hello from trsc");
}
```

Compile and run it:

```bash
./build/tools/trsc/trsc hello.rs -o hello
./hello
```
