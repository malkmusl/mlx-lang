#!/usr/bin/env bash
# Checked arithmetic on integers narrower than 64 bits (spec operators.xml,
# Arithmetic; compiler/selfhost/ir/lower.mlx, emitArithmeticPolicy):
# tests/524_narrow_overflow_runtime.mlx's values in range, wrapping and
# saturating, and each of its overflowing cases run as a program of its
# own, which the trap must end (status 132: SIGILL, ud2 on x86_64, UDF on
# AArch64). On x86_64, then as an AArch64 program in
# tools/aarch64_linux_emulator.py when python3 has unicorn (which has no
# fork: the test's own children are x86_64's).
#
#   tests/run_narrow_overflow.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0
test=524_narrow_overflow_runtime

# run LABEL PROGRAM...: the values (13), then case 1, 2, ... until the
# program says there is none (2), each ended by the trap (132).
run() {
    local label=$1
    shift
    local status=0 k=1 trapped=0 before=$failures
    "$@" values || status=$?
    if [[ $status -ne 13 ]]; then
        echo "FAIL $test on $label: the values in range, check $status"
        failures=$((failures + 1))
    fi
    while true; do
        # (The shell's report of the signal goes to the group's stderr.)
        status=0
        { "$@" "$k" > /dev/null 2>&1 || status=$?; } 2> /dev/null
        if [[ $status -eq 2 ]]; then break; fi
        if [[ $status -eq 132 ]]; then
            trapped=$((trapped + 1))
        else
            echo "FAIL $test on $label: case $k exited $status, not by the trap"
            failures=$((failures + 1))
        fi
        k=$((k + 1))
    done
    if [[ $failures -eq $before ]]; then
        echo "ok   $test on $label: the values in range; all $trapped overflowing cases trapped"
    else
        echo "FAIL $test on $label: $trapped of $((k - 1)) overflowing cases trapped"
    fi
}

if ! "$compiler" --quiet "tests/$test.mlx" -o "$work/$test"; then
    echo "FAIL $test did not compile"
    exit 1
fi
status=0
"$work/$test" || status=$?
if [[ $status -eq 13 ]]; then
    echo "ok   $test on x86_64: values in range, every case trapping in a child"
else
    echo "FAIL $test on x86_64 exited $status"
    failures=$((failures + 1))
fi
run x86_64 "$work/$test"

if python3 -c "import unicorn" 2> /dev/null; then
    if "$compiler" --quiet --target=aarch64-linux "tests/$test.mlx" -o "$work/${test}_aarch64"; then
        run aarch64 python3 tools/aarch64_linux_emulator.py "$work/${test}_aarch64"
    else
        echo "FAIL $test did not compile for aarch64"
        failures=$((failures + 1))
    fi
fi

if [[ $failures -ne 0 ]]; then
    echo "run_narrow_overflow.sh: $failures failure(s)"
    exit 1
fi
echo "run_narrow_overflow.sh: all checks passed"
