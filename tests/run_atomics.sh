#!/usr/bin/env bash
# The atomic builtins (spec/00-language/atomics-tls.xml) with the
# self-hosted compiler: tests/295_atomics_runtime.mlx runs (every operation,
# width and signedness, four threads on one word), and the programs in
# tests/support/atomics_invalid are rejected, each with the message that
# names its rule.
#
#   tests/run_atomics.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0

"$compiler" --quiet tests/295_atomics_runtime.mlx -o "$work/atomics"
status=0
"$work/atomics" || status=$?
if [[ $status -eq 13 ]]; then
    echo "ok   295_atomics_runtime: every builtin, and four threads on one word"
else
    echo "FAIL 295_atomics_runtime exited $status"
    failures=$((failures + 1))
fi

expect() {
    local fixture=$1 message=$2
    local output=""
    if output=$("$compiler" --quiet "tests/support/atomics_invalid/$fixture.mlx" -o "$work/$fixture" 2>&1); then
        echo "FAIL $fixture compiled"
        failures=$((failures + 1))
        return
    fi
    if [[ "$output" == *"$message"* ]]; then
        echo "ok   $fixture: $message"
    else
        echo "FAIL $fixture: expected '$message', got:"
        echo "$output" | head -3
        failures=$((failures + 1))
    fi
}
expect load_release "an atomic load does not release"
expect store_acquire "an atomic store does not acquire"
expect rmw_operation "the operation of @atomicRmw is one of xchg, add, sub, and, nand, or, xor, max, min"
expect cmpxchg_orders "a compare-exchange's success order is at least monotonic, and its failure order neither releases nor exceeds it"
expect fence_monotonic "@fence orders acquire, release, acq_rel or seq_cst"
expect float_type "an atomic works on an integer of 1, 2, 4 or 8 bytes, a bool, an enum or a pointer"
expect pointer_type "the second argument of an atomic is a pointer to its type"

if [[ $failures -ne 0 ]]; then
    echo "$failures atomics checks failed"
    exit 1
fi
echo "all atomics checks passed"
