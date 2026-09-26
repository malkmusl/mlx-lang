#!/usr/bin/env bash
# Materializes std.vulkan (std/src/vulkan.mlx) from the Vulkan API registry
# in this directory.
#
#   std/registry/vulkan/materialize.sh [--check] [compiler]
#
# The compiler defaults to mlx-out/bin/compiler/mlx4, then zig-out/bin/mlx1.
# vk.xml is verified against SOURCES before it is parsed, so the generated
# module always comes from the unmodified upstream registry.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$repo_root"

mode=""
if [[ "${1:-}" == "--check" ]]; then
    mode="--check"
    shift
fi
compiler=${1:-${MLX_COMPILER:-}}
if [[ -z "$compiler" ]]; then
    for candidate in mlx-out/bin/compiler/mlx4 zig-out/bin/mlx1; do
        if [[ -x "$candidate" ]]; then compiler=$candidate; break; fi
    done
fi
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
    echo "materialize.sh: no Mlx compiler found (build mlx1 with 'zig build mlx1')" >&2
    exit 1
fi

while read -r file _ _ _ hash; do
    [[ -z "$file" || "$file" == \#* ]] && continue
    actual=$(sha256sum "std/registry/vulkan/$file" | cut -d' ' -f1)
    if [[ "$actual" != "$hash" ]]; then
        echo "materialize.sh: $file does not match its recorded upstream hash" >&2
        exit 1
    fi
done < <(awk '!/^#/ && NF { print $1, $2, $3, $4, $NF }' std/registry/vulkan/SOURCES)

tool=$(mktemp -d)
trap 'rm -rf -- "$tool"' EXIT
"$compiler" --quiet std/registry/vulkan/materialize.mlx -o "$tool/materialize"
"$tool/materialize" $mode
