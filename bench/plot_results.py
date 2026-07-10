#!/usr/bin/env python3
"""Render benchmark charts + markdown table from bench/results/results.csv.

Outputs (into bench/results/):
    gflops_vs_size.png   GFLOPS (median) vs N, per level + cuBLAS references
    pct_of_cublas.png    % of cuBLAS at the largest common size, per GPU level
    kernel_vs_cublas.png kernel-only comparison (only if --profile data exists)
    results_table.md     size x level GFLOPS pivot for the README

Requires matplotlib.
"""

import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, NullFormatter, ScalarFormatter

RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Categorical slots 1-6 (validated light-mode palette), fixed order:
# level 1..6 always wears the same hue regardless of which levels ran.
SLOT = {
    "1": "#2a78d6", "2": "#1baf7a", "3": "#eda100",
    "4": "#008300", "5": "#4a3aa7", "6": "#e34948",
}
LEVEL_NAME = {
    "1": "L1 CPU naive", "2": "L2 GMEM coalesced", "3": "L3 SMEM tiling",
    "4": "L4 1D blocktile", "5": "L5 2D blocktile", "6": "L6 vectorized",
}
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASELINE = "#c3c2b7"


def style():
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE,
        "savefig.facecolor": SURFACE, "font.family": "sans-serif",
        "text.color": INK, "axes.labelcolor": INK2,
        "xtick.color": MUTED, "ytick.color": MUTED,
        "axes.edgecolor": BASELINE, "axes.linewidth": 1.0,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
        "axes.spines.top": False, "axes.spines.right": False,
        "legend.frameon": False, "figure.dpi": 150,
    })


def load() -> list[dict]:
    path = RESULTS_DIR / "results.csv"
    if not path.exists():
        sys.exit(f"plot_results.py: {path} not found — run run_bench.py first")
    with path.open() as f:
        return list(csv.DictReader(f))


def series(rows, label):
    pts = sorted((int(r["size"]), float(r["gflops_median"]))
                 for r in rows if r["level"] == label)
    return [p[0] for p in pts], [p[1] for p in pts]


def log2_axis(ax, sizes):
    ax.set_xscale("log", base=2)
    ax.xaxis.set_major_locator(FixedLocator(sorted(sizes)))
    ax.xaxis.set_major_formatter(ScalarFormatter())
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("matrix size N (N×N f32)")


def end_labels(ax, ends):
    """Direct-label line ends, nudging apart entries too close on a log axis."""
    ends = sorted(ends, key=lambda e: e[1])
    prev = None
    for x, y, text, color, bold in ends:
        ypos = y
        if prev is not None and ypos / prev < 1.25:  # too close on log scale
            ypos = prev * 1.25
        prev = ypos
        ax.annotate(text, (x, ypos), xytext=(6, 0),
                    textcoords="offset points", color=color, fontsize=8,
                    fontweight="bold" if bold else "normal", va="center")


def plot_gflops(rows):
    fig, ax = plt.subplots(figsize=(8, 5))
    all_sizes = {int(r["size"]) for r in rows}
    levels = sorted({r["level"] for r in rows if r["level"] in SLOT})
    ends = []
    for lv in levels:
        xs, ys = series(rows, lv)
        ax.plot(xs, ys, color=SLOT[lv], linewidth=2, marker="o",
                markersize=5, label=LEVEL_NAME[lv])
        ends.append((xs[-1], ys[-1], f"L{lv}", SLOT[lv], True))
    for label, dash, tone in (("cublas_kernel", (4, 2), INK2),
                              ("cublas_e2e", (1, 2), MUTED)):
        xs, ys = series(rows, label)
        if xs:
            ax.plot(xs, ys, color=tone, linewidth=2, linestyle=(0, dash),
                    label=label.replace("cublas_", "cuBLAS "))
            ends.append((xs[-1], ys[-1],
                         label.replace("cublas_", "cuBLAS "), tone, False))
    end_labels(ax, ends)
    ax.set_yscale("log")
    log2_axis(ax, all_sizes)
    ax.set_ylabel("GFLOP/s (median, 2N³/t)")
    ax.set_title("trsc matmul throughput by optimization level — GTX 1650",
                 color=INK, fontsize=11)
    ax.legend(fontsize=8, loc="upper left", ncols=2)
    fig.tight_layout()
    fig.savefig(RESULTS_DIR / "gflops_vs_size.png", bbox_inches="tight")
    plt.close(fig)


def plot_pct(rows):
    sizes_by_level = {}
    for r in rows:
        sizes_by_level.setdefault(r["level"], set()).add(int(r["size"]))
    gpu_levels = sorted(l for l in sizes_by_level if l in SLOT and int(l) >= 2)
    ref_labels = [l for l in ("cublas_kernel", "cublas_e2e")
                  if l in sizes_by_level]
    if not gpu_levels or "cublas_kernel" not in ref_labels:
        return
    common = set.intersection(*(sizes_by_level[l]
                                for l in gpu_levels + ref_labels))
    n = max(common)
    gf = {r["level"]: float(r["gflops_median"])
          for r in rows if int(r["size"]) == n}

    fig, ax = plt.subplots(figsize=(7, 4.2))
    ax.grid(axis="x", visible=False)
    width = 0.38
    xs = range(len(gpu_levels))
    for i, (ref, tone) in enumerate((("cublas_kernel", INK2),
                                     ("cublas_e2e", MUTED))):
        if ref not in gf:
            continue
        vals = [100.0 * gf[lv] / gf[ref] for lv in gpu_levels]
        offs = [x + (i - 0.5) * width for x in xs]
        bars = ax.bar(offs, vals, width=width * 0.94,
                      color=[SLOT[lv] for lv in gpu_levels],
                      alpha=1.0 if i == 0 else 0.45,
                      edgecolor=SURFACE, linewidth=1)
        for b, v in zip(bars, vals):
            ax.annotate(f"{v:.1f}%", (b.get_x() + b.get_width() / 2, v),
                        xytext=(0, 3), textcoords="offset points",
                        ha="center", fontsize=8, color=INK2)
    ax.set_xticks(list(xs))
    ax.set_xticklabels([LEVEL_NAME[lv] for lv in gpu_levels], fontsize=8)
    ax.set_ylabel("% of cuBLAS GFLOP/s")
    ax.set_title(f"trsc GPU levels vs cuBLAS at N={n} "
                 "(solid: vs kernel-only, faded: vs end-to-end)",
                 color=INK, fontsize=10)
    fig.tight_layout()
    fig.savefig(RESULTS_DIR / "pct_of_cublas.png", bbox_inches="tight")
    plt.close(fig)


def plot_kernel(rows):
    kernel_levels = sorted({r["level"] for r in rows
                            if r["level"].endswith("-kernel")})
    if not kernel_levels:
        return
    fig, ax = plt.subplots(figsize=(8, 5))
    all_sizes = {int(r["size"]) for r in rows}
    ends = []
    for lv in kernel_levels:
        base = lv.split("-")[0]
        xs, ys = series(rows, lv)
        ax.plot(xs, ys, color=SLOT.get(base, MUTED), linewidth=2, marker="o",
                markersize=5, label=f"{LEVEL_NAME.get(base, base)} (kernel)")
        ends.append((xs[-1], ys[-1], f"L{base}", SLOT.get(base, MUTED), True))
    xs, ys = series(rows, "cublas_kernel")
    if xs:
        ax.plot(xs, ys, color=INK2, linewidth=2, linestyle=(0, (4, 2)),
                label="cuBLAS kernel")
        ends.append((xs[-1], ys[-1], "cuBLAS", INK2, False))
    end_labels(ax, ends)
    ax.set_yscale("log")
    log2_axis(ax, all_sizes)
    ax.set_ylabel("GFLOP/s (median, kernel time only)")
    ax.set_title("Kernel-only throughput (TRSC_PROFILE) vs cuBLAS — GTX 1650",
                 color=INK, fontsize=11)
    ax.legend(fontsize=8, loc="upper left", ncols=2)
    fig.tight_layout()
    fig.savefig(RESULTS_DIR / "kernel_vs_cublas.png", bbox_inches="tight")
    plt.close(fig)


def write_table(rows):
    labels = sorted({r["level"] for r in rows},
                    key=lambda l: (not l.isdigit(), l))
    sizes = sorted({int(r["size"]) for r in rows})
    gf = {(int(r["size"]), r["level"]): r["gflops_median"] for r in rows}
    lines = ["| N | " + " | ".join(labels) + " |",
             "|" + "---|" * (len(labels) + 1)]
    for n in sizes:
        cells = [str(gf.get((n, l), "—")) for l in labels]
        lines.append(f"| {n} | " + " | ".join(cells) + " |")
    lines.append("")
    lines.append("GFLOP/s, median over reps. Levels 1–6 are `--matmul-opt-level`;"
                 " `-kernel` rows are kernel-only (`TRSC_PROFILE=1`).")
    (RESULTS_DIR / "results_table.md").write_text("\n".join(lines) + "\n")


def main():
    style()
    rows = load()
    plot_gflops(rows)
    plot_pct(rows)
    plot_kernel(rows)
    write_table(rows)
    for f in ("gflops_vs_size.png", "pct_of_cublas.png", "kernel_vs_cublas.png",
              "results_table.md"):
        p = RESULTS_DIR / f
        if p.exists():
            print(p)


if __name__ == "__main__":
    main()
