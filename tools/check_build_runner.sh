#!/usr/bin/env bash
# Checks that mlx build's runner gives back what each step's work took
# (std/src/build.mlx: std.turns around every step's look, start and end),
# so that a build of tens of thousands of steps runs in the memory of a
# small one. The compiler keeps every struct, array and tuple value in one
# 1 GiB arena that is only bumped (std/src/turns.mlx): a runner that keeps
# what finishing a step made (the manifest's input lines, a stat of each
# header) died with "the values' arena is full" after ~1370 of WebKit's
# 2037 steps. Probes, in a temporary tree, the runner watched by std.leaks
# (MLX_LEAK_CHECK=1, its snapshots in $XDG_RUNTIME_DIR/mlx/memory, which
# `mlx-codemap memory runner` reads):
#
#   - 2,000 and then 20,000 trivial commands: the runner's values (the
#     arena's fill, the snapshot's `values`) after the large build are
#     within 4 MiB of the small one's;
#   - 400 and then 2,000 steps whose depfiles name 1,000 headers each (as
#     a WebKit unit's does): the same;
#   - the steps are current afterwards (the manifests were written whole).
#
#   tools/check_build_runner.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
[[ "$compiler" == /* ]] || compiler="$repo_root/$compiler"

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && tail -n 30 "$2" >&2
    exit 1
}
export MLX_BUILD_CACHE=off
# What a turn gives back is overwritten: a value kept by mistake shows.
export MLX_ARENA_POISON=1

tree="$work/tree"
mkdir -p "$tree/headers" "$tree/deps"
ln -s "$repo_root/std" "$tree/std"
ln -s "$repo_root/compiler" "$tree/compiler"
for h in $(seq 1 1000); do echo "#define H$h $h" > "$tree/headers/h$h.h"; done
# The depfiles: make's rule naming the 1,000 headers (mlx-cc -MD's form).
{
    printf 'out.o: \\\n'
    for h in $(seq 1 1000); do printf '  headers/h%d.h \\\n' "$h"; done
    printf '  headers/h1.h\n'
} > "$work/rule.d"
for d in $(seq 0 1999); do cp "$work/rule.d" "$tree/deps/$d.d"; done
cat > "$tree/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")
const std = @import("std")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "probe", .version = "1.0.0", .description = "a probe of the runner's memory" }
}

fn number(b: *Build.Builder, prefix: []const u8, value: usize) -> []const u8 {
    var digits: [24]u8 = undefined
    const bytes: [*]u8 = unsafe { @ptrCast([*]u8, &digits) }
    const length = std.text.appendUnsigned(bytes, 0, 24, value)
    return Build.concat(b, prefix, bytes[0..length])
}

fn count(b: *Build.Builder, name: []const u8) -> usize {
    const text = Build.environment(b, name)
    var value: usize = 0
    for byte in text { value = value * 10 + @intCast(usize, byte - 48) }
    return value
}

// (A group holds at most 128 steps: groups of 100 steps in groups of 100
// groups.)
pub fn build(b: *Build.Builder) -> void {
    const trivial = Build.group(b, "trivial", "trivial commands")
    var chunk = trivial
    var part = trivial
    var index: usize = 0
    const steps = count(b, "PROBE_STEPS")
    while index < steps {
        if index % 10000 == 0 {
            part = Build.group(b, number(b, "trivial-part-", index), "")
            Build.add(b, trivial, part)
        }
        if index % 100 == 0 {
            chunk = Build.group(b, number(b, "trivial-", index), "")
            Build.add(b, part, chunk)
        }
        Build.add(b, chunk, Build.command(b, number(b, "t/", index), "", "/bin/true"))
        index += 1
    }
    const headers = Build.group(b, "headers", "commands whose depfiles name 1,000 headers")
    chunk = headers
    index = 0
    const deps = count(b, "PROBE_DEPS")
    while index < deps {
        if index % 100 == 0 {
            chunk = Build.group(b, number(b, "headers-", index), "")
            Build.add(b, headers, chunk)
        }
        const step = Build.command(b, number(b, "d/", index), "", "/bin/true")
        Build.depfile(b, step, Build.concat(b, number(b, "deps/", index), ".d"))
        Build.add(b, chunk, step)
        index += 1
    }
}
EOF_MLX

# Builds GROUP with PROBE_STEPS/PROBE_DEPS set; prints the runner's values
# (bytes) at its last snapshot.
values() {
    local group=$1 steps=$2 deps=$3 log=$4
    rm -rf "$work/run" "$tree/mlx-out/build/cache" "$tree/mlx-out/build/stamps"
    mkdir -p "$work/run"
    (cd "$tree" && PROBE_STEPS=$steps PROBE_DEPS=$deps XDG_RUNTIME_DIR="$work/run" MLX_LEAK_CHECK=1 "$compiler" build -j 4 "$group") > "$log" 2>&1 \
        || fail "the build of $steps trivial steps and $deps with depfiles failed" "$log"
    local snapshot
    snapshot=$(ls "$work"/run/mlx/memory/runner-*.memory 2> /dev/null | head -1)
    [[ -n "$snapshot" ]] || fail "the runner left no memory snapshot (MLX_LEAK_CHECK)" "$log"
    # (What `mlx-codemap memory runner` says of it, when there is one.)
    if [[ -x mlx-out/bin/mlx-codemap ]]; then
        mkdir -p "$work/empty"
        XDG_RUNTIME_DIR="$work/run" mlx-out/bin/mlx-codemap -C "$work/empty" memory runner 2> /dev/null | head -1 > "$log.codemap" || true
    fi
    # memory, program, pid, seconds, allocations, frees, blocks, bytes, mapped, peak, values, resident
    awk -F '\t' '$1 == "memory" { print $11; exit }' "$snapshot"
}

small=$(values trivial 2000 0 "$work/small.log")
large=$(values trivial 20000 0 "$work/large.log")
grep -q '^mlx build: 20000 built' "$work/large.log" || fail "the 20,000 steps were not all built" "$work/large.log"
(cd "$tree" && PROBE_STEPS=20000 PROBE_DEPS=0 "$compiler" build -j 4 trivial) > "$work/again.log" 2>&1 || fail "the build again failed" "$work/again.log"
grep -q '^mlx build: all 20000 steps current' "$work/again.log" || fail "the 20,000 steps are not current afterwards" "$work/again.log"
[[ $((large - small)) -lt 4194304 ]] || fail "the runner's values grew with the steps: $((small / 1024)) KiB after 2,000 steps, $((large / 1024)) KiB after 20,000"
echo "ok   the runner's values stay flat: $((small / 1024)) KiB after 2,000 trivial steps, $((large / 1024)) KiB after 20,000"
[[ -s "$work/large.log.codemap" ]] && echo "     mlx-codemap memory runner: $(cat "$work/large.log.codemap")"
grep -q "values (the compiler's arena) grew" "$work/large.log" && fail "std.leaks says the runner's values grew" "$work/large.log"
grep -q '^runner\[[0-9]*\]: no leaks' "$work/large.log" || fail "std.leaks found blocks the runner did not give back" "$work/large.log"

small=$(values headers 0 400 "$work/deps-small.log")
large=$(values headers 0 2000 "$work/deps-large.log")
grep -q '^mlx build: 2000 built' "$work/deps-large.log" || fail "the 2,000 steps with depfiles were not all built" "$work/deps-large.log"
(cd "$tree" && PROBE_STEPS=0 PROBE_DEPS=2000 "$compiler" build -j 4 headers) > "$work/deps-again.log" 2>&1 || fail "the build again failed" "$work/deps-again.log"
grep -q '^mlx build: all 2000 steps current' "$work/deps-again.log" || fail "the steps with depfiles are not current afterwards" "$work/deps-again.log"
[[ $((large - small)) -lt 4194304 ]] || fail "the runner's values grew with the depfiles' headers: $((small / 1024)) KiB after 400 steps of 1,000 headers, $((large / 1024)) KiB after 2,000"
echo "ok   the runner's values stay flat with large depfiles: $((small / 1024)) KiB after 400 steps of 1,000 headers, $((large / 1024)) KiB after 2,000"
[[ -s "$work/deps-large.log.codemap" ]] && echo "     mlx-codemap memory runner: $(cat "$work/deps-large.log.codemap")"
grep -q '^runner\[[0-9]*\]: no leaks' "$work/deps-large.log" || fail "std.leaks found blocks the runner did not give back" "$work/deps-large.log"
echo "check_build_runner.sh: all checks passed"
