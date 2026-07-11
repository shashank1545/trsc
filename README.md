# trsc: A Tiny Rust Compiler

[![CI](https://github.com/shashank1545/trsc/actions/workflows/ci.yml/badge.svg)](https://github.com/shashank1545/trsc/actions/workflows/ci.yml)

`trsc` is a compiler for a subset of Rust, built with C++17, LLVM, and MLIR. It demonstrates modern compiler architecture: a custom AST, semantic analysis (type and borrow checking), and MLIR-based code generation.

## Features

- **Lexer & Parser:** Hand-written recursive descent parser for a Rust-like syntax.
- **Semantic Analysis:**
  - **Symbol Table:** Nested scope management and shadowing support.
  - **Type Checker:** Strong static typing for primitive types and functions.
  - **Borrow Checker:** Initial support for memory safety and ownership rules.
- **MLIR Generation:** Lowers high-level AST constructs into a basic MLIR dialect for further optimization and lowering to LLVM IR.
- **Debugging Tools:** Comprehensive dumping flags for every compiler phase.

## Building the Project

### Prerequisites
- **LLVM & MLIR:** Must be installed and discoverable via CMake (`LLVM_DIR` and `MLIR_DIR`).
- **Clang:** Used as the default C++ compiler.
- **Python:** Required for running `lit` tests.

### Build Steps
```bash
cmake -B build -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DMLIR_DIR=/path/to/llvm/lib/cmake/mlir
cmake --build build -j8
```

## Testing

`trsc` is tested at two levels.

### 1. Unit tests (Google Test)
Cover individual C++ components such as the lexer and symbol table.
```bash
cmake --build build --target trsc_unit_tests
./build/test/Unit/trsc_unit_tests
```

### 2. Regression and integration tests (lit + FileCheck, ctest)
Verify compiler output end-to-end (AST, MLIR, generated code). Configure
with `-DENABLE_TESTING=ON` (off by default), then:
```bash
ctest --test-dir build              # add -LE GPU to skip GPU-dependent tests
# or run a lit suite directly
lit -v test/Regression
```

## Usage

Run the `trsc` compiler with various flags to inspect different phases:

```bash
# Dump the AST
./build/tools/trsc/trsc --dump-ast example.rs

# Dump the typed AST (post-sema, with resolved types and scopes)
./build/tools/trsc/trsc --dump-typedast example.rs

# Emit MLIR
./build/tools/trsc/trsc --emit-mlir example.rs

# Inspect the Symbol Table
./build/tools/trsc/trsc --dump-symboltable example.rs

# Tokenize only
./build/tools/trsc/trsc --dump-tokens example.rs
```

## Performance

trsc recognizes matmul loop nests, rewrites them to a `trscd.gemm` op, and
lowers them through a CUDA optimization ladder selected with
`--matmul-opt-level` (1 = naive CPU loops, 2 = coalesced GMEM kernel,
3 = shared-memory tiling, 4 = 1D blocktiling, 5 = 2D blocktiling,
6 = vectorized, 7 = double-buffered SMEM, 8 = warp tiling). Measured on a
GTX 1650 (sm_75), f32 square matmul, against cuBLAS:

![GFLOP/s by optimization level](bench/results/gflops_vs_size.png)

![Percent of cuBLAS at N=2048](bench/results/pct_of_cublas.png)

| N | L1 | L2 | L3 | L4 | L5 | L6 | L7 | L8 | cuBLAS e2e | cuBLAS kernel |
|---|---|---|---|---|---|---|---|---|---|---|
| 128 | 0.97 | 9.38 | 9.92 | 9.63 | 6.91 | 9.3 | 9.2 | 9.71 | 39.02 | 190.65 |
| 256 | 2.37 | 27.13 | 30.63 | 30.12 | 27.67 | 30.97 | 42.88 | 47.19 | 116.71 | 651.54 |
| 512 | 0.85 | 39.94 | 58.84 | 64.61 | 64.54 | 68.63 | 69.32 | 68.9 | 261.63 | 1451.0 |
| 1024 | 0.29 | 61.6 | 102.12 | 122.87 | 130.83 | 140.53 | 140.29 | 152.44 | 361.16 | 1354.45 |
| 2048 | — | 74.78 | 172.75 | 222.58 | 254.44 | 283.44 | 281.74 | 294.28 | 551.99 | 1416.37 |

GFLOP/s, median of 10 reps (3 for CPU) after 2 warmup calls; full data in
[bench/results/](bench/results/). Kernel-only times (CUDA events around
each launch) come from a `--profile` sweep; at N=2048 that ladder reaches
L2 95 / L3 310 / L4 515 / L5 682 / L6 912 / L7 904 / L8 1009 GFLOP/s
against a 1416 GFLOP/s cuBLAS sgemm — 6.7 / 21.9 / 36.3 / 48.1 / 64.4 /
63.9 / 71.3% of cuBLAS.

**Methodology.** Timing is honest end-to-end: each rep is one full program
call — host matrix allocation and initialization, device staging
(`gpu.alloc` + H2D `gpu.memcpy`), kernel launch, D2H copy-back, and sync —
measured with `CLOCK_MONOTONIC`; every rep verifies the numerical result
before its time counts. trsc kernels operate on device-resident buffers,
same as cuBLAS, which is shown both end-to-end (init + transfers + sgemm +
sync, with buffers and handle allocated once, as a real application would)
and kernel-only. Because trsc re-allocates host memory every rep and the
cuBLAS end-to-end loop does not, the kernel framing is the fair
trsc-vs-cuBLAS comparison. Kernel-only trsc times (`TRSC_PROFILE=1`, CUDA
events around each launch) are in the full results.
GPU clocks are not locked; medians + warmup + per-run clock snapshots
mitigate. Reproduce with `python3 bench/run_bench.py --all --profile` — see
[bench/README.md](bench/README.md).

**Findings.** The kernel-only ladder tracks the reference percentages from
[Boehm's CUDA matmul worklog](https://siboehm.com/articles/22/CUDA-MMM)
(his kernels 2–6 on an A6000: 8.5 / 12.8 / 36.5 / 68.7 / 78.4% of cuBLAS;
trsc L2–L6 on a GTX 1650: 6.7 / 21.9 / 36.3 / 48.1 / 64.4%). Two fixes got
it there. First, kernels originally read operands from pinned host memory
(`gpu.host_register` zero-copy), so every access crossed PCIe (~5.6 GB/s
observed) and capped all levels near 45 GFLOP/s; operands are now staged in
device memory around the launch. Second, L4–L6 kept per-thread accumulators
in `memref.alloca` arrays indexed by loop induction variables, which NVPTX
lowers to off-chip local memory (`ld.local`/`st.local` inside the FMA
loop) — the reason L5/L6 originally benched 3–4× *slower* than L3. The
thread tile is now fully unrolled at IR-build time with accumulators as
`scf.for` iter_args (registers), and L6 additionally stores the A tile
transposed in SMEM so fragments load as `vector<4xf32>`. Levels 7 and 8
continue the ladder: double buffering alone (L7) lands at L6 parity — the
sm_75 kernel has no `cp.async`, so prefetches are staged through registers
and the 64-thread blocks stay issue-bound — while warp tiling (L8) with
tile shapes autotuned for this card (`TRSC_GEMM_TILES` override, best
`64,64,16,4,4,32,32,2`) lifts the kernel to 71% of cuBLAS. Small sizes
(N ≤ 256) still show inversions — grids of a few blocks can't fill the SMs
and launch overhead dominates.

