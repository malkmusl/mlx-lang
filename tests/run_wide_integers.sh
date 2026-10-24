#!/usr/bin/env bash
# Integers wider than a word with the self-hosted compiler (spec
# operators.xml, ArbitraryWidth; compiler/selfhost/ir/lowering/wide.mlx):
# tests/341_wide_integers_runtime.mlx runs (u128, i128 and u100 values
# through locals, parameters, results, fields, arrays and pointers, with
# carries, products, shifts, division, comparisons and conversions), as
# an x86_64 program and, when python3 has unicorn, as an AArch64 one in
# tools/aarch64_linux_emulator.py.
#
#   tests/run_wide_integers.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0

run() {
    local test=$1 words=$2
    local status=0
    if ! "$compiler" --quiet "tests/$test.mlx" -o "$work/$test"; then
        echo "FAIL $test did not compile"
        failures=$((failures + 1))
        return
    fi
    "$work/$test" || status=$?
    if [[ $status -eq 13 ]]; then
        echo "ok   $test: $words"
    else
        echo "FAIL $test exited $status"
        failures=$((failures + 1))
    fi
}
run 341_wide_integers_runtime "u128, i128 and u100 arithmetic, comparisons and conversions at run time"

# The same program as an AArch64 executable, when the emulator's module
# (unicorn) is there: the expansion is the backends' common ground.
if python3 -c "import unicorn" 2> /dev/null; then
    if "$compiler" --quiet --target=aarch64-linux tests/341_wide_integers_runtime.mlx -o "$work/341_aarch64" \
        && status=0 && { python3 tools/aarch64_linux_emulator.py "$work/341_aarch64" || status=$?; } && [[ $status -eq 13 ]]; then
        echo "ok   341_wide_integers_runtime on aarch64: the same checks hold in the emulator"
    else
        echo "FAIL 341_wide_integers_runtime on aarch64 (exit ${status:-compile})"
        failures=$((failures + 1))
    fi
fi

if [[ $failures -ne 0 ]]; then
    echo "run_wide_integers.sh: $failures failure(s)"
    exit 1
fi
echo "run_wide_integers.sh: all checks passed"
