#!/usr/bin/env bash
# The Mlx port of WebKit, M3 (projects/webkit/PORT.md): a page loaded and
# laid out headless. webcore-dump (projects/webkit/tests/webcore-dump.cpp,
# mlx-out/webkit/mlx/bin/webcore-dump) loads each of WebKit's layout tests
# in projects/webkit/tests/layout (tests.txt; the README says which
# revision) at 800x600 and prints its render tree, which must be the one
# WebKitTestRunner dumps: the test's -expected.txt (platform/wpe's, else
# platform/glib's, else the generic one), compared line by line (spaces at
# line ends and blank lines at the end aside).
#
# The fonts are WebKitTestRunner's: Ahem as a test font (--font), the
# families of its GLib fonts.conf (tests/layout/fonts.conf) over the
# system's Liberation and DejaVu fonts (DejaVu Sans the fallback for
# characters Liberation lacks), through std.fontconfig (FONTCONFIG_FILE).
#
# Each test passes or fails; a failure is shown with its first differing
# line and its cause from tests/layout/failures.txt (`PATH cause`): the
# failures known. The check fails when a test fails that is not among
# them (a regression, or a failure with no cause written down); a known
# one that passes is shown, to be taken off the list.
#
#   tools/check_webkit_mlx_layout.sh [--no-build] [-j N] [--keep DIR] [TEST...]
#     (--no-build: the program as it is; -j: the build's jobs, 2 by
#     default; --keep: the render trees printed, kept in DIR; TEST: paths
#     of tests.txt, all by default)
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
layout=projects/webkit/tests/layout
work=$(mktemp -d /tmp/mlx-webkit-layout.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

build=1
jobs=2
keep=""
chosen=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        -j) jobs=$2; shift ;;
        --keep) keep=$2; shift ;;
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

# The fonts: Liberation's directory, WebKitTestRunner's families, Ahem.
serif=$(find /usr/share/fonts /usr/local/share/fonts -name LiberationSerif-Regular.ttf 2> /dev/null | head -1)
[[ -n "$serif" ]] || fail "no Liberation fonts (LiberationSerif-Regular.ttf) in /usr/share/fonts"
dejavu=$(find /usr/share/fonts /usr/local/share/fonts -name DejaVuSans.ttf 2> /dev/null | head -1)
[[ -n "$dejavu" ]] || fail "no DejaVu fonts (DejaVuSans.ttf) in /usr/share/fonts: the test runner's fallback"
cat > "$work/fonts.conf" << EOF
<?xml version="1.0"?>
<fontconfig>
    <dir>$(dirname "$serif")</dir>
    <dir>$(dirname "$dejavu")</dir>
    <include>$repo_root/$layout/fonts.conf</include>
</fontconfig>
EOF
mkdir -p "$work/home"
ahem=$repo_root/$layout/LayoutTests/resources/Ahem.ttf

# The tests, with their groups.
declare -A groupOf
tests=()
group=""
while IFS= read -r line; do
    case $line in
        "#"*|"") continue ;;
        ahem\ *|liberation\ *) group=$line ;;
        *) tests+=("$line"); groupOf[$line]=$group ;;
    esac
done < "$layout/tests.txt"
[[ ${#chosen[@]} -gt 0 ]] && tests=("${chosen[@]}")

declare -A causeOf
while IFS= read -r line; do
    case $line in "#"*|"") continue ;; esac
    causeOf[${line%% *}]=${line#* }
done < "$layout/failures.txt"

expectedFor() {
    local base=${1%.*}-expected.txt
    for prefix in platform/wpe/ platform/glib/ ""; do
        [[ -f "$layout/LayoutTests/$prefix$base" ]] && { echo "$layout/LayoutTests/$prefix$base"; return; }
    done
}

# A render tree without spaces at line ends and blank lines at the end.
normalized() { sed -e 's/[[:space:]]*$//' "$1" | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}'; }

passed=0
failed=0
unexplained=0
fixed=0
declare -A groupPassed groupTotal
for test in "${tests[@]}"; do
    g=${groupOf[$test]:-other}
    groupTotal[$g]=$(( ${groupTotal[$g]:-0} + 1 ))
    expected=$(expectedFor "$test")
    [[ -n "$expected" ]] || fail "$test has no -expected.txt"
    out=$work/out.txt
    status=0
    env -i HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf" timeout 300 "$program" --size 800x600 --font "$ahem" "$repo_root/$layout/LayoutTests/$test" > "$out" 2> "$work/err.txt" || status=$?
    if [[ -n "$keep" ]]; then
        mkdir -p "$keep/$(dirname "$test")"
        cp "$out" "$keep/${test%.*}-actual.txt"
    fi
    normalized "$expected" > "$work/expected.n"
    normalized "$out" > "$work/actual.n"
    if [[ $status -eq 0 ]] && cmp -s "$work/expected.n" "$work/actual.n"; then
        passed=$(( passed + 1 ))
        groupPassed[$g]=$(( ${groupPassed[$g]:-0} + 1 ))
        echo "PASS $test"
        if [[ -n "${causeOf[$test]:-}" ]]; then
            echo "     (listed as failing in $layout/failures.txt: take it off)"
            fixed=$(( fixed + 1 ))
        fi
        continue
    fi
    failed=$(( failed + 1 ))
    if [[ $status -ne 0 && ! -s "$out" ]]; then
        echo "FAIL $test: webcore-dump exits with $status: $(grep -v '^  ' "$work/err.txt" | tail -1)"
    else
        line=$(diff "$work/expected.n" "$work/actual.n" | sed -n '1s/^\([0-9]*\).*/\1/p')
        echo "FAIL $test: line $line"
        echo "     expected: $(sed -n "${line}p" "$work/expected.n")"
        echo "     printed:  $(sed -n "${line}p" "$work/actual.n")"
    fi
    if [[ -n "${causeOf[$test]:-}" ]]; then
        echo "     cause: ${causeOf[$test]}"
    else
        echo "     cause: not known (no line in $layout/failures.txt): a regression?"
        unexplained=$(( unexplained + 1 ))
    fi
done

echo "by group:"
for g in "${!groupTotal[@]}"; do
    echo "     $g: ${groupPassed[$g]:-0} of ${groupTotal[$g]}"
done | sort
total=$(( passed + failed ))
echo "check_webkit_mlx_layout.sh: $passed of $total tests pass, $failed fail"
[[ $fixed -eq 0 ]] || echo "check_webkit_mlx_layout.sh: $fixed tests listed in $layout/failures.txt pass now"
[[ $unexplained -eq 0 ]] || fail "$unexplained tests fail that $layout/failures.txt does not list"
echo "check_webkit_mlx_layout.sh: all checks passed"
