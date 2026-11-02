#!/usr/bin/env bash
# brixOS (projects/brix) end to end: the kernel (--target=x86_64-freestanding)
# and brix-image build, the image boots in QEMU (the test machine; nothing
# of it is in what is built), the kernel says itself on the serial line and
# starts mlx-sh from the boot archive in user mode; commands typed at its
# prompt run (builtins, arithmetic, the working directory, a glob over the
# archive's files), `exit 3` ends the shell and the kernel starts it again,
# and no system call it made was missing.
#
#   tools/check_brix.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip brixOS: no qemu-system-x86_64"
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

shell=mlx-out/bin/mlx-sh
[[ -x $shell ]] || { "$compiler" --quiet projects/shell/main.mlx -o "$work/mlx-sh"; shell=$work/mlx-sh; }
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$shell"
echo "ok   the kernel, brix-image and the boot image build"

mkfifo "$work/input"
qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
    -serial stdio -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"

# The serial output with the line editor's redraws taken out: carriage
# returns as line ends, erase-to-end-of-line sequences dropped.
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# Waits (up to 20 s) until the output has $2 (default 1) lines matching $1.
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 200 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
# Types $1 and Enter at the next prompt.
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:~# $' "$prompts"
    printf '%s\r' "$1" >&3
}

wait_for '^brixOS: kernel at 0x200000'
wait_for '^brixOS: [0-9]* MiB, [0-9]* MHz$'
echo "ok   the kernel boots and says itself on the serial line"

type_line 'echo hello brix'
wait_for '^hello brix$'
echo "ok   mlx-sh runs in user mode and its builtins answer"

type_line 'echo $((6 * 7)) $(( 1 << 10 ))'
wait_for '^42 1024$'
echo "ok   arithmetic expansion"

type_line 'pwd; cd /; echo *'
wait_for '^/$'
wait_for '^mlx-sh$'
echo "ok   the working directory and a glob over the boot archive"

type_line 'x=brix; echo ${x}OS ${#x}'
wait_for '^brixOS 4$'
echo "ok   variables"

type_line 'exit 3'
wait_for '^brixOS: the shell exited (3)$'
type_line 'echo again'
wait_for '^again$'
echo "ok   exit ends the shell and the kernel starts it again"

if output | grep -q '^\[brixOS: system call'; then
    fail "system calls mlx-sh made are missing: $(output | grep '^\[brixOS: system call' | tr '\n' ' ')"
fi
echo "ok   no system call mlx-sh made was missing"
