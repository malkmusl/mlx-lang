#!/usr/bin/env bash
# The native workloads of the port, timed (docs/ROADMAP.md, "Phase 6b"):
# what mlx-cc's code generation is measured by, run before and after a
# change to it. Each workload runs RUNS times (default 5) and its median
# wall time is printed, with the build's codegen epoch and the inlining
# setting of the binaries, so that two runs can be read side by side.
#
#   - jsc on projects/webkit/tests/bench/bench.js (JavaScriptCore: the
#     parser, the bytecode compiler, the JIT tiers' runtime, the GC);
#   - webcore-dump laying out and painting projects/webkit/tests/bench/
#     bench.html at 800x600 (style, layout, painting on the CPU);
#   - the same without scripts (--no-script).
#
#   tools/bench_native.sh [--runs N] [--no-build]
#
# `./mlx build libc webkit-jsc-shell webkit-mlx-webcore-dump` first unless
# --no-build. gcc -O2's numbers for the same workloads are what the
# goal is set against (under 2x); WebKit built by gcc is not in this
# repository, so they are taken from a build outside it when wanted.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
runs=5
build=1
while [[ $# -gt 0 ]]; do
    case $1 in
        --runs) runs=$2; shift ;;
        --no-build) build=0 ;;
        *) echo "usage: tools/bench_native.sh [--runs N] [--no-build]" >&2; exit 2 ;;
    esac
    shift
done
if [[ $build == 1 ]]; then
    ./mlx build libc webkit-jsc-shell webkit-mlx-webcore-dump > /dev/null
fi
jsc=mlx-out/webkit/bin/jsc
dump=mlx-out/webkit/mlx/bin/webcore-dump
bench=projects/webkit/tests/bench
[[ -x $jsc && -x $dump ]] || { echo "bench_native.sh: build jsc and webcore-dump first" >&2; exit 2; }
echo "epoch $(mlx-out/bin/mlx-cc --codegen-epoch 2>/dev/null || echo '?'), $runs runs each, the median in seconds"

# Runs the command RUNS times and prints the median wall time.
median() {
    local name=$1
    shift
    local times=()
    for ((i = 0; i < runs; i++)); do
        local start end
        start=$(date +%s.%N)
        "$@" > /dev/null 2>&1 || { echo "$name: failed" >&2; return 1; }
        end=$(date +%s.%N)
        times+=("$(echo "$end - $start" | bc -l)")
    done
    local sorted
    sorted=$(printf '%s\n' "${times[@]}" | sort -n)
    local mid=$(( (runs + 1) / 2 ))
    printf '%-28s %6.3f  (%s)\n' "$name" "$(echo "$sorted" | sed -n "${mid}p")" "$(echo "$sorted" | tr '\n' ' ' | sed 's/ $//')"
}

median "jsc bench.js" "$jsc" "$bench/bench.js"
median "webcore-dump bench.html" "$dump" --size 800x600 "$bench/bench.html"
median "  without scripts" "$dump" --size 800x600 --no-script "$bench/bench.html"
