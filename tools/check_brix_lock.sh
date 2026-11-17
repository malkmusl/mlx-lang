#!/usr/bin/env bash
# brixOS's session lock (projects/desktop/lock, umbra), all native: a
# person logs in through the greeter (tools/check_brix_login.sh has that
# chain), and Super+L, the compositor's lock hotkey, starts umbra
# (process_spawn). umbra takes the session through ext-session-lock-v1
# over its channel to the compositor, which hides the desktop behind the
# lock card, and asks sentinel org.mlx.Login.Unlock on the system bus in
# MLXIPC's native format (std.ipc). The bus lets umbra use login.session
# (the kernel names its program: channel_peer) and tells sentinel which
# user the caller is (the kernel's word again, not a name the caller
# gives): sentinel checks the password only for the person who holds the
# seat. A wrong password leaves the session locked and the card says so;
# the right one brings the desktop back. The same request from another
# user (tools/lock-login-test/caller.mlx, in the image as a second
# "umbra") is refused with the right password.
#
# Whose "yes" opens the lock: while the wrong password is asked, another
# user's program on the bus (tools/lock-login-test/forger.mlx) sends
# umbra replies "true" with the serials its calls have; the bus carries
# them (a reply nobody asked for needs only its rules). umbra takes the
# answer of the connection that owns org.mlx.Login alone: still locked.
#
# A locker that dies: a stand-in (tools/session-lock-test --die) locks
# and ends without unlocking. The session stays locked, no key opens it,
# and the lock hotkey, the one that still works then, starts umbra again
# (there is no other terminal to start one from); the right password
# brings the desktop back.
#
# The account brix has a password of the check's own here (the shipped
# one is the account's name, which every log holds): neither it nor the
# wrong one may reach the serial line, which carries the kernel's, the
# bus's and sentinel's logs and the session compositor's log file
# (kepler's and umbra's lines) as the root shell prints it.
# The screen is dumped through QEMU's monitor, which also types.
#
#   tools/check_brix_lock.sh [compiler]
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

# The image as shipped (build.mlx's brix group: brix.img and BOOTX64.EFI)
# has the locker, its app entry and its icon; without the program the
# lock hotkey starts nothing.
for entry in 'umbra=mlx-out/bin/brix/umbra' 'share/apps/Umbra.app/app.ini=projects/desktop/apps/Umbra.app/app.ini' 'share/apps/Umbra.app/icon.png=projects/desktop/apps/Umbra.app/icon.png'; do
    (( $(grep -c -- "mlx-out/bin/brix-image .* $entry " build.mlx || true) == 2 )) || fail "the brixOS image (build.mlx: brix.img and BOOTX64.EFI) has no $entry"
done
grep -q '"brix/umbra", "projects/desktop/lock/main.mlx"' build.mlx || fail "build.mlx's brix group does not build umbra for brixOS"
echo "ok   the brixOS image as shipped (build.mlx) has umbra, its app entry and its icon"

# The passwords: the account's here (its hash below; the other user's
# request, tools/lock-login-test/caller.mlx, holds it too), and a wrong
# one.
right=kestrel42
wrong=magpie17

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/services" "$work/caller"
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
"$compiler" --quiet $brix projects/desktop/lock/main.mlx -o "$work/umbra"
# umbra's request as a user of the check's choosing; "umbra" to the bus.
"$compiler" --quiet $brix tools/lock-login-test/caller.mlx -o "$work/caller/umbra"
# Another user's program that answers "true" in the login service's place,
# and a locker that dies without unlocking.
"$compiler" --quiet $brix tools/lock-login-test/forger.mlx -o "$work/forger"
"$compiler" --quiet $brix tools/session-lock-test/main.mlx -o "$work/lock-test"
keymap=()
if [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
    keymap=("settings/keyboard/us.xkb=$work/us.xkb")
fi
"$compiler" --quiet $brix projects/coreutils/cat/main.mlx -o "$work/bin/cat"

cp projects/brix/settings/init/services/serial-shell.service "$work/services/"
# The bus and the login as shipped, their logs on the serial console
# (--verbose on the bus: which app may use which permission).
sed -e 's/^exec = synapse /exec = synapse --verbose /' -e '$a log = console' \
    projects/brix/settings/init/services/ipcd.service > "$work/services/ipcd.service"
grep -q '^exec = synapse --verbose ' "$work/services/ipcd.service" || fail "ipcd.service is not the shipped one"
sed -e '$a log = console' \
    projects/brix/settings/init/services/login.service > "$work/services/login.service"
grep -q '^exec = sentinel --verbose ' "$work/services/login.service" || fail "login.service is not the shipped one"
# The shipped accounts, brix's password the check's own (SHA-512 crypt of
# $right; root keeps the shipped one).
grep '^root:' projects/brix/settings/shadow > "$work/shadow"
grep -q '^brix:' projects/brix/settings/shadow || fail "the shipped accounts have no brix"
# shellcheck disable=SC2016
printf '%s\n' 'brix:$6$brixlock$PsSN5qCi77gvmPMcefL6.l0B.cYNtvqLlb0HROvuvSw9MFwYuSAuDQYT.WBYV8cqoOTWh4V1ffS.olf7.R2k6.:20000:0:99999:7:::' >> "$work/shadow"

entries=()
for service in "$work"/services/*.service; do entries+=("settings/init/services/$(basename "$service")=$service"); done
"$work/brix-image" "$work/kernel" "$work/brix.img" \
    "flux=$work/flux" "impulse=$work/impulse" "impulsectl=$work/impulsectl" "ignition=$work/ignition" \
    "kepler=$work/kepler" "vector=$work/vector" "launch=$work/launch" "orbit=$work/orbit" "horizon=$work/horizon" \
    "synapse=$work/synapse" "sentinel=$work/sentinel" "aurora=$work/aurora" "umbra=$work/umbra" \
    "check/umbra=$work/caller/umbra" "check/forger=$work/forger" "check/lock-test=$work/lock-test" \
    "settings/users=projects/brix/settings/users" "settings/groups=projects/brix/settings/groups" "settings/shadow=$work/shadow" \
    "share/wayland-sessions/brix.desktop=projects/brix/settings/share/wayland-sessions/brix.desktop" \
    "share/apps/Vector.app/app.ini=projects/desktop/apps/Vector.app/app.ini" \
    "share/apps/Vector.app/icon.png=projects/desktop/apps/Vector.app/icon.png" \
    "share/apps/Umbra.app/app.ini=projects/desktop/apps/Umbra.app/app.ini" \
    "share/apps/Umbra.app/icon.png=projects/desktop/apps/Umbra.app/icon.png" \
    "${keymap[@]}" "fonts/DejaVuSansMono.ttf=$fonts/DejaVuSansMono.ttf" "fonts/DejaVuSans.ttf=$fonts/DejaVuSans.ttf" \
    "${entries[@]}" "$work/bin/"
echo "ok   the kernel, the desktop, synapse, sentinel, aurora, umbra and a boot image with the bus and login services build"

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
# Types a word, one key at a time, then Enter.
type_line() {
    local word=$1 keys=() i
    for (( i = 0; i < ${#word}; i++ )); do keys+=("sendkey ${word:i:1}"); done
    monitor "${keys[@]}" "sendkey ret"
}
# Of a PPM dump, for the lock card (430x292 in the middle of the screen,
# its button 350x40, 218 below its top, in the accent blue #5aa0ff; a
# refusal in red #e04f54 above the button), four numbers: how much of the
# screen outside the card has the top left pixel's colour (per mille; the
# lock's flat background: 1000), the accent pixels of the button's place,
# the red pixels of the message's place, and how much of the screen is
# the same as in a second dump (per mille; 1000 without one). The lock's
# backdrop is the desktop's, blurred: no edge of a window, text or bar
# shows in it, so "flat" counts the pixels outside the card that differ
# from their right neighbour by at most 4 in each channel (per mille).
measure() {
    python3 - "$@" <<'PY'
import sys
def load(path):
    data = open(path, "rb").read()
    parts = data.split(b"\n", 3)
    width, height = map(int, parts[1].split())
    return width, height, parts[3]
width, height, pixels = load(sys.argv[1])
left, top = (width - 430) // 2, (height - 292) // 2
corner = pixels[0:3]
flat = outside = accent = red = bar = 0
for y in range(height):
    row = y * width * 3
    for x in range(width):
        i = row + x * 3
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        inside = left <= x < left + 430 and top <= y < top + 292
        if not inside and x + 1 < width:
            outside += 1
            if all(abs(pixels[i + c] - pixels[i + 3 + c]) <= 4 for c in range(3)): flat += 1
            # The top bar's sharp edges (its clock, its symbols): the
            # desktop is shown, not the lock.
            elif y < 30 and any(abs(pixels[i + c] - pixels[i + 3 + c]) > 48 for c in range(3)): bar += 1
        elif left + 40 <= x < left + 390:
            if top + 218 <= y < top + 258 and abs(r - 0x5a) < 24 and abs(g - 0xa0) < 24 and b > 220: accent += 1
            if top + 190 <= y < top + 216 and r > 180 and g < 130 and b < 130: red += 1
same = 1000
if len(sys.argv) > 2:
    other = load(sys.argv[2])
    if other[:2] != (width, height):
        same = 0
    else:
        equal = sum(1 for i in range(0, width * height * 3, 3) if pixels[i:i + 3] == other[2][i:i + 3])
        same = equal * 1000 // (width * height)
print(flat * 1000 // outside, accent, red, same, bar)
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
# The session compositor's log (the session has the screen, so kepler and
# what it starts, umbra among them, write to a file in the person's home):
# the root shell prints it on the serial line between two marks.
# What it holds now is then in $work/session.log.
dumps=0
session_log() {
    dumps=$((dumps + 1))
    printf 'echo log-%s-begin; cat /users/brix/.config/mlx/compositor.log; echo log-%s-end\r' "$dumps" "$dumps" >&3
    wait_for "^log-$dumps-end\$"
    output | sed -n "/^log-$dumps-begin\$/,/^log-$dumps-end\$/p" > "$work/session.log"
}
# Waits until the session's log has the line `$1`.
wait_log() {
    local tries=0 want=${2:-1}
    while true; do
        session_log
        (( $(grep -c -x -- "$1" "$work/session.log" || true) >= want )) && break
        (( tries++ < 60 )) || fail "no \"$1\" (x$want) in the session compositor's log"
        sleep 0.5
    done
}
# How often the session's log, as last read, has the line `$1`.
logged() { grep -c -x -- "$1" "$work/session.log" || true; }
# Dumps the screen to `$1` until `$2` (a test of $flat, $accent, $red and
# $same, measured against the dump `$3` when given) holds.
flat=0
accent=0
red=0
same=0
bar=0
screen_until() {
    local i
    for (( i = 0; i < 30; i++ )); do
        monitor "screendump $1"
        sleep 0.3
        read -r flat accent red same bar < <(measure "$1" ${3:+"$3"})
        if eval "$2"; then return 0; fi
        sleep 0.3
    done
    return 1
}
for (( i = 0; i < 100; i++ )); do [[ -S $work/monitor ]] && break; sleep 0.1; done

wait_for '^impulse: brixOS.s init$'
wait_for '^impulse: services defined: 3$'
wait_for 'ipc: listening at unix:path=/temporary/bus$'
wait_for '^login: greeter started$'
wait_for '^ipc: :1\.[0-9]* is aurora (mlx)$'
sleep 2
# The greeter (its password field is focused): the account's password,
# then Enter.
type_line "$right"
wait_for '^login: session started$'
wait_for '^kepler: logging to /users/brix/'
wait_for '^root@brix:[^#]*# '
# The desktop is up: its top bar and dock have their layers.
wait_log 'layer: horizon 1024x30 at 0,0 in layer 2'
wait_log 'layer: orbit .*'
sleep 1
screen_until "$work/desktop.ppm" '(( bar > 40 ))' || fail "the desktop is not on the display after the login"
echo "ok   brix logs in through the greeter (the check's password); the session's desktop is on the display"

# Super+L, the lock hotkey: the compositor starts the locker
# (lock-command, umbra by default), which locks the session.
monitor "sendkey meta_l-l"
wait_log 'launch: umbra'
wait_log 'lock: locked'
wait_log 'umbra: locked'
wait_log 'lock: surface 1024x768'
echo "ok   Super+L starts umbra (process_spawn); it locks the session: the compositor says locked and maps the lock surface over the output"
# The card on the screen: nothing of the desktop around it, its Unlock
# button in the accent colour, no refusal yet.
screen_until "$work/locked.ppm" '(( flat > 990 && accent > 9000 && red == 0 ))' || fail "the lock card is not on the display ($flat per mille of the screen around it flat, $accent accent pixels in its button, $red red)"
echo "ok   the lock card is on the display: the desktop is hidden behind its blurred backdrop ($flat per mille smooth), the Unlock button is there ($accent accent pixels)"

# A wrong password, while another user's program on the bus answers
# "true" in the login service's place (it waits for umbra's connection,
# which comes with the first password, and ends once it has answered it;
# the root shell is not of the session: it is told the bus the session
# was given). sentinel says no (after its delay), and its answer alone
# counts: the session stays locked and the card says so. (The bus's and
# sentinel's lines are not anchored from here on: the serial shell's
# prompt may stand before one.)
printf 'export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/temporary/bus\r' >&3
printf '/system/check/forger 1001 umbra\r' >&3
wait_for 'forger: watching$'
type_line "$wrong"
wait_for 'ipc: :1\.[0-9]* is umbra (mlx)$'
wait_for 'forger: answered umbra$'
sleep 1
session_log
if (( $(logged 'lock: unlocked') > 0 )); then fail "a wrong password unlocked the session: another program's reply was taken for the login service's"; fi
wait_for 'ipc: umbra may use login.session$'
wait_for 'login: unlock refused$'
wait_log 'umbra: authentication failed'
if (( $(logged 'lock: unlocked') > 0 )); then fail "a wrong password unlocked the session"; fi
screen_until "$work/denied.ppm" '(( flat > 990 && accent > 9000 && red > 150 ))' || fail "after a wrong password the card does not say so, or is gone ($flat per mille flat, $accent accent pixels, $red red pixels)"
echo "ok   a wrong password, and another user's program answering \"true\" for the login service: umbra asks sentinel over the bus as a native (mlx) client, which may use login.session; sentinel's refusal alone counts, still locked, and the card says so ($red red pixels)"

# The right one: sentinel allows, umbra unlocks, the desktop is back.
type_line "$right"
wait_for 'login: unlock allowed$'
wait_log 'umbra: unlocked'
wait_log 'lock: unlocked'
screen_until "$work/after.ppm" '(( bar > 40 && same > 950 ))' "$work/desktop.ppm" || fail "the desktop is not back after the right password ($same per mille of the screen as before the lock)"
echo "ok   the right password: sentinel allows, umbra unlocks, the desktop is back ($same per mille of the screen as before the lock)"

# Who may ask: umbra's request with the right password from another user
# (the kernel names the caller's user to the bus, the bus to sentinel) is
# refused; the same program as brix is allowed.
printf '/system/check/umbra 1001 brix\r' >&3
wait_for 'caller 1001: denied$'
wait_for "login: unlock refused: the caller is not the session's user\$"
printf '/system/check/umbra 1000 brix\r' >&3
wait_for 'caller 1000: allowed$'
wait_for 'login: unlock allowed$' 2
echo "ok   sentinel unlocks only for the person who holds the seat: the right password from another user (the kernel's word, through the bus) is refused, from brix allowed"

# A locker that dies while it holds the session: a stand-in that locks
# and ends two seconds later without unlocking, started by the root shell
# on the session's display (the session's log names it). The session
# stays locked and no key opens it; the lock hotkey starts umbra again,
# and the right password brings the desktop back.
display=$(sed -n 's/^listening on \(.*\)$/\1/p' "$work/session.log" | head -n 1)
runtime=$(sed -n "s|^display: told in \(.*\)/$display\.display\$|\1|p" "$work/session.log" | head -n 1)
[[ -n "$display" && -n "$runtime" ]] || fail "the session compositor's log does not name its display"
printf 'XDG_RUNTIME_DIR=%s WAYLAND_DISPLAY=%s /system/check/lock-test --die\r' "$runtime" "$display" >&3
wait_for 'lock-test: locked$'
wait_log 'lock: locked' 2
wait_log 'lock: the locker went without unlocking; the session stays locked'
type_line "$right"
sleep 1
session_log
(( $(logged 'lock: unlocked') == 1 && $(logged 'launch: umbra') == 1 )) || fail "with its locker gone the session did not stay locked, or a key that is not the lock hotkey started something"
monitor "sendkey meta_l-l"
wait_log 'launch: umbra' 2
wait_log 'umbra: locked' 2
screen_until "$work/relocked.ppm" '(( flat > 990 && accent > 9000 ))' || fail "the lock card is not back after the lock hotkey ($flat per mille of the screen around it flat, $accent accent pixels in its button)"
type_line "$right"
wait_for 'login: unlock allowed$' 3
wait_log 'lock: unlocked' 2
screen_until "$work/again.ppm" '(( bar > 40 && same > 950 ))' "$work/desktop.ppm" || fail "the desktop is not back after the second lock ($same per mille of the screen as before the lock)"
echo "ok   a locker that dies without unlocking leaves the session locked; the lock hotkey starts umbra again and the right password brings the desktop back ($same per mille)"

# Neither password reached the serial line: not the kernel's, the bus's
# or sentinel's log, nor the session compositor's (kepler and umbra),
# printed there above, nor the greeter compositor's (the login's
# password went through it), printed now.
session_log
printf 'echo greeter-log-begin; cat /temporary/users/greeter/.config/mlx/compositor.log; echo greeter-log-end\r' >&3
wait_for '^greeter-log-end$'
output | sed -n '/^greeter-log-begin$/,/^greeter-log-end$/p' | grep -q '^kepler: started freestanding$' || fail "the greeter compositor's log was not read"
if grep -a -F -q -e "$right" -e "$wrong" "$work/serial.txt"; then fail "a password reached a log"; fi
# Nor as key codes: a compositor's log names no key it passed on.
if output | grep -q '^key [0-9]* -> '; then fail "a compositor's log has the keys that were typed"; fi
echo "ok   neither password is in the kernel's, the bus's, sentinel's, the greeter compositor's or the session compositor's log, as text or as key codes"

# A clean power off.
printf 'poweroff\r' >&3 2> /dev/null || true
echo "PASS brixOS session lock: Super+L, umbra, sentinel over the native bus, wrong and right password, a forged answer, a locker that dies"
