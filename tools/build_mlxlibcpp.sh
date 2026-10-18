#!/usr/bin/env bash
# Builds mlxlibcpp (compiler/libcpp): the C++ standard library for mlx-cc,
# as a static library mlx-cc links into every C++ program by itself.
#
#   tools/build_mlxlibcpp.sh [options]
#     --out DIR        where to put it (default: mlx-out/libcpp)
#     --compiler PATH  the Mlx compiler (default: tools/ensure_compiler.sh)
#
# Builds, into DIR:
#   libmlxcpp.a    the library (new and delete, std::string, iostream),
#                  found by mlx-cc through MLX_CC_LIB=DIR, next to the
#                  compiler as lib/, or in /usr/local/lib/mlx-cc/lib
#   mlx-cc, mlx-ar the tools that built it
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

out=mlx-out/libcpp
compiler=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --out) out=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_mlxlibcpp.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)
mkdir -p "$out/obj"

"$compiler" --quiet compiler/cc/main.mlx -o "$out/mlx-cc"
"$compiler" --quiet compiler/cc/ar.mlx -o "$out/mlx-ar"
export MLX_CC_INCLUDE="$repo_root/compiler/cc/include"
objects=()
for source in compiler/libcpp/*.cpp; do
    name=$(basename "$source" .cpp)
    "$out/mlx-cc" -c -o "$out/obj/$name.o" "$source"
    objects+=("$out/obj/$name.o")
done
rm -f "$out/libmlxcpp.a"
"$out/mlx-ar" rcs "$out/libmlxcpp.a" "${objects[@]}"
