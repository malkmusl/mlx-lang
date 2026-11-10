#!/usr/bin/env bash
# The slice of an array is as writable as the array is (a `[]const T` of a
# constant, of a parameter taken by value, of a field or an element of one,
# of what a `*const` pointer points to and of a call's result; a pointer
# capture likewise), with the self-hosted compiler:
# tests/443_constant_slices_runtime.mlx runs (the slices that stay
# writable, the constant ones read right, a callee's changes stay in its
# own copy), tests/442_constant_table_static_runtime.mlx still runs (a
# table's slices read its image), and the programs in
# tests/support/constant_slices_invalid are rejected, each with the message
# that names its mistake: a write through such a slice or capture, or the
# slice given where a `[]u8` is taken.
#
#   tests/run_constant_slices.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0

run() {
    local name=$1 what=$2
    local status=0
    if ! "$compiler" --quiet "tests/$name.mlx" -o "$work/$name"; then
        echo "FAIL $name did not compile"
        failures=$((failures + 1))
        return
    fi
    (cd tests && "$work/$name") || status=$?
    if [[ $status -eq 13 ]]; then
        echo "ok   $name: $what"
    else
        echo "FAIL $name exited $status"
        failures=$((failures + 1))
    fi
}
run 443_constant_slices_runtime "slices of variables and of mutable pointers' targets written, slices of constants and of by-value parameters read, a callee's changes in its own copy"
run 442_constant_table_static_runtime "a constant table's slices and pointer captures read its image"
# (Its main returns an error union: 0 when every check held.)
if "$compiler" --quiet tests/445_tried_places_runtime.mlx -o "$work/445_tried_places_runtime" > "$work/445.log" 2>&1 && (cd tests && "$work/445_tried_places_runtime"); then
    echo "ok   445_tried_places_runtime: places reached through a try are as writable as without one"
else
    echo "FAIL 445_tried_places_runtime"
    head -3 "$work/445.log"
    failures=$((failures + 1))
fi

expect() {
    local fixture=$1 message=$2
    local output=""
    if output=$("$compiler" --quiet "tests/support/constant_slices_invalid/$fixture.mlx" -o "$work/$fixture" 2>&1); then
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
unwritable='this place cannot be written'
constant='expected `[]u8`, found `[]const u8`'
expect constant_written "$unwritable"
expect constant_to_callee "$constant"
expect constant_captured "$unwritable"
expect constant_declared_mutable "$constant"
expect constant_member "$constant"
expect constant_field "$constant"
expect constant_two_modules "$constant"
expect local_constant "$constant"
expect parameter_written "$unwritable"
expect parameter_to_callee "$constant"
expect parameter_captured "$unwritable"
expect parameter_field "$constant"
expect const_pointer_target "$constant"
expect const_self_field "$constant"
expect optional_payload "$constant"
expect call_result "$constant"

if [[ $failures -ne 0 ]]; then
    echo "$failures constant slice checks failed"
    exit 1
fi
echo "all constant slice checks passed"
