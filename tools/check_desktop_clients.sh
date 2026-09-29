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
#     into it);
#   - runs the top bar (mlx-topbar) with a terminal: the bar is along the
#     top and reserves it (the terminal moves below it, maximized it stays
#     below), shows the active app and a fixed time in German, and the
#     local times clock.mlx computes match python's zoneinfo;
#   - runs the file manager (mlx-files) on a home of its own: it shows the
#     folders first, makes a folder (Ctrl+Shift+N) and names it, moves a
#     picture into it by dragging, renames a file (F2), moves one to the
#     trash (Delete), opens a folder with a double click, goes back with
#     Alt+Left; drawn alike on the GPU and on the CPU;
#   - opens a folder from the dock's stack: the file manager's window
#     grows out of the stack (the compositor's zoom), which goes as it
#     comes.
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
"$compiler" --quiet examples/mlx-topbar/main.mlx -o "$work/mlx-topbar"
"$compiler" --quiet tools/clock-probe/main.mlx -o "$work/clock-probe"
"$compiler" --quiet examples/mlx-files/main.mlx -o "$work/mlx-files"
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
        --terminal "$work/mlx-terminal" --launcher "$work/mlx-launcher" --dock "$work/mlx-dock" --topbar none \
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
        --terminal "$work/mlx-terminal" --launcher none --dock "$work/mlx-dock" --topbar none --run "$work/drag.sh" > "$work/drops.log" 2>&1 || status=$?
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

# The top bar with a terminal, at a fixed time (2024-09-27 14:05 UTC, 16:05
# in Berlin), in German.
# The file manager (mlx-files) on a home of its own, its apps drawing on
# $1 (cpu or gpu). Its window is at (24, 24), the icons' centres 118
# pixels apart from x 296, the rows 104 apart from y 125.
files() {
    local canvas=$1
    local runtime
    runtime=$(mktemp -d)
    local home="$work/files-$canvas/home"
    local data="$work/files-$canvas/data"
    rm -rf -- "$work/files-$canvas"
    mkdir -p "$home/Documents" "$home/Downloads" "$home/Project 2" "$home/Project 10" "$data" "$work/files-$canvas/config"
    echo notes > "$home/notes.txt"
    echo report > "$home/report.pdf"
    echo hidden > "$home/.hidden"
    cp tests/support/png/rgba8.png "$home/photo.png"
    touch -d '2024-03-01 10:00' "$home"/* "$home/Documents" "$home/Downloads"
    # Folders first, numbers by value: Documents, Downloads, Project 2,
    # Project 10, notes.txt, photo.png; then report.pdf. After Stuff is
    # made it comes fifth; photo.png moves into it, report.pdf takes its
    # place (the second row's first).
    cat > "$work/files-$canvas.script" <<SCRIPT
wait 3000
shot $work/files-$canvas/grid.ppm
down 29
down 42
down 49
up 49
up 42
up 29
wait 1500
type Stuff
enter
wait 1500
pointer 296 230
wait 200
press 272
wait 100
pointer 310 225
wait 100
pointer 600 150
wait 100
pointer 768 125
wait 800
release 272
wait 2500
pointer 886 125
wait 200
press 272
release 272
wait 400
down 60
up 60
wait 500
type ideas
enter
wait 1500
pointer 296 230
wait 200
press 272
release 272
wait 400
down 111
up 111
wait 2500
pointer 296 125
wait 200
press 272
release 272
wait 100
press 272
release 272
wait 1500
down 29
down 42
down 49
up 49
up 42
up 29
wait 1500
type Inner
enter
wait 1500
down 56
down 105
up 105
up 56
wait 1500
shot $work/files-$canvas/back.ppm
close
SCRIPT
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-files "$work/us.xkb" "$work/files-$canvas.script" > "$work/files-$canvas-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-files" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-files \
        XDG_DATA_HOME="$data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/files-$canvas/config" VK_DRIVER_FILES="$manifest" MLX_CANVAS="$canvas" \
        LANG=en_US.UTF-8 TZ=UTC \
        timeout 90 "$work/mlx-compositor" --verbose --no-fps --renderer cpu --socket nested-files \
        --launcher none --dock none --topbar none --run "$work/mlx-files" > "$work/files-$canvas.log" 2>&1 || status=$?
    wait "$host_pid" || fail "files: the test host failed" "$work/files-$canvas-host.log"
    rm -rf -- "$runtime"
    local log="$work/files-$canvas.log"
    [[ $status -eq 0 ]] || fail "files: the compositor exited with status $status" "$log"
    grep -q "^window: 940x580 at 24,24" "$log" || fail "files: the file manager's window did not open" "$log"
    python3 - "$work/files-$canvas/grid.ppm" <<'PY' || fail "files ($canvas): the folders are not shown first" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
def blue(p): return p[2] > 200 and p[0] < 120
# Four folders (blue) in the first row, then pages (white).
for x in (296, 414, 532, 650):
    assert blue(at(x - 18, 130)), (x, at(x - 18, 130))
assert min(at(768, 128)) > 200, at(768, 128)
# The sidebar: the home selected, its icon blue.
assert blue(at(48, 82)), at(48, 82)
PY
    # The picture of photo.png, made by the thumbnail worker into the
    # freedesktop cache (named by the MD5 of its URI).
    local thumb
    thumb=$(python3 -c 'import hashlib, sys; print(hashlib.md5(("file://" + sys.argv[1]).encode()).hexdigest())' "$home/photo.png")
    [[ -s "$home/.cache/thumbnails/normal/$thumb.png" ]] || fail "files ($canvas): no thumbnail of photo.png in ~/.cache/thumbnails/normal" "$log"
    [[ -d "$home/Stuff" && ! -e "$home/untitled folder" ]] || fail "files ($canvas): Ctrl+Shift+N and typing did not make the folder Stuff ($(ls "$home" | tr '\n' ' '))" "$log"
    grep -q "^drag: dropped on" "$log" && [[ -f "$home/Stuff/photo.png" && ! -e "$home/photo.png" ]] || fail "files ($canvas): the picture dragged onto Stuff did not move into it" "$log"
    [[ -f "$home/ideas.txt" && ! -e "$home/notes.txt" ]] || fail "files ($canvas): F2 did not rename notes.txt to ideas.txt ($(ls "$home" | tr '\n' ' '))" "$log"
    if command -v gio > /dev/null; then
        [[ -f "$data/Trash/files/report.pdf" && ! -e "$home/report.pdf" ]] || fail "files ($canvas): Delete did not move report.pdf to the trash" "$log"
    fi
    [[ -d "$home/Documents/Inner" ]] || fail "files ($canvas): a double click did not open Documents" "$log"
    echo "ok   files ($canvas): folders first; makes, renames, moves, trashes, opens folders"
}

# The dock's stack opening in the file manager (mlx-files on the PATH):
# the window grows out of the stack, the stack goes. The dock holds the
# terminal and the Downloads folder (at x 548); its stack of ten entries
# has its Open button at (741, 473).
zoom() {
    local renderer=$1
    local runtime
    runtime=$(mktemp -d)
    local base="$work/zoom-$renderer"
    rm -rf -- "$base"
    mkdir -p "$base/home/Downloads/Photos" "$base/config/mlx" "$base/data" "$base/bin"
    for i in 1 2 3 4 5 6 7; do echo "$i" > "$base/home/Downloads/doc$i.txt"; done
    echo paper > "$base/home/Downloads/paper.pdf"
    cp tests/support/png/rgba8.png "$base/home/Downloads/pic.png"
    printf 'terminal\n' > "$base/config/mlx/dock"
    ln -s "$work/mlx-files" "$base/bin/mlx-files"
    cat > "$base.script" <<SCRIPT
wait 3500
pointer 548 728
wait 300
press 272
release 272
wait 1500
pointer 741 473
wait 300
press 272
release 272
wait 3000
shot $base/opened.ppm
close
SCRIPT
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-zoom "$work/us.xkb" "$base.script" > "$base-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-zoom" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$base/bin:$PATH" HOME="$base/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-zoom \
        XDG_DATA_HOME="$base/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$base/config" VK_DRIVER_FILES="$manifest" \
        LANG=en_US.UTF-8 TZ=UTC \
        timeout 90 "$work/mlx-compositor" --verbose --no-fps --renderer "$renderer" --socket nested-zoom \
        --launcher none --dock "$work/mlx-dock" --topbar none > "$base.log" 2>&1 || status=$?
    wait "$host_pid" || fail "zoom: the test host failed" "$base-host.log"
    rm -rf -- "$runtime"
    local log="$base.log"
    [[ $status -eq 0 ]] || fail "zoom ($renderer): the compositor exited with status $status" "$log"
    grep -q "^map: Downloads" "$log" || fail "zoom ($renderer): Open in the stack did not open the file manager" "$log"
    grep -q "^zoom: from 484x242 at 305,450" "$log" || fail "zoom ($renderer): the window did not grow out of the stack" "$log"
    python3 - "$base/opened.ppm" <<'PY' || fail "zoom ($renderer): the stack is still there" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
# Below the window, where the stack's lower part was, the desktop's
# background shows again: no dark panel.
assert at(320, 650)[2] > 70 and at(320, 650)[2] > at(320, 650)[0] + 20, at(320, 650)
PY
    echo "ok   zoom ($renderer): a folder opened from the dock's stack grows out of it into the file manager"
}

topbar() {
    local runtime
    runtime=$(mktemp -d)
    cat > "$work/topbar.script" <<SCRIPT
wait 3000
shot $work/topbar.ppm
pointer 300 200
press 272
release 272
wait 300
down 125
down 103
up 103
up 125
wait 1200
close
SCRIPT
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-topbar "$work/us.xkb" "$work/topbar.script" > "$work/topbar-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-topbar" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$work/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-topbar \
        XDG_DATA_HOME="$work/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/config" MLX_CANVAS=cpu \
        LANG=de_DE.UTF-8 TZ="CET-1CEST,M3.5.0,M10.5.0/3" MLX_TOPBAR_TIME=1727445900 \
        timeout 60 "$work/mlx-compositor" --verbose --no-fps --renderer cpu --socket nested-topbar \
        --terminal "$work/mlx-terminal" --launcher none --dock none --topbar "$work/mlx-topbar" --run "$work/mlx-terminal" > "$work/topbar.log" 2>&1 || status=$?
    wait "$host_pid" || fail "topbar: the test host failed" "$work/topbar-host.log"
    rm -rf -- "$runtime"
    [[ $status -eq 0 ]] || fail "topbar: the compositor exited with status $status" "$work/topbar.log"
    local log="$work/topbar.log"
    grep -q "^layer: mlx-topbar 1024x30 at 0,0 in layer 2" "$log" || fail "topbar: the bar is not along the top in the top layer" "$log"
    # The terminal came before the bar reserved the top: it moved below.
    grep -q "^window: 652x396 at 24,24" "$log" || fail "topbar: the terminal did not open at (24, 24)" "$log"
    grep -q "^maximize: Mlx Terminal" "$log" && grep -q "^window: 1020x700 at 2,52" "$log" || fail "topbar: the maximized terminal is not below the bar" "$log"
    python3 - "$work/topbar.ppm" <<'PY' || fail "topbar: the bar does not show the app and the time" "$log"
import sys
data = open(sys.argv[1], 'rb').read()
header, size, depth, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
def lit(x0, x1): return sum(1 for x in range(x0, x1) for y in range(8, 22) if min(at(x, y)) > 200)
# White text at the left (the app's name) and the right (the time), none
# in the middle; the terminal's title bar starts below the bar.
assert lit(40, 110) > 20, lit(40, 110)
assert lit(890, 1010) > 40, lit(890, 1010)
assert lit(400, 600) == 0, lit(400, 600)
assert min(at(300, 40)) > 80, at(300, 40)
PY
    echo "ok   the top bar is along the top, reserves it, and shows the app and the time"
    # The zone arithmetic against python's zoneinfo, where it is there.
    if python3 -c 'import zoneinfo; zoneinfo.ZoneInfo("Europe/Berlin")' 2> /dev/null; then
        python3 - "$work/clock-probe" <<'PY' || fail "clock.mlx and zoneinfo disagree" /dev/null
import subprocess, random, datetime, zoneinfo, sys
random.seed(7)
bad = 0
for name in ["Europe/Berlin", "America/New_York", "Australia/Sydney", "Asia/Kolkata", "America/St_Johns", "UTC"]:
    zone = zoneinfo.ZoneInfo(name)
    times = [random.randint(0, 4102444800) for _ in range(400)]
    for year in (2024, 2031, 2039):
        for month in (3, 4, 10, 11):
            base = int(datetime.datetime(year, month, 1, tzinfo=datetime.timezone.utc).timestamp())
            times += [base + day * 86400 + hour * 3600 - 1 for day in range(31) for hour in range(4)]
    lines = subprocess.run([sys.argv[1]] + [str(t) for t in times], env={"TZ": name}, capture_output=True, text=True).stdout.split("\n")
    for t, line in zip(times, lines):
        d = datetime.datetime.fromtimestamp(t, zone)
        want = f"{d.year:04d}-{d.month:02d}-{d.day:02d} {d.hour:02d}:{d.minute:02d}:{d.second:02d} {int(d.utcoffset().total_seconds())}"
        if line != want:
            bad += 1
            if bad < 5: print(name, t, line, "!=", want)
sys.exit(1 if bad else 0)
PY
        echo "ok   local times from zone files match python's zoneinfo"
    fi
}

drops
topbar
files cpu
files gpu
zoom cpu
zoom vulkan
python3 - "$work/files-cpu/grid.ppm" "$work/files-gpu/grid.ppm" <<'PY' || fail "the file manager drawn on the GPU differs from the one drawn on the CPU" /dev/null
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    return width, pixels
width, first = load(sys.argv[1])
_, second = load(sys.argv[2])
# All but the status bar (the free space on the disk may change).
rows = [y for y in range(768) if not 574 <= y < 604]
bad = sum(1 for y in rows if first[y * width * 3:(y + 1) * width * 3] != second[y * width * 3:(y + 1) * width * 3])
sys.exit(1 if bad else 0)
PY
echo "ok   the file manager draws the same pixels on the GPU as on the CPU"
