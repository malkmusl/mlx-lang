#!/usr/bin/env bash
# Checks `mlx build` (std.build and the compiler's build command) on a small
# project in a temporary directory: what a first build makes, that a second
# builds nothing, that only the steps reading a changed module are built
# again (and not for a file saved unchanged), commands with input trees and
# with another step's output, a changed build.mlx and a changed compiler,
# a failing step, --keep-going, --dry-run, --force, --clean, --list, and the
# compiler's --depfile (without the result cache: check_build_cache.sh),
# and that the runner's memory stays flat over 20,000 steps
# (check_build_runner.sh).
#
#   tools/check_build.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
[[ "$compiler" == /* ]] || compiler="$repo_root/$compiler"

work=$(mktemp -d)
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT

fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && cat "$2" >&2
    exit 1
}

# What is built here, built: the result cache (the user's, shared by their
# checkouts) has a check of its own, tools/check_build_cache.sh.
export MLX_BUILD_CACHE=off

# The project: std/ is where the compiler looks for it.
project="$work/project"
mkdir -p "$project/src" "$project/data"
ln -s "$repo_root/std" "$project/std"
cat > "$project/src/greet.mlx" <<'EOF_MLX'
pub fn word() -> []const u8 { return "hello" }
EOF_MLX
cat > "$project/src/hello.mlx" <<'EOF_MLX'
const std = @import("std")
const greet = @import("./greet.mlx")
pub fn main(arguments: [][*]const u8) -> u8 {
    const written = std.fs.writeAll(std.fs.File.{ .descriptor = 1 }, greet.word())
    return 0
}
EOF_MLX
cat > "$project/src/other.mlx" <<'EOF_MLX'
pub fn main(arguments: [][*]const u8) -> u8 { return 7 }
EOF_MLX
echo one > "$project/data/a.txt"
echo two > "$project/data/b.txt"
cat > "$project/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "probe", .version = "1.2.3", .description = "a probe of std.build" }
}

pub fn build(b: *Build.Builder) -> void {
    const all = Build.group(b, "all", "everything")
    const hello = Build.program(b, "hello", "src/hello.mlx")
    Build.add(b, all, hello)
    Build.add(b, all, Build.program(b, "other", "src/other.mlx"))
    const link = Build.link(b, "hi", "hello")
    Build.dependOn(b, link, hello)
    Build.add(b, all, link)
    const text = Build.command(b, "text", "mlx-out/text.txt", "sh -c \"cat data/*.txt > mlx-out/text.txt\"")
    Build.inputTree(b, text, "data", ".txt")
    Build.add(b, all, text)
    const greeting = Build.command(b, "greeting", "mlx-out/greeting.txt", "sh -c \"mlx-out/bin/hello > mlx-out/greeting.txt\"")
    Build.input(b, greeting, "mlx-out/bin/hello")
    Build.dependOn(b, greeting, hello)
    Build.add(b, all, greeting)
    Build.add(b, all, Build.copy(b, "data-copy", "data/a.txt", "mlx-out/a-copy.txt"))
    Build.byDefault(b, all)
}
EOF_MLX

build() {
    local log=$1
    shift
    local status=0
    (cd "$project" && "$compiler" build "$@") > "$log" 2>&1 || status=$?
    return $status
}

# The steps a run started, one per line ("[n/m] NAME").
started() {
    sed -n 's/^\[[0-9]*\/[0-9]*\] //p' "$1" | sort | tr '\n' ' ' | sed 's/ $//'
}

build "$work/first.log" -j 2 || fail "the first build failed" "$work/first.log"
[[ "$(started "$work/first.log")" == "data-copy greeting hello hi other text" ]] || fail "the first build did not build every step" "$work/first.log"
grep -q '^mlx build: compiling build.mlx$' "$work/first.log" || fail "build.mlx was not compiled" "$work/first.log"
[[ "$("$project/mlx-out/bin/hi")" == hello && "$(cat "$project/mlx-out/greeting.txt")" == hello ]] || fail "the outputs are wrong"
[[ "$(cat "$project/mlx-out/text.txt")" == $'one\ntwo' && "$(cat "$project/mlx-out/a-copy.txt")" == one ]] || fail "the command or the copy made the wrong file"
"$project/mlx-out/bin/other" && fail "mlx-out/bin/other is not the program built" || [[ $? -eq 7 ]] || fail "mlx-out/bin/other returned the wrong status"
echo "ok   a first build makes programs, a link, commands and a copy"

build "$work/again.log" || fail "the second build failed" "$work/again.log"
grep -qx 'mlx build: all 6 steps current ([0-9.]* s)' "$work/again.log" || fail "a second build built something" "$work/again.log"
touch "$project/src/greet.mlx" "$project/data/a.txt"
build "$work/touched.log" || fail "the build after touch failed" "$work/touched.log"
grep -q 'all 6 steps current' "$work/touched.log" || fail "files saved unchanged caused a build" "$work/touched.log"
echo "ok   nothing is built again when nothing changed (files saved unchanged included)"

echo 'pub fn word() -> []const u8 { return "servus" }' > "$project/src/greet.mlx"
build "$work/changed.log" || fail "the build after a change failed" "$work/changed.log"
[[ "$(started "$work/changed.log")" == "greeting hello" ]] || fail "a changed import did not rebuild exactly its program and what reads its output" "$work/changed.log"
[[ "$(cat "$project/mlx-out/greeting.txt")" == servus ]] || fail "the command reading the program's output was not run again"
echo three > "$project/data/c.txt"
build "$work/tree.log" || fail "the build after a new tree file failed" "$work/tree.log"
[[ "$(started "$work/tree.log")" == "text" && "$(cat "$project/mlx-out/text.txt")" == $'one\ntwo\nthree' ]] || fail "a file added to an input tree did not run its command again" "$work/tree.log"
echo "ok   a changed module rebuilds only its users; a command runs again for its inputs, input trees and another step's output"

sed -i 's/a probe of std.build/a probe/' "$project/build.mlx"
build "$work/renewed.log" || fail "the build after a build.mlx change failed" "$work/renewed.log"
grep -q 'compiling build.mlx again' "$work/renewed.log" && grep -q 'all 6 steps current' "$work/renewed.log" || fail "a changed build.mlx was not compiled again (or rebuilt steps)" "$work/renewed.log"
build "$work/list.log" --list || fail "--list failed" "$work/list.log"
grep -q '^probe 1.2.3: a probe$' "$work/list.log" && grep -q '^  all  *everything (the default)$' "$work/list.log" && grep -q '^  hello  *program  *mlx-out/bin/hello$' "$work/list.log" || fail "--list is wrong" "$work/list.log"
echo "ok   a changed build.mlx is compiled again; --list names the package, groups and steps"

cp "$compiler" "$work/other-compiler"
printf '\n' >> "$work/other-compiler"
status=0
(cd "$project" && mlx-out/build/runner --compiler="$work/other-compiler") > "$work/compiler.log" 2>&1 || status=$?
[[ $status -eq 0 && "$(started "$work/compiler.log")" == "hello other" ]] || fail "another compiler did not rebuild exactly the Mlx steps" "$work/compiler.log"
build "$work/back.log" || fail "the build back on the compiler failed" "$work/back.log"
echo "ok   another compiler builds the Mlx steps (and build.mlx) again"

echo 'pub fn main( -> u8 {' > "$project/src/other.mlx"
echo 'pub fn word() -> []const u8 { return "moin" }' > "$project/src/greet.mlx"
status=0
build "$work/failed.log" -j 1 || status=$?
[[ $status -eq 1 ]] || fail "a failed step did not fail the build" "$work/failed.log"
grep -q '^FAILED other (exit [0-9]*); its log: mlx-out/build/logs/other.log$' "$work/failed.log" && grep -q 'src/other.mlx:1:.*error' "$work/failed.log" || fail "the failure was not reported with its log" "$work/failed.log"
before=$(cat "$project/mlx-out/greeting.txt"; cksum < "$project/mlx-out/bin/hello")
status=0
build "$work/dry.log" --dry-run || status=$?
grep -q 'to build' "$work/dry.log" || fail "--dry-run did not say what it would build" "$work/dry.log"
[[ "$(cat "$project/mlx-out/greeting.txt"; cksum < "$project/mlx-out/bin/hello")" == "$before" ]] || fail "--dry-run built something" "$work/dry.log"
status=0
build "$work/going.log" -k -j 1 || status=$?
[[ $status -eq 1 && "$(cat "$project/mlx-out/greeting.txt")" == moin ]] || fail "--keep-going did not build the rest" "$work/going.log"
grep -q '1 failed' "$work/going.log" || fail "--keep-going did not count the failure" "$work/going.log"
echo 'pub fn main(arguments: [][*]const u8) -> u8 { return 7 }' > "$project/src/other.mlx"
build "$work/fixed.log" || fail "the build after the fix failed" "$work/fixed.log"
[[ "$(started "$work/fixed.log")" == "other" ]] || fail "after the fix more than the failed step was built" "$work/fixed.log"
echo "ok   a failing step fails the build with its log; --dry-run builds nothing; --keep-going builds the rest"

build "$work/forced.log" --force hello || fail "--force failed" "$work/forced.log"
[[ "$(started "$work/forced.log")" == "hello" ]] || fail "--force did not build the step named" "$work/forced.log"
status=0
build "$work/unknown.log" no-such-step || status=$?
[[ $status -eq 2 ]] && grep -q 'no step called no-such-step' "$work/unknown.log" || fail "an unknown step was accepted" "$work/unknown.log"
build "$work/clean.log" --clean || fail "--clean failed" "$work/clean.log"
build "$work/after-clean.log" || fail "the build after --clean failed" "$work/after-clean.log"
grep -q '^mlx build: 6 built' "$work/after-clean.log" || fail "after --clean not everything was built" "$work/after-clean.log"
echo "ok   --force, an unknown step, --clean"

(cd "$project" && "$compiler" --quiet --depfile="$work/deps" src/hello.mlx -o "$work/hello") || fail "--depfile failed"
head -n 2 "$work/deps" | tr '\n' ' ' | grep -qx 'src/hello.mlx src/greet.mlx ' || head -n 1 "$work/deps" | grep -qx 'src/hello.mlx' || fail "--depfile did not list the modules" "$work/deps"
grep -qx 'src/greet.mlx' "$work/deps" && grep -q '^std/' "$work/deps" || fail "--depfile left out imports" "$work/deps"
echo "ok   the compiler lists the modules it read with --depfile"
# A command's dependency file in make's form (as mlx-cc -MD writes it):
# the files it names are the command's inputs.
deps_project=$work/depsproject
mkdir -p "$deps_project/in"
ln -s "$repo_root/std" "$deps_project/std"
echo one > "$deps_project/in/a.h"
echo two > "$deps_project/in/b c.h"
cat > "$deps_project/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "deps", .version = "1.0.0", .description = "make depfiles" }
}

pub fn build(b: *Build.Builder) -> void {
    const joined = Build.command(b, "joined", "mlx-out/joined.txt", "sh -c \"cat in/a.h 'in/b c.h' > mlx-out/joined.txt; printf 'mlx-out/joined.txt: in/a.h \\\\\\n  in/b\\\\ c.h\\n' > mlx-out/joined.d\"")
    Build.depfile(b, joined, "mlx-out/joined.d")
    Build.byDefault(b, joined)
}
EOF_MLX
(cd "$deps_project" && "$compiler" build) > "$work/deps1.log" 2>&1 || fail "the depfile build failed" "$work/deps1.log"
grep -q 'input .* in/b c.h$' "$deps_project/mlx-out/build/cache/joined.manifest" || fail "the make depfile's names did not become inputs" "$deps_project/mlx-out/build/cache/joined.manifest"
(cd "$deps_project" && "$compiler" build) > "$work/deps2.log" 2>&1 || fail "the second depfile build failed" "$work/deps2.log"
grep -q 'the step is current' "$work/deps2.log" || fail "a current command with a depfile ran again" "$work/deps2.log"
echo three > "$deps_project/in/b c.h"
(cd "$deps_project" && "$compiler" build) > "$work/deps3.log" 2>&1 || fail "the third depfile build failed" "$work/deps3.log"
[[ "$(started "$work/deps3.log")" == "joined" && "$(cat "$deps_project/mlx-out/joined.txt")" == $'one\nthree' ]] || fail "a file of the depfile changed and the command did not run again" "$work/deps3.log"
echo "ok   a command's make-style depfile lists its inputs"
# The runner's memory over tens of thousands of steps (std.turns).
tools/check_build_runner.sh "$compiler" | grep -v '^check_build_runner.sh:' || fail "the runner's memory check failed"
echo "all mlx build checks passed"
