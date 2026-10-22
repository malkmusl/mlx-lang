#!/usr/bin/env bash
# The inliner with the self-hosted compiler: tests/393_inline_own_names_runtime.mlx
# runs at the default level (-O2), at -O3 and at -O0 (an inlined function
# reads its own module's variables, not the caller's locals of the same
# names).
#
#   tests/run_inline.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0
for level in "" -O3 -O0; do
    status=0
    if ! "$compiler" --quiet $level tests/393_inline_own_names_runtime.mlx -o "$work/inline"; then
        echo "FAIL 393_inline_own_names_runtime ${level:-default level}: did not compile"
        failures=$((failures + 1))
        continue
    fi
    "$work/inline" || status=$?
    if [[ $status -eq 13 ]]; then
        echo "ok   393_inline_own_names_runtime ${level:-(default level)}: inlined functions read their own module's variables"
    else
        echo "FAIL 393_inline_own_names_runtime ${level:-default level}: exited $status"
        failures=$((failures + 1))
    fi
done
[[ $failures -eq 0 ]] || exit 1
echo "run_inline.sh: all checks passed"
