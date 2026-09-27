#!/usr/bin/env bash
# The dock and the launcher (examples/mlx-dock, examples/mlx-launcher) in
# the compositor, end to end: wlr-layer-shell, the compositor's windows
# for the dock (wlr-foreign-toplevel-management), the blur behind both
# (ext-background-effect-v1), Super, and starting apps.
#
# The nested compositor runs under tools/wayland-test-host with the dock
# and two Mlx terminals, on its own set of apps (desktop entries whose
# command appends a word to a file). The script:
#   - waits for the dock (a layer surface in the top layer, along the
#     bottom) with its apps button, the pinned terminal and the pinned
#     test app;
#   - taps Super: the launcher opens centred in the overlay layer and takes
#     the keyboard; typing narrows it to the test app, Enter starts it and
#     the launcher closes;
#   - clicks the dock's terminal (the next terminal window comes forward)
#     and the dock's test app (it starts);
#   - opens the launcher with the dock's apps button and closes it with
#     Escape.
# The window behind the launcher must show blurred through it, and the
# screenshots must be the same with the CPU and the Vulkan renderer.
#
# The Vulkan run uses lavapipe (the apps' data directories are the test's,
# so the loader is pointed at its manifest).
#
#   tools/check_desktop_clients.sh [compiler] [lavapipe manifest]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
manifest=${2:-/usr/share/vulkan/icd.d/lvp_icd.json}
[[ -f "$manifest" ]] || { echo "check_desktop_clients.sh: no Vulkan driver manifest at $manifest" >&2; exit 2; }
command -v xkbcli > /dev/null || { echo "check_desktop_clients.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet examples/wayland-compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet examples/wayland-terminal/main.mlx -o "$work/mlx-terminal"
"$compiler" --quiet examples/mlx-dock/main.mlx -o "$work/mlx-dock"
"$compiler" --quiet examples/mlx-launcher/main.mlx -o "$work/mlx-launcher"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

# The apps: only these (no system directories), one with an icon.
mkdir -p "$work/data/applications" "$work/data/icons/hicolor/64x64/apps" "$work/config/mlx" "$work/home"
cat > "$work/mark.sh" <<SCRIPT
#!/bin/sh
echo "\$1" >> "$work/marks"
SCRIPT
chmod +x "$work/mark.sh"
cat > "$work/data/applications/org.mlx.Marker.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=Marker Writer
Exec=$work/mark.sh started %U
Icon=org.mlx.Marker
ENTRY
cat > "$work/data/applications/org.mlx.Other.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=Other App
Exec=$work/mark.sh other
ENTRY
cp tests/support/png/rgba8.png "$work/data/icons/hicolor/64x64/apps/org.mlx.Marker.png"
printf 'terminal\norg.mlx.Marker\n' > "$work/config/mlx/dock"

# The dock at 1024x768: the apps button, the terminal, the test app, with
# resting centres at x 454, 512 and 570.
cat > "$work/desktop.script" <<SCRIPT
wait 3000
shot SHOTS/rest.ppm
down 125
up 125
wait 2000
shot SHOTS/launcher.ppm
type marker
wait 500
enter
wait 1500
pointer 512 730
wait 300
press 272
release 272
wait 500
pointer 570 730
wait 300
press 272
release 272
wait 1000
pointer 454 730
wait 300
press 272
release 272
wait 2000
down 1
up 1
wait 800
leave
wait 300
close
SCRIPT

fail() { echo "FAIL $1" >&2; [[ -f "$2" ]] && cat "$2" >&2; exit 1; }

# Runs the scenario with renderer $1; the screenshots go to $work/$1.
run() {
    local renderer=$1
    local runtime
    runtime=$(mktemp -d)
    mkdir -p "$work/$renderer"
    rm -f "$work/marks"
    sed "s|SHOTS|$work/$renderer|" "$work/desktop.script" > "$work/$renderer.script"
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-desktop "$work/us.xkb" "$work/$renderer.script" > "$work/$renderer-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-desktop" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$work/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-desktop \
        XDG_DATA_HOME="$work/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/config" VK_DRIVER_FILES="$manifest" \
        timeout 60 "$work/mlx-compositor" --verbose --no-fps --renderer "$renderer" --socket nested-desktop \
        --terminal "$work/mlx-terminal" --launcher "$work/mlx-launcher" --dock "$work/mlx-dock" \
        --run "$work/mlx-terminal" --run "$work/mlx-terminal" > "$work/$renderer.log" 2>&1 || status=$?
    wait "$host_pid" || fail "$renderer: the test host failed" "$work/$renderer-host.log"
    rm -rf -- "$runtime"
    [[ $status -eq 0 ]] || fail "$renderer: the compositor exited with status $status" "$work/$renderer.log"
    local log="$work/$renderer.log"

    grep -q "^layer: mlx-dock 300x150 at 362,618 in layer 2" "$log" || fail "$renderer: the dock is not along the bottom in the top layer" "$log"
    grep -q "^layer: mlx-launcher 720x540 at 152,114 in layer 3" "$log" || fail "$renderer: Super did not open the launcher centred in the overlay layer" "$log"
    grep -q "^focus: mlx-launcher" "$log" || fail "$renderer: the launcher did not get the keyboard" "$log"
    grep -q "^key 50 -> mlx-launcher" "$log" || fail "$renderer: typing did not reach the launcher" "$log"
    [[ "$(cat "$work/marks" 2> /dev/null)" == "$(printf 'started\nstarted')" ]] || fail "$renderer: the launcher and the dock did not start the app (marks: $(cat "$work/marks" 2> /dev/null | tr '\n' ' '))" "$log"
    [[ $(grep -c "^layer: mlx-launcher" "$log") -eq 2 ]] || fail "$renderer: the dock's apps button did not open the launcher" "$log"
    grep -q "^key 1 -> mlx-launcher" "$log" || fail "$renderer: Escape did not reach the launcher" "$log"
    # Focus: the two terminals, the launcher, a terminal again after it
    # closed, then the other one from the dock.
    [[ $(grep -c "^focus: Mlx Terminal" "$log") -ge 4 ]] || fail "$renderer: the dock did not bring the next terminal forward" "$log"
    echo "ok   $renderer: dock along the bottom, Super opens the launcher, apps start from both, the dock switches windows"

    python3 - "$work/$renderer/launcher.ppm" <<'PY' || fail "$renderer: the window behind the launcher is not blurred" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
# The second terminal's right edge (x 716) seen through the launcher: a
# blur spreads it over many pixels, a sharp edge changes at once.
y = 300
row = [tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3]) for x in range(696, 738)]
steps = sum(1 for a, b in zip(row, row[1:]) if a != b)
assert steps >= 8, row
PY
}

run cpu
echo "ok   cpu: the window behind the launcher shows blurred"
run vulkan
echo "ok   vulkan: the window behind the launcher shows blurred"
cmp -s "$work/cpu/rest.ppm" "$work/vulkan/rest.ppm" || fail "the dock differs between the CPU and the Vulkan renderer"
cmp -s "$work/cpu/launcher.ppm" "$work/vulkan/launcher.ppm" || fail "the launcher and its blur differ between the CPU and the Vulkan renderer"
echo "ok   the CPU and the Vulkan renderer draw the dock, the launcher and the blur alike"
