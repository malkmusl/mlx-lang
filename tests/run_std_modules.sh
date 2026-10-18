#!/usr/bin/env bash
# The small standard modules with the self-hosted compiler, each against
# its standard's own test vectors:
#   tests/347_base64_runtime.mlx        std.base64 (RFC 4648 section 10)
#   tests/348_md5_blake2_runtime.mlx    std.md5 (RFC 1321 A.5) and
#                                       std.blake2 (RFC 7693, the KAT)
#   tests/349_sha2_runtime.mlx          std.sha2's SHA-224 and SHA-384
#                                       (FIPS 180-4's examples)
#   tests/350_math_helpers_runtime.mlx  std.math's min, max, clamp,
#                                       absolute, sqrt and smoothstep
#   tests/351_random_runtime.mlx        std.random (xoshiro256**'s and
#                                       splitmix64's reference outputs)
#   tests/352_terminal_tty_runtime.mlx  std.terminal.tty on a pty of its own
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
run 347_base64_runtime "base64, base64url, base32, base32hex, base16 and hex"
run 348_md5_blake2_runtime "MD5 and BLAKE2b, plain, keyed and in pieces"
run 349_sha2_runtime "SHA-224 and SHA-384 (and SHA-1, SHA-256, SHA-512)"
run 350_math_helpers_runtime "min, max, clamp, absolute, sqrt and smoothstep"
run 351_random_runtime "getrandom, xoshiro256**, splitmix64 and xorshift64*"
run 352_terminal_tty_runtime "a pty's size, raw mode and process groups"

if [[ $failures -ne 0 ]]; then
    echo "$failures standard module checks failed"
    exit 1
fi
echo "all standard module checks passed"
