#!/usr/bin/env bash
# The toolchain's version module, mlx-out/version.mlx, which the compilers'
# sources import (compiler/selfhost/main.mlx, compiler/cc/main.mlx,
# compiler/ld/main.mlx), for what builds them outside ./mlx build
# (tools/ensure_compiler.sh, tools/check_bootstrap.sh; ./mlx build writes
# it itself, build.mlx's toolchainVersion):
#
#   - with a compiler: mlx-build-number (tools/build-number, std.git) built
#     by it writes the module from VERSION and the git history, when it
#     changed;
#   - without one, or when that fails, and there is no module yet: a dev
#     module (VERSION's MAJOR.MINOR.dev, no commit), what mlx0 and the
#     bootstrap build with.
#
#   tools/version_module.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
module=mlx-out/version.mlx
compiler=${1:-}
mkdir -p mlx-out
if [[ -n "$compiler" && -x "$compiler" ]]; then
    # (In mlx-out: a /tmp mounted noexec would not run it.)
    work=$(mktemp -d mlx-out/version.XXXXXX)
    trap 'rm -rf -- "$work"' EXIT
    if "$compiler" --quiet tools/build-number/main.mlx -o "$work/mlx-build-number" > "$work/build.log" 2>&1; then
        "$work/mlx-build-number" --module="$module" > /dev/null 2>&1 || true
    fi
fi
if [[ ! -f $module ]]; then
    version=$(head -n 1 VERSION 2> /dev/null | tr -d ' \t\r' || true)
    printf '// The toolchain'"'"'s version: a dev one, made by tools/version_module.sh\n// without mlx-build-number; ./mlx build writes the numbers.\npub const VERSION: []const u8 = "%s.dev"\npub const COMMIT: []const u8 = ""\n' "${version:-0.0}" > "$module"
fi
