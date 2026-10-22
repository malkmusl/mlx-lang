#!/usr/bin/env bash
# mlx-codemap's C and C++ (tools/codemap/cpp.mlx), on the small project of
# tools/codemap/tests/cpp: classes, a template, overloads, namespaces,
# macros, a virtual function and its override, C beside C++, and an Mlx
# `export fn` the C and C++ call through the header mlx-cheaders makes.
#   - mlx-cc --index=DIR (and MLX_CC_INDEX=DIR) writes the index of what
#     it compiles, and the objects are byte for byte the ones it writes
#     without (no CODEGEN_EPOCH bump needed);
#   - a shard is written once: compiling again writes none;
#   - find, refs, callers, callees, outline, def, show, members, imports
#     and importers answer as tools/codemap/tests/cpp/expected/ says,
#     across C, C++ and Mlx.
#
#   tools/check_codemap_cpp.sh [compiler]
#   CHECK_UPDATE=1 tools/check_codemap_cpp.sh    writes the expected files
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
cc="$repo_root/mlx-out/bin/mlx-cc"
codemap="$repo_root/mlx-out/bin/codemap"
project="$repo_root/tools/codemap/tests/cpp"
[[ -x "$cc" && -x "$codemap" ]] || { echo "check_codemap_cpp.sh: ./mlx build cc codemap first" >&2; exit 2; }

work=$(mktemp -d)
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

# The project in a tree of its own (its Mlx too), the header of api.mlx.
tree="$work/tree"
mkdir -p "$tree/include"
cp "$project"/*.h "$project"/*.cpp "$project"/*.c "$project"/*.mlx "$tree/"
"$compiler" --quiet compiler/cc/cheaders.mlx -o "$work/mlx-cheaders"
"$work/mlx-cheaders" "$tree/include" "$tree/api.mlx"
[[ -f "$tree/include/shapes_api.h" ]] || fail "mlx-cheaders wrote no shapes_api.h"

# Objects with and without the index: the same bytes.
index="$tree/mlx-out/codemap/cpp"
cd "$tree"
"$cc" -c -I include shapes.cpp -o "$work/plain.o"
"$cc" --index="$index" -c -I include shapes.cpp -o "$work/indexed.o"
cmp -s "$work/plain.o" "$work/indexed.o" || fail "--index changed the object of shapes.cpp"
"$cc" -c -I include legacy.c -o "$work/legacy-plain.o"
MLX_CC_INDEX="$index" "$cc" -c -I include legacy.c -o "$work/legacy.o"
cmp -s "$work/legacy-plain.o" "$work/legacy.o" || fail "MLX_CC_INDEX changed the object of legacy.c"
echo "ok   mlx-cc --index and MLX_CC_INDEX leave the objects as they are"

for file in shapes.cpp shapes.h legacy.c include/shapes_api.h; do
  grep -lq "^file [0-9a-f]* $tree/$file\$" "$index"/*.cxi || fail "no shard of $file"
done
before=$(ls -l --time-style=+%s.%N "$index" | md5sum)
"$cc" --index="$index" -c -I include shapes.cpp -o "$work/again.o"
after=$(ls -l --time-style=+%s.%N "$index" | md5sum)
[[ "$before" == "$after" ]] || fail "compiling again wrote shards again"
echo "ok   a shard per file read, written once ($(ls "$index" | wc -l) shards)"
cd "$repo_root"

# The answers.
ask() {
  local name=$1
  shift
  "$codemap" -C "$tree" "$@" > "$work/$name.txt" 2>&1 || fail "mlx-codemap $* failed" "$work/$name.txt"
  if [[ -n "${CHECK_UPDATE:-}" ]]; then
    cp "$work/$name.txt" "$project/expected/$name.txt"
  elif ! diff -u "$project/expected/$name.txt" "$work/$name.txt" > "$work/$name.diff"; then
    fail "mlx-codemap $* answered otherwise" "$work/$name.diff"
  fi
  echo "ok   mlx-codemap $*"
}
ask find-count find count
ask find-qualified find geo::Circle
ask refs-shape refs geo::Shape
ask refs-macro refs SQUARE
ask refs-member refs Circle.radius
ask callers-count callers geo::count
ask callers-area callers geo::Shape::area
ask callers-mangled callers _ZN3geo5countEi
ask callees-total callees shapes.cpp:total
ask outline-cpp outline shapes.cpp
ask outline-h outline shapes.h
ask outline-c outline legacy.c
ask def-call def shapes.cpp:21:30
ask def-template def shapes.cpp:20:9
ask show-area show geo::Circle::area
ask members-box members geo::Box
ask importers-h importers shapes.h
ask imports-cpp imports shapes.cpp
ask callers-mlx callers scale.mlx:shapes_scale
ask refs-c refs counted
ask callers-c callers bump
