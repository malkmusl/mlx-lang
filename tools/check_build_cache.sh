#!/usr/bin/env bash
# Checks std.build's result cache (std/src/build/store.mlx) on a small
# project of C and Mlx steps (mlx-cc, mlx-ld and mlx-ar built as in build.mlx,
# objects with depfiles, an archive, a link, an Mlx program, a command
# that is not cached) in copies of its tree, with a cache of its own in a
# temporary directory:
#
#   - a second copy (another directory) takes every cached step from the
#     cache, byte for byte what the first built, linked (one inode), and
#     builds only what is not cached;
#   - a changed header builds only the steps that read it; changed back,
#     they come from the cache again (A, B, A); so do the steps of a
#     command line changed and changed back, and of a changed program
#     (mlx-cc) or Mlx compiler, which are built, never taken;
#   - a damaged cache file is found out (and removed), a removed cache
#     builds everything again, MLX_BUILD_CACHE=off neither reads nor
#     writes it; two builds sharing it at once both succeed;
#   - MLX_BUILD_CACHE_SIZE: the least recently used files go, with their
#     entries; a cache on another file system (/dev/shm) is copied from;
#   - --cache-stats and --cache-clear.
#
#   tools/check_build_cache.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
[[ "$compiler" == /* ]] || compiler="$repo_root/$compiler"

work=$(mktemp -d)
shm_cache=""
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$work"
    [[ -n "$shm_cache" ]] && rm -rf -- "$shm_cache"
    exit "$status"
}
trap cleanup EXIT

fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && cat "$2" >&2
    exit 1
}

export MLX_BUILD_CACHE="$work/cache"
unset MLX_BUILD_CACHE_SIZE

# The project's tree (without mlx-out): std/ and compiler/ are this
# repository's.
tree="$work/tree"
mkdir -p "$tree/src" "$tree/data"
ln -s "$repo_root/std" "$tree/std"
ln -s "$repo_root/compiler" "$tree/compiler"
cat > "$tree/src/shared.h" <<'EOF_C'
#define GREETING "result"
EOF_C
cat > "$tree/src/util.h" <<'EOF_C'
#include "shared.h"
#define FACTOR 2
int scaled(int value);
int offset(void);
EOF_C
cat > "$tree/src/util.c" <<'EOF_C'
#include "util.h"
int scaled(int value) { return value * FACTOR; }
EOF_C
cat > "$tree/src/other.c" <<'EOF_C'
int offset(void) { return 1; }
EOF_C
cat > "$tree/src/main.c" <<'EOF_C'
#include <stdio.h>
#include "util.h"
int main(void) {
    printf("%s %d\n", GREETING, scaled(20) + offset());
    return 0;
}
EOF_C
cat > "$tree/src/greet.mlx" <<'EOF_MLX'
const std = @import("std")
pub fn main(arguments: [][*]const u8) -> u8 {
    _ = std.fs.writeAll(std.fs.File.{ .descriptor = 1 }, "hello")
    return 0
}
EOF_MLX
echo one > "$tree/data/a.txt"
cat > "$tree/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "probe", .version = "1.0.0", .description = "a probe of the result cache" }
}

const FLAGS: []const u8 = " -Isrc"

fn object(b: *Build.Builder, cc: Build.Step, include: Build.Step, name: []const u8, source: []const u8) -> Build.Step {
    const output = Build.concat(b, Build.concat(b, "mlx-out/obj/", name), ".o")
    const depfile = Build.concat(b, output, ".d")
    var line = Build.concat(b, "mlx-out/bin/mlx-cc -MD -MF ", depfile)
    line = Build.concat(b, Build.concat(b, line, FLAGS), " -c ")
    line = Build.concat(b, Build.concat(b, line, source), Build.concat(b, " -o ", output))
    const step = Build.command(b, Build.concat(b, "obj/", name), output, line)
    Build.depfile(b, step, depfile)
    Build.input(b, step, "mlx-out/bin/mlx-cc")
    Build.dependOn(b, step, cc)
    Build.dependOn(b, step, include)
    return step
}

pub fn build(b: *Build.Builder) -> void {
    const all = Build.group(b, "all", "everything")
    const cc = Build.program(b, "mlx-cc", "compiler/cc/main.mlx")
    const ar = Build.program(b, "mlx-ar", "compiler/cc/ar.mlx")
    const ld = Build.program(b, "mlx-ld", "compiler/ld/main.mlx")
    const include = Build.link(b, "mlx-cc-include", "../../compiler/cc/include")
    Build.output(b, include, "mlx-out/include/mlx-cc")
    const main = object(b, cc, include, "main", "src/main.c")
    const util = object(b, cc, include, "util", "src/util.c")
    const other = object(b, cc, include, "other", "src/other.c")
    const library = Build.command(b, "libutil.a", "mlx-out/lib/libutil.a", "mlx-out/bin/mlx-ar rcs mlx-out/lib/libutil.a mlx-out/obj/util.o mlx-out/obj/other.o")
    Build.input(b, library, "mlx-out/bin/mlx-ar")
    Build.input(b, library, "mlx-out/obj/util.o")
    Build.input(b, library, "mlx-out/obj/other.o")
    Build.cacheable(b, library)
    Build.dependOn(b, library, ar)
    Build.dependOn(b, library, util)
    Build.dependOn(b, library, other)
    const hello = Build.command(b, "hello", "mlx-out/bin/hello", "mlx-out/bin/mlx-cc mlx-out/obj/main.o mlx-out/lib/libutil.a -o mlx-out/bin/hello")
    Build.input(b, hello, "mlx-out/bin/mlx-cc")
    Build.input(b, hello, "mlx-out/obj/main.o")
    Build.input(b, hello, "mlx-out/lib/libutil.a")
    Build.input(b, hello, "mlx-out/bin/mlx-ld")
    Build.dependOn(b, hello, ld)
    Build.dependOn(b, hello, main)
    Build.dependOn(b, hello, library)
    Build.add(b, all, hello)
    Build.add(b, all, Build.program(b, "greet", "src/greet.mlx"))
    const text = Build.command(b, "text", "mlx-out/text.txt", "sh -c \"cat data/*.txt > mlx-out/text.txt\"")
    Build.inputTree(b, text, "data", ".txt")
    Build.add(b, all, text)
    Build.byDefault(b, all)
}
EOF_MLX

# A copy of the tree at $work/NAME.
copy() {
    cp -a "$tree" "$work/$1"
}

build() {
    local directory=$1 log=$2
    shift 2
    local status=0
    (cd "$work/$directory" && "$compiler" build "$@") > "$log" 2>&1 || status=$?
    return $status
}

# The steps a run built (not cached), and those it took from the cache.
built() {
    sed -n 's/^\[[0-9]*\/[0-9]*\] //p' "$1" | grep -v '^cached ' | sort | tr '\n' ' ' | sed 's/ $//' || true
}
cached() {
    sed -n 's/^\[[0-9]*\/[0-9]*\] cached //p' "$1" | sort | tr '\n' ' ' | sed 's/ $//' || true
}

objects_in() {
    find "$1/objects" -type f 2> /dev/null | wc -l
}

cached_files="mlx-out/bin/mlx-cc mlx-out/bin/mlx-ld mlx-out/bin/mlx-ar mlx-out/bin/greet mlx-out/obj/main.o mlx-out/obj/util.o mlx-out/obj/other.o mlx-out/lib/libutil.a"
cached_steps="greet libutil.a mlx-ar mlx-cc mlx-ld obj/main obj/other obj/util"

copy a
build a "$work/a.log" -j 2 || fail "the first build failed" "$work/a.log"
[[ "$(built "$work/a.log")" == "greet hello libutil.a mlx-ar mlx-cc mlx-cc-include mlx-ld obj/main obj/other obj/util text" && -z "$(cached "$work/a.log")" ]] \
    || fail "the first build (an empty cache) did not build every step" "$work/a.log"
[[ "$("$work/a/mlx-out/bin/hello")" == "result 41" && "$("$work/a/mlx-out/bin/greet")" == hello ]] || fail "the first build's programs are wrong"
# 8 steps, and the runner the compiler made for build.mlx.
[[ $(objects_in "$work/cache") -eq 9 ]] || fail "the cache does not hold the 8 cached steps' files and the runner ($(objects_in "$work/cache"))"
echo "ok   a first build fills the cache (Mlx steps, objects with depfiles, an archive marked cacheable)"

copy b
build b "$work/b.log" -j 2 || fail "the build of the second copy failed" "$work/b.log"
[[ "$(cached "$work/b.log")" == "$cached_steps" && "$(built "$work/b.log")" == "hello mlx-cc-include text" ]] \
    || fail "the second copy did not take exactly the cached steps from the cache" "$work/b.log"
grep -q "^mlx build: 3 built, 8 cached, 0 current" "$work/b.log" || fail "the summary does not count what was cached" "$work/b.log"
for file in $cached_files mlx-out/bin/hello; do
    cmp -s "$work/a/$file" "$work/b/$file" || fail "$file in the second copy is not the first's"
done
[[ "$(stat -c %i "$work/a/mlx-out/obj/util.o")" == "$(stat -c %i "$work/b/mlx-out/obj/util.o")" ]] || fail "a cached object was not linked into place"
[[ "$("$work/b/mlx-out/bin/hello")" == "result 41" ]] || fail "the second copy's program is wrong"
# The depfiles' absolute paths into the copy (mlx-cc's own headers) are
# kept relative to its root.
grep -rq "$work/a" "$work/cache/entries" && fail "an entry names a path in the first copy"
grep -q '^input [0-9a-f]* mlx-out/include/mlx-cc/stdio.h$' "$work/cache/entries"/*/* || fail "no entry names mlx-cc's stdio.h relative to the root"
build b "$work/b-again.log" || fail "the build after the cached one failed" "$work/b-again.log"
grep -qx 'mlx build: all 11 steps current ([0-9.]* s)' "$work/b-again.log" || fail "steps taken from the cache were not current afterwards" "$work/b-again.log"
echo "ok   another copy of the tree takes them from the cache, the same bytes, linked; then they are current"

sed -i 's/FACTOR 2/FACTOR 3/' "$work/b/src/util.h"
build b "$work/header.log" || fail "the build after a header change failed" "$work/header.log"
[[ "$(built "$work/header.log")" == "hello libutil.a obj/main obj/util" && -z "$(cached "$work/header.log")" ]] \
    || fail "a changed header did not build exactly the steps reading it (and what reads theirs)" "$work/header.log"
[[ "$("$work/b/mlx-out/bin/hello")" == "result 61" ]] || fail "the program after the header change is wrong"
cp "$work/b/mlx-out/obj/util.o" "$work/util-3.o"
sed -i 's/FACTOR 3/FACTOR 2/' "$work/b/src/util.h"
build b "$work/reverted.log" || fail "the build after the header was changed back failed" "$work/reverted.log"
[[ "$(cached "$work/reverted.log")" == "libutil.a obj/main obj/util" && "$(built "$work/reverted.log")" == "hello" ]] \
    || fail "the header changed back did not take its steps from the cache" "$work/reverted.log"
[[ "$("$work/b/mlx-out/bin/hello")" == "result 41" ]] && cmp -s "$work/a/mlx-out/obj/util.o" "$work/b/mlx-out/obj/util.o" || fail "the program after the header was changed back is wrong"
sed -i 's/FACTOR 2/FACTOR 3/' "$work/b/src/util.h"
build b "$work/again.log" || fail "the build after the header was changed again failed" "$work/again.log"
[[ "$(cached "$work/again.log")" == "libutil.a obj/main obj/util" ]] && cmp -s "$work/util-3.o" "$work/b/mlx-out/obj/util.o" \
    || fail "the header's other version did not come from the cache too (A, B, A, B)" "$work/again.log"
echo "ok   a changed header builds what reads it; its versions back and forth come from the cache"

sed -i 's/const FLAGS: \[\]const u8 = " -Isrc"/const FLAGS: []const u8 = " -Isrc -DUNUSED=1"/' "$work/b/build.mlx"
build b "$work/line.log" || fail "the build with another command line failed" "$work/line.log"
[[ "$(built "$work/line.log")" == "obj/main obj/other obj/util" && -z "$(cached "$work/line.log")" ]] \
    || fail "another command line did not build its steps" "$work/line.log"
sed -i 's/ -DUNUSED=1"/"/' "$work/b/build.mlx"
build b "$work/line-back.log" || fail "the build with the command line changed back failed" "$work/line-back.log"
[[ "$(cached "$work/line-back.log")" == "obj/main obj/other obj/util" ]] || fail "the command line changed back did not take its steps from the cache" "$work/line-back.log"
echo "ok   a changed command line builds; changed back, it is cached"

# Another mlx-cc (one byte more; a new file, not the one the cache shares).
cp "$work/b/mlx-out/bin/mlx-cc" "$work/mlx-cc.new"
printf '\0' >> "$work/mlx-cc.new"
mv "$work/mlx-cc.new" "$work/b/mlx-out/bin/mlx-cc"
build b "$work/program.log" || fail "the build with another mlx-cc failed" "$work/program.log"
[[ "$(built "$work/program.log")" == "hello obj/main obj/other obj/util" && -z "$(cached "$work/program.log")" ]] \
    || fail "another mlx-cc (the command's program) did not build its objects" "$work/program.log"
cmp -s "$work/a/mlx-out/bin/mlx-cc" "$work/b/mlx-out/bin/mlx-cc" && fail "the cache's mlx-cc was written into"
rm "$work/b/mlx-out/bin/mlx-cc"
build b "$work/program-back.log" || fail "the build with mlx-cc back failed" "$work/program-back.log"
[[ "$(cached "$work/program-back.log")" == "mlx-cc obj/main obj/other obj/util" ]] || fail "mlx-cc back did not take its objects from the cache" "$work/program-back.log"
cp "$compiler" "$work/other-compiler"
printf '\n' >> "$work/other-compiler"
status=0
(cd "$work/b" && mlx-out/build/runner --compiler="$work/other-compiler") > "$work/compiler.log" 2>&1 || status=$?
[[ $status -eq 0 && "$(built "$work/compiler.log")" == "greet mlx-ar mlx-cc mlx-ld" && -z "$(cached "$work/compiler.log")" ]] \
    || fail "another Mlx compiler did not build the Mlx steps" "$work/compiler.log"
build b "$work/compiler-back.log" || fail "the build back on the compiler failed" "$work/compiler-back.log"
[[ "$(cached "$work/compiler-back.log")" == "greet mlx-ar mlx-cc mlx-ld" ]] || fail "the compiler back did not take the Mlx steps from the cache" "$work/compiler-back.log"
echo "ok   another mlx-cc or Mlx compiler builds (the cache's files untouched); back again, it is cached"

# Damaged: every file of the cache replaced by as many zeros (new files:
# the copies' outputs keep theirs).
copy c
while read -r file; do
    head -c "$(stat -c %s "$file")" /dev/zero > "$file.zero"
    mv "$file.zero" "$file"
done < <(find "$work/cache/objects" -type f)
build c "$work/damaged.log" -j 2 || fail "the build over a damaged cache failed" "$work/damaged.log"
[[ -z "$(cached "$work/damaged.log")" && "$("$work/c/mlx-out/bin/hello")" == "result 41" ]] || fail "a damaged cache file was taken" "$work/damaged.log"
# (What the build made went in in place of the damaged files.)
for file in $cached_files; do
    [[ -n "$(find "$work/cache/objects" -samefile "$work/c/$file")" ]] || fail "the damaged cache file for $file was not replaced"
done
rm -rf "$work/cache"
copy d
build d "$work/removed.log" -j 2 || fail "the build without a cache failed" "$work/removed.log"
[[ -z "$(cached "$work/removed.log")" && $(objects_in "$work/cache") -gt 0 ]] || fail "a removed cache was not made again" "$work/removed.log"
copy e
before=$(find "$work/cache" -type f | wc -l)
MLX_BUILD_CACHE=off build e "$work/off.log" -j 2 || fail "the build with the cache off failed" "$work/off.log"
[[ -z "$(cached "$work/off.log")" && $(find "$work/cache" -type f | wc -l) -eq $before ]] || fail "MLX_BUILD_CACHE=off used the cache" "$work/off.log"
echo "ok   a damaged cache file is found out and removed; a removed cache is made again; off is off"

# Two builds at once, sharing it: both done, the same bytes.
rm -rf "$work/cache"
copy f
copy g
build f "$work/f.log" -j 2 &
first=$!
build g "$work/g.log" -j 2 || fail "one of two builds at once failed" "$work/g.log"
wait $first || fail "one of two builds at once failed" "$work/f.log"
for file in $cached_files mlx-out/bin/hello; do
    cmp -s "$work/f/$file" "$work/g/$file" || fail "$file differs between two builds at once"
done
copy h
build h "$work/h.log" -j 2 || fail "the build after two at once failed" "$work/h.log"
[[ "$(cached "$work/h.log")" == "$cached_steps" ]] || fail "what two builds at once stored is not found" "$work/h.log"
[[ -z "$(ls -A "$work/cache/tmp")" ]] || fail "files were left in the cache's tmp/"
echo "ok   two builds at once share the cache"

(cd "$work/h" && "$compiler" build --cache-stats) > "$work/stats.log" 2>&1 || fail "--cache-stats failed" "$work/stats.log"
grep -q "^result cache: $work/cache$" "$work/stats.log" && grep -q '^  9 files, ' "$work/stats.log" && grep -q '^  9 entries for 9 steps$' "$work/stats.log" \
    || fail "--cache-stats is wrong" "$work/stats.log"
(cd "$work/h" && MLX_BUILD_CACHE_SIZE=2M "$compiler" build --force) > "$work/trim.log" 2>&1 || fail "the build with a small limit failed" "$work/trim.log"
grep -q '^mlx build: the result cache passed its limit; removed [0-9]* of its files' "$work/trim.log" || fail "a cache over its limit was not trimmed" "$work/trim.log"
total=$(find "$work/cache/objects" -type f -printf '%s\n' | awk '{ s += $1 } END { print s + 0 }')
[[ $total -le $((2 * 1048576 * 4 / 5)) ]] || fail "the trimmed cache is still over four fifths of its limit ($total)"
while read -r entry; do
    object=$(sed -n 's/^object //p' "$entry")
    [[ -f "$work/cache/objects/${object:0:2}/$object" ]] || fail "an entry names a file the trim removed: $entry"
done < <(find "$work/cache/entries" -type f)
(cd "$work/h" && "$compiler" build --cache-clear) > "$work/clear.log" 2>&1 || fail "--cache-clear failed" "$work/clear.log"
[[ ! -e "$work/cache" ]] || fail "--cache-clear left the cache"
echo "ok   --cache-stats; over MLX_BUILD_CACHE_SIZE the least recently used go, with their entries; --cache-clear"

# On another file system the cache's files are copied, not linked.
if [[ -d /dev/shm && "$(stat -c %d /dev/shm)" != "$(stat -c %d "$work")" ]]; then
    shm_cache=$(mktemp -d /dev/shm/mlx-build-cache.XXXXXX)
    copy i
    copy j
    MLX_BUILD_CACHE="$shm_cache" build i "$work/i.log" -j 2 || fail "the build with the cache in /dev/shm failed" "$work/i.log"
    MLX_BUILD_CACHE="$shm_cache" build j "$work/j.log" -j 2 || fail "the second build with the cache in /dev/shm failed" "$work/j.log"
    [[ "$(cached "$work/j.log")" == "$cached_steps" ]] || fail "a cache on another file system was not used" "$work/j.log"
    for file in $cached_files; do
        cmp -s "$work/i/$file" "$work/j/$file" || fail "$file from a cache on another file system differs"
        [[ $(stat -c %h "$work/j/$file") -eq 1 ]] || fail "$file from a cache on another file system is a link"
    done
    echo "ok   a cache on another file system is copied from"
fi
echo "all result cache checks passed"
