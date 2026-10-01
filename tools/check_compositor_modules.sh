#!/usr/bin/env bash
# Loading the compositor's shell and renderer as shared objects and loading
# them again while it runs (projects/desktop/compositor/modules.mlx).
#
# The nested compositor runs under tools/wayland-test-host with --modules
# DIR, where libmlx-shell.so and libmlx-render.so were built from the
# sources. While an Mlx terminal is open and has been typed into, the two
# files are replaced by builds of a changed copy of the sources: the
# renderer paints the background red, the shell places new windows 200
# pixels further apart. The compositor must load both, keep the terminal
# (typing into it must still reach its shell, through handlers moved to the
# new code), place the next windows the new way and show the red
# background. Then a shell built for a changed record (a field added to
# state.Surface) must be refused while the compositor carries on.
#
#   tools/check_compositor_modules.sh [compiler] [cpu|vulkan]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
renderer=${2:-cpu}
command -v xkbcli > /dev/null || { echo "check_compositor_modules.sh: xkbcli is not installed" >&2; exit 2; }
python3 tools/check_compositor_modules.py

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
export MLX_ARENA_POISON=1
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet projects/desktop/terminal/main.mlx -o "$work/mlx-terminal"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

# The modules as the sources are, and as changed copies.
build_modules() {
    local sources=$1 output=$2
    mkdir -p "$output"
    "$compiler" --quiet --shared "$sources/projects/desktop/compositor/shell_module.mlx" -o "$output/libmlx-shell.so"
    "$compiler" --quiet --shared "$sources/projects/desktop/compositor/render_module.mlx" -o "$output/libmlx-render.so"
}
build_modules . "$work/modules"
mkdir -p "$work/changed/examples" "$work/layout/examples"
cp -r projects/desktop/compositor examples/vulkan-shared projects/desktop/shared "$work/changed/examples/"
cp -r projects/desktop/compositor examples/vulkan-shared projects/desktop/shared "$work/layout/examples/"
python3 - "$work/changed/projects/desktop/compositor/scene.mlx" <<'PY'
import sys
path = sys.argv[1]
text = open(path).read()
background = "    return 4278190080 | ((24 + shade / 2) << 16) | ((34 + shade / 2) << 8) | (52 + shade)\n"
assert text.count(background) == 1
text = text.replace(background, "    return 4278190080 | (200 << 16) | shade * 0\n")
spacing = "    compositor.*.nextWindowX += 40\n"
assert text.count(spacing) == 1
text = text.replace(spacing, "    compositor.*.nextWindowX += 200\n")
open(path, "w").write(text)
PY
python3 - "$work/layout/projects/desktop/compositor/state.mlx" <<'PY'
import sys
path = sys.argv[1]
text = open(path).read()
marker = "    belowCount: usize\n"
assert text.count(marker) == 1
text = text.replace(marker, marker + "    addedForTheCheck: u64\n")
open(path, "w").write(text)
PY
build_modules "$work/changed" "$work/changed-modules"
build_modules "$work/layout" "$work/layout-modules"

# Installs a module the way tools/build_compositor_modules.sh does: a new
# file renamed over the old one.
install_module() {
    cp "$1" "$work/modules/.next.so"
    mv "$work/modules/.next.so" "$work/modules/$(basename "$1")"
}

cat > "$work/modules.script" <<SCRIPT
wait 1500
pointer 200 200
press 272
release 272
wait 300
type echo before > $work/before.marker
enter
wait 4000
type echo after > $work/after.marker
enter
wait 800
down 56
down 28
up 28
up 56
wait 800
down 56
down 28
up 28
up 56
wait 1500
shot $work/changed.ppm
wait 3000
close
SCRIPT

"$work/test-host" host-modules "$work/us.xkb" "$work/modules.script" > "$work/host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-modules" ]] && break; sleep 0.1; done
WAYLAND_DISPLAY=host-modules timeout 60 "$work/mlx-compositor" --verbose --renderer "$renderer" --socket nested-modules \
    --modules "$work/modules" --terminal "$work/mlx-terminal" --run "$work/mlx-terminal" > "$work/compositor.log" 2>&1 &
compositor_pid=$!
# While the terminal runs: the changed modules, then (after the screenshot)
# a shell for another layout.
sleep 3.5
install_module "$work/changed-modules/libmlx-render.so"
install_module "$work/changed-modules/libmlx-shell.so"
sleep 7
install_module "$work/layout-modules/libmlx-shell.so"
status=0
wait "$compositor_pid" || status=$?
wait "$host_pid" || { echo "test host failed:" >&2; cat "$work/host.log" >&2; exit 1; }
fail() { echo "FAIL $1" >&2; cat "$work/compositor.log" >&2; exit 1; }
[[ $status -eq 0 ]] || fail "the compositor exited with status $status"

grep -q "^modules: loaded .*libmlx-shell.so (version 1" "$work/compositor.log" || fail "the shell module was not loaded at start-up"
grep -q "^modules: loaded .*libmlx-render.so (version 1)" "$work/compositor.log" || fail "the render module was not loaded at start-up"
[[ "$(cat "$work/before.marker" 2> /dev/null)" == "before" ]] || fail "typing did not reach the terminal before the reload"
echo "ok   the compositor runs its shell and renderer from shared objects"

grep -q "^modules: loaded .*libmlx-render.so (version 2)" "$work/compositor.log" || fail "the changed render module was not loaded"
grep -Eq "^modules: loaded .*libmlx-shell.so \(version 2, [1-9][0-9]* handler uses moved over" "$work/compositor.log" || fail "the changed shell module was not loaded, or moved no handlers"
[[ "$(cat "$work/after.marker" 2> /dev/null)" == "after" ]] || fail "typing did not reach the terminal after the shell was reloaded"
echo "ok   both modules were loaded again while the compositor ran; the terminal kept working"

# The second window goes where the old code left off (64, 56); the new
# code then moves 200 pixels on, so the third is at 264, 88.
grep -q "^window: 652x396 at 64,56" "$work/compositor.log" || fail "the second window is not where it should be"
grep -q "^window: 652x396 at 264,88" "$work/compositor.log" || fail "the reloaded shell did not place the third window its way"
python3 - "$work/changed.ppm" <<'PY' || fail "the reloaded renderer did not paint the background red"
import sys
data = open(sys.argv[1], 'rb').read()
header, rest = data.split(b'\n', 1)
size, rest = rest.split(b'\n', 1)
_, pixels = rest.split(b'\n', 1)
width, height = map(int, size.split())
x, y = width - 20, height - 20
offset = (y * width + x) * 3
assert tuple(pixels[offset:offset + 3]) == (200, 0, 0), tuple(pixels[offset:offset + 3])
PY
echo "ok   the reloaded modules' changes show: new window placement, a red background"

grep -q "libmlx-shell.so: it was built for records laid out differently" "$work/compositor.log" || fail "a shell built for another layout was not refused"
grep -q "^modules: loaded .*libmlx-shell.so (version 3" "$work/compositor.log" && fail "a shell built for another layout was loaded"
echo "ok   a module built for changed records is refused and the compositor carries on"
