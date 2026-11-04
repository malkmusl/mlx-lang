#!/usr/bin/env bash
# brixOS's init and boot screen (projects/init/brixos.mlx,
# projects/desktop/splash/brixos.mlx): the kernel starts mlx-init as the
# first process; it reads the services of /system/settings/init/services,
# shows the boot on the screen with mlx-splash while they start, starts
# them with their roles (a oneshot writing to the console and to its log
# file, the serial console's shell, a sleeper), holds the screen for the
# splash's minimum before mlx-console takes it, answers mlx-initctl over
# its FIFO (status, stop, start) and powers the machine off through
# system_power. The screen is dumped through QEMU's monitor: the splash's
# picture while the services start, then mlx-console's prompt, pixel for
# pixel what the host draws for it.
#
#   tools/check_brix_init.sh [compiler]
set -euo pipefail

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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/mlx-sh"
"$compiler" --quiet $brix projects/console/brixos.mlx -o "$work/mlx-console"
"$compiler" --quiet $brix projects/init/brixos.mlx -o "$work/mlx-init"
"$compiler" --quiet $brix projects/init/tool.mlx -o "$work/mlx-initctl"
"$compiler" --quiet $brix projects/desktop/splash/brixos.mlx -o "$work/mlx-splash"
"$compiler" --quiet projects/brix/tools/screen/main.mlx -o "$work/screen"
for program in cat echo ls sleep; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
cp projects/brix/settings/init/services/serial-shell.service projects/brix/settings/init/services/console.service "$work/services/"
cat > "$work/services/hello.service" <<'EOF'
description = a oneshot saying hello on the console
type = oneshot
exec = echo hello-from-the-oneshot
log = console
EOF
cat > "$work/services/logged.service" <<'EOF'
description = a oneshot writing to its log file
type = oneshot
exec = echo a-line-for-the-log
after = hello
EOF
cat > "$work/services/sleeper.service" <<'EOF'
description = sleeps, to be stopped and started
exec = sleep 3600
EOF
entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$work/mlx-sh" "mlx-console=$work/mlx-console" "mlx-init=$work/mlx-init" "mlx-initctl=$work/mlx-initctl" "mlx-splash=$work/mlx-splash" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" "${entries[@]}" "$work/bin/"
echo "ok   the kernel, mlx-init, mlx-initctl, mlx-splash, mlx-console, the screen tool and a boot image with five services build"

# The QEMU monitor on a socket: screen dumps.
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
# How many pixels of a PPM dump are not nearly black (the splash's night
# is dark blue with lit letters and rings; the console's prompt is a few
# hundred light pixels on a dark grey).
bright() {
    python3 - "$1" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
width, height = map(int, parts[1].split())
pixels = parts[3]
count = 0
blue = 0
for i in range(0, width * height * 3, 3):
    r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
    if r + g + b > 180: count += 1
    if b > r + 20 and b > g + 10: blue += 1
print(count, blue)
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
for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done
wait_for '^mlx-init: brixOS.s init$'
wait_for '^mlx-init: services defined: 5$'
echo "ok   the kernel starts mlx-init as the first process; it reads the five services"
wait_for '^hello-from-the-oneshot$'
echo "ok   a oneshot with log = console writes to the serial console through the init"
# The splash is on the screen while the services start (the console is
# held back for its minimum): a night of blue with lit pixels, not the
# console's dark grey.
lit=0
blue=0
for (( i = 0; i < 20; i++ )); do
    monitor "screendump $work/splash.ppm"
    sleep 0.2
    read -r lit blue < <(bright "$work/splash.ppm")
    (( blue > 100000 && lit > 2000 )) && break
    sleep 0.3
done
(( blue > 100000 )) || fail "the boot screen is not on the display ($blue blue pixels, $lit lit)"
(( lit > 2000 )) || fail "the boot screen shows nothing lit ($lit pixels)"
echo "ok   mlx-splash draws the boot screen on the framebuffer while the services start ($blue blue, $lit lit pixels)"
wait_for '^root@brix:[^#]*# $'
# Then mlx-console takes the screen: its prompt, pixel for pixel.
same() {
    local dump=$1 text=$2 what=$3
    local tries=0
    while true; do
        monitor "screendump $dump"
        sleep 0.3
        if "$work/screen" "$dump" "$fonts/DejaVuSansMono.ttf" 16 "$text" > "$work/screen.txt" 2>&1; then break; fi
        (( tries++ < 60 )) || fail "the screen is not $what: $(cat "$work/screen.txt")"
        sleep 0.5
    done
}
same "$work/console.ppm" 'root@brix:~# ' "mlx-console's prompt after the splash"
echo "ok   the splash is told to quit and mlx-console takes the screen (screen = yes): its prompt, pixel for pixel"
printf 'mlx-initctl status\r' >&3
wait_for '^console  *running  *[0-9]* *0 *[0-9]*s *mlx-console on the screen$'
wait_for '^hello  *done  *-  *0 *-  *a oneshot saying hello on the console$'
wait_for '^logged  *done  *-  *0 *-  *a oneshot writing to its log file$'
wait_for '^serial-shell  *running  *[0-9]* *0 *[0-9]*s *mlx-sh on the serial console$'
wait_for '^sleeper  *running  *[0-9]* *0 *[0-9]*s *sleeps, to be stopped and started$'
echo "ok   mlx-initctl status over the control FIFO: the oneshots done, the others running"
printf 'cat /temporary/logs/logged.log\r' >&3
wait_for '^a-line-for-the-log$'
echo "ok   a service's output goes to its log file (/temporary/logs/NAME.log)"
printf 'mlx-initctl stop sleeper; mlx-initctl status\r' >&3
wait_for '^mlx-init: sleeper: stopped$'
wait_for '^sleeper  *stopped  *-  *0 *-  *sleeps, to be stopped and started$'
printf 'mlx-initctl start sleeper; mlx-initctl status\r' >&3
wait_for '^sleeper  *running  *[0-9]* *0 *[0-9]*s *sleeps, to be stopped and started$' 2
echo "ok   mlx-initctl stop ends a service (terminate to its group) and start runs it again"
printf 'mlx-initctl poweroff\r' >&3
wait_for '^mlx-init: poweroff$'
wait_for '^brixOS: powering off$'
for (( i = 0; i < 100; i++ )); do kill -0 "$qemu_pid" 2> /dev/null || break; sleep 0.1; done
kill -0 "$qemu_pid" 2> /dev/null && fail "QEMU did not power off"
qemu_pid=
echo "ok   mlx-initctl poweroff stops the services and the kernel powers the machine off (system_power)"
echo PASS
