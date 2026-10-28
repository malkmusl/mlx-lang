#!/usr/bin/env bash
# WebKit's jsc (JavaScriptCore of the JSCOnly port, projects/webkit) on
# mlxlibc alone: the port's programs start on mlxlibc's loader
# (mlx-out/libc/libmlxc.so.1 as their PT_INTERP), so no glibc is in the
# process; mlxlibcpp is their C++ library, mlx-unicode their ICU.
#
#   - `./mlx build libc webkit-jsc-shell` builds mlxlibc's directory and
#     jsc (mlx-out/webkit/bin/jsc);
#   - jsc names mlxlibc's loader as its interpreter and nothing of glibc
#     but the names libc.so.6 and libm.so.6, which are mlxlibc's;
#   - the scripts of projects/webkit/tests/jsc print what they print on
#     the glibc-linked jsc (their .expected files; Intl and the core
#     language as node prints them, a stress of the runtime, sorting,
#     typed arrays), run with an empty environment; the benchmark
#     tests/bench/bench.js runs, its results right;
#   - a running jsc has mlxlibc's loader mapped and neither glibc's C
#     library nor its loader.
#
#   tools/check_jsc_mlxlibc.sh [--no-build]   (--no-build: jsc as it is)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d /tmp/mlx-jsc.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

if [[ "${1:-}" != --no-build ]]; then
    ./mlx build libc webkit-jsc-shell -j 2 > "$work/build.log" 2>&1 || { tail -20 "$work/build.log"; fail "./mlx build libc webkit-jsc-shell"; }
    echo "ok   ./mlx build libc webkit-jsc-shell"
fi
jsc=$repo_root/mlx-out/webkit/bin/jsc
[[ -x "$jsc" ]] || fail "$jsc is missing"

interpreter=$(readelf -l "$jsc" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')
[[ "$interpreter" == "$repo_root/mlx-out/libc/libmlxc.so.1" ]] || fail "jsc's interpreter is $interpreter"
needed=$(readelf -d "$jsc" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | tr '\n' ' ')
[[ "$needed" == "libc.so.6 $repo_root/mlx-out/lib/libmlxunicode.so libm.so.6 " ]] || fail "jsc needs $needed"
echo "ok   jsc starts on mlxlibc's loader and needs libc.so.6, mlx-unicode, libm.so.6"

tests=projects/webkit/tests/jsc
run() { # NAME SCRIPT [ARGUMENTS]
    local name=$1 status=0
    shift
    (cd "$tests" && env -i timeout 600 "$jsc" "$@" > "$work/$name.out" 2>&1) || status=$?
    [[ $status -eq 0 ]] || fail "$name: jsc exits with $status: $(tail -3 "$work/$name.out")"
    diff -u "$tests/$name.expected" "$work/$name.out" > "$work/$name.diff" || { head -20 "$work/$name.diff"; fail "$name prints otherwise than the glibc-linked jsc"; }
    ran+=("$name")
}
ran=()
run intl intl.js
run core core.js
run numbers-currency numbers.js -- currency
run numbers-percent numbers.js -- percent
run stress stress.js
run sort sort.js
run f64 f64.js
echo "ok   ${#ran[@]} scripts print what the glibc-linked jsc prints: ${ran[*]}"

# The benchmark (projects/webkit/tests/bench/bench.js): each section
# checks its result, the times are printed.
(cd projects/webkit/tests/bench && env -i timeout 600 "$jsc" bench.js > "$work/bench.out" 2>&1) || fail "bench.js: $(tail -3 "$work/bench.out")"
echo "ok   bench.js: $(tr '\n' ' ' < "$work/bench.out")"

# What the process maps while a script runs.
echo 'for (;;) {}' > "$work/spin.js"
env -i "$jsc" "$work/spin.js" &
pid=$!
maps=""
for _ in $(seq 100); do
    maps=$(cat "/proc/$pid/maps" 2> /dev/null || true)
    grep -q libmlxunicode <<< "$maps" && break
    sleep 0.1
done
kill "$pid" 2> /dev/null || true
wait "$pid" 2> /dev/null || true
grep -q "$repo_root/mlx-out/libc/libmlxc.so.1" <<< "$maps" || fail "jsc does not run on mlxlibc's loader"
grep -q "$repo_root/mlx-out/libc/libm.so.6" <<< "$maps" || fail "jsc does not have mlxlibc's libm.so.6"
! grep -E -q "/libc\.so\.6|/ld-linux|/libm\.so\.6" <<< "$(grep -v "$repo_root/mlx-out/libc/" <<< "$maps")" || fail "glibc is in jsc's process"
echo "ok   a running jsc has mlxlibc's loader and libm mapped, and nothing of glibc"

echo "check_jsc_mlxlibc.sh: all checks passed"
