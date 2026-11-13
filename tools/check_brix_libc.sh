#!/usr/bin/env bash
# mlxlibc on brixOS (compiler/libc, its system calls on the native
# interface: sys_calls.brixos.mlx): a C program mlx-cc built against it
# runs in QEMU without the kernel's Linux layer:
#   - the program names /system/programs/libmlxc.so.1 as its interpreter
#     (PT_INTERP): the kernel loads the library beside it (a shared object
#     at a place of its own, AT_BASE) and starts it, which maps the
#     program's libraries, relocates and enters the program;
#   - run as `libmlxc.so.1 PROGRAM ARGS`, the library is the program the
#     kernel loads (ET_DYN) and maps PROGRAM itself;
#   - either way the program prints what compiler/cc/tests/libc.c prints
#     on the host (memory, strings, conversions, a file written and read
#     back, errno, the environment, stdio), and neither the library nor
#     the program made a call of the Linux layer (the kernel's log names
#     each such call: "linux call: PROGRAM ...").
#
#   tools/check_brix_libc.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
cc=${MLX_CC:-mlx-out/bin/mlx-cc}

if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip brixOS: no qemu-system-x86_64"
    exit 0
fi
if [[ ! -x $cc ]]; then
    echo "skip: no mlx-cc ($cc; ./mlx build cc)"
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
fail() { echo "FAIL $1" >&2; [[ -f "${2:-$work/serial.txt}" ]] && cat -v "${2:-$work/serial.txt}" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/coreutils/cat/main.mlx -o "$work/cat"
"$compiler" --quiet $brix --plugin --stack-arena --no-libc --soname=libmlxc.so.1 --entry=__mlx_start --init=__mlx_init \
    compiler/libc/libc.mlx -o "$work/libmlxc.so.1"
# The host's test program, its file in brixOS's /temporary (/tmp is the
# Linux layer's name for it).
sed 's|/tmp/mlxcc-libc-test.txt|/temporary/mlxcc-libc-test.txt|' compiler/cc/tests/libc.c > "$work/libc.c"
"$cc" --dynamic-linker=/system/programs/libmlxc.so.1 -o "$work/ctest" "$work/libc.c" 2> "$work/cc.err" || fail "mlx-cc: $(head -3 "$work/cc.err")" "$work/cc.err"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "cat=$work/cat" "libmlxc.so.1=$work/libmlxc.so.1" "ctest=$work/ctest"
echo "ok   the kernel, flux, mlxlibc for brixOS, a C program mlx-cc built on it, and a boot image build"

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
    -serial stdio -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"

# The serial output with the line editor's redraws taken out.
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# Waits (up to 30 s) until the output has $2 (default 1) lines matching $1.
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 300 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
prompts=0
# Runs the command line $2 at the next prompt as the case $1: what it
# printed goes to $work/$1.out (its exit status as the last line).
run() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "echo @@ $1; $2; echo exit \$?; echo @@ $1 ended" >&3
    wait_for "^@@ $1 ended\$"
    output | sed -n "/^@@ $1\$/,/^@@ $1 ended/p" | sed '1d;$d' | grep -v '^$' | grep -v '^root@brix' > "$work/$1.out"
}
expected() { sed "s/^argc 2 arg\$/argc 2 $1/" compiler/cc/tests/libc.expected; }

wait_for '^brixOS: kernel at 0x200000'

# (Through a pipe, as on the host: stdout fully buffered, so the write(2)
# of "direct" comes before what printf kept.)
run interpreter "ctest one | cat"
expected one > "$work/interpreter.expected"
diff "$work/interpreter.expected" "$work/interpreter.out" > "$work/interpreter.diff" || fail "the program with libmlxc.so.1 as its interpreter prints otherwise (< expected, > brixOS): $(cat "$work/interpreter.diff")"
echo "ok   a C program naming /system/programs/libmlxc.so.1 as its interpreter starts on it (the kernel loads both) and prints what it does on the host"

run direct "libmlxc.so.1 /system/programs/ctest two | cat"
expected two > "$work/direct.expected"
diff "$work/direct.expected" "$work/direct.out" > "$work/direct.diff" || fail "libmlxc.so.1 PROGRAM prints otherwise (< expected, > brixOS): $(cat "$work/direct.diff")"
echo "ok   libmlxc.so.1 run as a program (a shared object the kernel loads) maps the C program itself and runs it"

run log "cat /devices/klog"
grep -E '^linux call: (ctest|libmlxc\.so\.1) ' "$work/log.out" > "$work/linux_calls" && fail "the C library or the program used the Linux layer: $(cat "$work/linux_calls")" "$work/log.out"
grep -q '^brixOS: kernel at 0x200000' "$work/log.out" || fail "the kernel's log was not read" "$work/log.out"
echo "ok   neither mlxlibc nor the program made a call of the Linux layer"

printf 'exit\r' >&3
echo "PASS mlxlibc on brixOS: C programs on the native interface, through PT_INTERP and run directly"
