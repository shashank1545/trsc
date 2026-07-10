# trsc: A Tiny Rust Compiler

`trsc` is a compiler for the Rust language, built using C++17, LLVM, and MLIR. It is designed to demonstrate modern compiler architecture, including a custom AST, semantic analysis (borrow checking, type checking), and MLIR-based code generation.

##  Features

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
mkdir build && cd build
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DMLIR_DIR=/path/to/llvm/lib/cmake/mlir
make 
```

## Testing Strategy

`trsc` uses a dual-testing approach for maximum reliability. See [test.md](test.md) for more details.

### 1. Unit Testing (Google Test)
Used for testing individual C++ components like the Lexer and Symbol Table.
```bash
make trsc_unit_tests
./test/trsc_unit_tests
```

### 2. Regression Testing (Lit + FileCheck)
Used for end-to-end verification of compiler output (AST, MLIR, etc.).
```bash
make check-trsc
# or
lit -v ../test
```

## Usage

Run the `trsc` compiler with various flags to inspect different phases:

```bash
# Dump the AST
./bin/trsc --dump-ast example.rs

# Dump TypedAST (this is the final ast after the sema containing type and scope info)
./bin/trsc --dump-typedast example.rs

# Emit MLIR
./bin/trsc --emit-mlir example.rs

# Inspect the Symbol Table
./bin/trsc --dump-symboltable example.rs

# Tokenize only
./bin/trsc --dump-tokens example.rs
```

## Performance

trsc recognizes matmul loop nests, rewrites them to a `trscd.gemm` op, and
lowers them through a CUDA optimization ladder selected with
`--matmul-opt-level` (1 = naive CPU loops, 2 = coalesced GMEM kernel,
3 = shared-memory tiling, 4 = 1D blocktiling, 5 = 2D blocktiling,
6 = vectorized). Measured on a GTX 1650 (sm_75), f32 square matmul, against
cuBLAS:

![GFLOP/s by optimization level](bench/results/gflops_vs_size.png)

![Percent of cuBLAS at N=2048](bench/results/pct_of_cublas.png)

| N | L1 | L2 | L3 | L4 | L5 | L6 | cuBLAS e2e | cuBLAS kernel |
|---|---|---|---|---|---|---|---|---|
| 128 | 2.16 | 9.52 | 9.54 | 9.77 | 6.4 | 9.41 | 12.0 | 161.32 |
| 256 | 2.11 | 26.63 | 27.18 | 28.59 | 24.39 | 26.95 | 49.38 | 651.54 |
| 512 | 0.84 | 35.17 | 47.48 | 59.4 | 57.88 | 63.81 | 132.01 | 1443.2 |
| 1024 | 0.17 | 40.09 | 54.83 | 81.23 | 84.91 | 84.14 | 131.3 | 1296.01 |
| 2048 | — | 60.61 | 126.07 | 160.32 | 180.72 | 178.14 | 276.08 | 1259.48 |

GFLOP/s, median of 10 reps (3 for CPU) after 2 warmup calls; full data in
[bench/results/](bench/results/). Kernel-only times (CUDA events around
each launch) come from a `--profile` sweep; at N=2048 that ladder reaches
L2 83 / L3 265 / L4 477 / L5 636 / L6 881 GFLOP/s against a 1292 GFLOP/s
cuBLAS sgemm — 6.5 / 20.5 / 36.9 / 49.2 / 68.2% of cuBLAS.

**Methodology.** Timing is honest end-to-end: each rep is one full program
call — host matrix allocation and initialization, device staging
(`gpu.alloc` + H2D `gpu.memcpy`), kernel launch, D2H copy-back, and sync —
measured with `CLOCK_MONOTONIC`; every rep verifies the numerical result
before its time counts. trsc kernels operate on device-resident buffers,
same as cuBLAS, which is shown both end-to-end (including allocation,
copies, and sync) and kernel-only. Kernel-only trsc times
(`TRSC_PROFILE=1`, CUDA events around each launch) are in the full results.
GPU clocks are not locked; medians + warmup + per-run clock snapshots
mitigate. Reproduce with `python3 bench/run_bench.py --all --profile` — see
[bench/README.md](bench/README.md).

**Findings.** The kernel-only ladder tracks the reference percentages from
[Boehm's CUDA matmul worklog](https://siboehm.com/articles/22/CUDA-MMM)
(his kernels 2–6 on an A6000: 8.5 / 12.8 / 36.5 / 68.7 / 78.4% of cuBLAS;
trsc L2–L6 on a GTX 1650: 6.5 / 20.5 / 36.9 / 49.2 / 68.2%). Two fixes got
it there. First, kernels originally read operands from pinned host memory
(`gpu.host_register` zero-copy), so every access crossed PCIe (~5.6 GB/s
observed) and capped all levels near 45 GFLOP/s; operands are now staged in
device memory around the launch. Second, L4–L6 kept per-thread accumulators
in `memref.alloca` arrays indexed by loop induction variables, which NVPTX
lowers to off-chip local memory (`ld.local`/`st.local` inside the FMA
loop) — the reason L5/L6 originally benched 3–4× *slower* than L3. The
thread tile is now fully unrolled at IR-build time with accumulators as
`scf.for` iter_args (registers), and L6 additionally stores the A tile
transposed in SMEM so fragments load as `vector<4xf32>`. Small sizes
(N ≤ 256) still show inversions — grids of a few blocks can't fill the SMs
and launch overhead dominates.

