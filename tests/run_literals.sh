#!/usr/bin/env bash
# String and character literals with the self-hosted compiler (spec
# lexical.xml, StringLiterals and CharacterLiterals):
# tests/302_string_escapes_runtime.mlx runs (\xHH, \u{H..H}, the old
# escapes, character literals), the programs in tests/support/escapes_invalid
# are rejected with the message that names the escapes, and
# tests/303_slice_address_runtime.mlx runs (a slice stored through its
# address keeps its length; the address of a local of several words).
#
#   tests/run_literals.sh [compiler]
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
run 302_string_escapes_runtime "\\xHH, \\u{H..H}, the old escapes and character literals"
run 303_slice_address_runtime "a slice stored through its address keeps its length"

expect() {
    local fixture=$1 message=$2
    local output=""
    if output=$("$compiler" --quiet "tests/support/escapes_invalid/$fixture.mlx" -o "$work/$fixture" 2>&1); then
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
message='unknown escape in a quoted literal (the escapes are \n \r \t \0 \\ \" \'"'"' \xHH and \u{H...H})'
expect unknown_escape "$message"
expect short_hex "$message"
expect open_unicode "$message"
expect character_escape "$message"

if [[ $failures -ne 0 ]]; then
    echo "$failures literal checks failed"
    exit 1
fi
echo "all literal checks passed"
