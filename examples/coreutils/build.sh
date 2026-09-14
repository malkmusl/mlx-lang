#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
bin_dir=${MLX_COREUTILS_BIN_DIR:-"$repo_root/zig-out/bin"}
compiler=${MLX_COMPILER:-${1:-"$repo_root/zig-out/bin/mlx2"}}
utilities=(true false echo cat wc pwd mkdir rmdir)

if [[ ! -x "$compiler" ]]; then
    printf 'compiler is not executable: %s\n' "$compiler" >&2
    exit 1
fi

mkdir -p "$bin_dir"
for utility in "${utilities[@]}"; do
    printf 'building mlx-%s\n' "$utility"
    "$compiler" \
        "$repo_root/examples/coreutils/$utility/main.mlx" \
        -o "$bin_dir/mlx-$utility"
    if [[ ! -x "$bin_dir/mlx-$utility" ]]; then
        printf 'compiler did not produce mlx-%s\n' "$utility" >&2
        exit 1
    fi
done
