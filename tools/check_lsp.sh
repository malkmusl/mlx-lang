#!/usr/bin/env bash
# The Mlx language server (tools/mlx-lsp) end to end: builds it and drives
# a scripted editor session (tools/check_lsp.py) on a small tree: go to
# definition, references, hover, outline, workspace symbols, highlights,
# completion, signature help, rename, semantic tokens, and the compiler's
# diagnostics when a file is saved broken and fixed.
#
#   tools/check_lsp.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
compiler=$(cd "$(dirname "$compiler")" && pwd)/$(basename "$compiler")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet tools/mlx-lsp/main.mlx -o "$work/mlx-lsp"
python3 tools/check_lsp.py "$work/mlx-lsp" "$compiler"
