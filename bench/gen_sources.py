#!/usr/bin/env python3
"""Generate bench/generated/matmul_{N}.rs from the integration-test template.

The template must stay byte-identical to test/Integration/matmul.rs at N=32;
run with --check to enforce this (run_bench.py does so before every sweep).
"""

import argparse
import sys
from pathlib import Path

BENCH_DIR = Path(__file__).resolve().parent
REPO_ROOT = BENCH_DIR.parent
REFERENCE = REPO_ROOT / "test" / "Integration" / "matmul.rs"

TEMPLATE = """\
fn main() -> f32 {{
    let a: [[f32; {n}]; {n}] = [[1.0; {n}]; {n}];
    let b: [[f32; {n}]; {n}] = [[2.0; {n}]; {n}];
    let mut c: [[f32; {n}]; {n}] = [[0.0; {n}]; {n}];

    for i in 0..{n} {{
        for j in 0..{n} {{
            for k in 0..{n} {{
                c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }}
        }}
    }}
    return c[0][0];
}}
"""


def render(n: int) -> str:
    return TEMPLATE.format(n=n)


def check_template() -> None:
    expected = REFERENCE.read_text()
    got = render(32)
    if got != expected:
        sys.exit(
            "gen_sources.py: template drifted from test/Integration/matmul.rs;\n"
            "benchmarked program would differ from the tested one. Fix TEMPLATE."
        )


def generate(sizes: list[int]) -> list[Path]:
    out_dir = BENCH_DIR / "generated"
    out_dir.mkdir(exist_ok=True)
    paths = []
    for n in sizes:
        path = out_dir / f"matmul_{n}.rs"
        content = render(n)
        # Idempotent write keeps mtimes stable so run_bench's compile cache holds.
        if not path.exists() or path.read_text() != content:
            path.write_text(content)
        paths.append(path)
    return paths


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sizes", default="128,256,512,1024,2048",
                    help="comma-separated matrix sizes")
    ap.add_argument("--check", action="store_true",
                    help="only verify the template matches the integration test")
    args = ap.parse_args()

    check_template()
    if args.check:
        print("gen_sources.py: template matches test/Integration/matmul.rs")
        return
    for path in generate([int(s) for s in args.sizes.split(",")]):
        print(path)


if __name__ == "__main__":
    main()
