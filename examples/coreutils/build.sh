#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
bin_dir=${MLX_COREUTILS_BIN_DIR:-"$repo_root/mlx-out/bin/coreutils"}
compiler=${MLX_COMPILER:-${1:-"$repo_root/mlx-out/bin/compiler/mlx4"}}
utilities=(true false echo cat wc pwd mkdir rmdir basename dirname head tail tee yes sleep uname printenv env nproc link unlink touch truncate mkfifo)

if [[ ! -x "$compiler" ]]; then
    printf 'compiler is not executable: %s\n' "$compiler" >&2
    exit 1
fi

mkdir -p "$bin_dir"
for utility in "${utilities[@]}"; do
    printf 'building mlx-%s\n' "$utility"
    "$compiler" \
        "$repo_root/examples/coreutils/$utility/main.mlx" \
        -o "$bin_dir/$utility"
    if [[ ! -x "$bin_dir/$utility" ]]; then
        printf 'compiler did not produce $utility\n' "$utility" >&2
        exit 1
    fi
done
