#!/usr/bin/env bash
# brixOS's login (projects/desktop/ipc synapse, projects/desktop/login
# sentinel and its greeter compositor, projects/desktop/greeter
# aurora), all native: impulse starts the system bus (ipcd.service,
# synapse on a listener node at /temporary/bus) and then the login
# (login.service, sentinel). sentinel takes the screen after the splash,
# starts the greeter (kepler --greeter aurora) dropped to an
# unprivileged uid, and owns org.mlx.Login and org.freedesktop.login1 on
# the bus. aurora connects to the compositor over a channel and to
# the bus over /temporary/bus; the kernel names its program to the bus
# (channel_peer), so the bus's policy lets aurora use login.session.
# A password (the account brix, password "brix", of /system/settings)
# makes sentinel branch, take the person's identity (identity_set) and
# replace itself with their session (kepler, the desktop) in their
# home /users/brix, made at the first login and owned by them (node_own).
# The screen is dumped through QEMU's monitor, which also types the
# password.
#
#   tools/check_brix_login.sh [compiler]
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 40 >&2; exit 1; }

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
"$compiler" --quiet $brix projects/desktop/ipc/main.mlx -o "$work/synapse"
"$compiler" --quiet $brix projects/desktop/login/main.mlx -o "$work/sentinel"
"$compiler" --quiet $brix projects/desktop/greeter/main.mlx -o "$work/aurora"
keymap=()
if [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
    keymap=("settings/keyboard/us.xkb=$work/us.xkb")
fi
for program in cat ls whoami uname; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done

cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The bus and the login as shipped, their logs on the serial console
# (--verbose on the bus: which app may use which permission).
sed -e 's/^exec = synapse /exec = synapse --verbose /' -e '$a log = console' \
    projects/brix/settings/init/services/ipcd.service > "$work/services/ipcd.service"
grep -q '^exec = synapse --verbose ' "$work/services/ipcd.service" || fail "ipcd.service is not the shipped one"
sed -e '$a log = console' \
    projects/brix/settings/init/services/login.service > "$work/services/login.service"
grep -q '^exec = sentinel ' "$work/services/login.service" || fail "login.service is not the shipped one"
# The desktop service is shipped disabled (the login has the screen).
grep -q '^disabled = yes$' projects/brix/settings/init/services/desktop.service || fail "desktop.service is not disabled for the login"

entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" \
    "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" \
    "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" \
    "synapse=$work/synapse" "sentinel=$work/sentinel" "aurora=$work/aurora" \
    "settings/users=projects/brix/settings/users" "settings/groups=projects/brix/settings/groups" "settings/shadow=projects/brix/settings/shadow" \
    "share/wayland-sessions/brix.desktop=projects/brix/settings/share/wayland-sessions/brix.desktop" \
    "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" \
    "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" \
    "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "${entries[@]}" "$work/bin/"
echo "ok   the kernel, the desktop, synapse, sentinel, aurora, the accounts and a boot image with the bus and login services build"

# The QEMU monitor on a socket: screen dumps and keys.
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

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 512M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 900 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done

wait_for '^impulse: brixOS.s init$'
wait_for '^impulse: services defined: 3$'
echo "ok   impulse defines the bus, the login and the serial shell"
# The bus listens, the login owns its names and starts the greeter.
wait_for 'ipc: listening at unix:path=/temporary/bus$'
wait_for '^login: org.mlx.Login and org.freedesktop.login1 on the system bus'
wait_for '^login: greeter started$'
# sentinel is itself a native (mlx) bus client: it speaks MLXIPC's native
# wire to synapse (the D-Bus path is only its fallback on a foreign host).
wait_for '^ipc: :1\.[0-9]* is sentinel (mlx)$'
echo "ok   the system bus listens and sentinel (a native mlx client) starts the greeter on the screen"
# The greeter is a bus client the kernel names (channel_peer), so the
# bus's policy can tell it apart; the greeter compositor logs to a file,
# so the login and bus messages on the serial line are what is checked.
wait_for '^ipc: :1\.[0-9]* is aurora (mlx)$'
echo "ok   the kernel names the greeter's program to the bus (channel_peer): it is aurora, a native (mlx) client, not unknown"
sleep 1

# The password "brix" of the account brix, one key at a time (the field
# is focused), then Enter.
for key in b r i x ret; do monitor "sendkey $key"; sleep 0.3; done
# The bus allows login.session for aurora; the session starts.
wait_for '^ipc: aurora may use login.session$'
wait_for '^login: greeter stopped$'
wait_for '^login: session started$'
echo "ok   aurora may use login.session; the password is accepted and the session starts"
# The session is the desktop, as the person, in their home (made at the
# first login): its compositor logs under /users/brix.
wait_for '^kepler: logging to /users/brix/'
echo "ok   the session is the desktop compositor as brix, in their home /users/brix"

# The home is the person's, not root's (node_own at the first login):
# read it from the serial root shell.
wait_for '^root@brix:[^#]*# '
printf 'ls -ld /users/brix\r' >&3
wait_for '^drwx------ 1 brix brix .* /users/brix$'
echo "ok   the login made /users/brix and gave it to brix (uid 1000) at the first login"

# The terminal ships as an app bundle (Vector.app: app.ini and icon.png,
# like a macOS .app) instead of a .desktop file and a theme icon; the
# launcher reads it.
printf 'ls -1 /system/share/apps/Vector.app\r' >&3
wait_for '^app.ini$'
wait_for '^icon.png$'
echo "ok   Vector ships as an app bundle (/system/share/apps/Vector.app) for the launcher and dock"

# The screen's picture goes from the program that leaves the framebuffer
# to the one that takes it in a file (/temporary/screen-picture,
# projects/desktop/splash/picture.mlx), so nobody reads the framebuffer
# back: sentinel ended the greeter's compositor (the terminate event),
# which left its last frame there as its own user; the session's, as
# brix, faded in from it and removed it (its log says so).
printf 'cat /users/brix/.config/mlx/compositor.log\r' >&3
wait_for 'brix: fading in from the picture the program before left$'
printf 'ls /temporary/screen-picture\r' >&3
wait_for "^ls: cannot access '/temporary/screen-picture'"
echo "ok   the session's compositor fades in from the picture the greeter's left in /temporary (another user's file, taken and removed; the framebuffer is not read back)"

# A clean power off.
printf 'poweroff\r' >&3 2> /dev/null || true
echo "PASS brixOS login: the bus, sentinel, the greeter and the session, all native"
