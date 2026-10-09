#!/usr/bin/env bash
# Builds mlxlibc (projects/libc): the C library written in Mlx, on system
# calls alone, for the programs and shared objects that want a libc with
# neither glibc nor musl under it.
#
#   tools/build_mlxlibc.sh [options]
#     --out DIR        where to put it (default: mlx-out/libc)
#     --compiler PATH  the Mlx compiler (default: tools/ensure_compiler.sh)
#
# Builds, into DIR:
#   libmlxc.so.1   the library: soname libmlxc.so.1, needing no other
#   libmlxc.so     a link to it for the linker (gcc ... -L DIR -lmlxc)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"

out=mlx-out/libc
compiler=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --out) out=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_mlxlibc.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ -n "$compiler" ]] || compiler=$(tools/ensure_compiler.sh)
mkdir -p "$out"

# A plugin on a stack arena with no C library under it: each export takes
# its aggregates from its own stack and the image names no DT_NEEDED.
"$compiler" --quiet --plugin --stack-arena --no-libc --soname=libmlxc.so.1 --entry=__mlx_start --init=__mlx_init projects/libc/libc.mlx -o "$out/libmlxc.so.1"
ln -sfn libmlxc.so.1 "$out/libmlxc.so"
echo "built $out/libmlxc.so.1"
