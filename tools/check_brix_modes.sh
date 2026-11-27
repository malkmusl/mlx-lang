#!/usr/bin/env bash
# A display's mode changed while brixOS runs: the modes the monitor
# offers listed, another one asked for through the native interface
# (spec/04-stdlib/os/brix.xml, <Displays>: display_modes, display_mode,
# the display's record, the handle's "changed"), confirmed or not, and
# remembered for the next boots.
#
# QEMU's standard VGA is the display (the image started by QEMU's own
# loader: the kernel sets 1024x768 on the adapter itself, as ever; the
# adapter's EDID names the monitor's modes), a virtio disk the system's
# disk. `display` (projects/brix/tools/display, the image's own tool) is
# the program that asks; the adapter's screen is dumped through QEMU's
# monitor and its size and what is on it looked at:
#
# 1. an image nobody asks anything of behaves as before: the kernel's
#    picture is 1024x768, its console on it; the kernel says how many
#    modes the display has and that its driver can set them;
# 2. the modes listed through the interface: the one that runs, the
#    monitor's own, one that is more than the adapter's memory holds;
# 3. another mode asked for: the screen has the new size and the kernel's
#    console is on it; the display's record says so (size, pitch) and
#    that it is pending; a program that waits for the display's change
#    is woken; a second request meanwhile is refused;
# 4. nobody confirms: after fifteen seconds the kernel puts the mode
#    before back, says so, and the screen is the old size again, the
#    console on it;
# 5. a size the monitor does not name (the adapter makes any its memory
#    holds), taken back by hand; sizes the adapter does not make, and
#    more than its memory holds, refused with the reason; a program of
#    another user may not ask; for every change the machine's other
#    processor stood still and went on (the kernel's log);
# 6. the system put on the disk and started from it; a mode asked for and
#    confirmed stays (twenty seconds later it is still there) and is
#    written to /system/settings/display-modes;
# 7. after a restart of the system the kernel sets that mode at boot, in
#    place of the one it sets by itself, and says so;
# 8. an image whose boot keeps the firmware's picture (settings/gpu:
#    firmware, what the key F is on a real machine) sets nothing: the
#    remembered mode is not set, and stays remembered;
# 9. the tool's own question at the terminal ("Can you read this? Press
#    Enter ..."): Enter keeps the mode, another answer brings the mode
#    before back; and who may ask beside root: the program that draws on
#    the display, when the display's node is its user's;
# 10. a display no driver of ours sets (a Cirrus VGA as OVMF left it;
#    skipped without OVMF): its mode is listed, none can be set, the
#    reason is said, a request is refused.
#
#   tools/check_brix_modes.sh [compiler]
#
# (BRIX_KERNEL_MAIN names another kernel's main.mlx to build in place of
# this tree's: with the kernel from before this was built the check fails
# at 1, where the kernel says nothing of the display's modes. MODES_SHOW=1
# prints the kernel's and the tool's lists of the modes at 2.)
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
kernel_main=${BRIX_KERNEL_MAIN:-projects/brix/kernel/main.mlx}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for QEMU's monitor"
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
serial=$work/serial.txt
fail() { echo "FAIL $1" >&2; [[ -f "$serial" ]] && cat -v "$serial" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding "$kernel_main" -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix projects/brix/tools/as/main.mlx -o "$work/as"
"$compiler" --quiet $brix projects/brix/tools/display/main.mlx -o "$work/display"
for program in ls cat cp mkdir chmod chown sync echo sleep rm; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "brixfs=$work/brixfs" "as=$work/as" "display=$work/display" "install=projects/brix/tools/install.sh" "$work/bin/"
printf 'firmware\n' > "$work/gpu-firmware"
"$work/brix-image" "$work/kernel" "$work/brix-firmware.img" "flux=$work/flux" "display=$work/display" "settings/gpu=$work/gpu-firmware" "$work/bin/"
echo "ok   the kernel, the display tool and the images build"

head -c 16777216 /dev/zero > "$work/disk.img"
# An image started by QEMU's loader with the standard VGA and the disk.
boot() {
    rm -f "$work/input" "$work/monitor"
    mkfifo "$work/input"
    : > "$serial"
    qemu-system-x86_64 -m 256M -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -cpu max -smp 2 -vga std \
        -serial stdio -kernel "$1" -drive "file=$work/disk.img,format=raw,if=virtio" < "$work/input" > "$serial" 2>&1 &
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
output() { sed 's/\x1b\[K//g' "$serial" | tr '\r' '\n'; }
# (Fifteen seconds of the system's clock are waited for here, on a host
# whose processors are shared: the tries are many.)
patience=1200
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -ac -- "$1" || true) < want )); do
        (( tries++ < patience )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
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
# The screen dumped and looked at: its size is $1 by $2, and the kernel's
# console is on it (text in its first rows, white on black and nothing
# else anywhere). What was seen in `seen`.
seen=
screen() {
    local width=$1 height=$2 tries=0
    while true; do
        rm -f "$work/screen.ppm"
        monitor "screendump $work/screen.ppm"
        sleep 0.3
        seen=$(python3 - "$work/screen.ppm" "$width" "$height" <<'PY' 2>&1
import sys
try:
    data = open(sys.argv[1], "rb").read()
except OSError:
    sys.exit("no dump")
parts = data.split(b"\n", 3)
if parts[0] != b"P6" or len(parts) < 4: sys.exit("no PPM")
w, h = map(int, parts[1].split())
px = parts[3]
if len(px) < w * h * 3: sys.exit("a dump cut short")
if (w, h) != (int(sys.argv[2]), int(sys.argv[3])): sys.exit("the screen is %dx%d, not %sx%s" % (w, h, sys.argv[2], sys.argv[3]))
lit = sum(1 for y in range(48) for v in px[y * w * 3:(y * w + min(w, 640)) * 3:3] if v > 200)
if lit < 300: sys.exit("no text in the screen's first lines (%d pixels lit)" % lit)
other = sum(1 for v in px[:w * h * 3] if v != 0 and v != 255)
if other: sys.exit("%d values on the screen are neither black nor white: not the kernel's console alone" % other)
print("%dx%d, %d pixels of text in its first three lines, white on black alone" % (w, h, lit))
PY
        ) && return 0
        (( tries++ < 20 )) || fail "the screen: $seen"
        sleep 0.5
    done
}

# ---- 1. an image nobody asks anything of ----
boot "$work/brix.img"
wait_for '^brixOS: display 1024x768 pitch 4096 format 1 at 0x[0-9a-f]* (Bochs VBE)$'
wait_for '^root@brix:[^#]*# $'
output | grep -aq "^brixOS: display 0: [0-9]* modes, of which its driver (QEMU's adapter) can set [0-9]*, and any other size its 16 MiB hold; it runs 1024x768 at 60.00 Hz\$" || fail "the kernel does not say the display's modes and its driver (display 0: N modes, of which its driver (QEMU's adapter) can set M, ...; it runs 1024x768 at 60.00 Hz)"
said_modes=$(output | grep -a "^brixOS: display 0: [0-9]* modes")
output | grep -aq 'the mode remembered' && fail "a mode is said to be remembered on a system that never asked for one"
screen 1024 768
echo "ok   an image nobody asks anything of: the kernel's picture is 1024x768 as ever, its console on it ($seen); the kernel says: ${said_modes#brixOS: }"

# ---- 2. the modes, through the interface ----
type_line 'display; echo listed $?'
wait_for '^listed 0$'
output | grep -aq "^display 0 (the main one): 1024x768 at 60.00 Hz, QEMU's adapter (it makes any other size its memory holds too)\$" || fail "display does not say the main display, what it runs and its driver"
output | grep -aq '^  1024x768 at 60.00 Hz  runs now$' || fail "the mode that runs is not marked (1024x768 at 60.00 Hz  runs now)"
output | grep -aq "^  1280x800 at [0-9.]* Hz  the monitor's own\$" || fail "the monitor's own mode is not marked (QEMU's EDID: 1280x800)"
output | grep -aq '^  800x600 at 60.31 Hz$' || fail "800x600 at 60.31 Hz (an established timing of the EDID) is not listed"
output | grep -aq "^  [0-9]*x[0-9]* at [0-9.]* Hz  cannot be set: more than the adapter's memory holds\$" || fail "no mode is said to be more than the adapter's memory holds"
count=$(output | grep -ac '^  [0-9]*x[0-9]*\( interlaced\)\? at [0-9.]* Hz')
[[ -n ${MODES_SHOW:-} ]] && output | grep -a "^  \|^display\|^brixOS: display" >&2 || true
echo "ok   the modes listed through the interface: $count of them, 1024x768 runs, 1280x800 is the monitor's own, the largest are more than the adapter's 16 MiB hold"

# ---- 3. another mode asked for ----
type_line 'display wait > /temporary/woken &'
type_line 'display ask 800x600; echo asked $?'
wait_for '^asked 0$'
asked_at=$(date +%s)
output | grep -aq '^brixOS: display 0: it runs 800x600 at 60.31 Hz now (before: 1024x768 at 60.00 Hz); unless that is confirmed within 15 seconds, the mode before comes back$' || fail "the kernel does not say the change (display 0: it runs 800x600 at 60.31 Hz now (before: 1024x768 at 60.00 Hz); unless that is confirmed within 15 seconds, ...)"
output | grep -aq '^display: 800x600 at 60.31 Hz runs now; `display confirm` within 1[45] s keeps it, else the mode before comes back$' || fail "the tool does not say that the mode runs and waits to be confirmed"
screen 800 600
type_line 'display state; echo stated; cat /temporary/woken; display ask 640x480; echo second $?'
wait_for '^second [0-9]*$'
output | grep -aq '^display 0: 800x600 at 60.31 Hz, pitch 3200, change 1; pending, [0-9]* s left to confirm$' || fail "the display's record does not say the new picture and that it is pending (800x600, pitch 3200, change 1; pending, N s left to confirm)"
(( $(output | grep -ac '^display 0: 800x600 at 60.31 Hz, pitch 3200, change 1; pending, [0-9]* s left to confirm$') >= 2 )) || fail "the program that waited for the display's change did not say the new state (it was not woken)"
output | grep -aq '^display: a mode is pending: `display confirm` keeps it, `display back` takes it back$' || fail "a second request while one is pending is not refused as busy"
output | grep -aq '^second 1$' || fail "a second request while one is pending did not fail"
echo "ok   800x600 asked for: the screen is that size and the kernel's console is on it ($seen); the record says 800x600, pitch 3200, pending; a program waiting for the change was woken; a second request meanwhile is refused"

# ---- 4. nobody confirms ----
wait_for '^brixOS: display 0: nobody confirmed 800x600 at 60.31 Hz within 15 seconds: the mode before is back, 1024x768 at 60.00 Hz$'
took=$(( $(date +%s) - asked_at ))
(( took >= 13 )) || fail "the mode was taken back after $took seconds, not fifteen"
screen 1024 768
type_line 'display state; echo stated again'
wait_for '^stated again$'
output | grep -aq '^display 0: 1024x768 at 60.00 Hz, pitch 4096, change 2; nothing pending$' || fail "the record after the kernel took the mode back is not the old picture's (1024x768, pitch 4096, change 2; nothing pending)"
echo "ok   nobody confirmed: after $took seconds the kernel put 1024x768 back and said so; the screen is that size again, the console on it ($seen)"

# ---- 5. other sizes, and who may ask ----
type_line 'display ask 1000x700; echo any $?'
wait_for '^any 0$'
output | grep -aq '^brixOS: display 0: it runs 1000x700 now (before: 1024x768 at 60.00 Hz)' || fail "a size the monitor does not name was not set (1000x700)"
screen 1000 700
type_line 'display back; echo back $?'
wait_for '^back 0$'
output | grep -aq '^brixOS: display 0: 1000x700 is taken back: the mode before is back, 1024x768 at 60.00 Hz$' || fail "the kernel does not say that the mode was taken back"
output | grep -aq '^display: taken back: 1024x768 at 60.00 Hz runs again$' || fail "the tool does not say that the mode before runs again"
screen 1024 768
type_line 'display ask 1001x700; echo odd $?; display ask 4096x2160; echo large $?; as 7 display ask 800x600; echo other $?; display confirm; echo none $?'
wait_for '^none [0-9]*$'
output | grep -aq '^display: that mode cannot be set: not a size this adapter makes$' || fail "a width that is no multiple of eight is not refused with its reason"
output | grep -aq "^display: that mode cannot be set: more than the adapter's memory holds\$" || fail "a size beyond the adapter's memory is not refused with its reason"
output | grep -aq '^display: not allowed: root may change a display.s mode, and the program that draws on it$' || fail "another user's program was not refused"
output | grep -aq '^display: there is no such mode, or nothing is pending$' || fail "confirming with nothing pending is not said"
for word in 'odd 1' 'large 1' 'other 1' 'none 1'; do
    output | grep -aq "^$word\$" || fail "expected \"$word\""
done
output | grep -aq '^brixOS: display 0: 1001x700 cannot be set: not a size this adapter makes$' || fail "the kernel's log does not say why 1001x700 was refused"
screen 1024 768
echo "ok   a size the monitor does not name (1000x700) set and taken back by hand; 1001x700 (not a size the adapter makes) and 4096x2160 (more than its memory holds) refused with the reason, nothing changed; another user's program may not ask; nothing to confirm is said"
# The other processor stood still for each of the four changes so far
# (the machine has two): the kernel's log says so, a line a change.
type_line 'cat /devices/klog; echo logged'
wait_for '^logged$'
stood=$(output | grep -ac '^brixOS: display 0: the other processors stood still for the change, [0-9]* us$' || true)
(( stood == 4 )) || fail "the kernel's log has $stood lines that the other processors stood still for a change, not 4"
output | grep -aq 'did not say so' && fail "a processor did not say that it stood still for a change"
echo "ok   for each of the four changes the machine's other processor was held and let go again (the kernel's log says so; none failed to answer)"

# ---- 6. the system on the disk; a mode confirmed ----
type_line 'brixfs format /devices/disk0 && install'
wait_for '^installed on /volumes/disk0: it is the system from the next boot on$'
type_line 'sync; echo synced'
wait_for '^synced$'
stop
boot "$work/brix.img"
wait_for '^brixOS: / from /volumes/disk0 (the boot archive at /volumes/boot)$'
type_line 'display ask 1280x720; echo asked $?'
wait_for '^asked 0$'
screen 1280 720
type_line 'display confirm; echo confirmed $?'
wait_for '^confirmed 0$'
output | grep -aq '^brixOS: display 0: 1280x720 is confirmed and remembered for this monitor (/system/settings/display-modes): the next boots set it$' || fail "the kernel does not say that the confirmed mode is remembered"
output | grep -aq '^display: confirmed: 1280x720 stays$' || fail "the tool does not say that the mode stays"
type_line 'cat /system/settings/display-modes; sleep 20; display state; echo waited'
wait_for '^waited$'
output | grep -aq '^mode 1234:1111 [0-9a-f]\{16\} 1280x720@0$' || fail "/system/settings/display-modes does not hold the mode (mode 1234:1111 MONITOR 1280x720@0)"
output | grep -aq 'nobody confirmed 1280x720' && fail "a confirmed mode was taken back"
output | grep -aq '^display 0: 1280x720, pitch 5120, change 1; nothing pending; a mode is remembered for the next boots$' || fail "the record of the confirmed mode is not as expected (1280x720, pitch 5120, change 1; nothing pending; a mode is remembered ...)"
screen 1280 720
type_line 'sync; echo synced'
wait_for '^synced$'
stop
echo "ok   booted from the disk; 1280x720 asked for and confirmed: it stays (twenty seconds later the screen is still that size, $seen) and /system/settings/display-modes holds it"

# ---- 7. after a restart ----
boot "$work/brix.img"
wait_for '^brixOS: display 1024x768 pitch 4096 format 1 at 0x[0-9a-f]* (Bochs VBE)$'
wait_for '^brixOS: display 0: the mode remembered for this monitor is set: 1280x720 (/system/settings/display-modes holds it)$'
wait_for '^root@brix:[^#]*# $'
screen 1280 720
type_line 'display state; display; echo listed $?'
wait_for '^listed 0$'
output | grep -aq '^display 0: 1280x720, pitch 5120, change 1; nothing pending; a mode is remembered for the next boots$' || fail "the record after the restart is not the remembered mode's"
output | grep -aq '^  1280x720  runs now  set at the next boots$' || fail "the list does not mark the remembered mode as the one that runs and that boots set"
stop
echo "ok   after a restart the kernel sets the remembered mode at boot and says so: the screen is 1280x720, the console on it ($seen)"

# ---- 8. a boot that keeps the firmware's picture ----
boot "$work/brix-firmware.img"
wait_for "^brixOS: display 0: the mode remembered for this monitor, 1280x720, is not set at this boot (the boot's choice sets no new mode); it stays remembered\$"
wait_for '^root@brix:[^#]*# $'
screen 1024 768
type_line 'cat /system/settings/display-modes; echo kept $?'
wait_for '^kept 0$'
output | grep -aq '^mode 1234:1111 [0-9a-f]\{16\} 1280x720@0$' || fail "the remembered mode is gone after a boot that did not set it"
echo "ok   an image whose boot keeps the firmware's picture (settings/gpu: firmware) sets no mode: the screen is 1024x768 ($seen), and the remembered mode stays remembered"

# ---- 9. the question at the terminal ----
# (After such a boot a mode can still be asked for by hand.)
type_line 'display 800x600; echo answered $?'
wait_for 'Can you read this? Press Enter within 1[45] seconds to keep it'
screen 800 600
printf '\r' >&3
wait_for '^answered [0-9]*$'
output | grep -aq '^display: kept; the next boots set it too$' || fail "Enter did not keep the mode"
output | grep -aq '^answered 0$' || fail "the tool did not end with 0 after the mode was kept"
type_line 'display 1024x768; echo declined $?'
wait_for 'Can you read this? Press Enter within 1[45] seconds to keep it' 2
screen 1024 768
printf 'n\r' >&3
wait_for '^declined [0-9]*$'
output | grep -aq '^display: not kept: 800x600 at 60.31 Hz runs again$' || fail "an answer that is not Enter did not bring the mode before back"
output | grep -aq '^declined 1$' || fail "the tool did not end with 1 after the mode was not kept"
screen 800 600
echo "ok   display 800x600 at the terminal asks whether the picture is seen: Enter keeps it; another answer to the next one brings the mode before back ($seen)"
# Who may ask beside root: the program that draws on the display (it has
# mapped its pixels: `display -m`), when the display's node is its
# user's. Neither alone is enough. (A program that maps the display takes
# it from the kernel's console: no screen is looked at after this.)
type_line 'as 7 display -m ask 1024x768; echo drawing $?; chown 7 /devices/framebuffer; as 7 display ask 1024x768; echo owning $?; as 7 display -m ask 1024x768; echo both $?; as 7 display -m back; echo mine $?; as 8 display -m ask 1024x768; echo another $?'
wait_for '^another [0-9]*$'
for word in 'drawing 1' 'owning 1' 'both 0' 'mine 0' 'another 1'; do
    output | grep -aq "^$word\$" || fail "who may ask: expected \"$word\""
done
(( $(output | grep -ac '^display: not allowed: root may change a display.s mode, and the program that draws on it$') == 3 )) || fail "the three requests that are not allowed were not all refused as such"
output | grep -aq '^brixOS: display 0: it runs 1024x768 at 60.00 Hz now (before: 800x600 at 60.31 Hz)' || fail "the request of the program that draws on its own display was not carried out"
output | grep -aq '^brixOS: display 0: 1024x768 at 60.00 Hz is taken back: the mode before is back, 800x600 at 60.31 Hz$' || fail "the program that draws on its own display could not take the mode back"
stop
echo "ok   who may ask beside root: a program of user 7 that draws on the display (has mapped it) once the display's node is user 7's; not while the node is root's, not without drawing on it, and not user 8's program"

# ---- 10. a display no driver of ours sets ----
firmware=""
for candidate in /usr/share/ovmf/OVMF.fd /usr/share/qemu/OVMF.fd /usr/share/OVMF/OVMF.fd /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd; do
    [[ -f $candidate ]] && firmware=$candidate && break
done
if [[ -z $firmware ]] || ! qemu-system-x86_64 -device help 2> /dev/null | grep -q 'name "cirrus-vga"'; then
    echo "skip: no OVMF or no cirrus-vga in this QEMU: a display that is as the firmware left it is not started"
    echo PASS
    exit 0
fi
mkdir -p "$work/esp/EFI/BOOT"
"$work/brix-image" "$work/kernel" "$work/esp/EFI/BOOT/BOOTX64.EFI" "flux=$work/flux" "display=$work/display" "$work/bin/"
rm -f "$work/input" "$work/monitor"
mkfifo "$work/input"
: > "$serial"
qemu-system-x86_64 -m 1G -cpu max -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -serial stdio -net none \
    -bios "$firmware" -drive "format=raw,file=fat:rw:$work/esp" -vga none -device cirrus-vga < "$work/input" > "$serial" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
prompts=0
patience=3600
wait_for "^brixOS: display 0: 1 mode, none of which can be set while the system runs: this display is as the firmware left it, and no driver of this system sets its mode; it runs [0-9]*x[0-9]*\$"
said_modes=$(output | grep -a "^brixOS: display 0: 1 mode")
type_line 'display; echo listed $?; display ask 640x480; echo asked $?'
wait_for '^asked [0-9]*$'
output | grep -aq '^display 0 (the main one): [0-9]*x[0-9]*, no driver: as the firmware left it$' || fail "display does not say that the display has no driver"
output | grep -aq '^  [0-9]*x[0-9]*  runs now$' || fail "the mode that runs is not listed for a display without a driver"
output | grep -aq '^  none of them can be set while the system runs: this display is as the firmware left it, and no driver of this system sets its mode$' || fail "the reason why no mode can be set is not said"
output | grep -aq '^display: there is no such mode, or nothing is pending$' || fail "a request on a display without a driver is not refused"
output | grep -aq '^asked 1$' || fail "a request on a display without a driver did not fail"
stop
echo "ok   a display that is as the firmware left it (a Cirrus VGA under OVMF): its mode is listed, none can be set and the reason is said (${said_modes#brixOS: display 0: }); a request is refused"
echo PASS
