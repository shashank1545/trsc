#!/usr/bin/env bash
# End-to-end integration test for one matmul optimization level.
#
# Compiles matmul.rs to an object file with trsc, renames its `main` to
# `trsc_main`, links it against the C harness (which prints the f32 result),
# runs the binary and compares the printed value with the expected result.
#
# Usage: run_matmul_test.sh <trsc> <level> <src.rs> <harness.c> \
#                           <runtime.a> <objcopy> <cc> <expected>
set -euo pipefail

TRSC=$1
LEVEL=$2
SRC=$3
HARNESS=$4
RUNTIME=$5
OBJCOPY=$6
CC=$7
EXPECTED=$8

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

"$TRSC" --matmul-opt-level="$LEVEL" -emit-obj "$SRC" -o "$WORK/mm.o"
"$OBJCOPY" --redefine-sym main=trsc_main "$WORK/mm.o"
"$CC" "$HARNESS" "$WORK/mm.o" "$RUNTIME" \
  -Wl,--as-needed -lcuda -lstdc++ -lm -o "$WORK/mm_test"

ACTUAL=$("$WORK/mm_test")

if [ "$ACTUAL" != "$EXPECTED" ]; then
  echo "FAIL: level $LEVEL produced '$ACTUAL', expected '$EXPECTED'" >&2
  exit 1
fi
echo "PASS: level $LEVEL -> $ACTUAL"
