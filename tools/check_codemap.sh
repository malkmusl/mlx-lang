#!/usr/bin/env bash
# MLX Codemap (examples/mlx-codemap) end to end, on a small tree of its own:
#   - the scene shader passes std.spirv.module;
#   - the command line (the same binary with a command) answers, also
#     with the workarounds and the kept crashes;
#   - the app reads the tree, lays it out in 3D and draws it; typing
#     searches, Enter flies to the result and selects it, with what it
#     connects to (the function is called from the other file); Escape
#     clears; a click on a group in the sidebar selects the group, one on
#     its check box leaves it out and the camera flies to what is shown;
#   - drawn alike on the GPU (lavapipe) and on the CPU (MLX_CANVAS=cpu).
# The app runs under tools/wayland-test-host; MLX_CODEMAP_TRACE makes it
# say what it read and selected.
#
#   tools/check_codemap.sh [compiler] [lavapipe manifest]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
manifest=${2:-/usr/share/vulkan/icd.d/lvp_icd.json}
[[ -f "$manifest" ]] || { echo "check_codemap.sh: no Vulkan driver manifest at $manifest" >&2; exit 2; }
command -v xkbcli > /dev/null || { echo "check_codemap.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"' EXIT
"$compiler" --quiet examples/mlx-codemap/check_shader.mlx -o "$work/check-shader"
"$compiler" --quiet examples/mlx-codemap/main.mlx -o "$work/mlx-codemap"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$work/check-shader" || fail "the scene shader is not valid SPIR-V"
echo "ok   the scene shader passes std.spirv.module"

# The tree: two groups, main calling a documented function of the other.
tree="$work/tree"
mkdir -p "$tree/app" "$tree/lib"
cat > "$tree/lib/helper.mlx" <<'MLX'
// What the app uses.

// Says hello, `times` times.
pub fn greet(times: usize) -> usize {
    return times + count
}

pub const count: usize = 3

pub const Point = struct {
    x: i32
    y: i32
}
MLX
cat > "$tree/lib/extra.mlx" <<'MLX'
const helper = @import("./helper.mlx")

pub fn twice() -> usize { return helper.greet(2) * 2 }

pub fn first(values: [4]u8) -> u8 {
    var copy = values
    var bytes: [*]u8 = undefined
    unsafe { bytes = @ptrCast([*]u8, &copy) }
    return bytes[0]
}
MLX
# A crash kept by a desktop program (examples/wayland-compositor/crash.mlx)
# in greet, called from main.
mkdir -p "$work/.local/state/mlx"
printf 'crash\t1790000000\tapp\tillegal instruction (a failed runtime check)\tlib/helper.mlx:4:greet+0x1c\tapp/main.mlx:3:main+0x42\t_start+0x58\n' > "$work/.local/state/mlx/crashes.log"
cat > "$tree/app/main.mlx" <<'MLX'
const helper = @import("../lib/helper.mlx")

pub fn main() -> u8 {
    const point = helper.Point.{ .x = 1, .y = 2 }
    return @intCast(u8, helper.greet(1) + @intCast(usize, point.x))
}
MLX

# The command line.
"$work/mlx-codemap" -C "$tree" callers greet > "$work/callers.txt"
grep -q "app/main.mlx:5:.*main" "$work/callers.txt" && grep -q "lib/extra.mlx:3:.*twice" "$work/callers.txt" || fail "the command line did not find greet's callers" "$work/callers.txt"
echo "ok   the command line: mlx-codemap -C TREE callers greet"
"$work/mlx-codemap" -C "$tree" workarounds > "$work/workarounds.txt"
grep -A1 "^== 1x Arrays cast to many-pointers" "$work/workarounds.txt" | grep -q "lib/extra.mlx:5:8: function first" \
    && grep -A1 "^== 1x Declared undefined, set in unsafe" "$work/workarounds.txt" | grep -q "lib/extra.mlx:5:8: function first" \
    || fail "the command line did not find the workarounds in first" "$work/workarounds.txt"
echo "ok   the command line: mlx-codemap -C TREE workarounds"
HOME="$work" XDG_STATE_HOME= "$work/mlx-codemap" -C "$tree" crashes > "$work/crashes.txt"
grep -q "^lib/helper.mlx:4:8: function greet (crashed here 1x)$" "$work/crashes.txt" \
    && grep -q "^app/main.mlx:3:8: function main (on the way 1x)$" "$work/crashes.txt" \
    && grep -q "^   from _start+0x58$" "$work/crashes.txt" \
    || fail "the command line did not put the kept crash on greet and main" "$work/crashes.txt"
echo "ok   the command line: mlx-codemap -C TREE crashes"

# The app: at 1180x760 the sidebar's rows are 26 apart from y 44 (Alles,
# app, lib).
cat > "$work/app.script" <<SCRIPT
wait 4000
shot SHOTS/start.ppm
type greet
wait 700
shot SHOTS/search.ppm
enter
wait 2000
shot SHOTS/selected.ppm
down 1
up 1
wait 300
down 1
up 1
wait 1500
pointer 60 83
wait 200
press 272
release 272
wait 4000
shot SHOTS/group.ppm
pointer 20 109
wait 200
press 272
release 272
wait 1000
close
SCRIPT

# Runs the scenario as $1 (the screenshots go to $work/$1), drawing with
# MLX_CANVAS=$2.
run() {
    local name=$1
    local canvas=$2
    local shots="$work/$name"
    mkdir -p "$shots"
    sed "s|SHOTS|$shots|g" "$work/app.script" > "$shots/app.script"
    local runtime
    runtime=$(mktemp -d)
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-codemap "$work/us.xkb" "$shots/app.script" > "$shots/host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-codemap" ]] && break; sleep 0.1; done
    local status=0
    env -i PATH="$PATH" HOME="$work" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-codemap \
        VK_DRIVER_FILES="$manifest" MLX_CANVAS="$canvas" MLX_CODEMAP_TRACE=1 LANG=de_DE.UTF-8 \
        timeout 60 "$work/mlx-codemap" "$tree" > "$shots/app.log" 2>&1 || status=$?
    wait "$host_pid" || fail "codemap ($name): the test host failed" "$shots/host.log"
    rm -rf -- "$runtime"
    local log="$shots/app.log"
    [[ $status -eq 0 ]] || fail "codemap ($name): the app exited with status $status" "$log"
    grep -q "^codemap: read 3 files, " "$log" || fail "codemap ($name): it did not read the three files" "$log"
    grep -q "^codemap: crashes 1 kept, on 2 declarations$" "$log" || fail "codemap ($name): the kept crash is not on greet and main" "$log"
    grep -q "^codemap: selected tree/lib/helper.mlx/greet/$" "$log" || fail "codemap ($name): typing greet and Enter did not select it" "$log"
    grep -A1 "^codemap: selected tree/lib/helper.mlx/greet/$" "$log" | grep -q "^codemap: related out 1 in 2$" || fail "codemap ($name): greet should use count and be called from main and twice" "$log"
    grep -q "^codemap: selected $" "$log" || fail "codemap ($name): Escape did not clear the selection" "$log"
    grep -q "^codemap: selected tree/app/$" "$log" || fail "codemap ($name): the sidebar's app group was not selected" "$log"
    grep -A1 "^codemap: selected tree/app/$" "$log" | grep -q "^codemap: related out 1 in 0$" || fail "codemap ($name): the app group should depend on the lib group" "$log"
    # Its check box leaves lib out: its declarations, and the workarounds
    # in it; the crash (through main in app) stays.
    grep -q "^codemap: shown [0-9]* nodes, findings 0 0 [0-9]* 2 1$" "$log" || fail "codemap ($name): at first everything is shown" "$log"
    grep -q "^codemap: shown [0-9]* nodes, findings 0 0 [0-9]* 0 1$" "$log" || fail "codemap ($name): the lib group's check box did not leave it out" "$log"
    grep -q "^codemap: flying to tree/app/$" "$log" || fail "codemap ($name): the camera did not fly to the group still shown" "$log"
    python3 - "$shots/start.ppm" "$shots/selected.ppm" <<'PY' || fail "codemap ($name): the view or the detail panel is not drawn" "$log"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    width, height = map(int, size.split())
    return width, height, pixels
width, height, start = load(sys.argv[1])
_, _, selected = load(sys.argv[2])
def at(pixels, x, y): return tuple(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
assert (width, height) == (1180, 760), (width, height)
# Something lit in the view (spheres), over the dark background.
lit = sum(1 for y in range(60, 720, 4) for x in range(230, 1170, 4) if max(at(start, x, y)) > 150)
assert lit > 20, lit
# The detail panel on the right once greet is selected.
changed = sum(1 for y in range(80, 700, 8) for x in range(850, 1170, 8) if max(abs(a - b) for a, b in zip(at(start, x, y), at(selected, x, y))) > 20)
assert changed > 100, changed
PY
    echo "ok   codemap ($name): reads the tree, searches, selects, shows what connects"
}

run gpu ""
run cpu cpu
python3 - "$work" <<'PY' || fail "codemap: the GPU and the CPU draw it differently"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    return pixels
for name in ('start', 'selected', 'group'):
    gpu = load(f'{sys.argv[1]}/gpu/{name}.ppm')
    cpu = load(f'{sys.argv[1]}/cpu/{name}.ppm')
    assert len(gpu) == len(cpu), name
    # The scene's floats round a little differently; text and panels are
    # the same.
    off = sum(1 for at in range(0, len(gpu), 3) if max(abs(gpu[at + k] - cpu[at + k]) for k in range(3)) > 24)
    assert off * 3 * 200 < len(gpu), (name, off)
PY
echo "ok   codemap: the same on the GPU and the CPU"
