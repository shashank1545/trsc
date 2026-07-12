# trsc Architecture

trsc is a compiler for a small Rust-like language that lowers, through MLIR, to
either native code or CUDA kernels. This document explains the design: how the
pipeline is staged, why a custom dialect sits in the middle of it, and how the
GEMM path chooses its code-generation strategy. It is written to be read
alongside the code, not instead of it — file pointers are given per section.

## Pipeline overview

```
 source (.rs-like)
      │
      ▼
 ┌───────────┐   hand-written lexer + recursive-descent parser
 │    AST    │   (lib/Lex, lib/Parse)
 └───────────┘
      │  Sema: symbol table → name resolution → type check → borrow check
      ▼         (lib/Sema)
 ┌───────────┐
 │ typed AST │
 └───────────┘
      │  MLIRGen (lib/MLIR/MLIRGen)
      ▼
 ┌──────────────────────────────┐
 │  trsc dialect + scf/memref   │   trscd.add/mul/…, scf.for loops,
 │  (linalg.fill for splats)    │   memref for aggregates
 └──────────────────────────────┘
      │  canonicalize + CSE, mem2reg, LICM, loop fusion
      ▼
 ┌──────────────────────────────┐
 │  canonical loop nests        │   the form pattern-raising expects
 └──────────────────────────────┘
      │  MatMul pipeline: recognition → epilogue fusion → (autotune) → lowering
      ▼
 ┌──────────────────────────────┐
 │  gpu.launch + vector/scf     │   one kernel body per trscd.gemm,
 │  (host code stays func/scf)  │   shape chosen by -O level
 └──────────────────────────────┘
      │  late loop opts (tile, unroll — skip GEMM-generated code)
      │  convert-linalg-to-loops, then upstream GPU→NVVM pipeline
      ▼
 ┌──────────────────────────────┐
 │  nvvm + llvm dialects        │   device: PTX → cubin (serialized "isa")
 └──────────────────────────────┘   host: LLVM IR → object → link
```

Driver orchestration lives in `tools/trsc/main.cpp`; the pipeline builders in
`lib/MLIR/Transforms/PassPipeline.cpp` and
`lib/MLIR/MatMulOpts/MatMulPassPipeline.cpp`.

Two ordering constraints shape the pipeline more than anything else:

1. **Recognition needs canonical IR.** GEMM raising matches one exact
   post-cleanup loop form, so canonicalize/CSE/mem2reg must run *before* the
   matmul pipeline, and generic loop tiling/unrolling must run *after* it —
   a tiled loop nest no longer matches the pattern. Late loop passes skip
   GEMM-generated code via a `trscd.gemm_generated` marker attribute.
2. **The NVVM pipeline never lowers linalg.** Any remaining linalg ops are
   converted to loops immediately before the upstream GPU→NVVM pipeline runs.

## Why a custom dialect

MLIRGen could emit arith/scf directly. It emits `trscd.*` ops first, for three
reasons:

**Progressive lowering keeps source semantics available.** `trscd.add` on the
language's types is a different contract than `arith.addf` on `f32` — the
dialect layer is where language-level rules (types resolved by Sema, future
overflow/ownership semantics) still exist as ops that passes can reason about.
Once lowered to arith, that information is gone. This mirrors how production
frontends (Flang FIR, CIRCT, ClangIR) structure their MLIR pipelines.

**`trscd.gemm` is a semantic anchor, not an instruction.** The dialect's most
important op carries no implementation; it asserts "this is a matrix multiply
of A×B into C with α/β" (`include/trsc/MLIR/TrscOps.td`). That single op
decouples three concerns that would otherwise be tangled in one pass:

- *what* the computation is (recognition raises loop nests to `trscd.gemm`),
- *how well* to compile it (a tuning pass attaches a `tiling_params`
  dictionary attribute to the op — decisions travel with the op, not in pass
  state),
- *how* to implement it (one lowering pass reads the op + attributes and emits
  a kernel; strategy selected by optimization level).

New producers (a library `matmul()` call, a future tensor type) and new
lowering strategies (a tensor-core path) plug into the same op without
touching each other.

**Attributes as the pass-to-pass contract.** Because tuning decisions are
attributes on the op, they are visible in `-emit-mlir` dumps, testable with
FileCheck, and overridable from outside the compiler. There is no hidden
side-channel between the tuner and the code generator.

The deliberate tradeoff: upstream `linalg.matmul` would come with existing
transformations for free. The custom op was chosen because the interesting
transformations here (kernel-shape selection, epilogue fusion, the tuning
contract) are exactly the ones that needed custom control, and the raising
pass produces the op from plain loops either way.

## Raising: GEMM recognition

`lib/MLIR/MatMulOpts/MatMulRecognition.cpp` pattern-matches a triple loop nest
against the canonical form

```
scf.for %i { scf.for %j { scf.for %k {
  C[i,j] += A[i,k] * B[k,j]
}}}
```

with exact dataflow checks: exactly three loads / one mul / one add / one
store in the innermost body, `store(add(load C, mul(load A, load B)))`, index
expressions traced through `arith.index_cast` chains back to the induction
variables, and the store aliasing the C load. A/B operand order is recovered
from the index pattern (`A[i,k]`, `B[k,j]`), so commuted multiplies still
match. Anything that fails a check is left untouched — raising is
opportunistic and sound by construction: no match, no transformation.

Recognition-then-lowering (rather than "the user calls a gemm intrinsic") is
the design point worth defending: it means ordinary user-written loops get the
optimized path, and it forces the earlier pipeline to define a canonical loop
form — which is also what makes the pass testable in isolation.

After raising, `GemmEpilogueFusion.cpp` pulls adjacent element-wise consumers
(bias add, activation) into the gemm op so the lowering can emit them inside
the kernel's write-back, instead of a second pass over the output in global
memory.

## Lowering strategy: the optimization ladder

`GemmLowering.cpp` implements one lowering per optimization level. This is
deliberately a *ladder*, not a single kernel: each level adds exactly one
technique to the previous one, so any performance delta is attributable to
that technique (the `bench/` harness measures each rung). Each rung addresses
the next bottleneck the previous rung exposes:

| Level | Adds | Bottleneck addressed |
|-------|------|----------------------|
| 1 | naive `gpu.launch`, one thread per C element | correctness baseline |
| 2 | thread-index remap for coalesced GMEM access | DRAM transaction efficiency |
| 3 | shared-memory tiling (BM×BK, BK×BN tiles) | redundant GMEM traffic |
| 4 | 1D block-tiling, TM results per thread | arithmetic intensity |
| 5 | 2D block-tiling, TM×TN register accumulators | arithmetic intensity, again |
| 6 | `vector<4xf32>` GMEM loads; A stored transposed in SMEM | load pipe width, SMEM access pattern |
| 7 | double-buffered SMEM, register-staged prefetch | latency hiding (sm_75 has no `cp.async`) |
| 8 | warp tiling (WM×WN, WNITER subtiles) over level 7 | SMEM broadcast/coherence within warps |

The tiling scheme at the top of the ladder is the standard three-level
decomposition of the memory hierarchy:

- **Block tile** BM×BN, marching over K in BK steps — sizes the SMEM working
  set (`2·(BM+BN)·BK·4` bytes with double buffering) and the grid.
- **Warp tile** WM×WN, iterated in WNITER column subtiles — keeps each warp's
  SMEM reads row-coherent.
- **Thread tile** TM×TN — the per-thread register accumulator block; the
  FMA loop body is fully unrolled into SSA values so accumulators live in
  registers, never local memory.

Every shape is checked for legality before use (divisibility, and that the
warp tile decomposes into 32 lanes of TM×TN patches); illegal shapes fall
back to known-good defaults rather than miscompiling.

## Tuning: search space and cost model

The tunable point is one struct of kernel-shape parameters:

```
BM, BN, BK      block tile
TM, TN          thread tile
WM, WN, WNITER  warp tile (level 8)
vector width    fixed at 4 (128-bit transactions)
```

Three sources supply it, in priority order:

1. **`tiling_params` attribute** on the gemm op, set by the autotuning pass
   (`AutoTuning.cpp`, enabled at the top optimization level).
2. **`TRSC_GEMM_TILES` environment override** (`BM,BN,BK,TM,TN[,WM,WN,WNITER]`)
   — this exists *for* the tuner: it lets a sweep harness re-run the same
   binary across the search space without recompiling the compiler.
3. **Per-level defaults**, the result of an offline sweep on the development
   GPU (GTX 1650, sm_75): 16 legal configurations × three problem sizes
   (N = 512/1024/2048), best-throughput shape baked in (level 8 lands on
   64×64×16 block, 4×4 thread, 32×32 warp tiles — 128-thread blocks, 32
   accumulators/thread, 16 KB double-buffered SMEM).

The cost model is two-stage, cheap-first:

**Stage 1 — analytic pruning.** Legality and resource constraints eliminate
most of the space before anything runs: divisibility of tiles, warp
decomposition into 32 lanes, SMEM budget (48 KB/block on sm_75), and register
pressure (TM×TN accumulators plus staging registers per thread — past ~64
accumulators, occupancy collapses and no memory-hierarchy win recovers it).
This is why the swept space is 16 configurations and not thousands.

**Stage 2 — empirical measurement.** Among analytically-plausible shapes,
measured throughput decides. An analytic model alone was rejected: on real
hardware, SMEM bank conflicts, achieved occupancy, and the interaction between
unroll factor and the compiler's register allocator are where the performance
actually moves, and none of them are predictable to the ~20% precision the
final choice requires. Measuring is cheap here because the search space is
already pruned and the parameter plumbing (env var → kernel shape) makes each
probe a process launch, not a rebuild.

Known limitation, by design: the tuning pass currently applies one
Cutlass-style default shape rather than searching per problem size at compile
time. The architecture anticipates the upgrade — the search loop belongs in
the tuning pass, the measurement channel already exists, and the lowering
already honors whatever shape arrives in `tiling_params`.

## Backend

Device-side lowering reuses the upstream MLIR GPU→NVVM pipeline
(`buildLowerToNVVMPassPipeline`, opt level 3, cubin serialized from PTX);
host-side falls through the standard LLVM path to an object file and links
against a thin CUDA runtime wrapper (`lib/Runtime/CudaRuntimeWrappers.cpp`).
Reusing the upstream pipeline is a scoping decision: the novel work in this
compiler is deciding *what kernel to build* — the mechanics of NVVM
conversion, PTX emission, and cubin embedding are commodity and better
maintained upstream.
