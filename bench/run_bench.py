#!/usr/bin/env python3
"""Orchestrate the trsc matmul benchmark sweep.

Compiles one trsc object per (size, level), links each against
harness_bench.c using the same recipe as test/Integration/run_matmul_test.sh,
runs everything size-ascending, and aggregates results/raw.csv into
results/results.csv (gflops = 2*n^3 / (ms * 1e6)).

Typical use:
    python3 bench/run_bench.py --all            # full sweep + cuBLAS + plots
    python3 bench/run_bench.py --sizes 128 --levels 1,6 --reps 3   # smoke
    python3 bench/run_bench.py --all --profile  # add kernel-only series
"""

import argparse
import csv
import os
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

BENCH_DIR = Path(__file__).resolve().parent
REPO_ROOT = BENCH_DIR.parent
GEN_DIR = BENCH_DIR / "generated"
WORK_DIR = BENCH_DIR / "work"
RESULTS_DIR = BENCH_DIR / "results"

DEFAULT_SIZES = [128, 256, 512, 1024, 2048]
BIG_SIZES = [4096]
DEFAULT_LEVELS = [1, 2, 3, 4, 5, 6]
CPU_LEVELS = {0, 1}
CPU_SIZE_CAP = 1024  # naive CPU beyond this is ~minutes per rep
CPU_REPS = 3


def log(msg: str) -> None:
    print(f"[bench] {msg}", file=sys.stderr, flush=True)


def run(cmd, dry_run=False, env=None, capture=False):
    display = " ".join(str(c) for c in cmd)
    if dry_run:
        print(f"DRY: {display}")
        return None
    result = subprocess.run(
        [str(c) for c in cmd], env=env,
        capture_output=capture, text=True, cwd=REPO_ROOT)
    if result.returncode != 0:
        if capture:
            sys.stderr.write(result.stderr or "")
        raise RuntimeError(f"command failed ({result.returncode}): {display}")
    return result


def find_cuda_home(arg: str | None) -> Path | None:
    candidates = []
    if arg:
        candidates.append(Path(arg))
    if os.environ.get("CUDA_HOME"):
        candidates.append(Path(os.environ["CUDA_HOME"]))
    nvcc = shutil.which("nvcc")
    if nvcc:
        candidates.append(Path(nvcc).resolve().parent.parent)
    candidates += [Path("/opt/cuda"), Path("/usr/local/cuda")]
    for c in candidates:
        if (c / "include" / "cublas_v2.h").exists():
            return c
    return None


def sanity(build_dir: Path, need_gpu: bool) -> tuple[Path, Path]:
    trsc = build_dir / "tools" / "trsc" / "trsc"
    runtime = build_dir / "lib" / "libTrscCudaRuntime.a"
    for path, name in [(trsc, "trsc"), (runtime, "libTrscCudaRuntime.a")]:
        if not path.exists():
            sys.exit(
                f"[bench] {name} not found at {path}.\n"
                f"Configure and build first:\n"
                f"  cmake -B {build_dir} -DLLVM_DIR=<llvm>/lib/cmake/llvm "
                f"-DMLIR_DIR=<llvm>/lib/cmake/mlir\n"
                f"  cmake --build {build_dir} -j8")
    if need_gpu:
        try:
            run(["nvidia-smi", "-L"], capture=True)
        except (FileNotFoundError, RuntimeError):
            sys.exit("[bench] nvidia-smi failed — GPU levels (>=2) need a "
                     "working NVIDIA driver. Use --levels 0,1 for CPU only.")
    return trsc, runtime


def snapshot_env() -> None:
    RESULTS_DIR.mkdir(exist_ok=True)
    lines = [f"date: {time.strftime('%Y-%m-%d %H:%M:%S %z')}"]
    for cmd in (
        ["uname", "-sr"],
        ["nvidia-smi", "--query-gpu=name,driver_version,clocks.sm,clocks.mem,temperature.gpu",
         "--format=csv"],
    ):
        try:
            out = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            lines.append(f"$ {' '.join(cmd)}\n{out.stdout.strip()}")
        except (FileNotFoundError, subprocess.TimeoutExpired):
            lines.append(f"$ {' '.join(cmd)}\n(unavailable)")
    (RESULTS_DIR / "env.txt").write_text("\n".join(lines) + "\n")


def needs_rebuild(target: Path, deps: list[Path]) -> bool:
    if not target.exists():
        return True
    t = target.stat().st_mtime
    return any(d.stat().st_mtime > t for d in deps)


def compile_variant(trsc: Path, runtime: Path, n: int, level: int,
                    dry_run: bool) -> Path:
    WORK_DIR.mkdir(exist_ok=True)
    src = GEN_DIR / f"matmul_{n}.rs"
    obj = WORK_DIR / f"mm_{n}_l{level}.o"
    binary = WORK_DIR / f"mm_{n}_l{level}"
    harness = BENCH_DIR / "harness_bench.c"
    deps = [src, harness, trsc, runtime]
    if not dry_run and not needs_rebuild(binary, deps):
        log(f"cache hit: {binary.name}")
        return binary
    t0 = time.monotonic()
    run([trsc, f"--matmul-opt-level={level}", "-emit-obj", src, "-o", obj],
        dry_run=dry_run)
    run(["objcopy", "--redefine-sym", "main=trsc_main", obj], dry_run=dry_run)
    run(["cc", harness, obj, runtime,
         "-Wl,--as-needed", "-lcuda", "-lstdc++", "-lm", "-o", binary],
        dry_run=dry_run)
    if not dry_run:
        log(f"compiled {binary.name} in {time.monotonic() - t0:.1f}s")
    return binary


def compile_cublas(cuda_home: Path, dry_run: bool) -> Path:
    binary = WORK_DIR / "cublas_bench"
    src = BENCH_DIR / "cublas_bench.c"
    WORK_DIR.mkdir(exist_ok=True)
    if not dry_run and not needs_rebuild(binary, [src]):
        return binary
    run(["cc", src,
         f"-I{cuda_home}/include", f"-L{cuda_home}/lib64",
         "-lcublas", "-lcudart", f"-Wl,-rpath,{cuda_home}/lib64",
         "-O2", "-o", binary], dry_run=dry_run)
    return binary


def parse_rows(stdout: str) -> list[dict]:
    rows = []
    for line in stdout.splitlines():
        parts = line.strip().split(",")
        if len(parts) == 5:
            rows.append(dict(zip(["size", "level", "rep", "ms", "ok"], parts)))
    return rows


def run_variant(binary: Path, n: int, label: str, warmup: int, reps: int,
                dry_run: bool, profile: bool = False) -> list[dict]:
    env = None
    if profile:
        env = dict(os.environ, TRSC_PROFILE="1")
    cmd = [binary, str(n), label, str(warmup), str(reps)]
    if dry_run:
        prefix = "TRSC_PROFILE=1 " if profile else ""
        print(f"DRY: {prefix}{' '.join(str(c) for c in cmd)}")
        return []
    result = subprocess.run([str(c) for c in cmd], env=env,
                            capture_output=True, text=True, cwd=REPO_ROOT)
    if result.returncode != 0:
        log(f"FAILED (exit {result.returncode}): {binary.name} — "
            f"{(result.stderr or '').strip().splitlines()[-1] if result.stderr else 'no stderr'}")
        return [{"size": str(n), "level": label, "rep": "0", "ms": "",
                 "ok": "0"}]
    rows = parse_rows(result.stdout)
    if profile:
        kernel_ms = [float(line.split("kernel_ms=")[1])
                     for line in (result.stderr or "").splitlines()
                     if "TRSC_PROFILE kernel_ms=" in line]
        total_calls = warmup + reps
        if kernel_ms and len(kernel_ms) % total_calls == 0:
            per_call = len(kernel_ms) // total_calls  # kernels per trsc_main()
            sums = [sum(kernel_ms[i * per_call:(i + 1) * per_call])
                    for i in range(total_calls)]
            rows = [{"size": str(n), "level": f"{label}-kernel", "rep": str(r),
                     "ms": f"{sums[warmup + r]:.3f}", "ok": "1"}
                    for r in range(reps)]
        else:
            log(f"profile parse skipped for {binary.name}: {len(kernel_ms)} "
                f"kernel_ms lines not divisible by {total_calls} calls")
            rows = []
    return rows


def aggregate(raw_rows: list[dict]) -> list[dict]:
    grouped: dict[tuple[int, str], list[float]] = {}
    for row in raw_rows:
        if row["ok"] != "1" or not row["ms"]:
            continue
        grouped.setdefault((int(row["size"]), row["level"]), []).append(
            float(row["ms"]))
    out = []
    for (n, level), ms in sorted(grouped.items()):
        med = statistics.median(ms)
        out.append({
            "size": n, "level": level, "reps": len(ms),
            "ms_median": round(med, 3), "ms_min": round(min(ms), 3),
            "ms_stddev": round(statistics.stdev(ms), 3) if len(ms) > 1 else 0.0,
            "gflops_median": round(2 * n**3 / (med * 1e6), 2),
            "gflops_best": round(2 * n**3 / (min(ms) * 1e6), 2),
        })
    return out


def warn_inversions(agg: list[dict]) -> None:
    by_size: dict[int, dict[str, float]] = {}
    for row in agg:
        if str(row["level"]).isdigit():
            by_size.setdefault(row["size"], {})[row["level"]] = row["gflops_median"]
    for n, levels in sorted(by_size.items()):
        ordered = sorted((int(l), g) for l, g in levels.items() if int(l) >= 2)
        for (l1, g1), (l2, g2) in zip(ordered, ordered[1:]):
            if g2 < g1 * 0.95:  # >5% regression up the ladder
                log(f"WARNING inversion at N={n}: level {l2} "
                    f"({g2} GF) < level {l1} ({g1} GF)")


def write_csv(path: Path, rows: list[dict], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sizes", help="comma-separated sizes "
                    f"(default {','.join(map(str, DEFAULT_SIZES))})")
    ap.add_argument("--levels", help="comma-separated matmul-opt levels "
                    f"(default {','.join(map(str, DEFAULT_LEVELS))})")
    ap.add_argument("--reps", type=int, default=10)
    ap.add_argument("--warmup", type=int, default=2)
    ap.add_argument("--big", action="store_true", help="add N=4096")
    ap.add_argument("--skip-cublas", action="store_true")
    ap.add_argument("--profile", action="store_true",
                    help="also collect kernel-only times (TRSC_PROFILE=1; "
                    "needs runtime built with the instrumented wrappers)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the command matrix without running")
    ap.add_argument("--all", action="store_true",
                    help="full default sweep + cuBLAS + plots")
    ap.add_argument("--build-dir", default=str(REPO_ROOT / "build"))
    ap.add_argument("--cuda-home")
    args = ap.parse_args()

    sizes = ([int(s) for s in args.sizes.split(",")] if args.sizes
             else list(DEFAULT_SIZES))
    if args.big:
        sizes += [s for s in BIG_SIZES if s not in sizes]
    levels = ([int(l) for l in args.levels.split(",")] if args.levels
              else list(DEFAULT_LEVELS))
    if 9 in levels:
        sys.exit("[bench] level 9 lowering is not implemented; refusing.")

    need_gpu = any(l >= 2 for l in levels) or not args.skip_cublas
    build_dir = Path(args.build_dir)
    if not args.dry_run:
        trsc, runtime = sanity(build_dir, need_gpu)
    else:
        trsc = build_dir / "tools" / "trsc" / "trsc"
        runtime = build_dir / "lib" / "libTrscCudaRuntime.a"

    # Template check + source generation (fails loudly on drift).
    run([sys.executable, BENCH_DIR / "gen_sources.py", "--check"])
    run([sys.executable, BENCH_DIR / "gen_sources.py",
         "--sizes", ",".join(map(str, sizes))], capture=True)

    cublas = None
    if not args.skip_cublas:
        cuda_home = find_cuda_home(args.cuda_home)
        if cuda_home is None:
            if args.dry_run:
                cuda_home = Path("<CUDA_HOME>")
            else:
                sys.exit("[bench] CUDA toolkit not found (need cublas_v2.h). "
                         "Pass --cuda-home or --skip-cublas.")
        cublas = compile_cublas(cuda_home, args.dry_run)
        if not args.dry_run:
            run([cublas, "--selftest"], capture=True)
            log("cuBLAS selftest passed")

    # Compile matrix.
    plan = []
    for n in sorted(sizes):
        for level in levels:
            if level in CPU_LEVELS and n > CPU_SIZE_CAP:
                log(f"skip level {level} at N={n} (CPU cap {CPU_SIZE_CAP})")
                continue
            plan.append((n, level))
    binaries = {(n, l): compile_variant(trsc, runtime, n, l, args.dry_run)
                for n, l in plan}

    # Run matrix, size-ascending; cuBLAS last per size.
    raw: list[dict] = []
    for n in sorted(sizes):
        for level in levels:
            if (n, level) not in binaries:
                continue
            reps = CPU_REPS if level in CPU_LEVELS else args.reps
            log(f"run N={n} level={level} reps={reps}")
            raw += run_variant(binaries[(n, level)], n, str(level),
                               args.warmup, reps, args.dry_run)
            if args.profile and level >= 2:
                raw += run_variant(binaries[(n, level)], n, str(level),
                                   args.warmup, reps, args.dry_run,
                                   profile=True)
            if not args.dry_run:
                time.sleep(2)  # thermal settling between binaries
        if cublas is not None:
            for mode in ("e2e", "kernel"):
                log(f"run N={n} cublas_{mode}")
                raw += run_variant(cublas, n, mode, args.warmup, args.reps,
                                   args.dry_run)
                if not args.dry_run:
                    time.sleep(2)

    if args.dry_run:
        return

    RESULTS_DIR.mkdir(exist_ok=True)
    write_csv(RESULTS_DIR / "raw.csv", raw,
              ["size", "level", "rep", "ms", "ok"])
    agg = aggregate(raw)
    write_csv(RESULTS_DIR / "results.csv", agg,
              ["size", "level", "reps", "ms_median", "ms_min", "ms_stddev",
               "gflops_median", "gflops_best"])
    warn_inversions(agg)
    snapshot_env()
    failures = [r for r in raw if r["ok"] != "1"]
    if failures:
        log(f"{len(failures)} FAILED variants: "
            + ", ".join(f"N={r['size']} L={r['level']}" for r in failures))
    log(f"wrote {RESULTS_DIR / 'raw.csv'} ({len(raw)} rows), "
        f"results.csv ({len(agg)} groups)")

    if args.all:
        run([sys.executable, BENCH_DIR / "plot_results.py"])


if __name__ == "__main__":
    main()
