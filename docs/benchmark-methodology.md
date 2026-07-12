# Benchmark Methodology

How the numbers in [`bench/results/`](../bench/results/) and the README
performance table are measured, and why the comparison is framed the way
it is.

## Hardware and setup

- **GPU:** NVIDIA GTX 1650 (sm_75), f32 square matmul.
- **Baseline:** cuBLAS `sgemm`, measured both end-to-end and kernel-only.
- **Sizes:** N = 128, 256, 512, 1024, 2048 (N×N f32).
- **Reps:** median of 10 reps (3 for the CPU level) after 2 warmup calls.
- Environment details for each run are captured in
  [`bench/results/env.txt`](../bench/results/env.txt).

## What one rep measures

Timing is honest end-to-end: each rep is one full program call —

1. host matrix allocation and initialization,
2. device staging (`gpu.alloc` + H2D `gpu.memcpy`),
3. kernel launch,
4. D2H copy-back, and
5. sync

— measured with `CLOCK_MONOTONIC`. Every rep verifies the numerical
result before its time counts; a rep that produces a wrong answer is
discarded.

## End-to-end vs kernel-only framing

trsc kernels operate on device-resident buffers, same as cuBLAS. The
cuBLAS baseline is shown two ways:

- **End-to-end:** init + transfers + sgemm + sync, with buffers and the
  cuBLAS handle allocated once, as a real application would.
- **Kernel-only:** CUDA events around the sgemm launch alone.

Because trsc re-allocates host memory every rep while the cuBLAS
end-to-end loop does not, the **kernel-only framing is the fair
trsc-vs-cuBLAS comparison**. Kernel-only trsc times come from a
`--profile` sweep (`TRSC_PROFILE=1`, CUDA events around each launch) and
are the `<level>-kernel` rows in
[`bench/results/results.csv`](../bench/results/results.csv).

## Clock stability

GPU clocks are not locked. Mitigations: medians over reps, warmup calls
before timing, and per-run clock snapshots recorded alongside the
results.

## Reproducing

```bash
python3 bench/run_bench.py --all --profile
```

See [`bench/README.md`](../bench/README.md) for harness details, and
[`bench/results/findings.md`](../bench/results/findings.md) for analysis
of the results.
