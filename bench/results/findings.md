# Benchmark Findings

Analysis of the matmul optimization ladder results in this directory.
For how the numbers are measured, see
[`docs/benchmark-methodology.md`](../../docs/benchmark-methodology.md).

## Headline

At N=2048 the kernel-only ladder reaches L2 95 / L3 310 / L4 514 /
L5 682 / L6 912 / L7 904 / L8 1009 GFLOP/s against a 1398 GFLOP/s cuBLAS
sgemm — 6.8 / 22.1 / 36.8 / 48.8 / 65.2 / 64.7 / 72.2% of cuBLAS.

This tracks the reference percentages from
[Boehm's CUDA matmul worklog](https://siboehm.com/articles/22/CUDA-MMM):
his kernels 2–6 on an A6000 hit 8.5 / 12.8 / 36.5 / 68.7 / 78.4% of
cuBLAS; trsc L2–L6 on a GTX 1650 hit 6.8 / 22.1 / 36.8 / 48.8 / 65.2%.

## Two fixes that got the ladder on track

### 1. Device staging instead of zero-copy host memory

Kernels originally read operands from pinned host memory
(`gpu.host_register` zero-copy), so every access crossed PCIe
(~5.6 GB/s observed) and capped all levels near 45 GFLOP/s. Operands are
now staged in device memory around the launch (`gpu.alloc` + H2D
`gpu.memcpy`).

### 2. Registers instead of local memory for accumulators

L4–L6 kept per-thread accumulators in `memref.alloca` arrays indexed by
loop induction variables, which NVPTX lowers to off-chip local memory
(`ld.local`/`st.local` inside the FMA loop) — the reason L5/L6
originally benched 3–4× *slower* than L3. The thread tile is now fully
unrolled at IR-build time with accumulators as `scf.for` iter_args
(registers), and L6 additionally stores the A tile transposed in SMEM so
fragments load as `vector<4xf32>`.

## Levels 7 and 8

- **L7 (double-buffered SMEM)** lands at L6 parity: the sm_75 kernel has
  no `cp.async`, so prefetches are staged through registers and the
  64-thread blocks stay issue-bound.
- **L8 (warp tiling)**, with tile shapes autotuned for this card
  (`TRSC_GEMM_TILES` override, best `64,64,16,4,4,32,32,2`), lifts the
  kernel to 72% of cuBLAS.

## Known limitation: small sizes

Small sizes (N ≤ 256) still show inversions — grids of a few blocks
can't fill the SMs and launch overhead dominates.
