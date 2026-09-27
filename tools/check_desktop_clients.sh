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
#   - clicks the dock's terminal: with two windows it shows their
#     previews above the dock; a click on one brings it forward; then
#     clicks the dock's test app (it starts);
#   - opens the launcher with the dock's apps button and closes it with
#     Escape;
#   - right-clicks the test app in the dock and runs its desktop action
#     from the menu;
#   - opens the launcher, drags the other app onto the dock (it is
#     pinned: drag and drop through the compositor) and unpins it from the
#     launcher's right-click menu;
#   - clicks the Downloads folder (its stack shows) and empties the trash
#     from its menu;
#   - then, with tools/wayland-drag-source as a file manager, drops a
#     folder on the dock (it is pinned) and a file on the trash (it goes
#     into it).
# The window behind the launcher must show blurred through it, and the
# screenshots must be the same with the CPU and the Vulkan renderer, and
# with the apps drawing on the GPU (their standard) and on the CPU
# (MLX_CANVAS=cpu).
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
"$compiler" --quiet tools/wayland-drag-source/main.mlx -o "$work/drag-source"
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
Actions=new-window;

[Desktop Action new-window]
Name=New Marker Window
Exec=$work/mark.sh action
ENTRY
cat > "$work/data/applications/org.mlx.Other.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=Other App
Exec=$work/mark.sh other
ENTRY
cp tests/support/png/rgba8.png "$work/data/icons/hicolor/64x64/apps/org.mlx.Marker.png"
mkdir -p "$work/home/Downloads"
echo old > "$work/home/Downloads/report.pdf"
touch -d '2020-01-01' "$work/home/Downloads/report.pdf"
echo new > "$work/home/Downloads/photo.png"
printf 'terminal\norg.mlx.Marker\n' > "$work/config/mlx/dock"

# The dock at 1024x768: the apps button, the terminal, the test app, then
# the Downloads folder and the trash, with resting centres at x 387, 445,
# 503, 577 and 635.
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
pointer 445 730
wait 300
press 272
release 272
wait 800
shot SHOTS/previews.ppm
pointer 498 500
wait 300
press 272
release 272
wait 500
pointer 503 730
wait 300
press 272
release 272
wait 1000
pointer 387 730
wait 300
press 272
release 272
wait 2000
down 1
up 1
wait 800
pointer 503 730
wait 300
press 273
release 273
wait 1500
shot SHOTS/menu.ppm
pointer 493 630
wait 300
press 272
release 272
wait 1000
down 125
up 125
wait 2000
pointer 344 260
wait 300
press 272
wait 100
pointer 350 270
wait 100
pointer 400 400
wait 100
pointer 500 600
wait 100
pointer 530 725
wait 500
pointer 534 728
wait 1500
release 272
wait 2500
pointer 344 260
wait 300
press 273
release 273
wait 2000
pointer 380 281
wait 300
press 272
release 272
wait 2500
down 1
up 1
wait 800
pointer 577 730
wait 300
press 272
release 272
wait 1500
shot SHOTS/stack.ppm
pointer 800 300
wait 300
press 272
release 272
wait 800
pointer 635 730
wait 300
press 273
release 273
wait 1500
pointer 635 671
wait 300
press 272
release 272
wait 1500
leave
wait 300
close
SCRIPT

fail() { echo "FAIL $1" >&2; [[ -f "$2" ]] && cat "$2" >&2; exit 1; }

# Runs the scenario with renderer $1; the screenshots go to $work/$1.
run() {
    local renderer=$1
    # $2: cpu draws the apps on the CPU (MLX_CANVAS), else on the GPU.
    local name=$renderer${2:+-canvas-$2}
    local runtime
    runtime=$(mktemp -d)
    mkdir -p "$work/$name"
    rm -f "$work/marks"
    mkdir -p "$work/data/Trash/files" "$work/data/Trash/info"
    echo thrown > "$work/data/Trash/files/old.txt"
    sed "s|SHOTS|$work/$name|" "$work/desktop.script" > "$work/$name.script"
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-desktop "$work/us.xkb" "$work/$name.script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-desktop" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$work/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-desktop \
        XDG_DATA_HOME="$work/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/config" VK_DRIVER_FILES="$manifest" MLX_CANVAS="${2:-}" \
        timeout 60 "$work/mlx-compositor" --verbose --no-fps --renderer "$renderer" --socket nested-desktop \
        --terminal "$work/mlx-terminal" --launcher "$work/mlx-launcher" --dock "$work/mlx-dock" \
        --run "$work/mlx-terminal" --run "$work/mlx-terminal" > "$work/$name.log" 2>&1 || status=$?
    wait "$host_pid" || fail "$renderer: the test host failed" "$work/$name-host.log"
    rm -rf -- "$runtime"
    [[ $status -eq 0 ]] || fail "$renderer: the compositor exited with status $status" "$work/$name.log"
    local log="$work/$name.log"

    grep -q "^layer: mlx-dock 433x150 at 295,618 in layer 2" "$log" || fail "$renderer: the dock is not along the bottom in the top layer" "$log"
    grep -q "^layer: mlx-launcher 720x540 at 152,114 in layer 3" "$log" || fail "$renderer: Super did not open the launcher centred in the overlay layer" "$log"
    grep -q "^focus: mlx-launcher" "$log" || fail "$renderer: the launcher did not get the keyboard" "$log"
    grep -q "^key 50 -> mlx-launcher" "$log" || fail "$renderer: typing did not reach the launcher" "$log"
    [[ "$(cat "$work/marks" 2> /dev/null)" == "$(printf 'started\nstarted\naction')" ]] || fail "$renderer: the launcher and the dock did not start the app, or the dock's menu its action (marks: $(cat "$work/marks" 2> /dev/null | tr '\n' ' '))" "$log"
    [[ $(grep -c "^layer: mlx-launcher" "$log") -eq 3 ]] || fail "$renderer: the dock's apps button did not open the launcher" "$log"
    grep -q "^key 1 -> mlx-launcher" "$log" || fail "$renderer: Escape did not reach the launcher" "$log"
    # Focus: the two terminals, the launcher, a terminal again after it
    # closed, then the other one from its preview.
    grep -q "^previews: 2 windows, panel 516x206 at 254,478" "$log" || fail "$renderer: the dock's terminal (two windows) did not show their previews above the dock" "$log"
    grep -q "^previews closed" "$log" || fail "$renderer: a click on a preview did not close them" "$log"
    [[ $(grep -c "^focus: Mlx Terminal" "$log") -ge 4 ]] || fail "$renderer: the preview did not bring the other terminal forward" "$log"
    echo "ok   $renderer: dock along the bottom, Super opens the launcher, apps start from both, the dock's previews switch windows"
    grep -q "^drag: dropped on mlx-dock" "$log" && grep -q "^mlx-dock: pinned org.mlx.Other" "$log" || fail "$renderer: an app dragged from the launcher was not pinned to the dock" "$log"
    grep -q "^mlx-dock: the pinned apps changed" "$log" || fail "$renderer: the dock did not take in the launcher's change to its pins" "$log"
    [[ "$(cat "$work/config/mlx/dock")" == "$(printf 'terminal\norg.mlx.Marker')" ]] || fail "$renderer: the launcher's menu did not unpin the app (pins: $(tr '\n' ' ' < "$work/config/mlx/dock"))" "$log"
    printf 'terminal\norg.mlx.Marker\n' > "$work/config/mlx/dock"
    echo "ok   $renderer: the dock's menu runs an app's action; apps pin by dragging from the launcher and unpin from its menu"
    [[ "$(cat "$work/config/mlx/dock-folders")" == "~/Downloads" ]] || fail "$renderer: the dock did not pin ~/Downloads by default" "$log"
    [[ -d "$work/data/Trash/files" && -z "$(ls -A "$work/data/Trash/files")" ]] || fail "$renderer: Empty Trash in the trash's menu left files" "$log"
    python3 - "$work/$name/stack.ppm" <<'PY' || fail "$renderer: a click on the Downloads folder did not show its newest files" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
# The stack above the folder: a grid (two files: the first cell has an
# icon, the third is empty) under a header with the Open button.
assert at(577, 640) == (30, 33, 42), at(577, 640)
assert at(393, 622) != (30, 33, 42), at(393, 622)
assert at(742, 566) == (58, 62, 74), at(742, 566)
assert at(577, 500) != (30, 33, 42), at(577, 500)
PY
    echo "ok   $renderer: the dock shows the Downloads folder as a stack and empties the trash"

    python3 - "$work/$name/launcher.ppm" <<'PY' || fail "$renderer: the window behind the launcher is not blurred" "$log"
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

    python3 - "$work/$name/previews.ppm" <<'PY' || fail "$renderer: the previews are not drawn" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
# The panel's colour between the thumbnails and below them; each
# thumbnail shows its terminal's dark background.
assert at(512, 560) == (0x20, 0x24, 0x30), at(512, 560)
assert at(262, 670) == (0x20, 0x24, 0x30), at(262, 670)
assert at(380, 600) != (0x20, 0x24, 0x30) and max(at(380, 600)) < 64, at(380, 600)
assert at(630, 600) != (0x20, 0x24, 0x30) and max(at(630, 600)) < 64, at(630, 600)
PY
}

run cpu
echo "ok   cpu: the window behind the launcher shows blurred"
run vulkan
echo "ok   vulkan: the window behind the launcher shows blurred"
cmp -s "$work/cpu/rest.ppm" "$work/vulkan/rest.ppm" || fail "the dock differs between the CPU and the Vulkan renderer"
cmp -s "$work/cpu/launcher.ppm" "$work/vulkan/launcher.ppm" || fail "the launcher and its blur differ between the CPU and the Vulkan renderer"
echo "ok   the CPU and the Vulkan renderer draw the dock, the launcher and the blur alike"
run cpu cpu
cmp -s "$work/cpu/rest.ppm" "$work/cpu-canvas-cpu/rest.ppm" || fail "the dock drawn on the GPU differs from the dock drawn on the CPU"
cmp -s "$work/cpu/launcher.ppm" "$work/cpu-canvas-cpu/launcher.ppm" || fail "the launcher drawn on the GPU differs from the launcher drawn on the CPU"
echo "ok   the apps draw the same pixels on the GPU (paint shader) as on the CPU"

# Files dragged in (tools/wayland-drag-source, as from a file manager): a
# folder dropped on the dock is pinned among the folders; a file dropped
# on the trash goes into it.
drops() {
    local runtime
    runtime=$(mktemp -d)
    mkdir -p "$work/home/Documents"
    echo junk > "$work/home/junk.txt"
    cat > "$work/drag.sh" <<SCRIPT
#!/bin/sh
exec "$work/drag-source" "file://$work/home/Documents" "file://$work/home/junk.txt"
SCRIPT
    chmod +x "$work/drag.sh"
    # The drag source's window at (24, 24); in the dock it runs as an app
    # of its own, so the trash is at x 731 once the folder joined.
    cat > "$work/drops.script" <<SCRIPT
wait 3000
pointer 100 100
wait 300
press 272
wait 100
pointer 110 110
wait 100
pointer 300 400
wait 100
pointer 540 720
wait 300
pointer 541 728
wait 800
release 272
wait 2000
pointer 100 100
wait 300
press 272
wait 100
pointer 110 110
wait 100
pointer 400 500
wait 100
pointer 725 725
wait 300
pointer 731 730
wait 800
release 272
wait 2500
close
SCRIPT
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-drops "$work/us.xkb" "$work/drops.script" > "$work/drops-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-drops" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$work/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-drops \
        XDG_DATA_HOME="$work/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/config" MLX_CANVAS=cpu \
        timeout 60 "$work/mlx-compositor" --verbose --no-fps --renderer cpu --socket nested-drops \
        --terminal "$work/mlx-terminal" --launcher none --dock "$work/mlx-dock" --run "$work/drag.sh" > "$work/drops.log" 2>&1 || status=$?
    wait "$host_pid" || fail "drops: the test host failed" "$work/drops-host.log"
    rm -rf -- "$runtime"
    [[ $status -eq 0 ]] || fail "drops: the compositor exited with status $status" "$work/drops.log"
    ! grep -q "crashed" "$work/drops.log" || fail "drops: the dock crashed" "$work/drops.log"
    grep -qx "$work/home/Documents" "$work/config/mlx/dock-folders" || fail "drops: a folder dropped on the dock was not pinned (folders: $(tr '\n' ' ' < "$work/config/mlx/dock-folders"))" "$work/drops.log"
    grep -q "^mlx-dock: moved to the trash" "$work/drops.log" || fail "drops: a file dropped on the trash was not moved there" "$work/drops.log"
    echo "ok   a folder dropped on the dock is pinned, a file dropped on the trash goes into it"
    # gio trash does the moving (when it is installed).
    if command -v gio > /dev/null; then
        for _ in $(seq 1 20); do [[ -e "$work/data/Trash/files/junk.txt" ]] && break; sleep 0.1; done
        [[ -e "$work/data/Trash/files/junk.txt" && ! -e "$work/home/junk.txt" ]] || fail "drops: gio trash did not move the file into the trash" "$work/drops.log"
        echo "ok   gio trash moved the file into \$XDG_DATA_HOME/Trash"
    fi
}

drops
