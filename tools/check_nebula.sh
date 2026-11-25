#!/usr/bin/env bash
# Nebula, the painting program (projects/nebula), end to end:
#   - its tests (projects/nebula/test.sh: the engine, the brushes, the
#     filters, the file formats, the widgets and the program as a whole,
#     each built for Linux and for brixOS and run here);
#   - the program builds for both systems;
#   - played by a script without a window (nebula --script): a new image,
#     a stroke that becomes one step of the history, undo and redo by
#     their keys, the eraser, the frame saved as a picture;
#   - in a window under tools/wayland-test-host: it says it draws on the
#     CPU, its window shows the canvas with the image on it, a stroke
#     dragged over the canvas paints, Ctrl+Z takes it back.
# brixOS itself is not booted here (tools/check_brix_apps.sh's way, with
# QEMU, would start it from the launcher).
#
#   tools/check_nebula.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
[[ -x "$compiler" ]] || { echo "check_nebula.sh: no compiler at $compiler" >&2; exit 2; }

work=$(mktemp -d)
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

# The tests.
projects/nebula/test.sh -c "$compiler" > "$work/tests.log" 2>&1 || fail "nebula's tests" "$work/tests.log"
echo "ok   nebula's tests: $(grep -c '^ok' "$work/tests.log") passed, for Linux and built for brixOS"

# The program, for both systems.
"$compiler" --quiet projects/nebula/main.mlx -o "$work/nebula" || fail "nebula does not build"
"$compiler" --quiet --target=x86_64-brixos projects/nebula/main.mlx -o "$work/nebula-brixos" || fail "nebula does not build for brixOS"
echo "ok   nebula builds for Linux and for brixOS"

# Played by a script.
cat > "$work/first.script" <<EOF
size 1000 700
new 800 600 FFFFFF
expect 400 300 FFFFFFFF
color 2040C0
drag 300 380 700 380 24
expect 400 300 FF2040C0
key Ctrl+Z
expect 400 300 FFFFFFFF
expect-note Undone
key Ctrl+Shift+Z
expect 400 300 FF2040C0
shot $work/script.png
EOF
(cd "$work" && timeout 60 ./nebula --script first.script) > "$work/script.log" 2>&1 || fail "nebula --script" "$work/script.log"
[[ -s "$work/script.png" ]] || fail "the script's picture was not written" "$work/script.log"
echo "ok   a script without a window: a stroke, undone and redone, the frame as a picture"

# In a window under the test host.
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
"$work/brix-keymap" us "$work/us.xkb" || fail "no keymap (the host's XKB data)"
cat > "$work/app.script" <<EOF
wait 1500
shot $work/start.ppm
pointer 500 400
wait 100
press 272
pointer 560 400
wait 30
pointer 640 400
wait 30
pointer 760 400
wait 30
release 272
wait 400
shot $work/stroke.ppm
down 29
down 44
up 44
up 29
wait 400
shot $work/undone.ppm
close
EOF
runtime=$(mktemp -d)
XDG_RUNTIME_DIR=$runtime "$work/test-host" host-nebula "$work/us.xkb" "$work/app.script" > "$work/host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$runtime/host-nebula" ]] && break; sleep 0.1; done
status=0
env -i PATH="$PATH" HOME="$work" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-nebula LANG=en_US.UTF-8 \
    timeout 60 "$work/nebula" > "$work/app.log" 2>&1 || status=$?
wait "$host_pid" || fail "the test host failed" "$work/host.log"
rm -rf -- "$runtime"
[[ $status -eq 0 ]] || fail "nebula exited with status $status" "$work/app.log"
grep -q "^nebula: drawing on the cpu" "$work/app.log" || fail "nebula did not say it draws on the CPU" "$work/app.log"
python3 - "$work/start.ppm" "$work/stroke.ppm" "$work/undone.ppm" <<'PY' || fail "the window's canvas" "$work/app.log"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    return width, height, pixels
width, height, start = load(sys.argv[1])
_, _, stroke = load(sys.argv[2])
_, _, undone = load(sys.argv[3])
def at(pixels, x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
assert (width, height) == (1280, 800), (width, height)
# The white image in the middle of the canvas.
assert at(start, 640, 300) == (255, 255, 255), at(start, 640, 300)
# The stroke: dark along the line the pointer went, white above it.
dark = sum(1 for x in range(520, 740, 4) if max(at(stroke, x, 400)) < 60)
assert dark > 40, dark
assert at(stroke, 640, 340) == (255, 255, 255), at(stroke, 640, 340)
# Undone: white again.
assert at(undone, 640, 400) == (255, 255, 255), at(undone, 640, 400)
PY
echo "ok   in a window: the canvas shown, a stroke painted, Ctrl+Z takes it back"
echo PASS
