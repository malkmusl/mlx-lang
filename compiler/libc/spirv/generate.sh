#!/usr/bin/env bash
# Makes mlx-spirv's API modules (api/spirv.mlx and api/glsl_std_450.mlx,
# of which mlx-cheaders makes spirv/unified1/spirv.hpp and GLSL.std.450.h)
# from the SPIR-V grammars std.spirv is made of.
#
#   compiler/libc/spirv/generate.sh [--check] [compiler]
#
# The compiler defaults to mlx-out/bin/compiler/mlx4.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$repo_root"
mode=""
if [[ "${1:-}" == "--check" ]]; then
    mode="--check"
    shift
fi
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
[[ -x "$compiler" ]] || { echo "generate.sh: no Mlx compiler at $compiler (./mlx makes one)" >&2; exit 1; }
tool=$(mktemp -d)
trap 'rm -rf -- "$tool"' EXIT
"$compiler" --quiet compiler/libc/spirv/generate.mlx -o "$tool/generate"
"$tool/generate" $mode
