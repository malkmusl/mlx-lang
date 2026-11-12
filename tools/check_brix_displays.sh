#!/usr/bin/env bash
# brixOS on a machine with more than one graphics adapter: every display
# the firmware offers is the system's, not only its first.
#
# The EFI loader hands every graphics output with a picture of its own to
# the kernel (the first in the boot info's fields, as ever; the others in
# a page of records) and sets each one's mode; the kernel names them in
# its log (which is which: size, memory, the PCI adapter), draws its
# console on all of them, and gives programs the main one as
# /devices/framebuffer and the others as /devices/framebuffer1 and on;
# the boot screen, the desktop and the virtual console show the main
# display's picture on the others too, fitted to their size (std.displays).
#
# Here: OVMF in QEMU with two adapters whose firmware drivers each offer a
# graphics output with a framebuffer, of different sizes: a Cirrus VGA
# (800x600; the firmware's first) and a second standard VGA (1280x800).
# (Found to work with QEMU 8.2 and its OVMF: any two of VGA,
# secondary-vga, bochs-display, cirrus-vga, virtio-vga; all but the
# Cirrus come up in 1280x800. A virtio-gpu-pci's output only takes
# blocks: it has no framebuffer, and the loader passes it over, as it
# does the console's own output in front of the two.) Both adapters'
# screens are dumped through QEMU's monitor:
#
# 1. the kernel alone: its log names both displays, a line each, and says
#    what the firmware offered; the boot log is on both screens; a program
#    reads both nodes' geometry;
# 2. the boot screen (ignition) stays on both, the same picture, fitted;
# 3. the desktop (kepler) is on both, the same picture, fitted (bars above
#    and below on the 800x600 one); a window opened and a line typed
#    arrive on both;
# 4. an image whose settings/display names the Cirrus as the main display
#    ("main 1013:00b8"): it is /devices/framebuffer now, and the virtual
#    console (mlx-console) on it is on the larger display too, made larger;
# 5. monitors that say their own mode (a test image's loader gives the
#    firmware an EDID for its outputs, 3840x1080, as tools/check_brix_efi.sh
#    does): each display has its monitor's word and its own mode set (the
#    second VGA the nearest it has, 1920x1080; the Cirrus stays), the
#    kernel says so for each, and a program reads the further display's
#    shape;
# 6. one adapter with a framebuffer beside one without (virtio-gpu-pci):
#    one display, as before, and the outputs passed over are said;
# 7. the same two the other way round (the firmware's first output is the
#    one without a framebuffer): the adapter behind it is the display,
#    and every output is accounted for.
#
#   tools/check_brix_displays.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
fonts=/usr/share/fonts/truetype/dejavu
firmware=""
for candidate in /usr/share/ovmf/OVMF.fd /usr/share/qemu/OVMF.fd /usr/share/OVMF/OVMF.fd /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd; do
    [[ -f $candidate ]] && firmware=$candidate && break
done
if ! command -v qemu-system-x86_64 > /dev/null || [[ -z $firmware ]]; then
    echo "skip: no qemu-system-x86_64 or OVMF"
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
for device in cirrus-vga secondary-vga virtio-gpu-pci; do
    if ! qemu-system-x86_64 -device help 2> /dev/null | grep -q "name \"$device\""; then
        echo "skip: this QEMU has no $device"
        exit 0
    fi
done
work=$(mktemp -d)
qemu_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
serial=$work/serial.txt
fail() { echo "FAIL $1" >&2; [[ -f "$serial" ]] && cat -v "$serial" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/ignition"
"$compiler" --quiet $brix projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet $brix projects/desktop/terminal/main.mlx -o "$work/vector"
"$compiler" --quiet $brix projects/desktop/launcher/main.mlx -o "$work/launch"
"$compiler" --quiet $brix projects/desktop/dock/main.mlx -o "$work/orbit"
"$compiler" --quiet $brix projects/desktop/topbar/main.mlx -o "$work/horizon"
"$compiler" --quiet $brix projects/console/brixos.mlx -o "$work/mlx-console"
for program in cat echo head base64 sleep; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
echo "ok   the kernel, brix-image, impulse, ignition, kepler and its helpers, mlx-console build"

# The two adapters: the Cirrus is the firmware's first.
two=(-vga none -device cirrus-vga,id=video0 -device secondary-vga,id=video1)

# An image on its FAT disk, started by OVMF with the display adapters
# given; its serial line is the shell's.
start() {
    local esp=$1
    shift
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$serial"
    qemu-system-x86_64 -m 1G -cpu max -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -serial stdio -net none \
        -bios "$firmware" -drive "format=raw,file=fat:rw:$esp" "$@" < "$work/input" > "$serial" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$serial" | tr '\r' '\n'; }
# (The firmware takes its time where the processor is emulated.)
wait_for() {
    local tries=0
    while ! output | grep -aq -- "$1"; do
        (( tries++ < 2400 )) || fail "no \"$1\" on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
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
    time.sleep(0.15)
    drain()
PY
}
# Both adapters' screens, dumped one after the other: $1 the Cirrus's
# (video0), $2 the other's (video1).
dump() {
    rm -f "$1" "$2"
    monitor "screendump $1 video0" "screendump $2 video1"
    sleep 0.3
}

# What the pictures are compared by (PPM dumps, read by hand):
#   console A B        both show the kernel's console with the same first
#                      two lines (the top 32 rows of pixels, 640 across,
#                      the same on both; white on black and nothing else:
#                      no firmware's picture left)
#   fitted MAIN OTHER  OTHER shows MAIN's picture fitted to its size: as
#                      large as it goes in MAIN's shape, in the middle,
#                      black beside it; compared by the mean colours of
#                      40 by 30 parts of the picture (their edges fall
#                      between pixels on one of the two: a part's mean may
#                      differ by a little where a sharp edge runs through)
#   differ A B         A and B are not the same picture (parts as above)
#   blue FILE          how many pixels are the focused window's blue
cat > "$work/pictures.py" <<'PY'
import sys
def ppm(path):
    data = open(path, "rb").read()
    parts = data.split(b"\n", 3)
    if parts[0] != b"P6" or len(parts) < 4: raise ValueError("no PPM")
    w, h = map(int, parts[1].split())
    if len(parts[3]) < w * h * 3: raise ValueError("a dump cut short")
    return w, h, parts[3]
def fit(pw, ph, w, h):
    wide, high = pw * h, ph * w
    if wide >= high:
        fh = max(1, min(h, (h * high + wide // 2) // wide))
        return 0, (h - fh) // 2, w, fh
    fw = max(1, min(w, (w * wide + high // 2) // high))
    return (w - fw) // 2, 0, fw, h
COLS, ROWS = 40, 30
def parts(w, h, px, x0, y0, fw, fh):
    out = []
    for r in range(ROWS):
        top, bottom = y0 + r * fh // ROWS, y0 + (r + 1) * fh // ROWS
        sums = [[0, 0, 0] for _ in range(COLS)]
        for y in range(top, bottom):
            row = px[y * w * 3:(y + 1) * w * 3]
            for c in range(COLS):
                left, right = x0 + c * fw // COLS, x0 + (c + 1) * fw // COLS
                piece = row[left * 3:right * 3]
                sums[c][0] += sum(piece[0::3]); sums[c][1] += sum(piece[1::3]); sums[c][2] += sum(piece[2::3])
        for c in range(COLS):
            left, right = x0 + c * fw // COLS, x0 + (c + 1) * fw // COLS
            area = max(1, (bottom - top) * (right - left))
            out.append([v / area for v in sums[c]])
    return out
def distance(a, b):
    total = far = 0
    for p, q in zip(a, b):
        d = max(abs(p[k] - q[k]) for k in range(3))
        total += d
        if d > 20: far += 1
    return total / len(a), far
what = sys.argv[1]
if what == "console":
    aw, ah, a = ppm(sys.argv[2]); bw, bh, b = ppm(sys.argv[3])
    lit = 0
    for y in range(32):
        ra, rb = a[y * aw * 3:y * aw * 3 + 640 * 3], b[y * bw * 3:y * bw * 3 + 640 * 3]
        if ra != rb: sys.exit("the first two lines differ at row %d" % y)
        lit += sum(1 for v in ra[0::3] if v > 200)
    if lit < 600: sys.exit("no text in the first two lines (%d pixels lit)" % lit)
    for name, (w, h, px) in (("first", (aw, ah, a)), ("second", (bw, bh, b))):
        other = sum(1 for v in px if v != 0 and v != 255)
        if other: sys.exit("the %s screen has %d values that are neither black nor white: not the kernel's console alone" % (name, other))
    print("%dx%d and %dx%d, %d pixels of text in the first two lines" % (aw, ah, bw, bh, lit))
elif what == "fitted":
    mw, mh, m = ppm(sys.argv[2]); ow, oh, o = ppm(sys.argv[3])
    x0, y0, fw, fh = fit(mw, mh, ow, oh)
    for y in range(oh):
        row = o[y * ow * 3:(y + 1) * ow * 3]
        outside = row if y < y0 or y >= y0 + fh else row[:x0 * 3] + row[(x0 + fw) * 3:]
        if outside.count(0) != len(outside): sys.exit("row %d beside the picture (%dx%d at %d,%d of %dx%d) is not black" % (y, fw, fh, x0, y0, ow, oh))
    main, other = parts(mw, mh, m, 0, 0, mw, mh), parts(ow, oh, o, x0, y0, fw, fh)
    flat = max(max(p[k] for p in main) - min(p[k] for p in main) for k in range(3))
    if flat < 20: sys.exit("the main display's picture is one colour: nothing to compare")
    mean, far = distance(main, other)
    if mean > 2.5 or far > len(main) // 50: sys.exit("not the same picture: its parts differ by %.1f on average, %d of %d by more than 20" % (mean, far, len(main)))
    print("%dx%d shown as %dx%d at %d,%d of %dx%d, parts differ by %.2f on average" % (mw, mh, fw, fh, x0, y0, ow, oh, mean))
elif what == "differ":
    aw, ah, a = ppm(sys.argv[2]); bw, bh, b = ppm(sys.argv[3])
    if (aw, ah) != (bw, bh): sys.exit("not of one size")
    x0, y0, fw, fh = (int(v) for v in sys.argv[4:8]) if len(sys.argv) > 4 else (0, 0, aw, ah)
    mean, far = distance(parts(aw, ah, a, x0, y0, fw, fh), parts(bw, bh, b, x0, y0, fw, fh))
    if far < 10: sys.exit("the same picture (%d parts differ)" % far)
    print(far)
elif what == "blue":
    w, h, px = ppm(sys.argv[2])
    print(sum(1 for k in range(0, w * h * 3, 3) if abs(px[k] - 0x5a) < 28 and abs(px[k + 1] - 0xa0) < 28 and px[k + 2] > 215))
PY
pictures() { python3 "$work/pictures.py" "$@"; }
# Waits until the second standard VGA's screen ($2: video1) and the
# Cirrus's ($1: video0) show the same picture, the one named first in
# `fitted`'s order ($3: "main-first": video1 is the main display); what
# `fitted` says of it in `seen`.
seen=
same_on_both() {
    local cirrus=$1 other=$2 main=$3 what=$4 tries=0 said=
    while true; do
        dump "$cirrus" "$other"
        if [[ $main == other ]]; then
            said=$(pictures fitted "$other" "$cirrus" 2>&1) && break
        else
            said=$(pictures fitted "$cirrus" "$other" 2>&1) && break
        fi
        (( tries++ < 60 )) || fail "$what: $said"
        sleep 0.5
    done
    seen=$said
}

# ---- 1. the kernel alone ----
mkdir -p "$work/plain/EFI/BOOT"
"$work/brix-image" "$work/kernel" "$work/plain/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "$work/bin/"
start "$work/plain" "${two[@]}"
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
output | grep -aq '^brixOS: display 0: 800x600 at 0x[0-9a-f]*, PCI [0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]\.[0-7] 1013:00b8, /devices/framebuffer1$' || fail "the kernel's log does not name the Cirrus's display (display 0: 800x600, PCI 1013:00b8, /devices/framebuffer1)"
output | grep -aq '^brixOS: display 1: 1280x800 at 0x[0-9a-f]*, PCI [0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]\.[0-7] 1234:1111, /devices/framebuffer, the main display (the largest picture)$' || fail "the kernel's log does not name the second VGA's display as the main one (display 1: 1280x800, PCI 1234:1111, /devices/framebuffer, the largest picture)"
output | grep -aq '^brixOS: display 1280x800 pitch 5120 format 1 at 0x[0-9a-f]* (the firmware.s)$' || fail "the main display's line is not the 1280x800 one's"
output | grep -aq '^brixOS: the firmware offers 3 graphics outputs: 2 displays, 1 without pixels the kernel can use (passed over)$' || fail "the kernel does not say what the firmware offered (3 graphics outputs: 2 displays, the console's own without pixels)"
echo "ok   the kernel's log names both displays, a line each: 800x600 on the Cirrus (PCI 1013:00b8, /devices/framebuffer1) and 1280x800 on the second VGA (PCI 1234:1111, /devices/framebuffer, the main one: the largest picture); of the firmware's 3 graphics outputs the console's own has no pixels and is passed over"
wait_for '^root@brix:[^#]*# $'
# (The kernel says no more: its log is on both screens as it stays.)
sleep 1
dump "$work/a0.ppm" "$work/a1.ppm"
said=$(pictures console "$work/a0.ppm" "$work/a1.ppm" 2>&1) || fail "the boot log is not on both screens: $said"
echo "ok   the boot log is on both screens, the kernel's console alone ($said)"
# A program reads both nodes: ten words each (80 bytes: a line of 107
# characters and a "="). An answer counts after the line `$2` (a word
# said in between tells a second answer from the first).
record() {
    local node=$1 after=$2 line=
    for (( i = 0; i < 40 && ${#line} == 0; i++ )); do
        printf 'head -c 80 %s | base64 -w 0; echo\r' "$node" >&3
        for (( k = 0; k < 20 && ${#line} == 0; k++ )); do
            sleep 0.1
            line=$(output | sed -n "/^$after\$/,\$p" | grep -a '^[A-Za-z0-9+/]\{107\}=$' | tail -1 || true)
        done
    done
    [[ -n $line ]] || fail "no geometry read from $node"
    python3 -c 'import base64, struct, sys; print(*struct.unpack("<10Q", base64.b64decode(sys.argv[1])))' "$line"
}
printf 'echo first\r' >&3
wait_for '^first$'
main_words=$(record /devices/framebuffer first)
read -r wide high pitch format shape_wide shape_high number count vendor device <<< "$main_words"
[[ $wide == 1280 && $high == 800 && $pitch == 5120 && $shape_wide == 1280 && $shape_high == 800 && $number == 1 && $count == 2 && $vendor == $((0x1234)) && $device == $((0x1111)) ]] || fail "/devices/framebuffer says \"$main_words\", not 1280x800, display 1 of 2, adapter 1234:1111"
printf 'echo next\r' >&3
wait_for '^next$'
other_words=$(record /devices/framebuffer1 next)
read -r wide high pitch format shape_wide shape_high number count vendor device <<< "$other_words"
[[ $wide == 800 && $high == 600 && $pitch -ge 3200 && $number == 0 && $count == 2 && $vendor == $((0x1013)) && $device == $((0xb8)) ]] || fail "/devices/framebuffer1 says \"$other_words\", not 800x600, display 0 of 2, adapter 1013:00b8"
stop
echo "ok   a program reads both: /devices/framebuffer is the 1280x800 display (number 1 of 2, adapter 1234:1111), /devices/framebuffer1 the 800x600 one (number 0, adapter 1013:00b8)"

# ---- 2. the boot screen ----
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# A service that takes the screen and draws nothing: the splash is told to
# quit keeping its picture, which then stays on the displays.
cat > "$work/services/still.service" <<'UNIT'
description = takes the screen and leaves the picture on it
exec = sleep 3600
screen = yes
after = serial-shell
UNIT
mkdir -p "$work/splash/EFI/BOOT"
"$work/brix-image" "$work/kernel" "$work/splash/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" \
    "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "settings/init/services/serial-shell.service=$work/services/serial-shell.service" "settings/init/services/still.service=$work/services/still.service" "$work/bin/"
start "$work/splash" "${two[@]}"
wait_for 'ignition: a further display: the same picture on each, fitted$'
wait_for 'ignition: told to quit, the picture stays$'
wait_for 'ignition: 1280x800, [0-9]* frames in '
same_on_both "$work/b0.ppm" "$work/b1.ppm" other "the boot screen is not on both displays"
stop
echo "ok   the boot screen stays on both displays, the same picture, fitted ($seen)"

# ---- 3. the desktop ----
sed -e 's/^exec = kepler$/exec = kepler --verbose/' -e '/^disabled = yes$/d' -e '$a log = console' -e '$a env = MLX_COMPOSITOR_LOG=-' \
    projects/brix/settings/init/services/desktop.service > "$work/services/desktop.service"
grep -q '^exec = kepler --verbose$' "$work/services/desktop.service" || fail "desktop.service is not the shipped one"
mkdir -p "$work/desktop/EFI/BOOT"
"$work/brix-image" "$work/kernel" "$work/desktop/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" \
    "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" \
    "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "settings/init/services/serial-shell.service=$work/services/serial-shell.service" "settings/init/services/desktop.service=$work/services/desktop.service" "$work/bin/"
start "$work/desktop" "${two[@]}"
wait_for 'brix: 1 further display shows the same picture$'
wait_for '^layer: orbit '
wait_for '^layer: horizon 1280x30 at 0,0 in layer 2$'
same_on_both "$work/c0.ppm" "$work/c1.ppm" other "the desktop is not on both displays"
echo "ok   the desktop is on both displays, the same picture, fitted with bars above and below on the smaller ($seen)"
# A change: Alt+Enter opens the terminal, a line is typed. Both screens
# show the window (its blue title bar and border) and are the same
# picture again; the smaller one is another picture than before.
monitor "sendkey alt-ret"
wait_for '^map: Mlx Terminal$'
wait_for '^focus: Mlx Terminal$'
for key in e c h o spc t w o spc d i s p l a y s ret; do monitor "sendkey $key"; done
sleep 1
same_on_both "$work/d0.ppm" "$work/d1.ppm" other "the desktop with the terminal's window is not on both displays"
blue_main=$(pictures blue "$work/d1.ppm")
blue_other=$(pictures blue "$work/d0.ppm")
(( blue_main > 2000 )) || fail "no window on the main display ($blue_main pixels of its blue)"
(( blue_other > 700 )) || fail "the window did not arrive on the further display ($blue_other pixels of its blue; $blue_main on the main one)"
changed=$(pictures differ "$work/c0.ppm" "$work/d0.ppm" 2>&1) || fail "the further display shows what it showed before the window opened: $changed"
stop
echo "ok   a window opened and a line typed arrive on both ($seen; the window's blue: $blue_main pixels on the main display, $blue_other on the other; $changed parts of the smaller screen changed)"

# ---- 4. the main display named; the virtual console ----
printf 'monitor\nmain 1013:00b8\n' > "$work/display"
sed -e '/^disabled = yes$/d' -e '$a log = console' projects/brix/settings/init/services/console.service > "$work/services/console.service"
mkdir -p "$work/named/EFI/BOOT"
"$work/brix-image" "$work/kernel" "$work/named/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" "mlx-console=$work/mlx-console" \
    "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "settings/display=$work/display" \
    "settings/init/services/serial-shell.service=$work/services/serial-shell.service" "settings/init/services/console.service=$work/services/console.service" "$work/bin/"
start "$work/named" "${two[@]}"
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
output | grep -aq '^brixOS: display 800x600 pitch [0-9]* format 1 at 0x[0-9a-f]* (the firmware.s)$' || fail "with \"main 1013:00b8\" in settings/display the main display's line is not the Cirrus's 800x600"
output | grep -aq '^brixOS: display 0: 800x600 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1013:00b8, /devices/framebuffer, the main display (named in settings/display)$' || fail "the kernel does not say the Cirrus's display is the main one, named in settings/display"
output | grep -aq '^brixOS: display 1: 1280x800 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1234:1111, /devices/framebuffer1$' || fail "the second VGA's display is not /devices/framebuffer1 where the other is named the main one"
wait_for '^mlx-console: 800x600, '
sleep 2
# (Something to see: the start of the kernel's log, typed for on the
# console's own keyboard.)
for key in h e a d spc minus c spc 7 0 0 spc slash d e v i c e s slash k l o g ret; do monitor "sendkey $key"; done
sleep 2
same_on_both "$work/e0.ppm" "$work/e1.ppm" cirrus "the virtual console is not on both displays"
text=$(python3 -c 'import sys; d = open(sys.argv[1], "rb").read().split(b"\n", 3)[3]; print(sum(1 for v in d[0::3] if v > 128))' "$work/e0.ppm")
(( text > 3000 )) || fail "the virtual console shows no text to compare ($text pixels lit)"
stop
echo "ok   settings/display names the main display (\"main 1013:00b8\": the Cirrus's 800x600 is /devices/framebuffer, the other /devices/framebuffer1), and mlx-console on it is on the larger display too, made larger ($seen)"

# ---- 5. each display's monitor's word ----
# The bytes $2... (two hex digits each) as a block of 128 bytes, zeros
# after them and the checksum last, appended to the file $1.
edid_block() {
    local file=$1 sum=0 count=0 byte
    shift
    for byte in "$@"; do
        printf "\\x$byte" >> "$file"
        sum=$(( (sum + 0x$byte) & 255 ))
        count=$(( count + 1 ))
    done
    while (( count < 127 )); do
        printf '\0' >> "$file"
        count=$(( count + 1 ))
    done
    printf "\\x$(printf '%02x' $(( (256 - sum) & 255 )))" >> "$file"
}
# A base block alone: the header, a maker and a product, EDID 1.4, a
# digital input; the preferred timing 3840x1080 (549.12 MHz, 3840 of 4000,
# 1080 of 1144), the name, two empty descriptors; no extension.
: > "$work/edid.bin"
edid_block "$work/edid.bin" 00 ff ff ff ff ff ff 00 4c 2d 12 34 00 00 00 00 01 21 01 04 a5 78 22 78 3a \
    00 00 00 00 00 00 00 00 00 00 00 00 00 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 \
    80 d6 00 a0 f0 38 40 40 30 20 3a 00 b0 50 41 00 00 1a \
    00 00 00 fc 00 57 49 44 45 20 54 45 53 54 0a 20 20 20 \
    00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
    00 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 \
    00
[[ $(wc -c < "$work/edid.bin") -eq 128 ]] || fail "the test EDID is not one block"
# (The Cirrus named the main display: the other's lines are a further
# display's then.)
printf 'main 0\n' > "$work/display-first"
mkdir -p "$work/monitors/EFI/BOOT"
"$work/brix-image" "--test-edid=$work/edid.bin" "$work/kernel" "$work/monitors/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "settings/display=$work/display-first" "$work/bin/"
start "$work/monitors" "${two[@]}"
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
output | grep -aq '^brixOS: display 0: 800x600 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1013:00b8, its monitor says 3840x1080, /devices/framebuffer, the main display (named in settings/display)$' || fail "the kernel does not say the Cirrus's display with its monitor's word, the main one by \"main 0\""
output | grep -aq '^brixOS: display 1: 1920x1080 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1234:1111, its monitor says 3840x1080, /devices/framebuffer1$' || fail "the second VGA's display is not in the nearest mode it has to its monitor's own (1920x1080), with its monitor's word"
output | grep -aq "^brixOS: display 1: the loader set this mode, the nearest the firmware has to the monitor's own; the firmware's was 1280x800\$" || fail "the kernel does not say whose mode the loader set on the further display"
output | grep -aq "^brixOS: display 1: its monitor's own mode is 3840x1080, which was not set\$" || fail "the kernel does not say the further display's monitor's own mode"
output | grep -aq '^brixOS: the loader set this mode' && fail "the kernel says the loader set a mode on the Cirrus, which has none nearer to its monitor's than the firmware's"
wait_for '^root@brix:[^#]*# $'
printf 'echo first\r' >&3
wait_for '^first$'
far_words=$(record /devices/framebuffer1 first)
read -r wide high pitch format shape_wide shape_high number count vendor device <<< "$far_words"
[[ $wide == 1920 && $high == 1080 && $shape_wide == 3840 && $shape_high == 1080 && $number == 1 && $count == 2 ]] || fail "/devices/framebuffer1 says \"$far_words\", not 1920x1080 on a screen of the shape 3840:1080, display 1 of 2"
stop
echo "ok   with an EDID from the firmware for each output: every display has its monitor's word (3840x1080) and its own mode (the second VGA 1920x1080, the nearest it has; the Cirrus stays in 800x600), the kernel says both, and /devices/framebuffer1 gives its screen's shape (3840:1080)"

# ---- 6. one display, and outputs without pixels ----
start "$work/plain" -vga none -device VGA,id=video0 -device virtio-gpu-pci,id=video1
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
output | grep -aq '^brixOS: display 0: 1280x800 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1234:1111, /devices/framebuffer, the main display (the only one)$' || fail "with one adapter that has a framebuffer the kernel does not say one display, the only one"
output | grep -aq '^brixOS: display 1: ' && fail "an output without a framebuffer became a display"
output | grep -aq '^brixOS: the firmware offers 3 graphics outputs: 1 display, 2 without pixels the kernel can use (passed over)$' || fail "the kernel does not say the outputs passed over (the console's own and the virtio adapter's, which only take blocks)"
wait_for '^root@brix:[^#]*# $'
printf 'echo first\r' >&3
wait_for '^first$'
one_words=$(record /devices/framebuffer first)
read -r wide high pitch format shape_wide shape_high number count vendor device <<< "$one_words"
[[ $wide == 1280 && $high == 800 && $number == 0 && $count == 1 ]] || fail "/devices/framebuffer says \"$one_words\", not 1280x800, display 0 of 1"
printf 'head -c 8 /devices/framebuffer1 || echo no second display\r' >&3
wait_for '^no second display$'
stop
echo "ok   one adapter with a framebuffer beside one without: one display as before (/devices/framebuffer, no /devices/framebuffer1), and the kernel says the two outputs passed over"

# ---- 7. the firmware's first output without pixels ----
# The same two adapters the other way round: the output the firmware
# lists first (the virtio adapter's) has no framebuffer. The VGA behind
# it is the display, in the firmware's mode, with the kernel's console on
# it; and the line on what the firmware offered accounts for all three
# outputs (the first one too is one of those passed over).
start "$work/plain" -vga none -device virtio-gpu-pci,id=video0 -device VGA,id=video1
wait_for '^brixOS: display [0-9]*x[0-9]* '
wait_for '^brixOS: [0-9]* MiB'
output | grep -aq '^brixOS: display 1280x800 pitch 5120 format 1 at 0x[0-9a-f]* (the firmware.s)$' || fail "with the firmware's first output without pixels the display is not the VGA's in the firmware's mode (1280x800)"
output | grep -aq '^brixOS: display 0: 1280x800 at 0x[0-9a-f]*, PCI [0-9a-f:.]* 1234:1111, /devices/framebuffer, the main display (the only one)$' || fail "with the firmware's first output without pixels the kernel does not say the VGA's display, the only one"
output | grep -aq '^brixOS: the firmware offers 3 graphics outputs: 1 display, 2 without pixels the kernel can use (passed over)$' || fail "the kernel's line does not account for all three outputs where the firmware's first has no pixels (1 display, 2 passed over): $(output | grep -a 'graphics outputs' | head -n 1)"
wait_for '^root@brix:[^#]*# $'
sleep 1
dump "$work/g0.ppm" "$work/g1.ppm"
lit=$(python3 -c 'import sys; d = open(sys.argv[1], "rb").read().split(b"\n", 3)[3]; print(sum(1 for v in d[0::3] if v > 200))' "$work/g1.ppm")
(( lit > 3000 )) || fail "the boot log is not on the VGA's screen where the firmware's first output has no pixels ($lit pixels lit)"
stop
echo "ok   the firmware's first output without pixels: the adapter behind it is the display (1280x800, the firmware's mode), the boot log is on it ($lit pixels of text), and the kernel's line accounts for all three outputs"
echo PASS
