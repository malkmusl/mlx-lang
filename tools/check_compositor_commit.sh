#!/usr/bin/env bash
# Checks what the compositor (CPU renderer) takes of a commit that names
# its damage (projects/desktop/compositor/buffers.mlx takePixels,
# windows.mlx applyCommit): only that part of the buffer is copied and
# only that part of the scene composed again, and the screen must show
# pixel for pixel what whole commits give.
#
# tools/compositor-test/commit-damage.mlx (std.ui's panel, which passes a
# draw's damage on as wl_surface.damage) lays rectangles over a picture, a
# frame each. Run with `part` it draws only the new rectangle of a frame,
# names it as the damage and fills the rest of its buffer with other
# pixels; run with `whole` it draws every frame whole and names no damage.
# Each runs in the compositor nested in tools/wayland-test-host, as a
# window, as a layer surface across the top (a panel) and as a window
# larger than the output, and the host's screenshots of the two must be
# the same file: pixels read from outside the damage, or a scene damaged
# too little, would show. On the small output some of the damage is off
# it: the client must get its frame callbacks all the same (it draws its
# next frame after each, and says when the last was taken).
#
# Needs an xkb keymap for the host: xkbcli's, or brix-keymap's from the
# host's XKB data. Usage: tools/check_compositor_commit.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
export XDG_STATE_HOME="$work/state"
export MLX_ARENA_POISON=${MLX_ARENA_POISON:-1}
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx"
# (A window is where it rests from its first frame on.)
printf 'animations = off\n' > "$XDG_CONFIG_HOME/mlx/compositor.conf"

if command -v xkbcli > /dev/null; then
    xkbcli compile-keymap --layout us > "$work/us.xkb"
elif [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
else
    echo "skip: no xkb keymap for the test host (neither xkbcli nor the XKB data)"
    exit 0
fi
"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet tools/compositor-test/commit-damage.mlx -o "$work/commit-damage"

# One run `name`-`mode`: the client with `mode` (part, whole) as `kind`
# (window, layer), the compositor with the arguments that follow; the
# host's screenshot once its frames are in.
run() {
    local name=$1-$3 kind=$2 mode=$3
    shift 3
    printf '#!/bin/sh\nexec "%s" %s %s > "%s" 2>&1\n' "$work/commit-damage" "$mode" "$kind" "$work/$name-client.log" > "$work/$name-client"
    chmod +x "$work/$name-client"
    printf 'wait 5000\nshot %s\nwait 200\nclose\n' "$work/$name.ppm" > "$work/$name.script"
    "$work/test-host" "host-$name" "$work/us.xkb" "$work/$name.script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 60 "$work/kepler" --verbose --no-fps --renderer cpu --socket "nested-$name" --run "$work/$name-client" "$@" > "$work/$name-compositor.log" 2>&1 || true
    wait "$host_pid" || { echo "check_compositor_commit.sh: FAILED: the test host ($name):" >&2; cat "$work/$name-host.log" >&2; exit 1; }
    grep -q '^commit-damage: done$' "$work/$name-client.log" || { echo "check_compositor_commit.sh: FAILED: the client's frames were not all taken before the screenshot ($name)" >&2; cat "$work/$name-client.log" "$work/$name-compositor.log" >&2; exit 1; }
    [[ -s "$work/$name.ppm" ]] || { echo "check_compositor_commit.sh: FAILED: no screenshot ($name)" >&2; exit 1; }
}

# The same picture from whole commits and from ones that name their damage.
compare() {
    local name=$1 kind=$2 what=$3
    shift 3
    run "$name" "$kind" whole "$@"
    run "$name" "$kind" part "$@"
    if ! cmp -s "$work/$name-whole.ppm" "$work/$name-part.ppm"; then
        echo "check_compositor_commit.sh: FAILED: commits that name their damage show other pixels than whole ones ($what): $(cmp "$work/$name-whole.ppm" "$work/$name-part.ppm" 2>&1 || true)" >&2
        exit 1
    fi
    echo "ok   $what: commits that name their damage show pixel for pixel what whole commits show"
}
compare window window "a window"
compare layer layer "a layer surface"
compare small window "a window larger than the output (damage off it: its frame callbacks still come)" --size 320x200
# The two pictures are not one: the layer's run shows the layer.
if cmp -s "$work/window-whole.ppm" "$work/layer-whole.ppm"; then
    echo "check_compositor_commit.sh: FAILED: the window's and the layer's screenshots are the same" >&2
    exit 1
fi
echo "check_compositor_commit.sh: commits are taken where the client says they changed"
