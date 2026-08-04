#!/usr/bin/env bash
# Verify that forced CUDA execution fails cleanly when the driver is absent.
# Usage: run_cuda_unavailable_test.sh <trsc> <src.rs> <llvm-readelf>
set -euo pipefail

TRSC=$1
SRC=$2
READELF=$3

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

"$TRSC" --device=cuda --matmul-opt-level=2 "$SRC" -o "$WORK/cuda_test"

# The CUDA driver is loaded lazily. The executable must not acquire a hard
# dependency on libcuda.so, otherwise CPU-only CI cannot start it.
if "$READELF" -d "$WORK/cuda_test" | grep -q 'Shared library: \[libcuda\.so'; then
  echo "FAIL: forced CUDA executable has a hard libcuda dependency" >&2
  exit 1
fi

set +e
TRSC_CUDA_DRIVER_PATH=/definitely/missing/libcuda.so \
  "$WORK/cuda_test" >"$WORK/stdout" 2>"$WORK/stderr"
STATUS=$?
set -e

if [ "$STATUS" -ne 1 ]; then
  echo "FAIL: forced CUDA returned $STATUS instead of 1" >&2
  cat "$WORK/stderr" >&2
  exit 1
fi
if ! grep -Fqi "no compatible operational NVIDIA GPU" "$WORK/stderr"; then
  echo "FAIL: missing-CUDA diagnostic not found" >&2
  cat "$WORK/stderr" >&2
  exit 1
fi

echo "PASS: forced CUDA rejects an unavailable driver"
