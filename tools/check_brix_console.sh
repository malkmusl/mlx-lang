#!/usr/bin/env bash
# brixOS's virtual console: the display (the Bochs adapter QEMU gives a
# -kernel boot) as /devices/framebuffer, the PS/2 keyboard as
# /devices/keyboard, a session pair, and mlx-console (projects/console/
# brixos.mlx) drawing flux with std.truetype on the screen: typed on the
# keyboard through QEMU's monitor (sendkey), the screen dumped
# (screendump) and compared pixel by pixel with what the same code draws
# on the host for the same text (projects/brix/tools/screen).
#
#   tools/check_brix_console.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
font=/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for QEMU's monitor"
    exit 0
fi
if [[ ! -f $font ]]; then
    echo "skip: no $font"
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/console/brixos.mlx -o "$work/mlx-console"
"$compiler" --quiet projects/brix/tools/screen/main.mlx -o "$work/screen"
for program in cat echo ls; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "mlx-console=$work/mlx-console" "fonts/DejaVuSansMono.ttf=$font" "$work/bin/"
echo "ok   the kernel, mlx-console for brixOS, the screen tool and a boot image build"

# The QEMU monitor on a socket: keys and screen dumps.
monitor() {
    python3 - "$work/monitor" "$@" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
s.settimeout(1)
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

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 600 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
wait_for '^brixOS: display 1024x768 at 0x[0-9a-f]* (Bochs VBE)$'
wait_for '^root@brix:[^#]*# $'
printf 'ls -l /devices/framebuffer /devices/keyboard\r' >&3
wait_for '^crw-rw-rw- .* 29, *0 .* /devices/framebuffer$'
wait_for '^crw-rw-rw- .* 13, *64 .* /devices/keyboard$'
echo "ok   the display is found (1024x768, the Bochs adapter), /devices/framebuffer and /devices/keyboard are there"
wait_for '^root@brix:[^#]*# $' 2
# The console in the background: it takes the screen and starts flux on
# a session pair; the serial shell stays ours.
printf 'mlx-console &\r' >&3
sleep 4
# The expected screen, drawn on the host, must match what is dumped: the
# prompt alone first, then after a command typed on the keyboard.
same() {
    local dump=$1 text=$2 what=$3
    local tries=0
    while true; do
        monitor "screendump $dump"
        sleep 0.3
        if "$work/screen" "$dump" "$font" 16 "$text" > "$work/screen.txt" 2>&1; then break; fi
        (( tries++ < 40 )) || fail "the screen is not $what: $(cat "$work/screen.txt")"
        sleep 0.5
    done
}
same "$work/prompt.ppm" 'root@brix:~# ' "the prompt alone"
echo "ok   mlx-console draws flux's prompt on the screen, pixel for pixel what the host draws for it"
monitor "sendkey e" "sendkey c" "sendkey h" "sendkey o" "sendkey spc" "sendkey s" "sendkey c" "sendkey r" "sendkey e" "sendkey e" "sendkey n" "sendkey minus" "sendkey o" "sendkey k" "sendkey ret"
same "$work/typed.ppm" 'root@brix:~# echo screen-ok\nscreen-ok\nroot@brix:~# ' "the command typed and answered"
echo "ok   keys typed on the PS/2 keyboard reach the shell through the session pair; its answer is on the screen"
monitor "sendkey shift-6" "sendkey shift-2" "sendkey shift-3" "sendkey ret"
same "$work/shifted.ppm" 'root@brix:~# echo screen-ok\nscreen-ok\nroot@brix:~# ^@#\nmlx-sh: ^@#: command not found\nroot@brix:~# ' "the shifted keys"
echo "ok   Shift types the upper row's characters"
printf 'echo serial\r' >&3
wait_for '^serial$'
echo "ok   the serial console goes on beside the screen's"
echo PASS
