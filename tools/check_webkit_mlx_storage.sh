#!/usr/bin/env bash
# The Mlx port of WebKit, M7 (projects/webkit/PORT.md, "Storage"): web
# storage and IndexedDB. webcore-dump (projects/webkit/tests/webcore-dump.cpp)
# runs each of WebKit's layout tests of storage/domstorage and
# storage/indexeddb that tests/layout/storage.txt lists (the README says
# which revision they are of) as WebKitTestRunner runs it (`--test`: its
# testRunner, dumpAsText, waitUntilDone and notifyDone, the console's
# messages) in a profile of its own (a temporary one each test, so that no
# test sees another's databases), and compares what it prints with the
# test's -expected.txt (platform/wpe's, else platform/glib's, else the
# generic one) line by line, spaces at line ends and blank lines at the
# end aside, as tools/check_webkit_mlx_layout.sh does. As run-webkit-tests
# has it, the time zone is US/Pacific (dates in the expected text).
#
# Every test listed must pass; the tests of those directories that do not
# yet are in PORT.md ("Storage"), with why.
#
# The same runs WebKit's crypto tests (M7, PORT.md "Crypto": crypto/subtle
# and crypto's getRandomValues) with --list crypto.txt, as
# tools/check_webkit_mlx_crypto.sh does.
#
#   tools/check_webkit_mlx_storage.sh [--no-build] [-j N] [--keep DIR] [--list NAME] [TEST...]
#     (--no-build: the program as it is; -j: the build's jobs, 2 by
#     default; --keep: what each test printed, kept in DIR; --list: the
#     list of tests/layout, storage.txt by default; TEST: paths of the
#     list, all by default)
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
layout=projects/webkit/tests/layout
work=$(mktemp -d /tmp/mlx-webkit-storage.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

build=1
jobs=2
keep=""
list=storage.txt
chosen=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        -j) jobs=$2; shift ;;
        --keep) keep=$2; shift ;;
        --list) list=$2; shift ;;
        -*) fail "unknown argument $1" ;;
        *) chosen+=("$1") ;;
    esac
    shift
done

targets="libc webkit-mlx-port webkit-mlx-webcore-dump"
if [[ $build -eq 1 ]]; then
    ./mlx build $targets -j "$jobs" > "$work/build.log" 2>&1 || { grep -E "^(FAILED|mlx build)" "$work/build.log" | head -20; tail -20 "$work/build.log"; fail "./mlx build $targets"; }
    echo "ok   ./mlx build $targets"
fi
program=$repo_root/mlx-out/webkit/mlx/bin/webcore-dump
[[ -x "$program" ]] || fail "$program is missing"
mkdir -p "$work/home" "$work/tmp"

tests=()
while IFS= read -r line; do
    case $line in "#"*|"") continue ;; esac
    tests+=("$line")
done < "$layout/$list"
[[ ${#chosen[@]} -gt 0 ]] && tests=("${chosen[@]}")

expectedFor() {
    local base=${1%.*}-expected.txt
    for prefix in platform/wpe/ platform/glib/ ""; do
        [[ -f "$layout/LayoutTests/$prefix$base" ]] && { echo "$layout/LayoutTests/$prefix$base"; return; }
    done
}

normalized() { sed -e 's/[[:space:]]*$//' "$1" | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}'; }

passed=0
failed=0
for test in "${tests[@]}"; do
    expected=$(expectedFor "$test")
    [[ -n "$expected" ]] || fail "$test has no -expected.txt"
    out=$work/out.txt
    status=0
    env -i HOME="$work/home" TMPDIR="$work/tmp" TZ=US/Pacific timeout 120 "$program" --test --size 800x600 --timeout 60 "$repo_root/$layout/LayoutTests/$test" > "$out" 2> "$work/err.txt" || status=$?
    if [[ -n "$keep" ]]; then
        mkdir -p "$keep/$(dirname "$test")"
        cp "$out" "$keep/${test%.*}-actual.txt"
    fi
    normalized "$expected" > "$work/expected.n"
    normalized "$out" > "$work/actual.n"
    if [[ $status -eq 0 ]] && cmp -s "$work/expected.n" "$work/actual.n"; then
        passed=$(( passed + 1 ))
        echo "PASS $test"
        continue
    fi
    failed=$(( failed + 1 ))
    if [[ $status -ne 0 && ! -s "$out" ]]; then
        echo "FAIL $test: webcore-dump exits with $status: $(grep -v '^  ' "$work/err.txt" | tail -1)"
    else
        line=$(diff "$work/expected.n" "$work/actual.n" | sed -n '1s/^\([0-9]*\).*/\1/p')
        echo "FAIL $test: line ${line:-?} (exit $status)"
        echo "     expected: $(sed -n "${line:-1}p" "$work/expected.n")"
        echo "     printed:  $(sed -n "${line:-1}p" "$work/actual.n")"
    fi
done
# (The temporary profiles are gone: webcore-dump removes its own.)
leftover=$(find "$work/tmp" -mindepth 1 -maxdepth 1 | wc -l)
[[ $leftover -eq 0 ]] || fail "$leftover temporary profiles left in TMPDIR"
total=$(( passed + failed ))
echo "check_webkit_mlx_storage.sh: $passed of $total tests of $list pass, $failed fail"
[[ $failed -eq 0 ]] || fail "$failed of the tests of $list fail"
echo "check_webkit_mlx_storage.sh: all checks passed"
