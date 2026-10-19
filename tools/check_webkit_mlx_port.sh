#!/usr/bin/env bash
# The Mlx port of WebKit (projects/webkit/PORT.md, M2): WebCore and PAL
# compiled for PlatformMlx and linked whole on mlxlibc.
#
#   - `./mlx build webkit-mlx-webcore-test` builds the port's libPAL.a and
#     libWebCore.a (mlx-out/webkit/mlx/lib), JSCOnly's JavaScriptCore,
#     WTF, bmalloc and Skia, the port's Mlx side (libmlxwebport.so) and
#     the program of projects/webkit/tests/webcore-mlx.cpp
#     (mlx-out/webkit/mlx/bin/webcore-test), which links them;
#   - the program names mlxlibc's loader as its interpreter, needs nothing
#     of glibc but the names libc.so.6 and libm.so.6 (mlxlibc's), and each
#     name it leaves undefined is defined by one of the libraries it needs
#     (mlxlibc, the libraries in Mlx, the port's): nothing is unresolved;
#   - run with an empty environment, it makes a Page with the empty
#     clients, its main frame's view and document, and prints what
#     tests/webcore-mlx.expected says.
# The port's deliberate stubs (`// STUB (Mn...)` in projects/webkit/port)
# are listed at the end.
#
#   tools/check_webkit_mlx_port.sh [--no-build] [-j N]
#     (--no-build: the program as it is; -j: the build's jobs, 2 by default)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-webkit-port.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

build=1
jobs=2
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        -j) jobs=$2; shift ;;
        *) fail "unknown argument $1" ;;
    esac
    shift
done

targets="libc webkit-mlx-port webkit-mlx-pal webkit-mlx-webcore webkit-mlx-webcore-test"
if [[ $build -eq 1 ]]; then
    ./mlx build $targets -j "$jobs" > "$work/build.log" 2>&1 || { grep -E "^(FAILED|mlx build)" "$work/build.log" | head -20; tail -20 "$work/build.log"; fail "./mlx build $targets"; }
    echo "ok   ./mlx build $targets"
fi
for library in mlx-out/webkit/mlx/lib/libPAL.a mlx-out/webkit/mlx/lib/libWebCore.a mlx-out/lib/libmlxwebport.so; do
    [[ -s $library ]] || fail "$library is missing"
done
program=$repo_root/mlx-out/webkit/mlx/bin/webcore-test
[[ -x "$program" ]] || fail "$program is missing"
echo "ok   libPAL.a $(stat -c %s mlx-out/webkit/mlx/lib/libPAL.a) bytes, libWebCore.a $(stat -c %s mlx-out/webkit/mlx/lib/libWebCore.a) bytes, webcore-test $(stat -c %s "$program") bytes"

interpreter=$(readelf -l "$program" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')
[[ "$interpreter" == "$repo_root/mlx-out/libc/libmlxc.so.1" ]] || fail "webcore-test's interpreter is $interpreter"
mapfile -t needed < <(readelf -d "$program" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
libraries=()
for name in "${needed[@]}"; do
    case $name in
        libc.so.6) libraries+=("$repo_root/mlx-out/libc/libmlxc.so.1") ;;
        libm.so.6) libraries+=("$repo_root/mlx-out/libc/libm.so.6") ;;
        "$repo_root"/mlx-out/lib/libmlx*.so) libraries+=("$name") ;;
        *) fail "webcore-test needs $name" ;;
    esac
done
echo "ok   webcore-test starts on mlxlibc's loader and needs ${needed[*]##*/}"

# Every name the program imports, against the names its libraries define.
nm -D --defined-only "${libraries[@]}" 2> /dev/null | awk 'NF >= 3 { sub(/@.*/, "", $3); print $3 }' | sort -u > "$work/defined"
nm -D --undefined-only "$program" | awk '$1 == "U" { sub(/@.*/, "", $2); print $2 }' | sort -u > "$work/imported"
comm -23 "$work/imported" "$work/defined" > "$work/unresolved"
if [[ -s "$work/unresolved" ]]; then
    echo "$(wc -l < "$work/unresolved") names undefined anywhere:" >&2
    c++filt < "$work/unresolved" | head -60 >&2
    fail "webcore-test leaves names unresolved"
fi
echo "ok   the $(wc -l < "$work/imported") names webcore-test imports are all defined by its libraries"

status=0
env -i timeout 600 "$program" > "$work/webcore-test.out" 2>&1 || status=$?
[[ $status -eq 0 ]] || fail "webcore-test exits with $status: $(tail -5 "$work/webcore-test.out")"
diff -u projects/webkit/tests/webcore-mlx.expected "$work/webcore-test.out" > "$work/webcore-test.diff" || { head -20 "$work/webcore-test.diff"; fail "webcore-test prints otherwise than tests/webcore-mlx.expected"; }
echo "ok   webcore-test makes a Page with the empty clients, a view and a document, and exits"

echo "the port's deliberate stubs:"
grep -rn "^// STUB" projects/webkit/port | sed 's/^/     /'
echo "check_webkit_mlx_port.sh: all checks passed"
