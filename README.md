# trsc: A Tiny Rust Compiler

[![CI](https://github.com/shashank1545/trsc/actions/workflows/ci.yml/badge.svg)](https://github.com/shashank1545/trsc/actions/workflows/ci.yml)

`trsc` is an experimental compiler for a small, Rust-like language. It is built
with C++17, LLVM 23, and MLIR 23. The current driver runs the complete path:

```text
source -> lexing -> parsing -> semantic analysis -> MLIR -> LLVM IR
       -> native object -> linked executable
```

It is not a drop-in replacement for `rustc`; language coverage and command-line
interfaces are still evolving. CI currently builds and tests the compiler from
source. Prebuilt executables are planned but are not published yet.

## Features

- **Frontend:** Hand-written lexer and recursive-descent parser for declarations,
  primitive values, arrays, expressions, functions and calls, returns, control
  flow, ranges, string literals, and `println!`.
- **Semantic analysis:** Nested scopes, shadowing, symbol-table inspection,
  type checking, call-arity checking, forward calls, logical operators, and an
  initial borrow-checking pass.
- **MLIR middle-end:** A custom `trscd` dialect with canonicalization/CSE,
  mem2reg, loop-invariant code motion, loop fusion, tiling, unrolling, matmul
  recognition, and GEMM epilogue fusion.
- **Native code generation:** MLIR lowers to the LLVM dialect and LLVM IR, then
  to a native object file or a linked executable.
- **CUDA path:** GEMM lowering supports CPU execution, CUDA kernels, target
  architecture selection, CPU/GPU dispatch, and CPU fallback when CUDA cannot
  be used at runtime.
- **Verification:** GoogleTest unit tests, lit/FileCheck regressions, LLVM
  lowering checks, executable integration tests, matmul correctness checks, and
  CUDA-level coverage.

## Getting Started

The compiler binary is produced at `build/tools/trsc/trsc`. The build also
creates `build/tools/trsc-opt/trsc-opt`, an MLIR pass driver used by the test
suite. The commands below focus on the end-user `trsc` driver. Choose Docker
for the repository's reproducible Ubuntu/LLVM toolchain, or build directly on
a machine with LLVM and MLIR installed.

### Option 1: Docker

`docker.yaml` is the current build-environment recipe. Its non-standard name
means it must be passed explicitly to `docker build`; it does not publish a
prebuilt compiler image.

```bash
docker build -f docker.yaml -t trsc:llvm23 .
docker run --rm -it trsc:llvm23 bash
```

Inside the container:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTING=ON \
  -DMLIR_DIR=/usr/lib/llvm-23/lib/cmake/mlir \
  -DLLVM_DIR=/usr/lib/llvm-23/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-23 \
  -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build
ctest --test-dir build --output-on-failure -LE GPU
```

The image contains CUDA development packages, but GPU tests also require an
NVIDIA driver and container runtime on the host. Run the container with
`--gpus all` and omit `-LE GPU` only when that setup is available.

### Option 2: Build from source

The tested toolchain is Ubuntu 24.04 with LLVM/MLIR 23, Clang 23, LLD, CMake,
Ninja, Python 3 with `lit`, GoogleTest, and the CUDA development toolkit. The
current CMake build requires the toolkit for its CUDA runtime library, but
CPU-only compilation and the non-GPU CI suite do not require a visible GPU.

On Ubuntu 24.04, the same dependency script used by CI and Docker can install
the toolchain:

```bash
sudo bash ci/install-llvm23.sh
```

Configure and build:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTING=ON \
  -DMLIR_DIR=/usr/lib/llvm-23/lib/cmake/mlir \
  -DLLVM_DIR=/usr/lib/llvm-23/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=clang-23 \
  -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build
```

For a compiler-only build, omit `-DENABLE_TESTING=ON`. If LLVM and MLIR are
installed elsewhere, replace the two `*_DIR` values with their CMake package
directories.

### First compilation

Create a small source file such as `hello.rs`:

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

The default output is `a.out` when `-o` is omitted. The driver links the
generated object with the compiler runtime; a C compiler driver (`clang`,
`gcc`, or `cc`) must therefore be available on `PATH`.

## Testing

Configure with `-DENABLE_TESTING=ON`, then use CTest as the single entry point:

```bash
# Unit, regression, and CPU/integration coverage; matches the non-GPU CI job.
ctest --test-dir build --output-on-failure -j4 -LE GPU

# Include GPU-labelled integration tests when an NVIDIA GPU is available.
ctest --test-dir build --output-on-failure -j4
```

The test suite includes GoogleTest unit tests, lit/FileCheck frontend and MLIR
regressions, LLVM lowering checks, executable-link tests, `println!` coverage,
matmul correctness checks, CUDA levels 2–8, and CPU fallback checks. To run
only the lit regression suite:

```bash
lit -v build/test/Regression
```

GitHub Actions uses LLVM 23, builds the complete project, and runs
`ctest --test-dir build --output-on-failure -j4 -LE GPU`. It validates the
source build; it does not currently upload a release executable.

## Command-line reference

There is no built-in `--help` output yet. The current driver accepts one input
file and the following flags. Flags use the spelling implemented by the
compiler; for example, token dumping is `-dump-token`, not `--dump-tokens`.

### Frontend and diagnostics

| Flag | Effect |
| --- | --- |
| `-dump-token` | Lex the input, print tokens, then exit. |
| `-dump-ast` | Print the parsed AST, then exit. |
| `-dump-typedast` | Print the semantically resolved/typed AST, then exit. |
| `-dump-symbol` | Print the symbol table after semantic analysis, then exit. |
| `-dump-symboltable` | Print the symbol-table scope tree, then exit. |
| `-v`, `--verbose` | Print phase and output-status messages to stderr. |

### MLIR, LLVM, and native output

| Flag | Effect |
| --- | --- |
| `-emit-mlir` | Print or write MLIR and stop before LLVM translation. |
| `-emit-llvm` | Print or write translated LLVM IR. |
| `-emit-obj` | Emit a native object file and stop. Defaults to `<input-stem>.o`. |
| `-o <file>` | Write the selected dump, IR, object, or executable to `<file>`. |
| `-optim=<stage>` | Select the MLIR inspection stage when used with `-emit-mlir`. |

Valid `-optim` stages are `raw` (immediately after generation), `rawcln`
(cleaned), `loop` (loop-optimized), `stdlowering` (standard/LLVM lowering),
and `finopt` (optimized MLIR). Normal executable, object, and LLVM-IR builds
run the full lowering pipeline; `-optim` is primarily for inspecting MLIR.

### Target and matmul lowering

| Flag | Effect |
| --- | --- |
| `--device=auto` | Keep CPU/GPU dispatch and fall back to CPU when CUDA is unavailable. |
| `--device=cpu` | Force the CPU matmul lowering. |
| `--device=cuda` | Require the CUDA matmul lowering. |
| `--cuda-arch=sm_NN` | Select the CUDA target architecture; the minimum is `sm_75`. |
| `--matmul-opt-level=N` | Select the matmul lowering level; default is `6`. |

Matmul levels are currently:

| Level | Lowering |
| --- | --- |
| `0` | Disable matmul recognition/optimization; keep the loop nest. |
| `1` | Recognized GEMM lowered to a scalar CPU loop nest. |
| `2` | CUDA kernel with coalesced global-memory access. |
| `3` | Shared-memory tile caching. |
| `4` | 1D block tiling. |
| `5` | 2D block tiling with register tiles. |
| `6` | Vectorized staging and loads. |
| `7` | Level 6 plus shared-memory double buffering. |
| `8` | Level 7 plus warp tiling. |
| `9` | Experimental autotuning metadata/contract; not yet a complete native lowering path. |

Levels 2–8 target CUDA. `--device=cpu` forces the CPU implementation even when
the selected level is higher. `--device=auto` embeds dispatch/fallback logic;
it can run the CPU path when the CUDA driver or a suitable device is absent.

For more detail on the benchmark harness and tile-shape experiments, see
[bench/README.md](bench/README.md).

## Performance

trsc recognizes matmul loop nests, rewrites them to a `trscd.gemm` op, and
lowers them through the level 1–8 optimization ladder selected with
`--matmul-opt-level` (0 disables recognition; 1 = naive CPU loops, 2 = coalesced GMEM kernel,
3 = shared-memory tiling, 4 = 1D blocktiling, 5 = 2D blocktiling,
6 = vectorized, 7 = double-buffered SMEM, 8 = warp tiling). Measured on a
GTX 1650 (sm_75), f32 square matmul, against cuBLAS. Kernel-only timings
(CUDA events around each launch) are the fair trsc-vs-cuBLAS comparison —
both operate on device-resident buffers:

GPU levels emit PTX for compute capability 7.5 by default. This is a portable
baseline for Turing and newer NVIDIA GPUs; the installed driver JIT-compiles
the PTX for the selected device. The GTX 1650 is the benchmark machine, not a
runtime requirement.

```bash
# Prefer CUDA and fall back to scalar CPU lowering when unavailable.
./build/tools/trsc/trsc --device=auto --matmul-opt-level=6 program.rs

# Emit CPU-only or require CUDA explicitly.
./build/tools/trsc/trsc --device=cpu --matmul-opt-level=6 program.rs
./build/tools/trsc/trsc --device=cuda --cuda-arch=sm_80 \
  --matmul-opt-level=6 program.rs
```

`--device=auto` embeds CPU and GPU implementations. Missing `libcuda`, no
visible device, insufficient compute capability, or a CUDA execution error
selects the CPU implementation without corrupting the output buffer.

![Kernel-only throughput vs cuBLAS](bench/results/kernel_vs_cublas.png)

| N | L2 | L3 | L4 | L5 | L6 | L7 | L8 | cuBLAS |
|---|---|---|---|---|---|---|---|---|
| 128 | 68.76 | 113.36 | 82.24 | 31.78 | 61.68 | 67.11 | 102.3 | 182.36 |
| 256 | 116.91 | 220.75 | 209.72 | 126.62 | 238.82 | 264.21 | 325.77 | 657.93 |
| 512 | 81.46 | 228.16 | 347.49 | 367.22 | 505.53 | 506.48 | 612.17 | 1394.47 |
| 1024 | 94.06 | 251.55 | 403.06 | 485.91 | 632.73 | 633.85 | 731.18 | 1333.43 |
| 2048 | 94.68 | 309.57 | 514.37 | 681.65 | 911.64 | 904.11 | 1008.83 | 1398.27 |

Kernel-only GFLOP/s, median of 10 reps after 2 warmup calls. The ladder
peaks at **1009 GFLOP/s at N=2048 with L8 warp tiling — 72.2% of cuBLAS
sgemm** (1398 GFLOP/s) on this card.

End-to-end (each rep a full program call: host alloc + init, device
staging, launch, copy-back, sync), staging overhead dominates —
L8 reaches 256 GFLOP/s at N=2048 vs 552 for a hoisted-allocation cuBLAS
loop:

![End-to-end GFLOP/s by optimization level](bench/results/gflops_vs_size.png)

Full data (e2e + kernel series, all levels) in
[bench/results/](bench/results/).

Reproduce with `python3 bench/run_bench.py --all --profile` — see
[bench/README.md](bench/README.md). Measurement details are in
[docs/benchmark-methodology.md](docs/benchmark-methodology.md); analysis
of the results in
[bench/results/findings.md](bench/results/findings.md).
