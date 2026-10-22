#!/usr/bin/env bash
# Checks mlx build's memory-aware parallelism (std/src/build/memory.mlx)
# on a probe project in a temporary tree:
#
#   - three commands that each declare more memory than the machine has
#     (Build.memory) run one at a time even at -j 3, and the summary says
#     how often a ready step waited; with --no-memory-limit they run at
#     once;
#   - steps that declare little run side by side;
#   - a job's peak resident memory is kept in its manifest (`took MS
#     PEAK_KB`) and is what the step is expected to take next time.
#
#   tools/check_build_memory.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
[[ "$compiler" == /* ]] || compiler="$repo_root/$compiler"

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && cat "$2" >&2
    exit 1
}
export MLX_BUILD_CACHE=off

tree="$work/tree"
mkdir -p "$tree"
ln -s "$repo_root/std" "$tree/std"
ln -s "$repo_root/compiler" "$tree/compiler"
cat > "$tree/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "probe", .version = "1.0.0", .description = "a probe of memory-aware jobs" }
}

fn sleeper(b: *Build.Builder, group: Build.Step, name: []const u8, megabytes: usize) -> void {
    const step = Build.command(b, name, Build.concat(b, "mlx-out/", name), Build.concat(b, Build.concat(b, "sh -c \"sleep 1; date +%s%N > mlx-out/", name), "\""))
    Build.memory(b, step, megabytes)
    Build.add(b, group, step)
}

pub fn build(b: *Build.Builder) -> void {
    const big = Build.group(b, "big", "three steps larger than the machine")
    sleeper(b, big, "big1", 1048576 * 64)
    sleeper(b, big, "big2", 1048576 * 64)
    sleeper(b, big, "big3", 1048576 * 64)
    const small = Build.group(b, "small", "three small steps")
    sleeper(b, small, "small1", 1)
    sleeper(b, small, "small2", 1)
    sleeper(b, small, "small3", 1)
    Build.byDefault(b, small)
}
EOF_MLX

build() {
    local log=$1
    shift
    local status=0
    (cd "$tree" && "$compiler" build "$@") > "$log" 2>&1 || status=$?
    return $status
}

# The seconds between the first and the last step's end.
spread() {
    local first last
    first=$(cat "$tree"/mlx-out/"$1"? | sort -n | head -1)
    last=$(cat "$tree"/mlx-out/"$1"? | sort -n | tail -1)
    echo $(( (last - first) / 1000000000 ))
}

build "$work/big.log" -j 3 big || fail "the large steps did not build" "$work/big.log"
[[ $(spread big) -ge 2 ]] || fail "the steps larger than the machine ran side by side (they ended within $(spread big) s)" "$work/big.log"
grep -q 'steps waited for memory [0-9]* times' "$work/big.log" || fail "the summary does not say the steps waited for memory" "$work/big.log"
build "$work/unlimited.log" -j 3 --force --no-memory-limit big || fail "the large steps without the limit did not build" "$work/unlimited.log"
[[ $(spread big) -le 1 ]] || fail "--no-memory-limit did not run the large steps at once" "$work/unlimited.log"
echo "ok   steps larger than the memory run one at a time at -j 3; --no-memory-limit runs them at once"

build "$work/small.log" -j 3 small || fail "the small steps did not build" "$work/small.log"
[[ $(spread small) -le 1 ]] || fail "small steps did not run side by side" "$work/small.log"
peak=$(sed -n 's/^took [0-9]* \([0-9]*\)$/\1/p' "$tree/mlx-out/build/cache/small1.manifest")
[[ -n "$peak" && "$peak" -gt 0 ]] || fail "the manifest does not keep the job's peak memory: $(cat "$tree/mlx-out/build/cache/small1.manifest")"
echo "ok   small steps run side by side; a job's peak memory ($peak KiB) is kept in its manifest"
echo "check_build_memory.sh: all checks passed"
