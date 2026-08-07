#!/usr/bin/env bash
# Compiles and runs a println! program and diffs its stdout against the
# expected text. The optimisation level is a parameter because println!
# declares module-level runtime symbols, which previously only broke once the
# matmul pipeline ran: level 0 alone would not have caught it.
set -euo pipefail

trsc=$1
source=$2
opt_level=${3:-}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

if [[ -n "$opt_level" ]]; then
  "$trsc" "--matmul-opt-level=$opt_level" "$source" -o "$work/println"
else
  "$trsc" "$source" -o "$work/println"
fi

# The exit status is captured rather than let through `set -e`: trsc's `main`
# returns whatever happens to be in the return register, so the status is
# garbage (see integration_driver_link). Only a signal death is a real failure,
# and stdout is what this test is actually about.
set +e
"$work/println" > "$work/output"
status=$?
set -e
if [[ $status -ge 128 ]]; then
  printf 'println program died on signal (status %d)\n' "$status" >&2
  exit 1
fi

cat > "$work/expected" <<'EOF'
8 16 64 3.5 2.25 true
true
literal {} and newline

quote:" backslash:\ tab:	end

0.3333333333333333 10000000000 1 0.1
iter 0
iter 1
iter 2
doubled 42
EOF

if ! diff -u "$work/expected" "$work/output" >&2; then
  printf 'unexpected println output (expected vs actual above)\n' >&2
  exit 1
fi
