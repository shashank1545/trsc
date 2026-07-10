# trsc matmul benchmarks

Measures the `--matmul-opt-level` ladder (1–6) against cuBLAS on square f32
matmuls and produces the charts + table embedded in the root README.

## Prerequisites

- A configured `build/` with `tools/trsc/trsc` and `lib/libTrscCudaRuntime.a`
  (`cmake -B build -DLLVM_DIR=... -DMLIR_DIR=... && cmake --build build -j8`)
- NVIDIA GPU + driver (`nvidia-smi` must work) for levels ≥ 2 and cuBLAS
- CUDA toolkit with cuBLAS (auto-detected via `--cuda-home`, `$CUDA_HOME`,
  `nvcc` on PATH, then `/opt/cuda`, `/usr/local/cuda`)
- Python 3 + `matplotlib` (plots only)

## Run

```bash
python3 bench/run_bench.py --all              # full sweep + cuBLAS + plots
python3 bench/run_bench.py --all --profile    # also kernel-only series
python3 bench/run_bench.py --sizes 128 --levels 1,6 --reps 3   # smoke
python3 bench/run_bench.py --big              # add N=4096
python3 bench/run_bench.py --dry-run ...      # print command matrix only
```

Outputs land in `bench/results/`: `raw.csv` (every rep), `results.csv`
(aggregates), `env.txt` (GPU/driver/clock snapshot), the PNGs, and
`results_table.md`. Commit that directory to publish numbers.

## How it measures

Each (size, level) pair is compiled to a real object file and linked into a
standalone binary using the same recipe as the integration tests
(`test/Integration/run_matmul_test.sh`): the trsc program's `main` is renamed
`trsc_main` via objcopy and driven by `harness_bench.c`.

- **Timing is honest end-to-end**: one rep = one full `trsc_main()` call —
  host matrix allocation + initialization, device staging (`gpu.alloc` +
  H2D `gpu.memcpy`, GPU levels), kernel launch, D2H copy-back, and sync.
  `CLOCK_MONOTONIC` wall clock.
- **Warmup = 2 calls** (CUDA context init + module JIT), **reps = 10** GPU /
  3 CPU. `results.csv` reports median, min, stddev; GFLOP/s = 2N³/t.
- **Every rep self-verifies** `c[0][0] == 2.0*N` (all-1.0 × all-2.0 input);
  a wrong result aborts the variant, so a broken kernel can never post a
  fast number.
- **cuBLAS is reported in two framings**: `cublas_e2e` (per rep: matrix
  init + H2D + sgemm + D2H + sync; pinned host buffers, device buffers and
  the cuBLAS handle are allocated once outside the loop, as a real
  application would) and `cublas_kernel` (CUDA events around the sgemm
  alone). `cublas_bench --selftest` validates the row-major layout mapping
  with non-uniform matrices before every sweep.
- **Kernel-only trsc numbers** (`--profile`) rebuild nothing: the runtime
  wrappers time each launch with CUDA events when `TRSC_PROFILE=1` is set
  and the orchestrator parses the per-launch times (rows labeled
  `{level}-kernel`).

### Fair-comparison caveats (read before quoting numbers)

- trsc GPU kernels operate on **device-resident buffers** (staged with
  `gpu.alloc` + `gpu.memcpy` around the launch), same as cuBLAS. End-to-end
  numbers include the PCIe transfers for both; `{level}-kernel` vs
  `cublas_kernel` is the apples-to-apples kernel comparison.
- `cublas_e2e` amortizes buffer/handle allocation outside the timed loop;
  `trsc_main()` re-allocates and re-pins host memory every rep. Expect a
  structural gap between trsc end-to-end and `cublas_e2e` beyond kernel
  quality — the closest trsc-vs-cuBLAS read is the kernel framing.
- Level 1 is a naive scalar CPU loop nest — it is the correctness baseline,
  not an optimized CPU implementation.
- GPU clocks are not locked (needs root); mitigations are warmup, medians,
  2 s sleeps between binaries, and a clock snapshot in `env.txt`.

### Plausibility gates

After a sweep, sanity-check before publishing:

- level 1: ~0.5–2 GFLOP/s; `cublas_kernel` at N=2048: ~1.5–2.5 TFLOP/s on a
  GTX 1650
- GPU levels roughly monotone 2 < 3 < 4 < 5 ≤ 6 at large N (the orchestrator
  warns on >5% inversions)
- with `--profile`: `kernel_ms` ≤ end-to-end ms for every rep
- two sweeps on different days should agree within ~5% on medians
