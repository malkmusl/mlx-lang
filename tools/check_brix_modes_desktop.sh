#!/usr/bin/env bash
# The desktop's part of changing the display's mode while brixOS runs:
# the compositor (kepler, drm.brixos.mlx) follows the display's change,
# the settings program's Display page lists what the kernel offers and
# asks the compositor for a mode, the compositor asks the kernel and puts
# the question whether to keep it on the new picture
# (mode_question.mlx), the kernel takes it back when nobody keeps it and
# remembers it when it is kept. QEMU's standard VGA is the display
# (QEMU's adapter: the kernel sets any mode on it), the pointer is moved
# and clicked and the screen dumped through QEMU's monitor:
#
# 1. the desktop at the first size, 1024x768, its top bar across it and
#    its dock in the middle of its bottom;
# 2. the settings program opened on its General page, its Display page
#    opened with the pointer: it lists the modes the kernel offers
#    (twenty to choose, four more than the adapter's memory holds);
# 3. a mode chosen with the pointer (1920x1080): the screen has the new
#    size, the top bar is laid out across it and the dock in the middle
#    of its bottom, the settings window is inside it, and the question
#    whether to keep the mode is on it;
# 4. no answer: after fifteen seconds the kernel brings 1024x768 back,
#    the desktop is laid out for it again and the question is gone;
# 5. chosen again and kept with Enter: the kernel remembers it, the
#    question goes, twenty seconds later the screen is still 1920x1080;
#    then a mode asked for and kept by another program (root's `display
#    ask` and `display confirm`): the question goes all the same;
# 6. after a restart the system comes up in it: the kernel sets the mode
#    it remembers and the desktop starts at that size. (The system here
#    runs from its image, which keeps nothing across a restart; a system
#    on a disk keeps the kernel's file there, but cannot run the desktop:
#    a listener node, the Wayland socket, cannot be made on a brixfs
#    volume. So the next boot's image is made with the file the kernel
#    wrote, as the disk would have kept it.)
# 7. a person logged in (the login, its greeter, the account brix): the
#    login gave the display to the session (node_own), so the session's
#    compositor, which runs as brix, may change the mode: the Display
#    page asks, the screen changes and the question is there; Escape
#    brings the mode before back;
# 8. mlx-console in the desktop's place: a mode asked for by hand at the
#    serial shell (root's `display ask`) reaches it, and it draws its
#    cells on the new picture.
#
# MODES_SHOTS=DIR keeps the screens looked at, as PNG files, in DIR.
#
#   tools/check_brix_modes_desktop.sh [compiler]
set -euo pipefail
. "$(dirname "$0")/brix_classic_look.sh"

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
fonts=/usr/share/fonts/truetype/dejavu
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for QEMU's monitor"
    exit 0
fi
if [[ ! -f $fonts/DejaVuSansMono.ttf || ! -f $fonts/DejaVuSans.ttf ]]; then
    echo "skip: no DejaVu fonts in $fonts"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services" "$work/login" "$work/console"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
for program in shell/main init/brixos init/tool desktop/splash/brixos desktop/compositor/main desktop/terminal/main desktop/launcher/main desktop/dock/main desktop/topbar/main desktop/settings/main desktop/ipc/main desktop/login/main desktop/greeter/main console/brixos brix/tools/display/main; do
    name=${program//\//-}
    "$compiler" --quiet $brix "projects/$program.mlx" -o "$work/$name"
done
for program in ls cat echo sleep; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
classic_look "$work"
# The desktop as root (the image's desktop.service), its log on the
# serial line.
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    -e "\$a $classic_env" projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
programs=("flux=$work/shell-main" "impulse=$work/init-brixos" "impulsectl=$work/init-tool" "ignition=$work/desktop-splash-brixos"
    "kepler=$work/desktop-compositor-main" "vector=$work/desktop-terminal-main" "launch=$work/desktop-launcher-main" "orbit=$work/desktop-dock-main"
    "horizon=$work/desktop-topbar-main" "control=$work/desktop-settings-main" "display=$work/brix-tools-display-main"
    "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png"
    "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf")
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "${programs[@]}" \
    "${entries[@]}" "${classic_entries[@]}" "$work/bin/"
# The login in the desktop's place (the image's login and bus services).
cp projects/brix/settings/init/services/serial-shell.service "$work/login/"
sed -e '$a log = console' projects/brix/settings/init/services/ipcd.service > "$work/login/ipcd.service"
sed -e '$a log = console' projects/brix/settings/init/services/login.service > "$work/login/login.service"
entries=()
for service in "$work"/login/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix-login.img" "${programs[@]}" "synapse=$work/desktop-ipc-main" "sentinel=$work/desktop-login-main" "aurora=$work/desktop-greeter-main" \
    "settings/users=projects/brix/settings/users" "settings/groups=projects/brix/settings/groups" "settings/shadow=projects/brix/settings/shadow" \
    "share/wayland-sessions/brix.desktop=projects/brix/settings/share/wayland-sessions/brix.desktop" "${entries[@]}" "$work/bin/"
# mlx-console in the desktop's place (the image's console.service).
cp projects/brix/settings/init/services/serial-shell.service "$work/console/"
sed -e '/^disabled = yes$/d' -e '$a log = console' projects/brix/settings/init/services/console.service > "$work/console/console.service"
entries=()
for service in "$work"/console/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix-console.img" "flux=$work/shell-main" "impulse=$work/init-brixos" "impulsectl=$work/init-tool" \
    "mlx-console=$work/console-brixos" "display=$work/brix-tools-display-main" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "${entries[@]}" "$work/bin/"
echo "ok   the kernel, the desktop, the settings program, the login, mlx-console, the display tool and three boot images build"

# QEMU with image $1, its serial line the root shell's, its monitor on
# the socket.
boot() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 512M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -smp 2 -vga std -serial stdio \
        -kernel "$1" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# (Fifteen seconds of the system's clock are waited for, on a host whose
# processors are shared: the tries are many.)
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -ac -- "$1" || true) < want )); do
        (( tries++ < 1500 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
type_line() {
    printf '%s\r' "$1" >&3
}
monitor() {
    python3 - "$work/monitor" "$@" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(0.5)
def drain():
    try:
        while s.recv(4096): pass
    except socket.timeout:
        pass
drain()
for command in sys.argv[2:]:
    s.sendall((command + "\n").encode())
    time.sleep(0.2)
    drain()
PY
}
# The pointer at $1 $2 of the screen, whatever it was at: into the top
# left corner first (the compositor keeps it on the screen), then out to
# there in steps the PS/2 mouse's packets hold.
pointer() {
    local x=$1 y=$2 moves=("mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250" "mouse_move -250 -250")
    while (( x > 0 || y > 0 )); do
        local dx=$(( x > 200 ? 200 : x )) dy=$(( y > 200 ? 200 : y ))
        moves+=("mouse_move $dx $dy")
        x=$(( x - dx ))
        y=$(( y - dy ))
    done
    monitor "${moves[@]}"
    sleep 0.3
}
click() {
    pointer "$1" "$2"
    monitor "mouse_button 1" "mouse_button 0"
}
# The screen dumped (as $1, kept as a PNG under MODES_SHOTS) and looked at:
# its size; light pixels (above 170 in every part: text, the clock, the
# dock's symbols) at the right end of the top bar and in the dock's place
# in the middle of the bottom; the accent's blue (the classic look's
# #5aa0ff: the question's Keep button, the focused window's title bar) on
# the question's Keep button and anywhere. `seen` holds it as
# "WIDTH HEIGHT BAR DOCK KEEP BLUE".
seen=
look() {
    local name=$1 tries=0
    while true; do
        rm -f "$work/screen.ppm"
        monitor "screendump $work/screen.ppm"
        sleep 0.4
        seen=$(python3 - "$work/screen.ppm" "${MODES_SHOTS:-}" "$name" <<'PY' 2>&1
import os, struct, sys, zlib
try:
    data = open(sys.argv[1], "rb").read()
except OSError:
    sys.exit("no dump")
parts = data.split(b"\n", 3)
if parts[0] != b"P6" or len(parts) < 4: sys.exit("no PPM")
w, h = map(int, parts[1].split())
px = parts[3]
if len(px) < w * h * 3: sys.exit("a dump cut short")
# (Every `step`th pixel across and down: the whole screen is looked at
# in samples, quickly.)
# (While the question is up the rest of the screen is dimmed: light is
# then what is light under the dimming.)
light = 170
def count(left, top, wide, high, kind, step=1):
    n = 0
    for y in range(max(top, 0), min(top + high, h), step):
        row = y * w * 3
        for x in range(max(left, 0), min(left + wide, w), step):
            i = row + x * 3
            r, g, b = px[i], px[i + 1], px[i + 2]
            if kind == "light" and r > light and g > light and b > light: n += 1
            if kind == "blue" and abs(r - 0x5a) < 30 and abs(g - 0xa0) < 30 and b > 220: n += 1
    return n * step * step
cx, cy = (w - 440) // 2, (h - 132) // 2
keep = count(cx + 440 - 22 - 112, cy + 132 - 22 + 4 - 30, 112, 30, "blue")
if keep > 500: light = 90
bar = count(w - 220, 0, 220, 28, "light")
dock = count((w - 317) // 2, h - 80, 317, 80, "light")
blue = count(0, 0, w, h, "blue", 4)
if sys.argv[2]:
    os.makedirs(sys.argv[2], exist_ok=True)
    raw = b"".join(b"\x00" + px[y * w * 3:(y + 1) * w * 3] for y in range(h))
    chunk = lambda k, v: struct.pack(">I", len(v)) + k + v + struct.pack(">I", zlib.crc32(k + v) & 0xffffffff)
    open(os.path.join(sys.argv[2], sys.argv[3] + ".png"), "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
print(w, h, bar, dock, keep, blue)
PY
        ) && return 0
        (( tries++ < 20 )) || fail "the screen: $seen"
        sleep 0.5
    done
}
# Looks until the screen is $2 by $3 and what $4 says holds (a test of
# the numbers in `seen`, as w h bar dock keep blue); $1 names it.
expect() {
    local name=$1 width=$2 height=$3 test=$4 tries=0
    while true; do
        look "$name"
        local w h bar dock keep blue
        read -r w h bar dock keep blue <<< "$seen"
        if (( w == width && h == height )) && eval "$test"; then return 0; fi
        (( tries++ < 40 )) || fail "$name: the screen is ${w}x$h (bar $bar, dock $dock, keep $keep, blue $blue light or blue pixels), not ${width}x$height with $test"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.5
    done
}
# Where the settings window puts things, the window being the first one
# (at 23,30 of the screen) on QEMU's 1024x768: its sidebar's Display row,
# and on the Display page the row "1920 × 1080, 60 Hz" (the seventh of
# the modes it lists, which the kernel lists the largest first).
sidebar_display=(88 123)
row_1920=(255 529)

# ---- 1. the desktop at its first size ----
boot "$work/brix.img"
wait_for '^layer: horizon 1024x30 at 0,0 in layer 2$'
wait_for '^layer: orbit 317x150 at 353,618 in layer 2$'
wait_for '^display: another mode is asked for at /temporary/wayland-mlx.modes$'
wait_for '^root@brix:[^#]*# '
expect desktop-1024x768 1024 768 '(( bar > 20 && dock > 50 && keep == 0 ))'
echo "ok   the desktop at 1024x768, the top bar across it and the dock in the middle of its bottom ($seen)"

# ---- 2. the Display page, opened with the pointer ----
type_line 'XDG_RUNTIME_DIR=/temporary WAYLAND_DISPLAY=wayland-mlx control &'
wait_for 'map: Settings$'
sleep 3
click "${sidebar_display[@]}"
wait_for "control: display: 1024x768 at 60 Hz runs (QEMU's adapter); 20 modes to choose, 4 not available; set at once\$"
sleep 2
expect page-1024x768 1024 768 '(( keep == 0 ))'
echo "ok   the settings program's Display page, opened with the pointer, lists what the kernel offers: 20 modes to choose, 4 not available (more than the adapter's memory holds)"

# ---- 3. a mode chosen with the pointer ----
click "${row_1920[@]}"
wait_for 'control: display: asked for 1920x1080: Set: keep it or go back on the screen.$'
asked_at=$(date +%s)
output | grep -aq '^brixOS: display 0: it runs 1920x1080 at 60.00 Hz now (before: 1024x768 at 60.00 Hz)' || fail "the kernel did not set 1920x1080"
wait_for '^output: 1024x768 is 1920x1080 now; work area 1920x978 at 0,30$'
wait_for '^layer: horizon configured 1920x30$'
wait_for '^display: asks whether to keep 1920x1080 ([0-9]* s left)$'
kept=$(output | grep -a '^output: Settings kept at ' | tail -n 1)
[[ $kept =~ kept\ at\ ([0-9]+),([0-9]+),\ ([0-9]+)x([0-9]+) ]] || fail "the compositor did not say where the settings window went"
(( BASH_REMATCH[1] + BASH_REMATCH[3] <= 1920 && BASH_REMATCH[2] + BASH_REMATCH[4] <= 1080 - 72 && BASH_REMATCH[2] >= 30 )) || fail "the settings window is not inside the new work area: $kept"
expect chosen-1920x1080 1920 1080 '(( bar > 20 && dock > 50 && keep > 1500 ))'
echo "ok   1920x1080 chosen with the pointer: the screen is that size, the top bar across it and the dock in the middle of its bottom, the settings window inside it (${kept#output: Settings }), the question whether to keep it on it ($seen)"

# ---- 4. no answer ----
wait_for '^brixOS: display 0: nobody confirmed 1920x1080 at 60.00 Hz within 15 seconds: the mode before is back, 1024x768 at 60.00 Hz$'
took=$(( $(date +%s) - asked_at ))
(( took >= 12 )) || fail "the mode went back after $took seconds, not fifteen"
wait_for '^output: 1920x1080 is 1024x768 now; work area 1024x666 at 0,30$'
wait_for '^layer: horizon configured 1024x30$'
expect timed-out-1024x768 1024 768 '(( bar > 20 && dock > 50 && keep == 0 ))'
echo "ok   no answer: after $took seconds the kernel brought 1024x768 back, the desktop is laid out for it again and the question is gone ($seen)"

# ---- 5. chosen again and kept ----
sleep 2
click "${row_1920[@]}"
wait_for 'control: display: asked for 1920x1080: Set: keep it or go back on the screen.$' 2
wait_for '^display: asks whether to keep 1920x1080 ([0-9]* s left)$' 2
expect asked-again-1920x1080 1920 1080 '(( keep > 1500 ))'
monitor "sendkey ret"
wait_for '^display: kept$'
wait_for '^brixOS: display 0: 1920x1080 at 60.00 Hz is confirmed'
sleep 20
output | grep -aq 'nobody confirmed 1920x1080 at 60.00 Hz within 15 seconds' && (( $(output | grep -ac 'nobody confirmed 1920x1080') > 1 )) && fail "the kept mode was taken back"
expect kept-1920x1080 1920 1080 '(( bar > 20 && dock > 50 && keep < 200 ))'
type_line 'cat /system/settings/display-modes; echo shown'
wait_for '^shown$'
kept_line=$(output | grep -a '^mode 1234:1111 [0-9a-f]\{16\} 1920x1080@60000$' | tail -n 1)
[[ -n $kept_line ]] || fail "/system/settings/display-modes does not hold 1920x1080@60000"
echo "ok   chosen again and kept with Enter: the kernel remembers it, the question is gone, twenty seconds later the screen is still 1920x1080 ($seen)"
# (A mode kept by another program changes no picture: the compositor sees
# that nothing waits any more and takes its question away.)
type_line 'display ask 1280x800; echo asked $?'
wait_for '^asked 0$'
wait_for '^display: asks whether to keep 1280x800 ([0-9]* s left)$'
expect other-asked-1280x800 1280 800 '(( keep > 1500 ))'
type_line 'display confirm; echo confirmed $?'
wait_for '^confirmed 0$'
expect other-kept-1280x800 1280 800 '(( keep < 200 && bar > 20 ))'
type_line 'cat /temporary/wayland-mlx.display; echo told'
wait_for '^told$'
output | grep -aq '^pending = ' && fail "the compositor's file still says a mode waits to be kept after another program kept it"
stop
echo "ok   a mode asked for and kept by another program (root's display tool): the question goes once the kernel has nothing pending, and the file says so ($seen)"

# ---- 6. after a restart ----
printf '%s\n' "$kept_line" > "$work/display-modes"
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix-kept.img" "${programs[@]}" "settings/display-modes=$work/display-modes" "${entries[@]}" "${classic_entries[@]}" "$work/bin/"
boot "$work/brix-kept.img"
wait_for '^brixOS: display 0: the mode remembered for this monitor is set: 1920x1080'
wait_for '^layer: horizon 1920x30 at 0,0 in layer 2$'
wait_for '^root@brix:[^#]*# '
expect restarted-1920x1080 1920 1080 '(( bar > 20 && dock > 50 && keep == 0 ))'
stop
echo "ok   after a restart the kernel sets the mode it remembers and the desktop comes up in it: 1920x1080, its top bar and dock laid out for it ($seen)"

# ---- 7. a person's session ----
boot "$work/brix-login.img"
wait_for '^login: greeter started$'
wait_for '^root@brix:[^#]*# '
sleep 3
for key in b r i x ret; do monitor "sendkey $key"; sleep 0.3; done
wait_for '^login: session started$'
sleep 6
type_line 'XDG_RUNTIME_DIR=/temporary/users/1000 WAYLAND_DISPLAY=wayland-mlx control display &'
wait_for "control: display: 1024x768 at 60 Hz runs (QEMU's adapter); 20 modes to choose, 4 not available; set at once\$"
sleep 3
click "${row_1920[@]}"
wait_for 'control: display: asked for 1920x1080: '
output | grep -aq 'control: display: asked for 1920x1080: Set: keep it or go back on the screen.$' || fail "the session's compositor (user brix) could not change the mode: $(output | grep -a 'control: display: asked for' | tail -n 1)"
output | grep -aq '^brixOS: display 0: it runs 1920x1080 at 60.00 Hz now (before: 1024x768 at 60.00 Hz)' || fail "the kernel did not set 1920x1080 for the session"
expect session-asked-1920x1080 1920 1080 '(( keep > 500 && bar > 20 && dock > 50 ))'
monitor "sendkey esc"
wait_for '^brixOS: display 0: 1920x1080 at 60.00 Hz is taken back: the mode before is back, 1024x768 at 60.00 Hz$'
expect session-back-1024x768 1024 768 '(( keep == 0 ))'
stop
echo "ok   a person's session: the login gave the display to brix, so the session's compositor (running as brix) changed the mode the Display page asked for; Escape on the question brought 1024x768 back"

# ---- 8. mlx-console ----
boot "$work/brix-console.img"
wait_for 'mlx-console: 1024x768, [0-9]*x[0-9]* cells$'
wait_for '^root@brix:[^#]*# '
sleep 2
type_line 'display ask 800x600; echo asked $?'
wait_for '^asked 0$'
wait_for 'mlx-console: the display is 800x600 now, [0-9]*x[0-9]* cells$'
expect console-800x600 800 600 'true'
read -r w h bar dock keep blue <<< "$seen"
type_line 'display back; echo back $?'
wait_for '^back 0$'
wait_for 'mlx-console: the display is 1024x768 now, [0-9]*x[0-9]* cells$'
expect console-1024x768 1024 768 'true'
stop
echo "ok   mlx-console follows a mode asked for by hand: its cells on the 800x600 picture, then on 1024x768 again"
echo PASS
