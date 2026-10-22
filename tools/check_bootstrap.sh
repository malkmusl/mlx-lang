#!/usr/bin/env bash
# Checks that the compiler's sources stay buildable by the compilers that
# bootstrap them (compiler/selfhost/README.md, "Bootstrapping"):
#
#   1. the previous canonical compiler (mlx-out/bin/compiler/mlx4.previous,
#      kept by tools/ensure_compiler.sh when it installs a new one; else the
#      canonical compiler itself) builds compiler/selfhost/main.mlx, and the
#      compiler it builds compiles and runs a test (generic containers);
#   2. the import closure of compiler/selfhost/main.mlx (the depfile of that
#      build) never reaches the std root, std/src/std.mlx;
#   3. mlx0 (zig-out/bin/mlx0, when there is one) parses that closure and
#      analyzes its module graph (no syntax it lacks, no import cycle), and
#      builds it; the build itself is reported, and required only with
#      MLX0_STRICT=1 (mlx0 lacks language changes made since, such as a
#      slice's `.length`, so it stops in its third step);
#   4. mlx-codemap layers finds no import against codemap.layers (which
#      says the same as 2 for each module of the closure).
#
#   tools/check_bootstrap.sh [previous-compiler] [mlx0]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
canonical=mlx-out/bin/compiler/mlx4
previous=${1:-${MLX_PREVIOUS_COMPILER:-}}
if [[ -z "$previous" ]]; then
    if [[ -x $canonical.previous ]]; then previous=$canonical.previous; else previous=$canonical; fi
fi
mlx0=${2:-${MLX0:-zig-out/bin/mlx0}}
[[ -x "$previous" ]] || { echo "check_bootstrap.sh: $previous is missing (run tools/ensure_compiler.sh)" >&2; exit 1; }

work=$(mktemp -d)
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT

failures=0
fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && tail -20 "$2" >&2
    failures=$((failures + 1))
}
pass() { echo "ok   $1"; }

# 1. The previous canonical compiler builds the sources.
if "$previous" --quiet --depfile="$work/closure.d" compiler/selfhost/main.mlx -o "$work/mlx-stage1" > "$work/stage1.log" 2>&1; then
    pass "$previous builds compiler/selfhost/main.mlx"
    smoke_status=0
    if "$work/mlx-stage1" --quiet tests/397_generic_containers_runtime.mlx -o "$work/smoke" > "$work/smoke.log" 2>&1; then
        "$work/smoke" > /dev/null 2>&1 || smoke_status=$?
    else
        smoke_status=-1
    fi
    if [[ $smoke_status == 13 ]]; then
        pass "the compiler it built compiles and runs tests/397_generic_containers_runtime.mlx"
    else
        fail "the compiler $previous built does not compile tests/397_generic_containers_runtime.mlx to a program exiting 13" "$work/smoke.log"
    fi
else
    fail "$previous cannot build compiler/selfhost/main.mlx" "$work/stage1.log"
fi

# 2. The closure stays off the std root.
if [[ -s "$work/closure.d" ]]; then
    modules=$(wc -l < "$work/closure.d")
    if grep -q 'std/src/std\.mlx' "$work/closure.d"; then
        fail "compiler/selfhost/main.mlx's import closure reaches std/src/std.mlx"
    else
        pass "the import closure ($modules modules) does not reach std/src/std.mlx"
    fi
else
    fail "no depfile from $previous (its --depfile)"
fi

# 3. mlx0.
if [[ -x "$mlx0" ]]; then
    if "$mlx0" compiler/selfhost/main.mlx -o "$work/mlx1" > "$work/mlx0.log" 2>&1; then
        pass "$mlx0 builds compiler/selfhost/main.mlx"
    else
        errors=$(grep -c 'error:' "$work/mlx0.log" || true)
        if grep -q '^\[mlx0 3/7\]' "$work/mlx0.log" && ! sed -n '/^\[mlx0 3\/7\]/q;p' "$work/mlx0.log" | grep -q 'error\|cannot\|cycle'; then
            pass "$mlx0 parses the closure and analyzes its module graph"
            first=$(grep -m1 'error:' "$work/mlx0.log" | sed "s|$repo_root/||")
            if [[ ${MLX0_STRICT:-0} == 1 ]]; then
                fail "$mlx0 does not build compiler/selfhost/main.mlx ($errors errors, first: $first)" "$work/mlx0.log"
            else
                echo "note $mlx0 stops analyzing the root module ($errors errors, first: $first)"
            fi
        else
            fail "$mlx0 does not parse compiler/selfhost/main.mlx's closure or its module graph" "$work/mlx0.log"
        fi
    fi
else
    echo "skip $mlx0 is missing (zig builds it: ./mlx build bootstrap)"
fi

# 4. The layer rules.
if mlx-out/bin/compiler/mlx4 --quiet tools/codemap/main.mlx -o "$work/mlx-codemap" > "$work/codemap.log" 2>&1; then
    "$work/mlx-codemap" -C "$repo_root" layers > "$work/layers.log" 2>&1 || true
    if grep -q '^-- 0 imports against the rules' "$work/layers.log"; then
        pass "mlx-codemap layers: no import against codemap.layers"
    else
        fail "mlx-codemap layers finds imports against codemap.layers" "$work/layers.log"
    fi
else
    fail "mlx-codemap does not build" "$work/codemap.log"
fi

if [[ $failures -ne 0 ]]; then
    echo "check_bootstrap.sh: $failures failed" >&2
    exit 1
fi
echo "check_bootstrap.sh: all checks passed"
