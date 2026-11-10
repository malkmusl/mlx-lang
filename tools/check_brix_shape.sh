#!/usr/bin/env bash
# The boot screen is round on a screen of another shape than its display's
# mode. A card's firmware often has no mode of a wide monitor's shape; the
# loader sets the nearest it has, the monitor stretches it across the
# screen, and what was drawn round is an ellipse. The kernel says the
# screen's shape with the display's geometry (/devices/framebuffer: the
# monitor's own mode, else the mode the image names), and ignition draws
# its picture for that shape and brings it to the mode's width on its way
# out.
#
# Here: an image that names 3840x1080 (a monitor twice as wide as OVMF's
# largest mode of that height, 1920x1080) started by OVMF in QEMU; the
# kernel says the shape, a program reads it, ignition says what it draws,
# and on the screen (dumped through QEMU's monitor) the letters lie half
# as wide for their height as in the picture the same ignition draws for
# a screen of the mode's own shape.
#
#   tools/check_brix_shape.sh [compiler]
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
mkdir -p "$work/bin" "$work/services" "$work/esp/EFI/BOOT"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/impulse"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/impulsectl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/ignition"
# (The same splash for the host: the picture to compare with.)
"$compiler" --quiet projects/desktop/splash/main.mlx -o "$work/ignition-host"
for program in cat echo head base64 sleep; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# A service that takes the screen and draws nothing: the splash is told to
# quit keeping its picture, which then stays on the display.
cat > "$work/services/still.service" <<'UNIT'
description = takes the screen and leaves the picture on it
exec = sleep 3600
screen = yes
after = serial-shell
UNIT
printf '3840x1080\n' > "$work/display"
"$work/brix-image" "$work/kernel" "$work/esp/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" \
    "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "settings/init/services/serial-shell.service=$work/services/serial-shell.service" "settings/init/services/still.service=$work/services/still.service" "settings/display=$work/display" "$work/bin/"
echo "ok   the kernel, impulse, ignition and an EFI image that names 3840x1080 build"

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
    time.sleep(0.12)
    drain()
PY
}
# How wide the lit letters lie in a picture (a PPM dump, or a PNG the
# splash wrote: both read by hand): over the nearly white pixels of the
# tallest band of rows that has such (the letters; the progress bar is a
# thin band, the rings' lights are few), the spread of their columns
# over the spread of their rows, times 1000; and the picture's size.
letters() {
    python3 - "$1" <<'PY'
import sys, math, zlib, struct
def ppm(data):
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, 3, parts[3]
def png(data):
    at = 8
    w = h = kind = 0
    packed = b""
    while at < len(data):
        size, tag = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + size]
        if tag == b"IHDR": w, h, depth, kind = struct.unpack(">IIBB", body[:10])
        if tag == b"IDAT": packed += body
        at += 12 + size
    step = 4 if kind == 6 else 3
    raw = zlib.decompress(packed)
    row = w * step
    out = bytearray()
    before = bytearray(row)
    at = 0
    for y in range(h):
        f = raw[at]
        line = bytearray(raw[at + 1:at + 1 + row])
        at += 1 + row
        for x in range(row):
            a = line[x - step] if x >= step else 0
            b = before[x]
            c = before[x - step] if x >= step else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        before = line
    return w, h, step, bytes(out)
data = open(sys.argv[1], "rb").read()
w, h, step, px = ppm(data) if data[:2] == b"P6" else png(data)
found = []
for y in range(h):
    row = px[y * w * step:(y + 1) * w * step]
    r, g, b = row[0::step], row[1::step], row[2::step]
    found.append([x for x in range(w) if r[x] > 150 and g[x] > 150 and b[x] > 150])
need = max(4, w // 200)
top = bottom = 0
y = 0
while y < h:
    if len(found[y]) >= need:
        start = y
        while y < h and len(found[y]) >= need: y += 1
        if y - start > bottom - top: top, bottom = start, y
    else:
        y += 1
n = sx = sy = sxx = syy = 0
for y in range(top, bottom):
    for x in found[y]:
        n += 1; sx += x; sy += y; sxx += x * x; syy += y * y
if n < 500:
    print(0, w, h)
else:
    across = math.sqrt(sxx / n - (sx / n) ** 2)
    down = math.sqrt(syy / n - (sy / n) ** 2)
    print(int(1000 * across / down) if down > 0 else 0, w, h)
PY
}

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 1G -cpu max -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -serial stdio -net none \
    -bios "$firmware" -drive "format=raw,file=fat:rw:$work/esp" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# (The firmware takes its time where the processor is emulated.)
wait_for() {
    local tries=0
    while ! output | grep -aq -- "$1"; do
        (( tries++ < 2400 )) || fail "no \"$1\" on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done

# The kernel: the nearest mode the firmware has, and the screen's shape
# from the mode the image names (OVMF's display says no mode of its own).
wait_for '^brixOS: display [0-9]*x[0-9]* '
mode=$(output | grep -a '^brixOS: display [0-9]*x[0-9]* ' | head -1 | grep -ao '[0-9][0-9]*x[0-9][0-9]*' | head -1)
[[ $mode == 1920x1080 ]] || fail "the image that names 3840x1080 came up in $mode, not in the nearest the firmware has (1920x1080)"
wait_for '^brixOS: the picture is shown on a screen of the shape 3840:1080 (the mode the image names); /devices/framebuffer says so to the programs that draw for it$'
echo "ok   the loader sets the nearest mode the firmware has ($mode) and the kernel takes the screen's shape from the mode the image names (3840:1080)"

# Ignition draws for the screen's shape. (Its notes land on the serial
# line beside the shell's prompt: both come up at once.)
wait_for "ignition: the screen's shape is 3840:1080, the display's mode 1920x1080: drawn 3840 wide and brought to the mode's width, so that it is round on the screen\$"
wait_for 'ignition: up, boot, on the display$'
echo "ok   ignition draws its picture 3840 wide for the screen's shape and brings it to the mode's 1920"
# The boot ends: the service that takes the screen draws nothing, the
# splash's last picture stays on it, and the one it leaves for the next
# program is the display's (1920 across), not the wider one it drew.
wait_for 'ignition: told to quit, the picture stays$'
wait_for 'ignition: 1920x1080, [0-9]* frames in '
output | grep -aq 'ignition: the picture could not be left' && fail "ignition could not leave its picture for the next program"
wait_for '^root@brix:[^#]*# '
# (It says what its frames cost before it writes the picture: asked for
# until it is there whole.)
left=
for (( i = 0; i < 20 && ${#left} == 0; i++ )); do
    printf 'head -c 32 /temporary/screen-picture | base64\r' >&3
    for (( k = 0; k < 50 && ${#left} == 0; k++ )); do
        sleep 0.1
        left=$(output | grep -a '^[A-Za-z0-9+/]\{43\}=$' | tail -1 || true)
    done
done
[[ -n $left ]] || fail "no picture left for the next program (/temporary/screen-picture)"
left_words=$(python3 -c 'import base64, struct, sys; d = base64.b64decode(sys.argv[1]); print(d[:8].decode("ascii", "replace"), *struct.unpack("<3Q", d[8:]))' "$left")
[[ $left_words == "brixpic1 1920 1080 "* ]] || fail "the picture left for the next program is \"$left_words\", not one of 1920x1080"
echo "ok   told to quit, ignition leaves the picture the display holds for the next program ($left_words)"

# A program reads the shape with the geometry: six words.
printf 'head -c 48 /devices/framebuffer | base64\r' >&3
wait_for '^[A-Za-z0-9+/]\{64\}$'
record=$(output | grep -a '^[A-Za-z0-9+/]\{64\}$' | tail -1)
words=$(python3 -c 'import base64, struct, sys; print(*struct.unpack("<6Q", base64.b64decode(sys.argv[1])))' "$record")
read -r wide high pitch format shape_wide shape_high <<< "$words"
[[ $wide == 1920 && $high == 1080 && $shape_wide == 3840 && $shape_high == 1080 ]] || fail "/devices/framebuffer says $words, not a 1920x1080 picture on a screen of the shape 3840:1080"
(( pitch >= 1920 * 4 )) || fail "/devices/framebuffer's pitch is $pitch"
echo "ok   /devices/framebuffer says the geometry and the screen's shape: ${wide}x${high}, pitch $pitch, format $format, shown on ${shape_wide}:${shape_high}"

# The screen: the letters lie half as wide for their height as in the
# picture drawn for a screen of the mode's own shape (the monitor makes
# them twice as wide again: round rings, letters as they are meant).
"$work/ignition-host" --size 1920x1080 --progress 100 --at 9 --png "$work/plain.png"
read -r plain plain_wide plain_high < <(letters "$work/plain.png")
(( plain > 2000 )) || fail "the picture to compare with has no letters to measure (spread $plain, ${plain_wide}x${plain_high})"
squeezed=0
for (( i = 0; i < 20; i++ )); do
    monitor "screendump $work/screen.ppm"
    sleep 0.3
    [[ -s $work/screen.ppm ]] || continue
    read -r squeezed screen_wide screen_high < <(letters "$work/screen.ppm")
    (( squeezed > 0 )) && break
    sleep 0.5
done
[[ ${screen_wide:-0} == 1920 && ${screen_high:-0} == 1080 ]] || fail "the screen dump is ${screen_wide:-?}x${screen_high:-?}, not 1920x1080"
(( squeezed > 0 )) || fail "no letters on the screen to measure"
(( squeezed * 100 >= plain * 42 && squeezed * 100 <= plain * 58 )) || fail "on the screen the letters' spread across over their spread down is $squeezed thousandths; drawn for a screen of the mode's shape it is $plain: not about half of it (the picture is not drawn for the screen twice as wide)"
echo "ok   on the screen the letters lie half as wide for their height ($squeezed thousandths, $plain when drawn for a screen of the mode's own shape): round on the monitor that stretches them"
echo PASS
