#!/usr/bin/env bash
# Builds the compositor's shell and renderer as shared objects into the
# directory the running compositor watches, so it loads them without a
# restart (projects/desktop/compositor/modules.mlx).
#
#   tools/build_compositor_modules.sh [options] [shell|render]...
#     --dir DIR        where to put them (default: $MLX_COMPOSITOR_MODULES,
#                      else ~/.local/lib/mlx-compositor, which the session
#                      watches)
#     --compiler PATH  compile with this Mlx compiler (default: ./mlx build,
#                      which builds a module only when it is not current)
#     --watch          build again whenever a source file changes
#
# Without shell or render, both are built. Each file is written next to its
# place and renamed over it, so the compositor never sees half a file. The
# compositor refuses a module built for other records (a changed state.mlx,
# or a record the renderer keeps): that needs a restart.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
source tools/lib/mlx_build.sh

directory=${MLX_COMPOSITOR_MODULES:-$HOME/.local/lib/mlx-compositor}
compiler=""
watch=0
kinds=()
while [[ $# -gt 0 ]]; do
    case $1 in
    --dir) directory=$2; shift ;;
    --compiler) compiler=$2; shift ;;
    --watch) watch=1 ;;
    shell|render) kinds+=("$1") ;;
    -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "build_compositor_modules.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ ${#kinds[@]} -eq 0 ]] && kinds=(shell render)
mkdir -p "$directory"

# Puts a built module in place, renamed over the old one; a module that
# did not change stays (the compositor would load it again for nothing).
place() {
    local built=$1 target=$2
    if [[ -f "$target" ]] && cmp -s "$built" "$target"; then
        echo "current $target"
        return 0
    fi
    cp "$built" "$target.next" && mv "$target.next" "$target" && echo "built $target"
}

build() {
    local kind status=0 steps=()
    if [[ -z "$compiler" ]]; then
        # build.mlx's steps libmlx-shell.so and libmlx-render.so.
        for kind in "${kinds[@]}"; do steps+=("libmlx-$kind.so"); done
        if ! mlx_build "${steps[@]}"; then
            echo "build_compositor_modules.sh: a module did not build; the compositor keeps what it runs" >&2
            return 1
        fi
        for kind in "${kinds[@]}"; do
            place "mlx-out/lib/libmlx-$kind.so" "$directory/libmlx-$kind.so" || status=1
        done
        return $status
    fi
    for kind in "${kinds[@]}"; do
        local target="$directory/libmlx-$kind.so"
        if "$compiler" --quiet --shared "projects/desktop/compositor/${kind}_module.mlx" -o "$target.next"; then
            mv "$target.next" "$target"
            echo "built $target"
        else
            rm -f "$target.next"
            echo "build_compositor_modules.sh: the $kind did not build; the compositor keeps what it runs" >&2
            status=1
        fi
    done
    return $status
}

if [[ $watch -eq 0 ]]; then
    build
    exit
fi

# --watch: the sources the modules are built from.
sources() { find projects/desktop/compositor projects/desktop/shared std/src -name '*.mlx' -printf '%T@ %p\n' | sort | md5sum; }
build || true
last=$(sources)
echo "watching the sources (Ctrl+C stops)"
while sleep 1; do
    now=$(sources)
    if [[ "$now" != "$last" ]]; then
        last=$now
        build || true
    fi
done
