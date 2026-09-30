#!/usr/bin/env bash
# The pictures of MLX Observatory's README (examples/mlx-observatory/screenshots),
# taken of this whole repository, with real data behind every finding:
#   - crashes kept by tests/support/crash_report.mlx (crashed three ways);
#   - profiles of mlx-observatory itself (its command line) under mlx-profile;
#   - built programs (mlx-observatory, mlx-lsp, mlx-files) for the machine code;
#   - the Git history (blame, and the workarounds of the last 20 commits).
# The app plays a demo script (MLX_CODEMAP_DEMO, see runDemo in
# examples/mlx-observatory/main.mlx) under tools/wayland-test-host, drawn on
# lavapipe (VK_DRIVER_FILES), and writes its own frames; Python with
# Pillow turns them into PNGs and the tour into an animated GIF.
#
#   tools/observatory_screenshots.sh [compiler] [output directory]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
out=${2:-examples/mlx-observatory/screenshots}
python3 -c "import PIL" 2> /dev/null || { echo "observatory_screenshots.sh: needs Python with Pillow (pip install pillow)" >&2; exit 2; }
command -v xkbcli > /dev/null || { echo "observatory_screenshots.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"' EXIT
home="$work/home"
state="$home/.local/state"
mkdir -p "$home" "$state" "$work/bin" "$work/frames" "$out"
say() { echo "== $*" >&2; }

say "building"
"$compiler" --quiet examples/mlx-observatory/main.mlx -o "$work/bin/mlx-observatory"
"$compiler" --quiet tools/mlx-lsp/main.mlx -o "$work/bin/mlx-lsp"
"$compiler" --quiet examples/mlx-files/main.mlx -o "$work/bin/mlx-files"
"$compiler" --quiet tools/profile/main.mlx -o "$work/mlx-profile"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet tests/support/crash_report.mlx -o "$work/crashy"
xkbcli compile-keymap --layout us > "$work/us.xkb"

say "crashes, profiles, history"
for how in index null deep; do XDG_STATE_HOME="$state" "$work/crashy" "$how" > /dev/null 2>&1 || true; done
for command in json copies workarounds; do
    XDG_STATE_HOME="$state" "$work/mlx-profile" "$work/bin/mlx-observatory" -C "$repo_root" "$command" > /dev/null 2>&1 || true
done
HOME="$home" "$work/bin/mlx-observatory" -C "$repo_root" churn > /dev/null
HOME="$home" "$work/bin/mlx-observatory" -C "$repo_root" history 20 > /dev/null 2>&1

# The editor on the hottest function of the profiles (scanFile): the
# heat, the code lens, a hover; then a name nobody declared, saved, and the
# compiler's error on its line, the commit list with that change; undone
# and saved again (the file is as it was), then Build of its program.
scan_file=tools/codemap/workarounds.mlx
git diff --quiet -- "$scan_file" || { echo "observatory_screenshots.sh: $scan_file has changes (the editor pictures edit it)" >&2; exit 2; }
# It was clean: whatever happens, it is again at the end.
trap '[[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"; git checkout -q -- "$scan_file"' EXIT
scan_line=$(grep -n "^fn scanFile(" "$scan_file" | cut -d: -f1)
use_line=$((scan_line + 2))
use_text=$(sed -n "${use_line}p" "$scan_file")
prefix=${use_text%%sourceOf*}
hover_column=$((${#prefix} + 3))
prefix=${use_text%%index_module*}
edit_column=$((${#prefix} + 1))

# The tour (a GIF), then a picture per feature. Each is its own run (they
# start from the same view).
cat > "$work/tour.demo" <<DEMO
settle
spin 220
record tour1 24 100
spin 0
search drawSidebar
record tour2 6 150
enter
record tour3 14 90
wait 300
select examples/mlx-observatory
record tour4 14 90
metric heat
record tour5 8 150
metric kind
finding crashes
record tour6 8 150
finding none
view editor
select $scan_file/scanFile
settle
record tour7 6 150
hoverat $use_line $hover_column
record tour8 8 150
quit
DEMO

cat > "$work/features.demo" <<'DEMO'
settle
wait 300
shot galaxy
search drawSidebar
wait 300
shot search
enter
settle
shot selected
escape
escape
select examples/mlx-observatory
settle
shot group
escape
only std
settle
shot only-std
all
programs open
program mlx-observatory
programs close
metric size
settle
shot program-machine-code
program all
metric churn
finding complex
settle
shot churn-complex
check complex off
metric risk
finding risky
settle
shot risk
check risky off
metric heat
finding hot
settle
shot heat
check hot off
metric kind
finding crashes
settle
shot crashes
workarounds open
finding workarounds
settle
shot workarounds
rewrite dropped
wait 300
shot rewrite
back
workarounds close
check workarounds off
finding copies
settle
shot copies
check copies off
finding untested
settle
shot untested
check untested off
finding structure
settle
shot structure
check structure off
finding none
settle
popup base
wait 300
shot base
base HEAD~3
settle
shot changed
check changed off
finding none
settle
popup filter
wait 300
shot filter
popup none
search kind:fn in:std/ uses:Allocator -untested
enter
settle
shot query
search @Untested std
enter
settle
fold groups
fold program
fold findings
fold color
shot views
quit
DEMO

cat > "$work/editor.demo" <<DEMO
settle
view editor
select $scan_file/scanFile
settle
wait 300
shot editor
hoverat $use_line $hover_column
wait 300
shot editor-hover
hoverat 1 1
cursor $use_line $edit_column
edit undefinedName + 
save
settle
wait 300
shot editor-problem
popup commit
message Workarounds: a faster scan
wait 300
shot commit
popup none
undo
save
settle
build
settle
shot build
view map
settle
shot map-after-editor
quit
DEMO

run_demo() {
    local name=$1
    local runtime
    runtime=$(mktemp -d)
    printf 'wait 900000\nclose\n' > "$work/hold.script"
    XDG_RUNTIME_DIR=$runtime "$work/test-host" host-shots "$work/us.xkb" "$work/hold.script" > "$work/host-$name.log" 2>&1 &
    local host=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-shots" ]] && break; sleep 0.1; done
    env -i PATH="$PATH" HOME="$home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=host-shots \
        VK_DRIVER_FILES="${VK_DRIVER_FILES:-/usr/share/vulkan/icd.d/lvp_icd.json}" LANG=en_US.UTF-8 MLX_CODEMAP_TRACE=1 MLX_CODEMAP_BINARIES="$work/bin" \
        MLX_CODEMAP_DEMO="$work/$name.demo" MLX_CODEMAP_DEMO_OUT="$work/frames" \
        timeout 900 "$work/bin/mlx-observatory" "$repo_root" > "$work/app-$name.log" 2>&1 || { cat "$work/app-$name.log" >&2; exit 1; }
    kill "$host" 2> /dev/null || true
    wait "$host" 2> /dev/null || true
    rm -rf -- "$runtime"
}

# MLX_SHOTS_ONLY=editor (tour, features): only that part.
for part in tour features editor; do
    [[ -z "${MLX_SHOTS_ONLY:-}" || "$MLX_SHOTS_ONLY" == "$part" ]] || continue
    say "the $part"
    run_demo "$part"
done
git diff --quiet -- "$scan_file" || { echo "observatory_screenshots.sh: $scan_file was left changed" >&2; exit 1; }

say "pictures"
python3 - "$work/frames" "$out" <<'PY'
import sys, os, glob
from PIL import Image
frames, out = sys.argv[1], sys.argv[2]
for path in sorted(glob.glob(os.path.join(frames, '*.ppm'))):
    name = os.path.basename(path)[:-4]
    if name.startswith('tour'):
        continue
    image = Image.open(path).convert('RGB')
    image.save(os.path.join(out, name + '.png'), optimize=True)
# The tour: 800 wide, one palette for all frames.
tour = [Image.open(p).convert('RGB') for p in sorted(glob.glob(os.path.join(frames, 'tour*.ppm')))]
if tour:
    width = 800
    height = round(tour[0].height * width / tour[0].width)
    small = [t.resize((width, height), Image.LANCZOS) for t in tour]
    palette = small[len(small) // 2].quantize(colors=128, method=Image.MEDIANCUT)
    gif = [s.quantize(palette=palette, dither=Image.FLOYDSTEINBERG) for s in small]
    gif[0].save(os.path.join(out, 'tour.gif'), save_all=True, append_images=gif[1:], duration=100, loop=0, optimize=True)
for name in sorted(os.listdir(out)):
    print('%-28s %8d bytes' % (name, os.path.getsize(os.path.join(out, name))))
PY
