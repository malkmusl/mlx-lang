#!/usr/bin/env bash
# The small standard modules with the self-hosted compiler, each against
# its standard's own test vectors:
#   tests/343_base64_runtime.mlx   std.base64 (RFC 4648 section 10)
#
#   tests/run_std_modules.sh [compiler]
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
        echo "FAIL $test: check $status failed"
        failures=$((failures + 1))
    fi
}
run 343_base64_runtime "base64, base64url, base32, base32hex, base16 and hex"

if [[ $failures -ne 0 ]]; then
    echo "$failures standard module checks failed"
    exit 1
fi
echo "all standard module checks passed"
