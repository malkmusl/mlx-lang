#!/usr/bin/env bash
# Materializes std.wayland from the canonical protocol XML in this directory.
#
#   std/protocols/wayland/materialize.sh [--check] [compiler]
#
# The compiler defaults to mlx-out/bin/compiler/mlx4, then zig-out/bin/mlx1.
# The XML files are verified against SOURCES before they are parsed, so the
# generated modules always come from unmodified upstream protocol text.
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

# Verify the vendored XML against the recorded upstream hashes.
while read -r file _ _ _ hash; do
    [[ -z "$file" || "$file" == \#* ]] && continue
    actual=$(sha256sum "std/protocols/wayland/$file" | cut -d' ' -f1)
    if [[ "$actual" != "$hash" ]]; then
        echo "materialize.sh: $file does not match its recorded upstream hash" >&2
        exit 1
    fi
done < <(awk '!/^#/ && NF { print $1, $2, $3, $4, $NF }' std/protocols/wayland/SOURCES)

tool=$(mktemp -d)
trap 'rm -rf -- "$tool"' EXIT
"$compiler" --quiet std/protocols/wayland/materialize.mlx -o "$tool/materialize"
"$tool/materialize" $mode
