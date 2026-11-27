#!/usr/bin/env bash
# brixOS on many processors: QEMU (its multi-threaded TCG) with -smp 8, 16
# and 64. The kernel starts every processor the MADT lists, however many
# (smp.mlx: their trap stacks, blocks, TSSs, GDT entries and arenas made
# for the processors found, all started at once and counted as they reach
# long mode), and says "smp: N processors listed, N running"; each one
# comes into the kernel. Then N busy programs (tools/smp-spin, a process
# each, made as a tree) run on all of them: sysinfo's processor times
# (system_info's words after its record) say every processor ran
# programs for most of the time, and the shell answers a pipeline while
# every processor is busy (the kernel lock taken by all of them, nobody
# left out for good). The kernel's waits for its lock are said as sysinfo
# says them. No fault on the way, no processor that did not come up.
#
#   tools/check_brix_smp_many.sh [compiler] [counts...]   (8 16 64)
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
shift || true
counts=("$@")
(( ${#counts[@]} > 0 )) || counts=(8 16 64)
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && output | grep -av '^root@brix:[^#]*# .' | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/sysinfo/main.mlx -o "$work/sysinfo"
"$compiler" --quiet $brix tools/smp-spin/main.mlx -o "$work/smp-spin"
for program in ls wc echo seq cat; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "sysinfo=$work/sysinfo" "smp-spin=$work/smp-spin" "$work/bin/" > /dev/null
echo "ok   the kernel, the image and smp-spin build"

boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 512M -smp "$1" -cpu max -accel tcg,thread=multi -display none -monitor none -no-reboot \
        -serial stdio -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
stop() {
    exec 3>&-
    kill "$qemu_pid" 2> /dev/null || true
    wait "$qemu_pid" 2> /dev/null || true
    qemu_pid=
}
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# A line on the serial line (`want` of them), waited for up to `seconds`.
wait_for() {
    local want=${2:-1} seconds=${3:-60} tries=0
    while (( $(output | grep -ac -- "$1" || true) < want )); do
        (( tries++ < seconds * 10 )) || fail "no \"$1\" (x$want) on the serial line within $seconds s"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts" "${2:-60}"
    printf '%s\r' "$1" >&3
}

for n in "${counts[@]}"; do
    spin=$(( n > 16 ? 20 : 10 ))
    began=$SECONDS
    boot "$n"
    wait_for "^brixOS: smp: $n processors listed, $n running\$" 1 120
    booted=$((SECONDS - began))
    wait_for '^brixOS: processor [0-9]* runs$' $((n - 1)) 120
    ran=$(output | grep -a '^brixOS: processor [0-9]* runs$' | sort -u | wc -l)
    (( ran == n - 1 )) || fail "$((n - 1)) processors besides the first do not each say they run ($ran do)"
    output | grep -aq 'did not come up\|memory for [0-9]* more' && fail "a processor did not come up"
    echo "ok   $n processors: \"smp: $n processors listed, $n running\" after ${booted} s, each one in the kernel"

    # N busy programs, a pipeline while they run, then where they ran.
    type_line "smp-spin $n $spin &" 120
    sleep 3
    type_line 'ls / | wc -l; seq 1 2000 | cat | wc -l; echo answered' 120
    wait_for '^answered$' 1 120
    output | grep -aq '^2000$' || fail "the pipeline under load did not give 2000"
    type_line 'wait; echo spun' 120
    wait_for '^spun$' 1 $((spin + 120))
    type_line 'sysinfo' 120
    wait_for '^processor times:' 1 120
    output | grep -aq "^processors: $n\$" || fail "sysinfo does not say $n processors"
    line=$(output | grep -a '^processors that ran programs: ' | tail -n 1)
    [[ $line =~ ^processors\ that\ ran\ programs:\ ([0-9]+)\ of\ ([0-9]+)\;\ the\ least\ ([0-9]+)\.[0-9]{3}\ s,\ the\ most\ ([0-9]+)\.[0-9]{3}\ s$ ]] || fail "sysinfo does not say where the programs ran: $line"
    (( BASH_REMATCH[1] == n && BASH_REMATCH[2] == n )) || fail "not every processor ran programs: $line"
    (( BASH_REMATCH[3] * 2 >= spin )) || fail "a processor ran programs for less than half of the ${spin} s every one was busy: $line"
    output | grep -a '^waits for the kernel: ' | tail -n 1 | sed "s/^/     ($n processors) /"
    output | grep -aq 'brixOS: page fault\|crashed\|kernel stopped\|every process waits' && fail "a fault on the way"
    stop
    echo "ok   $n processors: $n busy programs spread over all of them (${line#processors that ran programs: }), the shell answered meanwhile"
done
echo PASS
