#!/usr/bin/env bash
# The compositor's smaller protocols (projects/desktop/compositor), each
# with one client in a nested compositor of its own under
# tools/wayland-test-host. The script:
#   - crops and scales a buffer through a viewport (and a one-pixel
#     buffer stretched under it) alike on the CPU and on the GPU, sizes
#     the surface anew without a new buffer, says preferred scale 120
#     (fractional-scale), and ends a client whose crop leaves the buffer,
#     is a fraction of a pixel with no size, or that asks for a second
#     viewport (tools/wayland-viewport-test); draws the text cursor a
#     client names (cursor-shape) alike on the CPU and the GPU; says when
#     content was presented or discarded (presentation-time);
#   - opens a modal dialog centred over its window, keeps the keyboard
#     with it when the window is clicked (xdg-dialog), takes the
#     content-type and tearing-control hints, refuses a window as its own
#     parent and a second dialog object; parents a dialog through an
#     exported and imported handle (xdg-foreign), refuses unknown handles
#     and exports of surfaces that are no window;
#   - says idle after a time without input and resumed by input
#     (ext-idle-notify), held off by an idle inhibitor for those that
#     heed one;
#   - lets wl-copy and wl-paste set and read both selections without a
#     window (data-control), an X program on either side (skipped
#     without wl-clipboard).
#
# The Vulkan runs use lavapipe.
#
#   tools/check_wayland_protocols.sh [compiler] [lavapipe manifest]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
manifest=${2:-/usr/share/vulkan/icd.d/lvp_icd.json}
[[ -f "$manifest" ]] || { echo "check_wayland_protocols.sh: no Vulkan driver manifest at $manifest" >&2; exit 2; }
command -v xkbcli > /dev/null || { echo "check_wayland_protocols.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet tools/wayland-viewport-test/main.mlx -o "$work/viewport-test"
"$compiler" --quiet tools/x11-clipboard-test/main.mlx -o "$work/x11-clipboard-test"
xkbcli compile-keymap --layout us > "$work/us.xkb"
mkdir -p "$work/home" "$work/data" "$work/none"

fail() { echo "FAIL $1" >&2; [[ -f "$2" ]] && cat "$2" >&2; exit 1; }

# A client ($3, a command line) in a compositor of its own drawing on $2
# (cpu or vulkan), with Xwayland, nothing else; its first window is at
# (24, 24), and $name.script drives the host.
solo_run() {
    local name=$1 renderer=$2 command=$3 runtime status=0
    runtime=$(mktemp -d)
    printf '#!/bin/sh\nexec %s\n' "$command" > "$work/$name-client.sh"
    chmod +x "$work/$name-client.sh"
    XDG_RUNTIME_DIR=$runtime "$work/test-host" "host-$name" "$work/us.xkb" "$work/$name.script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$runtime/host-$name" ]] && break; sleep 0.1; done
    env -i PATH="$PATH" HOME="$work/home" XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY="host-$name" \
        XDG_DATA_HOME="$work/data" XDG_DATA_DIRS="$work/none" XDG_CONFIG_HOME="$work/viewport-config" VK_DRIVER_FILES="$manifest" \
        timeout 60 "$work/mlx-compositor" --verbose --no-fps --renderer "$renderer" --socket "nested-$name" \
        --launcher none --dock none --topbar none --run "$work/$name-client.sh" > "$work/$name.log" 2>&1 || status=$?
    wait "$host_pid" || fail "$name: the test host failed" "$work/$name-host.log"
    rm -rf -- "$runtime"
    [[ $status -eq 0 ]] || fail "$name: the compositor exited with status $status" "$work/$name.log"
    ! grep -q "crashed" "$work/$name.log" || fail "$name: a client crashed" "$work/$name.log"
}

# Viewports (viewport.mlx, buffers.mlx): the test window's 40 x 30
# checkerboard cropped to two squares (cyan and grey) scaled to 160 x 80,
# a one-pixel orange buffer stretched to 160 x 16 below it; the same
# pixels on the CPU and on the GPU, and again once the window's surface
# is sized 80 x 40 with the same buffer. Crops outside the buffer or of a
# fraction of a pixel with no size, and a second viewport, end the client.
viewports() {
    mkdir -p "$work/viewport-config/mlx"
    local renderer mode
    for renderer in cpu vulkan; do
        for mode in scale resize; do
            printf 'wait 2500\nshot %s\nclose\n' "$work/viewport-$mode-$renderer.ppm" > "$work/viewport-$mode-$renderer.script"
            solo_run "viewport-$mode-$renderer" "$renderer" "$work/viewport-test $mode"
            grep -q "^viewport-test: preferred scale 120$" "$work/viewport-$mode-$renderer.log" || fail "viewports: fractional-scale did not say 120" "$work/viewport-$mode-$renderer.log"
        done
        grep -q "^window: 80x40 at 24,24" "$work/viewport-resize-$renderer.log" || fail "viewports ($renderer): a new size without a new buffer did not size the window" "$work/viewport-resize-$renderer.log"
    done
    python3 - "$work" <<'PY' || fail "viewports: the cropped and scaled buffers do not look right" "$work/viewport-scale-cpu.log"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    return tuple(map(int, size.split())), pixels
work = sys.argv[1]
for mode in ("scale", "resize"):
    (width, height), cpu = load(f"{work}/viewport-{mode}-cpu.ppm")
    _, gpu = load(f"{work}/viewport-{mode}-vulkan.ppm")
    assert cpu == gpu, f"{mode}: the CPU and the GPU differ"
    def at(x, y): return tuple(cpu[(y * width + x) * 3:(y * width + x) * 3 + 3])
    side = 80 if mode == "scale" else 40
    # The squares end where the scale puts their edge, the bar under the
    # 160 x 80 the window had at first (it stays, a subsurface at 0, 80).
    assert at(24 + side // 2, 24 + side // 2) == (0, 255, 255), (mode, at(24 + side // 2, 24 + side // 2))
    assert at(24 + side - 1, 30) == (0, 255, 255), (mode, at(24 + side - 1, 30))
    assert at(24 + side, 30) == (128, 128, 128), (mode, at(24 + side, 30))
    assert at(24 + side * 2 - 13, 30) == (128, 128, 128), (mode, at(24 + side * 2 - 13, 30))
    assert at(24 + 80, 24 + 80 + 8) == (255, 128, 0), (mode, at(104, 112))
    assert at(24 + 159, 24 + 80 + 15) == (255, 128, 0), (mode, at(183, 119))
PY
    echo "ok   a viewport crops and scales a buffer, and stretches a one-pixel buffer, alike on the CPU and the GPU; preferred scale 120"
    echo "ok   a viewport sizes the surface anew without a new buffer"
    local expected
    for mode in outside fraction twice; do
        printf 'wait 2000\nclose\n' > "$work/viewport-$mode.script"
        solo_run "viewport-$mode" cpu "$work/viewport-test $mode"
        case $mode in
            outside) expected=2 ;;
            fraction) expected=1 ;;
            twice) expected=0 ;;
        esac
        grep -q "^viewport-test: error $expected$" "$work/viewport-$mode.log" || fail "viewports: the $mode case did not end the client with error $expected" "$work/viewport-$mode.log"
    done
    echo "ok   a crop outside the buffer, one of a fraction of a pixel with no size and a second viewport end the client"
    # cursor-shape: the window names the text cursor when the pointer
    # comes in (at 100, 60, over the cyan square); outside it the arrow.
    for renderer in cpu vulkan; do
        printf 'wait 2000\npointer 100 60\nwait 600\nshot %s\npointer 600 400\nwait 600\nshot %s\nclose\n' "$work/cursor-in-$renderer.ppm" "$work/cursor-out-$renderer.ppm" > "$work/viewport-cursor-$renderer.script"
        solo_run "viewport-cursor-$renderer" "$renderer" "$work/viewport-test cursor"
        grep -q "^cursor shape: 9$" "$work/viewport-cursor-$renderer.log" || fail "cursor-shape ($renderer): the text cursor was not taken" "$work/viewport-cursor-$renderer.log"
    done
    python3 - "$work" <<'PY' || fail "cursor-shape: the cursors do not look right" "$work/viewport-cursor-cpu.log"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    return tuple(map(int, size.split())), pixels
work = sys.argv[1]
for where in ("in", "out"):
    (width, height), cpu = load(f"{work}/cursor-{where}-cpu.ppm")
    _, gpu = load(f"{work}/cursor-{where}-vulkan.ppm")
    assert cpu == gpu, f"{where}: the CPU and the GPU differ"
    def at(x, y): return tuple(cpu[(y * width + x) * 3:(y * width + x) * 3 + 3])
    if where == "in":
        # The I-beam's stem at the pointer, a serif above, the square
        # beside it.
        assert at(100, 60) == (255, 255, 255), at(100, 60)
        assert at(98, 54) == (255, 255, 255), at(98, 54)
        assert at(97, 60) == (0, 255, 255), at(97, 60)
    else:
        assert at(601, 405) == (255, 255, 255), at(601, 405)
PY
    printf 'wait 2000\npointer 100 60\nwait 600\nclose\n' > "$work/viewport-badshape.script"
    solo_run viewport-badshape cpu "$work/viewport-test badshape"
    grep -q "^viewport-test: error 1$" "$work/viewport-badshape.log" || fail "cursor-shape: a shape there is not did not end the client" "$work/viewport-badshape.log"
    echo "ok   a client names the text cursor (cursor-shape), drawn alike on the CPU and the GPU; a shape there is not ends it"
    # presentation-time: two commits before a frame, the first's content
    # never shown.
    printf 'wait 2500\nclose\n' > "$work/viewport-presentation.script"
    solo_run viewport-presentation cpu "$work/viewport-test presentation"
    local log="$work/viewport-presentation.log"
    grep -q "^viewport-test: clock 1$" "$log" || fail "presentation: the clock is not CLOCK_MONOTONIC" "$log"
    grep -q "^viewport-test: feedback 1 discarded$" "$log" || fail "presentation: content replaced before a frame was not discarded" "$log"
    grep -q "^viewport-test: feedback 2 presented (refresh 16666666 ns)$" "$log" || fail "presentation: the shown content was not presented with the refresh period" "$log"
    echo "ok   presentation feedback: content replaced unseen is discarded, the shown content presented on the monotonic clock"
    # Dialogs (dialogs.mlx): the window's modal 120 x 60 dialog opens
    # centred over its 160 x 80 (at 44, 34), takes the keyboard and keeps
    # it when the window is clicked beside it, and stays above it; alike
    # on the CPU and the GPU. The window's hints (hints.mlx) are logged.
    for renderer in cpu vulkan; do
        printf 'wait 2500\nshot %s\npointer 30 100\npress 272\nrelease 272\nwait 600\nshot %s\nclose\n' "$work/dialog-open-$renderer.ppm" "$work/dialog-clicked-$renderer.ppm" > "$work/viewport-dialog-$renderer.script"
        solo_run "viewport-dialog-$renderer" "$renderer" "$work/viewport-test dialog"
        log="$work/viewport-dialog-$renderer.log"
        grep -q "^dialog: modal Viewport Dialog$" "$log" || fail "dialogs ($renderer): the dialog was not made modal" "$log"
        grep -q "^window: 120x60 at 44,34$" "$log" || fail "dialogs ($renderer): the dialog did not open centred over its window" "$log"
        [[ $(grep "^focus: " "$log" | tail -1) == "focus: Viewport Dialog" ]] || fail "dialogs ($renderer): clicking the window took the keyboard from its modal dialog" "$log"
        grep -q "^content: video Viewport Test$" "$log" || fail "hints ($renderer): the content type was not taken" "$log"
        grep -q "^tearing: async Viewport Test$" "$log" || fail "hints ($renderer): the tearing hint was not taken" "$log"
    done
    python3 - "$work" <<'PY' || fail "dialogs: the dialog does not look right" "$work/viewport-dialog-cpu.log"
import sys
def load(path):
    data = open(path, 'rb').read()
    header, size, depth, pixels = data.split(b'\n', 3)
    return tuple(map(int, size.split())), pixels
work = sys.argv[1]
for when in ("open", "clicked"):
    (width, height), cpu = load(f"{work}/dialog-{when}-cpu.ppm")
    _, gpu = load(f"{work}/dialog-{when}-vulkan.ppm")
    assert cpu == gpu, f"{when}: the CPU and the GPU differ"
    def at(x, y): return tuple(cpu[(y * width + x) * 3:(y * width + x) * 3 + 3])
    # The dialog's orange-red over the window's cyan, the window still
    # beside it, the dialog's frame in the focus colour.
    assert at(104, 64) == (255, 68, 0), (when, at(104, 64))
    assert at(30, 60) == (0, 255, 255), (when, at(30, 60))
    assert at(43, 50) == (90, 160, 255), (when, at(43, 50))
PY
    echo "ok   a modal dialog opens centred over its window, keeps the keyboard when the window is clicked and stays above it, alike on the CPU and the GPU"
    echo "ok   the content-type and tearing-control hints are taken"
    for mode in selfparent twodialogs; do
        printf 'wait 2000\nclose\n' > "$work/viewport-$mode.script"
        solo_run "viewport-$mode" cpu "$work/viewport-test $mode"
        case $mode in
            selfparent) expected=1 ;;
            twodialogs) expected=0 ;;
        esac
        grep -q "^viewport-test: error $expected$" "$work/viewport-$mode.log" || fail "dialogs: the $mode case did not end the client with error $expected" "$work/viewport-$mode.log"
    done
    echo "ok   a window named its own parent and a second dialog object of one window end the client"
    # Idle notifications (idle.mlx): told idled 800 ms after the last
    # input, resumed when the pointer moves, idled again; the one heeding
    # idle inhibitors stays quiet while the window holds one.
    local want
    for mode in idle idlefree; do
        printf 'wait 2500\npointer 100 60\nwait 1500\nclose\n' > "$work/viewport-$mode.script"
        solo_run "viewport-$mode" cpu "$work/viewport-test $mode"
        log="$work/viewport-$mode.log"
        want="input idled,input resumed,input idled"
        [[ $(grep "^viewport-test: input " "$log" | sed 's/^viewport-test: //' | paste -sd, -) == "$want" ]] || fail "idle ($mode): input idle notifications came wrong" "$log"
        [[ $mode == idlefree ]] && want="held idled,held resumed,held idled" || want=""
        [[ $(grep "^viewport-test: held " "$log" | sed 's/^viewport-test: //' | paste -sd, -) == "$want" ]] || fail "idle ($mode): the notification heeding inhibitors came wrong" "$log"
    done
    echo "ok   idle notifications: idled after the timeout without input, resumed by input; an idle inhibitor holds off those heeding it"
    # xdg-foreign (foreign.mlx): the window exported and imported by its
    # handle; the dialog made its child through the import opens centred
    # over it. A handle there is not is destroyed at once; exporting a
    # surface that is no window ends the client.
    printf 'wait 2500\nclose\n' > "$work/viewport-foreign.script"
    solo_run viewport-foreign cpu "$work/viewport-test foreign"
    log="$work/viewport-foreign.log"
    grep -q "^viewport-test: exported$" "$log" || fail "xdg-foreign: no handle for the window" "$log"
    grep -q "^foreign: Viewport Dialog over Viewport Test$" "$log" || fail "xdg-foreign: the import did not make the dialog the window's" "$log"
    grep -q "^window: 120x60 at 44,34$" "$log" || fail "xdg-foreign: the dialog did not open centred over the window" "$log"
    ! grep -q "^viewport-test: import destroyed$" "$log" || fail "xdg-foreign: a good handle's import was destroyed" "$log"
    printf 'wait 2000\nclose\n' > "$work/viewport-badhandle.script"
    solo_run viewport-badhandle cpu "$work/viewport-test badhandle"
    grep -q "^viewport-test: import destroyed$" "$work/viewport-badhandle.log" || fail "xdg-foreign: a handle there is not was not destroyed" "$work/viewport-badhandle.log"
    printf 'wait 2000\nclose\n' > "$work/viewport-badexport.script"
    solo_run viewport-badexport cpu "$work/viewport-test badexport"
    grep -q "^viewport-test: error 0$" "$work/viewport-badexport.log" || fail "xdg-foreign: exporting a surface that is no window did not end the client" "$work/viewport-badexport.log"
    echo "ok   xdg-foreign: a window imported by its handle becomes a dialog's parent; unknown handles and surfaces that are no window are refused"
}

# Clipboard tools (data_control.mlx): wl-copy and wl-paste, which speak
# wlr-data-control, set and read the selection and the primary selection
# without a window, an X program reads what wl-copy set and wl-paste what
# an X program owns, and wl-copy --clear leaves nothing.
data_control() {
    if ! command -v wl-copy > /dev/null || ! command -v wl-paste > /dev/null; then
        echo "skip data control: wl-clipboard is not installed"
        return
    fi
    cat > "$work/data-control-commands.sh" <<SCRIPT
#!/bin/sh
out=$work/data-control
wl-copy "from wl-copy"
sleep 1
wl-paste -n > "\$out-paste.txt" 2>&1
"$work/x11-clipboard-test" read UTF8_STRING > "\$out-x.txt" 2> /dev/null
printf 'second line' | wl-copy --primary
sleep 1
wl-paste -n --primary > "\$out-primary.txt" 2>&1
"$work/x11-clipboard-test" own 'hello from x11' > /dev/null 2>&1 &
sleep 2
wl-paste -n > "\$out-from-x.txt" 2>&1
wl-copy --clear
sleep 1
wl-paste -n > "\$out-cleared.txt" 2>&1
echo "data-control: done"
SCRIPT
    chmod +x "$work/data-control-commands.sh"
    printf 'wait 11000\nclose\n' > "$work/data-control.script"
    solo_run data-control cpu "$work/data-control-commands.sh"
    local log="$work/data-control.log"
    grep -q "^data-control: done" "$log" || fail "data control: the commands did not finish" "$log"
    [[ "$(cat "$work/data-control-paste.txt")" == "from wl-copy" ]] || fail "data control: wl-paste did not read wl-copy's text ($(cat "$work/data-control-paste.txt"))" "$log"
    [[ "$(cat "$work/data-control-x.txt")" == "from wl-copy" ]] || fail "data control: the X program did not read wl-copy's text ($(cat "$work/data-control-x.txt"))" "$log"
    [[ "$(cat "$work/data-control-primary.txt")" == "second line" ]] || fail "data control: wl-paste --primary did not read wl-copy --primary's text ($(cat "$work/data-control-primary.txt"))" "$log"
    [[ "$(cat "$work/data-control-from-x.txt")" == "hello from x11" ]] || fail "data control: wl-paste did not read the X program's text ($(cat "$work/data-control-from-x.txt"))" "$log"
    grep -q "Nothing is copied" "$work/data-control-cleared.txt" || fail "data control: wl-copy --clear left a selection ($(cat "$work/data-control-cleared.txt"))" "$log"
    grep -q "^data control: selection set" "$log" && grep -q "^data control: primary selection set" "$log" || fail "data control: wl-copy did not set the selections through data-control" "$log"
    ! grep -q "^map: \|^focus: " "$log" || fail "data control: wl-copy or wl-paste opened a window" "$log"
    echo "ok   wl-copy and wl-paste set and read the selection and the primary selection through data-control, with an X program both ways"
}

viewports
data_control
