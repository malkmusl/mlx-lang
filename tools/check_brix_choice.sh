#!/usr/bin/env bash
# One image, and what the GPU's driver does with the display chosen in it
# at boot (projects/brix/display_choice.mlx): the EFI loader reads a key
# from the firmware's keyboard, remembers F and N in a variable of the
# firmware's, and hands the kernel what it found; the kernel says the
# choice. Under OVMF in QEMU, the keys sent through QEMU's monitor
# (tapped from the machine's start until the kernel is there, as someone
# in front of a dark screen would):
#
# 1. no key: the monitor's own mode, the default; the loader says nothing
#    and looked for a key for a few tenths of a second;
# 2. F: the firmware's picture is kept; the loader says so on the
#    firmware's console; the next start without a key keeps it too (F is
#    remembered), and the firmware's own shell shows the variable: one
#    byte, F;
# 3. R: the firmware's own mode set again, for that boot; the start after
#    it is what was remembered before (R changes nothing remembered);
# 4. A: the monitor only asked (nothing set: the firmware's picture is
#    kept), for that boot;
# 5. N: the monitor's own mode again, remembered at the next start; N
#    once more writes nothing (the machine holds it already);
# 6. something remembered that is no choice (two bytes, then one byte
#    that is no remembered key's, put there by the firmware's shell): the
#    default, and the log says it is passed over; F then writes it anew;
# 7. an image built with a fixed choice (settings/gpu) and no key: the
#    image's word; a key remembered in the machine comes before it;
# 8. the same kernel started by a Multiboot loader (no loader of ours):
#    the default, or the image's word, and no key is named;
# 9. a digit (how far the card's engines go, projects/brix/engine_
#    choice.mlx): it reaches the kernel beside the letters and holds for
#    its one boot; QEMU has no card of ours, and the kernel says that
#    nothing of a card's engines is read or written.
#
# QEMU has no card this driver sets a mode on: the kernel says the choice
# there in the log alone ("gpu: no card of ours to set a mode on; this
# boot: ..."), which is the serial line's too. What the choice then does
# to a card is held on the host (tests/482, 460 and 474).
#
# The firmware's variables have to last from one start to the next: OVMF
# given by -bios keeps them in memory and loses what a loader writes, so
# the firmware is two flash drives here, its code read-only and a copy of
# its variable store that is written.
#
#   tools/check_brix_choice.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
code=""
vars=""
for pair in /usr/share/OVMF/OVMF_CODE_4M.fd:/usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_CODE.fd:/usr/share/OVMF/OVMF_VARS.fd \
            /usr/share/edk2/ovmf/OVMF_CODE.fd:/usr/share/edk2/ovmf/OVMF_VARS.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd:/usr/share/edk2/x64/OVMF_VARS.4m.fd; do
    if [[ -f ${pair%%:*} && -f ${pair##*:} ]]; then
        code=${pair%%:*}
        vars=${pair##*:}
        break
    fi
done
if ! command -v qemu-system-x86_64 > /dev/null || [[ -z $code ]]; then
    echo "skip: no qemu-system-x86_64 or no OVMF with a variable store of its own (OVMF_CODE and OVMF_VARS)"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for QEMU's monitor"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
keys_pid=
cleanup() {
    [[ -n "$keys_pid" ]] && kill "$keys_pid" 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
serial=$work/serial.txt
fail() { echo "FAIL $1" >&2; [[ -f "$serial" ]] && output | grep -av "^$" | tail -n 30 >&2; exit 1; }
# (The firmware's console moves its cursor about: those sequences out.)
output() { sed 's/\x1b\[[0-9;=?]*[A-Za-z]//g' "$serial" | tr '\r' '\n'; }
said() { output | grep -aqF -- "$1"; }

# One image, and nothing that makes or names a second.
[[ ! -e tools/brix-native-efi.sh ]] || fail "tools/brix-native-efi.sh is still there: there is one image"
if grep -rIl -e 'BOOTX64-native' -e 'BOOTX64-rehearsal' -e 'brix-native-efi' projects/brix tools docs build.mlx 2> /dev/null | grep -v '^tools/check_brix_choice.sh$' | grep -q .; then
    fail "a file still names a second image or the script that made it: $(grep -rIl -e 'BOOTX64-native' -e 'BOOTX64-rehearsal' -e 'brix-native-efi' projects/brix tools docs build.mlx | grep -v '^tools/check_brix_choice.sh$' | tr '\n' ' ')"
fi

mkdir -p "$work/bin" "$work/esp/EFI/BOOT" "$work/shell"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet --target=x86_64-brixos projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet --target=x86_64-brixos projects/coreutils/uname/main.mlx -o "$work/bin/uname"
image=$work/esp/EFI/BOOT/BOOTX64.EFI
"$work/brix-image" "$work/kernel" "$image" "flux=$work/flux" "$work/bin/"
echo "ok   the kernel and the one image build; no script and no file names a second image"

# The machine's variable store: empty, as the firmware comes.
fresh() { cp "$vars" "$work/vars.fd"; chmod u+w "$work/vars.fd"; }
firmware() { echo -drive "if=pflash,format=raw,unit=0,readonly=on,file=$code" -drive "if=pflash,format=raw,unit=1,file=$work/vars.fd"; }

# The image started by OVMF; $1: the key tapped from the start until the
# kernel is there (none: no key). Until the kernel has said the choice
# and how long the loader looked.
start() {
    local key=${1:-} tries=0
    rm -f "$work/monitor"
    : > "$serial"
    # shellcheck disable=SC2046
    qemu-system-x86_64 -m 1G -cpu max -display none -monitor "unix:$work/monitor,server,nowait" -no-reboot -serial stdio -net none \
        $(firmware) -drive "format=raw,file=fat:rw:$work/esp" < /dev/null > "$serial" 2>&1 &
    qemu_pid=$!
    if [[ -n $key ]]; then
        python3 - "$work/monitor" "$key" "$serial" <<'PY' &
import socket, sys, time
s = None
for _ in range(200):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(sys.argv[1])
        break
    except OSError:
        s = None
        time.sleep(0.05)
if s is None: sys.exit(1)
s.settimeout(0.05)
# A tap every tenth of a second, until the kernel is there (three
# minutes at most: the firmware takes its time where the processor is
# emulated).
for _ in range(1800):
    try:
        if b"brixOS: kernel at" in open(sys.argv[3], "rb").read(): break
    except OSError:
        pass
    try:
        s.sendall(("sendkey %s 30\n" % sys.argv[2]).encode())
        while s.recv(4096): pass
    except socket.timeout:
        pass
    except OSError:
        break
    time.sleep(0.1)
PY
        keys_pid=$!
    fi
    while ! output | grep -aq 'gpu: .*this boot: ' || ! output | grep -aq '^brixOS: [0-9]* MiB'; do
        (( tries++ < 1800 )) || fail "the kernel did not say this boot's choice"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
    if [[ -n $keys_pid ]]; then
        wait "$keys_pid" 2> /dev/null || true
        keys_pid=
    fi
}
# The same, and with a key until the loader took one: the firmware keeps
# a key only from its last moments before the image, the loader looks
# for three tenths of a second, and on a host that is busy the taps may
# all come too early or too late. A start at which no key was taken
# changed nothing in the machine, so it is made again (three at most).
boot() {
    local key=${1:-} again
    for again in 1 2 3; do
        start "$key"
        [[ -z $key ]] && return 0
        output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and took ' && return 0
        echo "     (the key did not reach the loader at this start: once more)"
    done
    fail "a key tapped from the machine's start until the kernel was there did not reach the loader at three starts"
}
# How long the loader looked for a key at the last boot (milliseconds).
looked() { output | grep -a '^gpu: the loader looked for a key for [0-9]* ms' | head -1 | grep -ao '[0-9][0-9]*' | head -1; }

# The firmware's own shell, on a disk with nothing to start but a script
# of the commands $@, then power off; its output is the serial line's.
# False when this firmware has no shell.
shell() {
    : > "$work/shell/startup.nsh"
    local command
    for command in "$@"; do printf '%s\r\n' "$command" >> "$work/shell/startup.nsh"; done
    printf 'reset -s\r\n' >> "$work/shell/startup.nsh"
    : > "$serial"
    # shellcheck disable=SC2046
    timeout 180 qemu-system-x86_64 -m 1G -cpu max -display none -monitor none -no-reboot -serial stdio -net none \
        $(firmware) -drive "format=raw,file=fat:rw:$work/shell" < /dev/null > "$serial" 2>&1 || true
    said "Shell> reset -s"
}
variable="BrixDisplay -guid 4e1050e5-03af-4698-8f99-584a105df8f3"

default="this boot: the monitor's own mode (the default; F while the machine starts keeps the firmware's picture)"

# ---- 1. no key ----
fresh
boot
said "gpu: no card of ours to set a mode on; $default" || fail "without a key and with nothing remembered the kernel does not say the default"
said "brixOS: display: " && fail "the loader says a choice though no key was pressed"
output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and saw none of F, N, R and A$' || fail "the kernel does not say how long the loader looked for a key"
waited=$(looked)
(( waited >= 250 && waited <= 2000 )) || fail "the loader looked for a key for $waited ms: a few tenths of a second were meant"
said "no choice of ours" && fail "an empty variable store is said to remember something"
echo "ok   no key: the monitor's own mode, the default; the loader says nothing, and looked for a key for $waited ms"

# ---- 2. F, and the start after it ----
boot f
said "brixOS: display: the firmware's picture is kept (F), remembered" || fail "the loader does not say on the firmware's console what F gave"
said "this boot: the firmware's picture is kept (F was pressed at this start and is remembered; N sets the monitor's own mode again)" || fail "after F the kernel does not say that the firmware's picture is kept"
output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and took F (written into the machine now)$' || fail "the kernel does not say that the loader took F and wrote it"
taken=$(looked)
(( taken <= 400 )) || fail "with a key the loader still looked for $taken ms"
boot
said "this boot: the firmware's picture is kept (F was pressed at an earlier start and is remembered; N sets the monitor's own mode again)" || fail "F is not remembered at the next start"
said "brixOS: display: " && fail "the loader says a choice at a start without a key"
echo "ok   F: the firmware's picture is kept, the loader says so and found the key after $taken ms; the next start without a key keeps it too"
have_shell=1
if shell "dmpstore $variable"; then
    output | grep -aq "^Variable NV+RT+BS '4E1050E5-03AF-4698-8F99-584A105DF8F3:BrixDisplay' DataSize = 0x01\$" || fail "the firmware's shell does not show the variable as one byte, non-volatile, for boot and run time"
    output | grep -aq '^  00000000: 46 ' || fail "the variable's byte is not F"
    echo "ok   the firmware's own shell shows what is remembered: the variable BrixDisplay, one byte, F"
else
    have_shell=
    echo "skip: this OVMF has no shell: the variable is not looked at from outside, and nothing wrong is put into it"
fi

# ---- 3. R: one boot ----
boot r
said "brixOS: display: the firmware's own mode set again by the card's tables (R), for this start only" || fail "the loader does not say what R gave"
said "this boot: the firmware's own mode set again by the card's tables (R was pressed at this start and holds for this boot only)" || fail "after R the kernel does not say the rehearsal"
output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and took R$' || fail "the kernel does not say that the loader took R (and wrote nothing)"
boot
said "this boot: the firmware's picture is kept (F was pressed at an earlier start and is remembered; N sets the monitor's own mode again)" || fail "after R's boot the start is not what was remembered before (F)"
echo "ok   R: the firmware's own mode set again for that boot; the start after it is F's again, R changed nothing remembered"

# ---- 4. A: one boot ----
boot a
said "brixOS: display: the monitor only asked (A), for this start only" || fail "the loader does not say what A gave"
said "this boot: the monitor only asked, nothing set: the firmware's picture is kept (A was pressed at this start and holds for this boot only)" || fail "after A the kernel does not say that the monitor is only asked"
echo "ok   A: the monitor only asked, nothing set: the kernel says so and keeps the firmware's picture, for that boot"

# ---- 5. N, the start after it, and N again ----
boot n
said "brixOS: display: the monitor's own mode (N), remembered" || fail "the loader does not say what N gave"
said "this boot: the monitor's own mode (N was pressed at this start and is remembered; F while the machine starts keeps the firmware's picture)" || fail "after N the kernel does not say the monitor's own mode"
output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and took N (written into the machine now)$' || fail "the kernel does not say that the loader wrote N"
boot
said "this boot: the monitor's own mode (N was pressed at an earlier start and is remembered; F while the machine starts keeps the firmware's picture)" || fail "N is not remembered at the next start"
boot shift-n
said "this boot: the monitor's own mode (N was pressed at this start and is remembered; F while the machine starts keeps the firmware's picture)" || fail "N pressed again (with shift) is not taken"
output | grep -aq '^gpu: the loader looked for a key for [0-9]* ms and took N (the machine held it already: nothing was written)$' || fail "N pressed again is written again, though the machine held it"
echo "ok   N: the monitor's own mode again, remembered at the next start; N once more (the capital) writes nothing"

# A digit (projects/brix/engine_choice.mlx: how far the card's engines go
# at this boot), read by the same loader beside the letters: it reaches
# the kernel, holds for its one boot and leaves the display's choice as it
# is; QEMU has no card of ours, so nothing of a card's engines is read or
# written, and the kernel says so.
none_here="gpu: there is no card of ours here: nothing of a card's engines is read or written"
digit() {
    local again
    for again in 1 2 3; do
        start "$1"
        said "$none_here ($1 was pressed at this start" && return 0
        echo "     (the digit did not reach the loader at this start: once more)"
    done
    fail "a digit tapped from the machine's start until the kernel was there did not reach the loader at three starts"
}
digit 2
said "$none_here (2 was pressed at this start and holds for this boot only)" || fail "the kernel does not say the digit 2"
said "this boot: the monitor's own mode (N was pressed at an earlier start and is remembered; F while the machine starts keeps the firmware's picture)" || fail "a digit changed the display's choice"
said "brixOS: display: " && fail "the loader says a display's choice for a digit"
digit 7
said "$none_here (7 was pressed at this start; this kernel has the stages up to 4)" || fail "the kernel does not say that 7 goes as far as the stages it has"
boot
said "$none_here (no digit was pressed; 1 to 4 held while the machine starts goes further)" || fail "a digit is remembered, or the start without one is not said"
echo "ok   a digit: it reaches the kernel beside the display's choice and holds for its one boot; without a card of ours nothing of a card's engines is read or written"

# ---- 6. something remembered that is no choice ----
if [[ -n $have_shell ]]; then
    shell "setvar $variable -nv -bs -rt =0x5a5a" || fail "the firmware's shell did not run"
    boot
    said "gpu: no card of ours to set a mode on; $default" || fail "with two bytes remembered that are no choice the kernel does not go by the default"
    said "gpu: what the machine remembers is no choice of ours (2 bytes): it is passed over; F or N while the machine starts writes it anew" || fail "the kernel does not say that what is remembered is no choice (two bytes)"
    shell "setvar $variable -nv -bs -rt =0x52" || fail "the firmware's shell did not run"
    boot
    said "gpu: no card of ours to set a mode on; $default" || fail "with R's letter remembered (which is never remembered) the kernel does not go by the default"
    said "gpu: what the machine remembers is no choice of ours (1 byte, 0x52): it is passed over; F or N while the machine starts writes it anew" || fail "the kernel does not say that what is remembered is no choice (one byte)"
    boot r
    shell "dmpstore $variable" || fail "the firmware's shell did not run"
    output | grep -aq '^  00000000: 52 ' || fail "R at a start changed what was remembered, though it was no choice"
    boot f
    said "this boot: the firmware's picture is kept (F was pressed at this start and is remembered; N sets the monitor's own mode again)" || fail "F over something that is no choice is not taken"
    boot
    said "this boot: the firmware's picture is kept (F was pressed at an earlier start and is remembered; N sets the monitor's own mode again)" || fail "F written over something that is no choice is not remembered"
    said "no choice of ours" && fail "after F the kernel still says that what is remembered is no choice"
    echo "ok   something remembered that is no choice (two bytes; one byte that is no remembered key's): the default, the log says it is passed over, R leaves it, F writes it anew"
fi

# ---- 7. an image with a fixed choice ----
printf 'firmware\n' > "$work/gpu"
"$work/brix-image" "$work/kernel" "$image" "flux=$work/flux" "settings/gpu=$work/gpu" "$work/bin/"
fresh
boot
said "this boot: the firmware's picture is kept (the image's settings/gpu says so; N while the machine starts sets the monitor's own mode)" || fail "an image whose settings/gpu says firmware does not keep the firmware's picture"
boot n
boot
said "this boot: the monitor's own mode (N was pressed at an earlier start and is remembered; F while the machine starts keeps the firmware's picture)" || fail "a key remembered in the machine does not come before the image's word"
printf 'rehearsal\n' > "$work/gpu"
"$work/brix-image" "$work/kernel" "$image" "flux=$work/flux" "settings/gpu=$work/gpu" "$work/bin/"
fresh
boot
said "this boot: the firmware's own mode set again by the card's tables (the image's settings/gpu says so)" || fail "an image whose settings/gpu says rehearsal does not say so"
printf 'off\n' > "$work/gpu"
"$work/brix-image" "$work/kernel" "$image" "flux=$work/flux" "settings/gpu=$work/gpu" "$work/bin/"
boot
said "gpu: no card of ours to set a mode on; $default" || fail "an image whose settings/gpu names no choice does not go by the default"
said "gpu: the image's settings/gpu names none of native, firmware, rehearsal and ask: it is passed over" || fail "the kernel does not say that the image's word names no choice"
echo "ok   an image built with settings/gpu and no key: the image's word (firmware, rehearsal); a key remembered in the machine comes before it; a word that names none is passed over and said"

# ---- 8. Multiboot: no loader of ours ----
multiboot() {
    local tries=0
    : > "$serial"
    qemu-system-x86_64 -m 256M -cpu max -display none -monitor none -no-reboot -serial stdio -net none -kernel "$work/brix.img" < /dev/null > "$serial" 2>&1 &
    qemu_pid=$!
    while ! output | grep -aq 'gpu: .*this boot: ' || ! output | grep -aq '^brixOS: [0-9]* MiB'; do
        (( tries++ < 1800 )) || fail "the kernel did not say this boot's choice (Multiboot)"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "$work/bin/"
multiboot
output | grep -aq "^gpu: no card of ours to set a mode on; this boot: the monitor's own mode (the default)\$" || fail "under Multiboot the kernel does not say the default without a key's name"
output | grep -aq "^gpu: there is no card of ours here: nothing of a card's engines is read or written (no loader of ours read the keys)\$" || fail "under Multiboot the kernel does not say that no loader read a digit"
said "the loader looked for a key" && fail "under Multiboot the kernel says that a loader looked for a key"
printf 'firmware\n' > "$work/gpu"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "settings/gpu=$work/gpu" "$work/bin/"
multiboot
output | grep -aq "^gpu: no card of ours to set a mode on; this boot: the firmware's picture is kept (the image's settings/gpu says so)\$" || fail "under Multiboot the image's word does not count"
echo "ok   started by a Multiboot loader (no loader of ours, no key): the default, or the image's word"
echo PASS
