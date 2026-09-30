#!/usr/bin/env bash
# MLX Observatory (examples/mlx-observatory) end to end, on a small tree of its own:
#   - the scene shader passes std.spirv.module;
#   - the command line (the same binary with a command) answers, also
#     with the workarounds, the kept crashes, the programs and what they
#     reach, machine code, cycles, layers and the history (with git), and
#     rewrites the workarounds the compiler no longer needs;
#   - the app reads the tree, lays it out in 3D and draws it; typing
#     searches, Enter flies to the result and selects it, with what it
#     connects to (the function is called from the other file); Escape
#     clears; a click on a group in the sidebar selects the group, one on
#     its check box leaves it out and the camera flies to what is shown;
#   - drawn alike on the GPU (lavapipe) and on the CPU (MLX_CANVAS=cpu).
# The app runs under tools/wayland-test-host; MLX_CODEMAP_TRACE makes it
# say what it read and selected.
#
#   tools/check_observatory.sh [compiler] [lavapipe manifest]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
manifest=${2:-/usr/share/vulkan/icd.d/lvp_icd.json}
[[ -f "$manifest" ]] || { echo "check_observatory.sh: no Vulkan driver manifest at $manifest" >&2; exit 2; }
command -v xkbcli > /dev/null || { echo "check_observatory.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"' EXIT
"$compiler" --quiet examples/mlx-observatory/check_shader.mlx -o "$work/check-shader"
"$compiler" --quiet examples/mlx-observatory/main.mlx -o "$work/mlx-observatory"
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
# A cycle (a <-> b) and an import against a rule (lib !-> app).
cat > "$tree/lib/a.mlx" <<'MLX'
const b = @import("./b.mlx")

pub fn alpha() -> usize { return 1 }
MLX
cat > "$tree/lib/b.mlx" <<'MLX'
const a = @import("./a.mlx")

pub fn beta() -> usize { return 2 }
MLX
cat > "$tree/lib/bad.mlx" <<'MLX'
const app = @import("../app/main.mlx")

pub fn gamma() -> usize { return 3 }
MLX
echo "lib !-> app" > "$tree/codemap.layers"
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
"$work/mlx-observatory" -C "$tree" callers greet > "$work/callers.txt"
grep -q "app/main.mlx:5:.*main" "$work/callers.txt" && grep -q "lib/extra.mlx:3:.*twice" "$work/callers.txt" || fail "the command line did not find greet's callers" "$work/callers.txt"
echo "ok   the command line: mlx-codemap -C TREE callers greet"
"$work/mlx-observatory" -C "$tree" workarounds > "$work/workarounds.txt"
grep -A1 "^== 1x Arrays cast to many-pointers" "$work/workarounds.txt" | grep -q "lib/extra.mlx:5:8: function first" \
    && grep -A1 "^== 1x Declared undefined, set in unsafe" "$work/workarounds.txt" | grep -q "lib/extra.mlx:5:8: function first" \
    || fail "the command line did not find the workarounds in first" "$work/workarounds.txt"
echo "ok   the command line: mlx-codemap -C TREE workarounds"
HOME="$work" XDG_STATE_HOME= "$work/mlx-observatory" -C "$tree" crashes > "$work/crashes.txt"
grep -q "^lib/helper.mlx:4:8: function greet (crashed here 1x)$" "$work/crashes.txt" \
    && grep -q "^app/main.mlx:3:8: function main (on the way 1x)$" "$work/crashes.txt" \
    && grep -q "^   from _start+0x58$" "$work/crashes.txt" \
    || fail "the command line did not put the kept crash on greet and main" "$work/crashes.txt"
echo "ok   the command line: mlx-codemap -C TREE crashes"
# The program app, built (from the repository: std is there) with its
# symbol table, and what it reaches.
mkdir -p "$work/bin"
"$compiler" --quiet "$tree/app/main.mlx" -o "$work/bin/app"
MLX_CODEMAP_BINARIES="$work/bin" "$work/mlx-observatory" -C "$tree" programs > "$work/programs.txt"
grep -q "^app: app/main.mlx, reaches [0-9]* declarations, built: $work/bin/app (" "$work/programs.txt" || fail "the command line did not find the program app and its binary" "$work/programs.txt"
MLX_CODEMAP_BINARIES="$work/bin" "$work/mlx-observatory" -C "$tree" reach app > "$work/reach.txt"
# (greet is small enough to be inlined: it has no code of its own.)
grep -q "^app/main.mlx:3:8: function main ([0-9]* bytes)$" "$work/reach.txt" && grep -q "^lib/helper.mlx:4:8: function greet" "$work/reach.txt" \
    || fail "app should reach main (with its machine code) and greet" "$work/reach.txt"
MLX_CODEMAP_BINARIES="$work/bin" "$work/mlx-observatory" -C "$tree" unreachable > "$work/unreachable.txt"
grep -q "^lib/extra.mlx:3:8: function twice$" "$work/unreachable.txt" && ! grep -q "greet" "$work/unreachable.txt" || fail "twice (and not greet) should be unreachable" "$work/unreachable.txt"
MLX_CODEMAP_BINARIES="$work/bin" "$work/mlx-observatory" -C "$tree" sizes app > "$work/sizes.txt"
grep -q "function main [0-9]* bytes$" "$work/sizes.txt" || fail "the machine code of main is not measured" "$work/sizes.txt"
"$work/mlx-observatory" -C "$tree" cycles > "$work/cycles.txt"
grep -A2 "^== 2 files import each other$" "$work/cycles.txt" | grep -q "lib/a.mlx" || fail "the cycle a <-> b is not found" "$work/cycles.txt"
"$work/mlx-observatory" -C "$tree" layers > "$work/layers.txt"
grep -q "^lib/bad.mlx:1:7: imports app/main.mlx (lib !-> app)$" "$work/layers.txt" || fail "the import against codemap.layers is not found" "$work/layers.txt"
echo "ok   the command line: programs, reach, unreachable, sizes, cycles, layers"
if command -v git > /dev/null; then
    ( cd "$tree" && git init -q && git add -A && git -c user.email=check@mlx -c user.name=check commit -qm one \
        && sed -i 's/return times + count/return times + count + 0/' lib/helper.mlx \
        && git -c user.email=check@mlx -c user.name=check commit -qam two )
    XDG_CACHE_HOME="$work/cache" "$work/mlx-observatory" -C "$tree" churn > "$work/churn.txt"
    grep -q "^lib/helper.mlx:4:8: function greet (2 commits, the file 2)$" "$work/churn.txt" || fail "greet should have two commits" "$work/churn.txt"
    echo "ok   the command line: churn (git blame)"
    # The workarounds at both commits (each counted from git archive and
    # kept in the cache, where the app finds them).
    HOME="$work" XDG_CACHE_HOME= "$work/mlx-observatory" -C "$tree" history > "$work/history.txt" 2> /dev/null
    [[ $(grep -c "^20[0-9-]* [0-9a-f]\{7\}  2 0 1 1 0 0 0 0 0 0$" "$work/history.txt") -eq 2 ]] \
        && grep -q "^▄▄ 1 Array casts$" "$work/history.txt" && [[ $(wc -l < "$work/.cache/mlx/codemap/workarounds-v1") -eq 2 ]] \
        || fail "history should count the two workarounds at both commits and keep them" "$work/history.txt"
    echo "ok   the command line: history (workarounds per commit)"
    "$work/mlx-observatory" -C "$tree" diff HEAD~1 > "$work/diff.txt"
    grep -q "^~ lib/helper.mlx:4:8: function greet$" "$work/diff.txt" \
        && grep -q "^-- 0 added, 1 changed, 0 removed in 1 files since HEAD~1$" "$work/diff.txt" \
        || fail "diff HEAD~1 should find greet changed" "$work/diff.txt"
    "$work/mlx-observatory" -C "$tree" query changed since:HEAD~1 > "$work/changed.txt"
    grep -q "^lib/helper.mlx:4:8: function greet$" "$work/changed.txt" && grep -q "^-- 1$" "$work/changed.txt" \
        || fail "the query changed since:HEAD~1 should answer greet" "$work/changed.txt"
    echo "ok   the command line: diff and changed since a commit"
    have_git=1
fi
# Queries and the views kept in codemap.views.
"$work/mlx-observatory" -C "$tree" query kind:fn calls:greet > "$work/query.txt"
grep -q "^app/main.mlx:3:8: function main$" "$work/query.txt" && grep -q "^lib/extra.mlx:3:8: function twice$" "$work/query.txt" && grep -q "^-- 2$" "$work/query.txt" \
    || fail "kind:fn calls:greet should answer main and twice" "$work/query.txt"
HOME="$work" XDG_STATE_HOME= "$work/mlx-observatory" -C "$tree" query crashed -in:app > "$work/query.txt"
grep -q "^lib/helper.mlx:4:8: function greet$" "$work/query.txt" && grep -q "^-- 1$" "$work/query.txt" \
    || fail "crashed -in:app should answer greet alone" "$work/query.txt"
"$work/mlx-observatory" -C "$tree" query kind:fish > "$work/query.txt"
grep -q "^not understood: kind:fish$" "$work/query.txt" || fail "kind:fish should not be understood" "$work/query.txt"
printf '# views\nGreeters = kind:fn calls:greet\n' > "$tree/codemap.views"
"$work/mlx-observatory" -C "$tree" views > "$work/views.txt"
grep -q "^@Greeters = kind:fn calls:greet$" "$work/views.txt" || fail "the view Greeters is not listed" "$work/views.txt"
"$work/mlx-observatory" -C "$tree" query @greeters > "$work/query.txt"
grep -q "^-- 2$" "$work/query.txt" || fail "the view @greeters should answer main and twice" "$work/query.txt"
echo "ok   the command line: query (kind, calls, crashed, not) and views"
# Rewriting workarounds away: a result bound only to be dropped becomes
# `_ = ...`, a length counted by hand `.length`; the program still builds
# and does the same.
mkdir -p "$work/rewrite/app"
cat > "$work/rewrite/app/main.mlx" <<'MLX'
fn three() -> usize { return 3 }

fn size(text: []const u8) -> usize {
    var length: usize = 0
    for byte in text { length += 1 }
    return length
}

pub fn main() -> u8 {
    const ignored = three()
    return @intCast(u8, size("abcd"))
}
MLX
MLX_COMPILER="$repo_root/$compiler" "$work/mlx-observatory" -C "$work/rewrite" rewrite dropped > "$work/rewrite.txt"
grep -A1 "^app/main.mlx:10: const ignored = three()$" "$work/rewrite.txt" | grep -q "^  -> _ = three()$" \
    && grep -q "^the compiler accepts them: --apply writes them$" "$work/rewrite.txt" \
    || fail "rewrite should show const ignored = three() becoming _ = three()" "$work/rewrite.txt"
MLX_COMPILER="$repo_root/$compiler" "$work/mlx-observatory" -C "$work/rewrite" rewrite dropped --apply > "$work/rewrite.txt"
MLX_COMPILER="$repo_root/$compiler" "$work/mlx-observatory" -C "$work/rewrite" rewrite counted --apply >> "$work/rewrite.txt"
grep -q "^    _ = three()$" "$work/rewrite/app/main.mlx" && grep -q "^    length += text.length$" "$work/rewrite/app/main.mlx" \
    || fail "rewrite --apply did not write both rewrites" "$work/rewrite/app/main.mlx"
"$compiler" --quiet "$work/rewrite/app/main.mlx" -o "$work/rewritten"
status=0
"$work/rewritten" || status=$?
[[ $status -eq 4 ]] || fail "the rewritten program should still exit with 4, not $status" "$work/rewrite/app/main.mlx"
"$work/mlx-observatory" -C "$work/rewrite" workarounds > "$work/rewrite.txt"
[[ ! -s "$work/rewrite.txt" ]] || fail "no workaround should be left after the rewrites" "$work/rewrite.txt"
echo "ok   the command line: rewrite dropped and counted (preview, --apply)"

# The app: at 1180x760 the sidebar's group rows are 26 apart from y 106
# (Alles, app, lib), under the view switch.
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
pointer 60 145
wait 200
press 272
release 272
wait 4000
shot SHOTS/group.ppm
pointer 20 171
wait 200
press 272
release 272
wait 1000
pointer 100 648
press 272
release 272
wait 500
pointer 100 229
press 272
release 272
wait 500
pointer 100 272
press 272
release 272
wait 2000
type kind:fn calls:greet
wait 1500
shot SHOTS/query.ppm
pointer 895 25
press 272
release 272
wait 300
pointer 840 25
press 272
release 272
wait 300
pointer 700 67
press 272
release 272
wait 800
pointer 100 90
press 272
release 272
wait 300
press 272
release 272
wait 300
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
        VK_DRIVER_FILES="$manifest" MLX_CANVAS="$canvas" MLX_CODEMAP_TRACE=1 MLX_CODEMAP_BINARIES="$work/bin" LANG=de_DE.UTF-8 \
        timeout 60 "$work/mlx-observatory" "$tree" --diff HEAD~1 > "$shots/app.log" 2>&1 || status=$?
    wait "$host_pid" || fail "codemap ($name): the test host failed" "$shots/host.log"
    rm -rf -- "$runtime"
    local log="$shots/app.log"
    [[ $status -eq 0 ]] || fail "codemap ($name): the app exited with status $status" "$log"
    grep -q "^codemap: read 6 files, " "$log" || fail "codemap ($name): it did not read the six files" "$log"
    grep -q "^codemap: measured 1 programs, 1 built$" "$log" || fail "codemap ($name): the program app and its binary are not measured" "$log"
    grep -q "^codemap: color by Machine code$" "$log" || fail "codemap ($name): the spheres are not colored by machine code" "$log"
    grep -q "^codemap: program app$" "$log" || fail "codemap ($name): the program app was not chosen" "$log"
    grep -q "^codemap: crashes 1 kept, on 2 declarations$" "$log" || fail "codemap ($name): the kept crash is not on greet and main" "$log"
    grep -q "^codemap: selected tree/lib/helper.mlx/greet/$" "$log" || fail "codemap ($name): typing greet and Enter did not select it" "$log"
    grep -A1 "^codemap: selected tree/lib/helper.mlx/greet/$" "$log" | grep -q "^codemap: related out 1 in 2$" || fail "codemap ($name): greet should use count and be called from main and twice" "$log"
    grep -q "^codemap: selected $" "$log" || fail "codemap ($name): Escape did not clear the selection" "$log"
    grep -q "^codemap: selected tree/app/$" "$log" || fail "codemap ($name): the sidebar's app group was not selected" "$log"
    grep -A1 "^codemap: selected tree/app/$" "$log" | grep -q "^codemap: related out 1 in 1$" || fail "codemap ($name): the app group should depend on the lib group (and lib/bad.mlx on app)" "$log"
    # Its check box leaves lib out: its declarations, and the workarounds
    # in it; the crash (through main in app) stays.
    grep -q "^codemap: shown [0-9]* nodes, findings 0 0 [0-9]* 2 1 [0-9]* [0-9]* [0-9]* 2 [0-9]* 0 [0-9]*$" "$log" || fail "codemap ($name): at first everything is shown" "$log"
    grep -q "^codemap: shown [0-9]* nodes, findings 0 0 [0-9]* 0 1 [0-9]* [0-9]* [0-9]* 0 [0-9]* 0 [0-9]*$" "$log" || fail "codemap ($name): the lib group's check box did not leave it out" "$log"
    grep -q "^codemap: flying to tree/app/$" "$log" || fail "codemap ($name): the camera did not fly to the group still shown" "$log"
    if [[ -n "${have_git:-}" ]]; then
        grep -q "^codemap: trend 2 commits, counted 2$" "$log" || fail "codemap ($name): the workarounds of both commits (history) are not read" "$log"
        grep -q "^codemap: diff since HEAD~1: 0 added, 1 changed, 0 removed$" "$log" || fail "codemap ($name): greet should be changed since HEAD~1" "$log"
        grep -q "^codemap: shown [0-9]* nodes, findings 0 0 [0-9]* 2 1 [0-9]* [0-9]* [0-9]* 2 [0-9]* 0 1$" "$log" || fail "codemap ($name): the finding Changed should hold greet" "$log"
    fi
    # A query in the search: of the functions calling greet only main is
    # left (lib is left out, and the program app is shown).
    grep -q "^codemap: query kind:fn calls:greet: 1 declarations$" "$log" || fail "codemap ($name): the query in the search should leave main" "$log"
    # The bookmark keeps it as a view, the filter adds a word, the
    # heading Groups folds up and opens again.
    grep -q "^codemap: view saved Ansicht [0-9]*$" "$log" || fail "codemap ($name): the bookmark did not keep the query as a view" "$log"
    grep -q "^codemap: filter word " "$log" || fail "codemap ($name): the filter's first word was not added" "$log"
    grep -q "^codemap: section 0 folded$" "$log" && grep -q "^codemap: section 0 opened$" "$log" || fail "codemap ($name): the heading Groups did not fold up and open" "$log"
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

# The demo mode (tools/observatory_screenshots.sh makes the README's pictures
# with it): the app plays a script and writes its own frames, a shot and a
# recorded flight.
mkdir -p "$work/demo"
cat > "$work/check.demo" <<'DEMO'
settle
shot start
search kind:fn calls:greet
settle
shot query
escape
select lib/helper.mlx/greet
record flight 3 100
open app/main.mlx 5
wait 300
shot editor
hoverat 5 59
hoverat 5 65
open lib/helper.mlx 4
fold 4
wait 200
shot folded
hoverat 8 11
fold 4
open app/main.mlx 5
cursor 3 22
edit \nhel
complete
accept
edit .gre
complete
accept
signature
wait 200
shot completion
cursor 3 22
select-to 4 18
delete-selection
cursor 3 22
edit \nconst fresh = helper.Point.{ .x = 3, .y = 4 }
wait 800
hoverat 4 11
cursor 3 22
select-to 4 200
delete-selection
wait 800
open lib/usestd.mlx 3
cursor 3 26
edit \nstd.fs.op
complete
escape
cursor 3 26
select-to 4 200
delete-selection
open lib/extra.mlx 3
cursor 3 8
rename double
wait 300
blameat 1
new-file lib
wait 200
shot create
escape
new-folder lib tools
new-file lib/tools util
wait 300
open app/main.mlx 5
wait 300
cursor 5 32
definition
wait 300
cursor 5 12
edit undefinedName + 
save
wait 4000
shot problem
undo
save
wait 4000
open lib/drop.mlx 4
wait 300
shot droplens
chip rewrite
save
wait 3000
open lib/helper.mlx 4
wait 300
chip uses
wait 300
shot references
choose
wait 300
popup base
wait 200
shot history
popup none
branch-new feature
branches
wait 200
shot branches
popup none
branch-switch -
program app
build
settle
edit // a line for the commit\n
save
settle
popup commit
message Observatory check: a line
commit
settle
escape
view map
settle
quit
DEMO
# A std of the tree's own (std/src/std.mlx, as the compiler finds it),
# and a file that uses it, for completion after std.
mkdir -p "$tree/std/src"
printf 'pub const fs = @import("./fs.mlx")\n' > "$tree/std/src/std.mlx"
cat > "$tree/std/src/fs.mlx" <<'MLX'
// Opens the file at path.
pub fn open(path: []const u8) -> i32 { return 0 }
pub fn openAt(directory: i32, path: []const u8) -> i32 { return 0 }
fn hidden() -> i32 { return 0 }
MLX
cat > "$tree/lib/usestd.mlx" <<'MLX'
const std = @import("std")

pub fn useStd() -> void {
}
MLX
# A result bound only to be dropped, for the editor's rewrite.
cat > "$tree/lib/drop.mlx" <<'MLX'
const extra = @import("./extra.mlx")

pub fn drop() -> void {
    const ignored = extra.twice()
}
MLX
printf 'wait 120000\nclose\n' > "$work/hold.script"
runtime=$(mktemp -d)
XDG_RUNTIME_DIR=$runtime "$work/test-host" host-demo "$work/us.xkb" "$work/hold.script" > "$work/demo-host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$runtime/host-demo" ]] && break; sleep 0.1; done
status=0
env -i PATH="$PATH" HOME="$work" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-demo MLX_CANVAS=cpu MLX_CODEMAP_TRACE=1 \
    GIT_AUTHOR_NAME=check GIT_AUTHOR_EMAIL=check@mlx GIT_COMMITTER_NAME=check GIT_COMMITTER_EMAIL=check@mlx \
    MLX_COMPILER="$repo_root/$compiler" MLX_CODEMAP_DEMO="$work/check.demo" MLX_CODEMAP_DEMO_OUT="$work/demo" timeout 90 "$work/mlx-observatory" "$tree" > "$work/demo.log" 2>&1 || status=$?
kill "$host_pid" 2> /dev/null || true
wait "$host_pid" 2> /dev/null || true
rm -rf -- "$runtime"
[[ $status -eq 0 ]] || fail "codemap (demo): the app exited with status $status" "$work/demo.log"
for picture in start query flight-000 flight-001 flight-002; do
    head -c 15 "$work/demo/$picture.ppm" 2> /dev/null | grep -q "^P6$" || fail "codemap (demo): $picture.ppm is missing" "$work/demo.log"
done
grep -q "^codemap: query kind:fn calls:greet: 2 declarations$" "$work/demo.log" || fail "codemap (demo): the query step did not run" "$work/demo.log"
cmp -s "$work/demo/flight-000.ppm" "$work/demo/flight-002.ppm" && fail "codemap (demo): the recorded flight does not move" "$work/demo.log"
echo "ok   codemap (demo): a script played, its frames written"
# The editor: the file opened, F12 on greet opens its declaration, a
# name nobody declared is saved and the compiler's error lands on its
# line; undone and saved, the file is as it was and the error gone.
for picture in editor problem; do
    head -c 15 "$work/demo/$picture.ppm" 2> /dev/null | grep -q "^P6$" || fail "editor: $picture.ppm is missing" "$work/demo.log"
done
grep -q "^codemap: editor open $tree/app/main.mlx$" "$work/demo.log" || fail "editor: app/main.mlx was not opened" "$work/demo.log"
grep -q "^codemap: definition greet$" "$work/demo.log" && grep -q "^codemap: editor open $tree/lib/helper.mlx$" "$work/demo.log" \
    || fail "editor: F12 on greet did not open its declaration" "$work/demo.log"
grep -q "^codemap: saved lib/helper.mlx$" "$work/demo.log" || fail "editor: the file was not saved" "$work/demo.log"
grep -A3 "^codemap: saved lib/helper.mlx$" "$work/demo.log" | grep -q "^codemap: checked, problems [1-9]" || fail "editor: the compiler's error did not come back" "$work/demo.log"
grep -A4 "^codemap: demo undo$" "$work/demo.log" | grep -q "^codemap: checked, problems 0$" || fail "editor: the error did not go after undo and save" "$work/demo.log"
( cd "$tree" && git diff --quiet -- lib/helper.mlx ) 2> /dev/null || [[ -z "${have_git:-}" ]] || fail "editor: undo and save did not bring the file back" "$work/demo.log"
grep -q "^codemap: hover local const point in function main, app/main.mlx:4 | type Point: struct in lib/helper.mlx:10$" "$work/demo.log" \
    && grep -q "^codemap: hover field x of struct Point, lib/helper.mlx:11 | $" "$work/demo.log" \
    || fail "editor: the hover does not say where a local and a field belong" "$work/demo.log"
echo "ok   editor: open, go to a declaration, hover (where a name belongs), edit, save, the compiler's problems, undo"
# What the map knows, in the editor: the lens of greet lists its uses (a
# click on one opens it), the dropped result's line offers its rewrite.
grep -q "^codemap: rewritten here:     _ = extra.double()$" "$work/demo.log" && grep -q "^    _ = extra.double()$" "$tree/lib/drop.mlx" \
    || fail "editor: the rewrite on the workaround's line did not happen" "$work/demo.log"
grep -q "^codemap: references greet$" "$work/demo.log" && grep -q "^codemap: references count: 2$" "$work/demo.log" && grep -q "^codemap: reference opened " "$work/demo.log" \
    || fail "editor: the lens's uses did not list greet's two uses" "$work/demo.log"
echo "ok   editor: the lens (uses, a click lists them and opens one), a workaround's rewrite on its line"
# Folding: greet's body folded, the lines under it move up (the hover on
# count finds it where it now is), unfolded again.
grep -q "^codemap: folded 4$" "$work/demo.log" && grep -q "^codemap: hover const count, lib/helper.mlx:8 | $" "$work/demo.log" && grep -q "^codemap: unfolded 4$" "$work/demo.log" \
    || fail "editor: folding greet's body" "$work/demo.log"
echo "ok   editor: a function's body folds and opens again"
# Completion from the index: what is in scope (the import helper), the
# members after the dot (greet); the signature of the call typed.
grep -q "^codemap: completion first helper$" "$work/demo.log" && grep -q "^codemap: completed helper$" "$work/demo.log" \
    && grep -q "^codemap: completion first greet$" "$work/demo.log" && grep -q "^codemap: completed greet($" "$work/demo.log" \
    && grep -q "^codemap: signature greet$" "$work/demo.log" && grep -q "^codemap: signature at, argument 0$" "$work/demo.log" \
    || fail "editor: completion (helper, then greet after the dot) or the signature of greet(" "$work/demo.log"
grep -q "^codemap: completion 2, first $" "$work/demo.log" && grep -q "^codemap: completion first open$" "$work/demo.log" \
    || fail "editor: completion after std.fs. should offer open and openAt (not hidden)" "$work/demo.log"
echo "ok   editor: completion (in scope, after a dot, in std.fs), signature help"
# Read again while editing: a local just typed is known to the hover (its
# type too) a moment after, without saving.
grep -q "^codemap: index read again app/main.mlx$" "$work/demo.log" \
    && grep -q "^codemap: hover local const fresh in function main, app/main.mlx:4 | type Point: struct in lib/helper.mlx:10$" "$work/demo.log" \
    || fail "editor: the index did not read the edited text again" "$work/demo.log"
echo "ok   editor: the index reads the edited text again (a new local in the hover)"
# F2: twice renamed where it is declared and where it is used; the line
# number's blame (Git).
grep -q "^codemap: renamed 2 places in 2 files: double$" "$work/demo.log" && grep -q "^pub fn double() -> usize" "$tree/lib/extra.mlx" \
    || fail "editor: F2 did not rename twice in both files" "$work/demo.log"
if [[ -n "${have_git:-}" ]]; then
    grep -q "^codemap: blame [0-9a-f]\{7\} · check · today · one$" "$work/demo.log" || fail "editor: the blame of a line is not Git's" "$work/demo.log"
fi
echo "ok   editor: rename (F2) across files, the blame of a line"
# A new folder in lib, a new file in it: made, read, opened.
grep -q "^codemap: created folder lib/tools$" "$work/demo.log" && grep -q "^codemap: created file lib/tools/util.mlx$" "$work/demo.log" \
    && grep -q "^codemap: editor open $tree/lib/tools/util.mlx$" "$work/demo.log" && [[ -f "$tree/lib/tools/util.mlx" ]] \
    || fail "sidebar: a new folder and a new file in it" "$work/demo.log"
echo "ok   sidebar: a new folder, a new file in it (made, read, opened)"
# Build: the program app into the tree's mlx-out/bin; Commit: the edit
# with its message (when the tree is a Git work tree).
grep -q "^codemap: build done: Built app in [0-9.]* s: mlx-out/bin/app$" "$work/demo.log" && [[ -x "$tree/mlx-out/bin/app" ]] \
    || fail "build: the program app was not built" "$work/demo.log"
echo "ok   build: the chosen program, into mlx-out/bin"
if [[ -n "${have_git:-}" ]]; then
    grep -q "^codemap: commit done: exit 0$" "$work/demo.log" && [[ "$(git -C "$tree" log -1 --format=%s)" == "Observatory check: a line" ]] \
        || fail "commit: the edit was not committed with its message" "$work/demo.log"
    echo "ok   commit: the edit, with its message"
    # The history's graph (the tree's two commits), a new branch made
    # (feature) and back to the one before.
    grep -q "^codemap: history graph, commits 2$" "$work/demo.log" && grep -q "^codemap: branch made feature$" "$work/demo.log" \
        && grep -q "^codemap: branch switched -$" "$work/demo.log" && git -C "$tree" rev-parse --verify -q feature > /dev/null \
        && [[ "$(git -C "$tree" rev-parse --abbrev-ref HEAD)" != feature ]] \
        || fail "git: the history's graph, a new branch, back to the one before" "$work/demo.log"
    echo "ok   git: the commit graph, a new branch, switching back"
fi
# Escape in the editor keeps its file; the map reads the saved files
# again when it is shown.
sed -n '/^codemap: commit done/,$p' "$work/demo.log" | grep -q "^codemap: editor open" && fail "editor: Escape opened another file" "$work/demo.log"
sed -n '/^codemap: demo view map$/,$p' "$work/demo.log" | grep -q "^codemap: read again$" || fail "map: the saved files were not read again" "$work/demo.log"
echo "ok   map and editor: Escape stays in the file, the map follows the saved files"

# A German keyboard in the editor: AltGr+Q and Ctrl+Alt+Q type @, AltGr+7
# types {, the key US keyboards have / on types - (and does not find).
keys="$work/keys"
mkdir -p "$keys/tree/lib"
printf 'pub fn greet() -> u8 { return 1 }\n' > "$keys/tree/lib/a.mlx"
xkbcli compile-keymap --layout de > "$work/de.xkb"
cat > "$keys/app.script" << 'SCRIPT'
wait 5000
pointer 1040 25
press 272
release 272
type greet
wait 300
enter
wait 300
down 29
mods 4
down 24
up 24
up 29
mods 0
wait 800
down 100
mods 128
down 16
up 16
down 8
up 8
up 100
mods 0
down 29
down 56
mods 12
down 16
up 16
up 56
up 29
mods 0
down 53
up 53
wait 300
down 29
mods 4
down 31
up 31
up 29
mods 0
wait 500
close
SCRIPT
runtime=$(mktemp -d)
XDG_RUNTIME_DIR=$runtime "$work/test-host" host-keys "$work/de.xkb" "$keys/app.script" > "$keys/host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$runtime/host-keys" ]] && break; sleep 0.1; done
env -i PATH="$PATH" HOME="$keys" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-keys MLX_CANVAS=cpu MLX_CODEMAP_TRACE=1 \
    timeout 60 "$work/mlx-observatory" "$keys/tree" > "$keys/app.log" 2>&1 || fail "keys: the app failed" "$keys/app.log"
wait "$host_pid" || fail "keys: the test host failed" "$keys/host.log"
rm -rf -- "$runtime"
[[ "$(head -1 "$keys/tree/lib/a.mlx")" == 'pub fn @{@-greet() -> u8 { return 1 }' ]] || fail "keys: AltGr, Ctrl+Alt or - did not type (the file: $(head -1 "$keys/tree/lib/a.mlx"))" "$keys/app.log"
echo "ok   keys: AltGr and Ctrl+Alt type @ and {, the - key types - in the editor"
