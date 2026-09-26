#!/usr/bin/env bash
# Materializes std.spirv.core (std/src/spirv/core.mlx) from the SPIR-V grammar
# in this directory.
#
#   std/registry/spirv/materialize.sh [--check] [compiler]
#
# The compiler defaults to mlx-out/bin/compiler/mlx4, then zig-out/bin/mlx1.
# The grammar is verified against SOURCES before it is parsed, so the generated
# module always comes from the unmodified upstream grammar.
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
    actual=$(sha256sum "std/registry/spirv/$file" | cut -d' ' -f1)
    if [[ "$actual" != "$hash" ]]; then
        echo "materialize.sh: $file does not match its recorded upstream hash" >&2
        exit 1
    fi
done < <(awk '!/^#/ && NF { print $1, $2, $3, $4, $NF }' std/registry/spirv/SOURCES)

tool=$(mktemp -d)
trap 'rm -rf -- "$tool"' EXIT
"$compiler" --quiet std/registry/spirv/materialize.mlx -o "$tool/materialize"
"$tool/materialize" $mode
