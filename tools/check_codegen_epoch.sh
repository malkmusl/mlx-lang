#!/usr/bin/env bash
# Checks that mlx build keys mlx-cc's objects by mlx-cc's code generation
# epoch (compiler/cc/epoch.mlx, Build.programEpoch) and not by its bytes,
# on a small project of C and C++ objects in a temporary tree with a
# result cache of its own:
#
#   - `mlx-cc --codegen-epoch` prints CODEGEN_EPOCH, and the stamp the
#     `mlx-cc-epoch` step writes says it;
#   - a new mlx-cc that keeps the epoch (other bytes) leaves every object
#     current, and another copy of the tree takes them from the cache;
#   - a moved epoch builds every object again; moved back, they are
#     cached again;
#   - --verify-epoch N makes N cached objects again and passes when they
#     are the same; when mlx-cc makes something else under the same epoch
#     (MLX_CC_READ_ALL_BODIES=1 stands for a forgotten bump) it fails and
#     names them;
#   - a failure is remembered: built again with the same mlx-cc and files
#     the step fails at once with its log, no job (--retry-failed runs
#     it); a new mlx-cc of the same epoch runs exactly the failed step
#     (the others stay current); a fixed source builds;
#   - with the WebKit sources there (WEBKIT_SOURCE, else the default of
#     projects/webkit/build.mlx), unless --no-webkit: 20 units of WTF
#     (JSCOnly), built or taken from the cache in this checkout, are
#     compiled again by this mlx-cc and are the same (--verify-epoch 20).
#
#   tools/check_codegen_epoch.sh [--no-webkit] [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
webkit=1
if [[ "${1:-}" == --no-webkit ]]; then
    webkit=0
    shift
fi
compiler=${1:-$(tools/ensure_compiler.sh)}
[[ "$compiler" == /* ]] || compiler="$repo_root/$compiler"

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() {
    echo "FAIL $1" >&2
    [[ -f "${2:-}" ]] && cat "$2" >&2
    exit 1
}

user_cache=${MLX_BUILD_CACHE-}
export MLX_BUILD_CACHE="$work/cache"
unset MLX_BUILD_CACHE_SIZE
# (What a step's turn gives back is overwritten: a value the runner kept
# by mistake shows; std/src/build.mlx.)
export MLX_ARENA_POISON=1

tree="$work/tree"
mkdir -p "$tree/src"
ln -s "$repo_root/std" "$tree/std"
ln -s "$repo_root/compiler" "$tree/compiler"
cat > "$tree/src/main.c" <<'EOF_C'
#include <stdio.h>
int twice(int);
int main(void) { printf("%d\n", twice(21)); return 0; }
EOF_C
# (An inline function nothing calls, holding a literal: mlx-cc leaves its
# body unread, MLX_CC_READ_ALL_BODIES=1 reads it and the object differs.)
cat > "$tree/src/twice.cpp" <<'EOF_CPP'
inline const char *unused() { return "a literal only an unread body holds"; }
extern "C" int twice(int value) { return value * 2; }
EOF_CPP
cat > "$tree/src/bad.c" <<'EOF_C'
#include <stddef.h>
int broken(void) { return undeclared_name; }
EOF_C
cat > "$tree/build.mlx" <<'EOF_MLX'
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "probe", .version = "1.0.0", .description = "a probe of objects keyed by mlx-cc's epoch" }
}

fn object(b: *Build.Builder, cc: Build.Step, epoch: Build.Step, include: Build.Step, name: []const u8, source: []const u8) -> Build.Step {
    const output = Build.concat(b, Build.concat(b, "mlx-out/obj/", name), ".o")
    const depfile = Build.concat(b, output, ".d")
    var line = Build.concat(b, "mlx-out/bin/mlx-cc -MD -MF ", depfile)
    line = Build.concat(b, Build.concat(b, line, " -c "), source)
    line = Build.concat(b, line, Build.concat(b, " -o ", output))
    const step = Build.command(b, Build.concat(b, "obj/", name), output, line)
    Build.depfile(b, step, depfile)
    Build.programEpoch(b, step, "mlx-out/bin/mlx-cc.epoch")
    Build.dependOn(b, step, cc)
    Build.dependOn(b, step, epoch)
    Build.dependOn(b, step, include)
    return step
}

pub fn build(b: *Build.Builder) -> void {
    const all = Build.group(b, "all", "everything")
    const cc = Build.program(b, "mlx-cc", "compiler/cc/main.mlx")
    const epoch = Build.command(b, "mlx-cc-epoch", "mlx-out/bin/mlx-cc.epoch", "mlx-out/bin/mlx-cc --codegen-epoch=mlx-out/bin/mlx-cc.epoch")
    Build.input(b, epoch, "mlx-out/bin/mlx-cc")
    Build.dependOn(b, epoch, cc)
    const include = Build.link(b, "mlx-cc-include", "../../compiler/cc/include")
    Build.output(b, include, "mlx-out/include/mlx-cc")
    Build.add(b, all, object(b, cc, epoch, include, "main", "src/main.c"))
    Build.add(b, all, object(b, cc, epoch, include, "twice", "src/twice.cpp"))
    const failing = Build.group(b, "failing", "the objects and one that does not compile")
    Build.add(b, failing, all)
    Build.add(b, failing, object(b, cc, epoch, include, "bad", "src/bad.c"))
    Build.byDefault(b, all)
}
EOF_MLX

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

built() {
    sed -n 's/^\[[0-9]*\/[0-9]*\] //p' "$1" | grep -v '^cached ' | sort | tr '\n' ' ' | sed 's/ $//' || true
}

cached() {
    sed -n 's/^\[[0-9]*\/[0-9]*\] cached //p' "$1" | sort | tr '\n' ' ' | sed 's/ $//' || true
}

copy a
build a "$work/a.log" -j 2 || fail "the first build failed" "$work/a.log"
[[ "$(built "$work/a.log")" == "mlx-cc mlx-cc-epoch mlx-cc-include obj/main obj/twice" ]] || fail "the first build did not build every step" "$work/a.log"
epoch=$(sed -n 's/^pub const CODEGEN_EPOCH: u64 = \([0-9]*\)$/\1/p' compiler/cc/epoch.mlx)
[[ -n "$epoch" && "$("$work/a/mlx-out/bin/mlx-cc" --codegen-epoch)" == "$epoch" ]] || fail "mlx-cc --codegen-epoch does not print CODEGEN_EPOCH ($epoch)"
[[ "$(cat "$work/a/mlx-out/bin/mlx-cc.epoch")" == "mlx-cc codegen epoch $epoch" ]] || fail "the stamp does not say the epoch: $(cat "$work/a/mlx-out/bin/mlx-cc.epoch")"
echo "ok   mlx-cc --codegen-epoch prints CODEGEN_EPOCH ($epoch); the mlx-cc-epoch step writes it"

# A new mlx-cc with the same epoch (one byte more: other bytes, the same
# code): the stamp is written again, the objects stay current.
cp "$work/a/mlx-out/bin/mlx-cc" "$work/mlx-cc.new"
printf '\0' >> "$work/mlx-cc.new"
mv "$work/mlx-cc.new" "$work/a/mlx-out/bin/mlx-cc"
build a "$work/same.log" -j 2 || fail "the build with another mlx-cc of the same epoch failed" "$work/same.log"
[[ "$(built "$work/same.log")" == "mlx-cc-epoch" && -z "$(cached "$work/same.log")" ]] || fail "another mlx-cc of the same epoch did more than write the stamp again" "$work/same.log"
copy b
cp -a "$work/a/mlx-out" "$work/b/"
rm -rf "$work/b/mlx-out/obj" "$work/b/mlx-out/build/cache"/obj_*
build b "$work/copy.log" -j 2 || fail "the build of a copy failed" "$work/copy.log"
[[ "$(cached "$work/copy.log")" == "obj/main obj/twice" ]] || fail "a copy with the new mlx-cc did not take the objects from the cache" "$work/copy.log"
echo "ok   another mlx-cc of the same epoch keeps every object current, and cached for another copy"

# A moved epoch: mlx-cc through a wrapper that writes another stamp.
mv "$work/a/mlx-out/bin/mlx-cc" "$work/a/mlx-out/bin/mlx-cc.real"
cat > "$work/a/mlx-out/bin/mlx-cc" <<EOF_SH
#!/bin/sh
case "\$1" in --codegen-epoch=*) printf 'mlx-cc codegen epoch 999999\n' > "\${1#--codegen-epoch=}"; exit 0 ;; esac
exec "$work/a/mlx-out/bin/mlx-cc.real" "\$@"
EOF_SH
chmod +x "$work/a/mlx-out/bin/mlx-cc"
build a "$work/moved.log" -j 2 || fail "the build with a moved epoch failed" "$work/moved.log"
[[ "$(built "$work/moved.log")" == "mlx-cc-epoch obj/main obj/twice" ]] || fail "a moved epoch did not build every object" "$work/moved.log"
mv "$work/a/mlx-out/bin/mlx-cc.real" "$work/a/mlx-out/bin/mlx-cc"
build a "$work/back.log" -j 2 || fail "the build with the epoch back failed" "$work/back.log"
[[ "$(cached "$work/back.log")" == "obj/main obj/twice" ]] || fail "the epoch moved back did not take the objects from the cache" "$work/back.log"
echo "ok   a moved epoch builds every object; moved back, they come from the cache"

build a "$work/verify.log" -j 2 --verify-epoch 5 || fail "--verify-epoch failed with an honest mlx-cc" "$work/verify.log"
grep -q '^mlx build: --verify-epoch: the 2 files made again are the same, byte for byte$' "$work/verify.log" || fail "--verify-epoch did not make the 2 objects again" "$work/verify.log"
status=0
(cd "$work/a" && MLX_CC_READ_ALL_BODIES=1 "$compiler" build -j 2 --verify-epoch 2) > "$work/stale.log" 2>&1 || status=$?
[[ $status -ne 0 ]] && grep -q '^\[verify [12]/2\] DIFFERS obj/twice$' "$work/stale.log" && grep -q 'bump it' "$work/stale.log" \
    || fail "--verify-epoch did not catch an object made otherwise under the same epoch" "$work/stale.log"
echo "ok   --verify-epoch makes cached objects again: the same passes, another one fails loudly"

# Failures remembered.
copy f
status=0
build f "$work/fail.log" -j 2 --keep-going failing || status=$?
[[ $status -ne 0 && "$(built "$work/fail.log")" == *"obj/bad"* ]] && grep -q '^FAILED obj/bad (exit 1)' "$work/fail.log" || fail "the failing step did not fail" "$work/fail.log"
status=0
build f "$work/fail-again.log" -j 2 --keep-going failing || status=$?
[[ $status -ne 0 && -z "$(built "$work/fail-again.log")" ]] && grep -q '^FAILED obj/bad (exit 1, remembered' "$work/fail-again.log" && grep -q "undeclared identifier 'undeclared_name'" "$work/fail-again.log" \
    && grep -q '1 failed (1 remembered)' "$work/fail-again.log" || fail "the remembered failure was run again, or not told with its log" "$work/fail-again.log"
grep -q "undeclared_name" "$work/f/mlx-out/build/logs/obj_bad.log" || fail "the remembered failure's log is not in its place"
status=0
build f "$work/retry.log" -j 2 --keep-going --retry-failed failing || status=$?
[[ $status -ne 0 && "$(built "$work/retry.log")" == "obj/bad" ]] || fail "--retry-failed did not run the failed step" "$work/retry.log"
cp "$work/f/mlx-out/bin/mlx-cc" "$work/mlx-cc.new"
printf '\0' >> "$work/mlx-cc.new"
mv "$work/mlx-cc.new" "$work/f/mlx-out/bin/mlx-cc"
status=0
build f "$work/fixed-cc.log" -j 2 --keep-going failing || status=$?
[[ $status -ne 0 && "$(built "$work/fixed-cc.log")" == "mlx-cc-epoch obj/bad" ]] || fail "a new mlx-cc of the same epoch did not run exactly the failed step" "$work/fixed-cc.log"
sed -i 's/undeclared_name/0/' "$work/f/src/bad.c"
build f "$work/fixed.log" -j 2 failing || fail "the fixed source did not build" "$work/fixed.log"
[[ "$(built "$work/fixed.log")" == "obj/bad" ]] || fail "the fixed source built more than its step" "$work/fixed.log"
echo "ok   a failure is remembered (told again at once, --retry-failed runs it); a new mlx-cc runs exactly the failed step"

source_dir=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}
if [[ $webkit -eq 1 && -d "$source_dir/Source/WTF" ]]; then
    # (The checkout's own result cache, the one its builds use.)
    if [[ -n "$user_cache" ]]; then export MLX_BUILD_CACHE="$user_cache"; else unset MLX_BUILD_CACHE; fi
    ./mlx build -j 2 cc > "$work/cc.log" 2>&1 || fail "./mlx build cc failed" "$work/cc.log"
    ./mlx build -j 2 webkit-wtf --verify-epoch 20 > "$work/wtf.log" 2>&1 || fail "WTF's objects made again by this mlx-cc differ from the cached ones of its epoch (or WTF did not build)" "$work/wtf.log"
    grep -q '^mlx build: --verify-epoch: the 20 files made again are the same, byte for byte$' "$work/wtf.log" || fail "--verify-epoch did not make 20 of WTF's objects again" "$work/wtf.log"
    echo "ok   20 of WTF's objects made again by this mlx-cc are the ones of its epoch"
fi
echo "check_codegen_epoch.sh: all checks passed"
