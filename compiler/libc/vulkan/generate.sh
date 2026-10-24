#!/usr/bin/env bash
# Makes mlx-vulkan's API module (api/vulkan_core.mlx, the declarations
# vulkan/vulkan_core.h is made of) and its dispatch (commands.mlx) from
# the Vulkan API registry std.vulkan is made of.
#
#   compiler/libc/vulkan/generate.sh [--check] [compiler]
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
"$compiler" --quiet compiler/libc/vulkan/generate.mlx -o "$tool/generate"
"$tool/generate" $mode
