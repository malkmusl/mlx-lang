#!/usr/bin/env bash
# brixOS on more than one processor: QEMU with -smp 4; the kernel finds
# them in ACPI's MADT and starts the other three through the local APIC
# (INIT, startup IPIs, the trampoline at layout.Ap.START); they come into
# the kernel (brix_ap), each with a trap stack, TSS and block of its own
# and its local APIC timer, and run processes beside the first one (the
# kernel itself on one processor at a time, by its lock): a pipeline's
# programs, background jobs, a shell restart while others run.
#
#   tools/check_brix_smp.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; cat -v "$work/serial.txt" | tail -n 40 >&2 || true; exit 1; }
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet --target=x86_64-brixos projects/shell/main.mlx -o "$work/flux"
for program in seq wc cat ls echo; do
    "$compiler" --quiet --target=x86_64-brixos "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$compiler" --quiet --target=x86_64-brixos projects/brix/tools/sysinfo/main.mlx -o "$work/sysinfo"
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "sysinfo=$work/sysinfo" "$work/bin/"
echo "ok   the kernel and the image (with the trampoline) build"
{
    sleep 5
    printf 'seq 1 100000 | wc -l; echo done\r'
    sleep 3
    printf 'ls / | wc -l; seq 1 50000 | cat | cat | wc -l; (seq 1 20000 | wc -l) & (seq 1 30000 | wc -l) & wait; echo waited\r'
    sleep 5
    printf 'sysinfo\r'
    sleep 2
} | timeout 32 qemu-system-x86_64 -m 256M -smp 4 -cpu max -display none -serial stdio \
    -kernel "$work/brix.img" 2>&1 | tr -d '\r' > "$work/serial.txt" || true
grep -aq '^brixOS: 4 processors, 3 more started$' "$work/serial.txt" || fail "the other processors did not start"
for cpu in 1 2 3; do
    grep -aq "^brixOS: processor $cpu runs$" "$work/serial.txt" || fail "processor $cpu does not run the kernel"
done
for expected in '^100000$' '^done$' '^5$' '^50000$' '^20000$' '^30000$' '^waited$'; do
    grep -aq "$expected" "$work/serial.txt" || fail "no \"$expected\" on the serial line"
done
# (The kernel's report of one, not sysinfo's count of the page faults served.)
grep -aq 'brixOS: page fault\|crashed\|every process waits' "$work/serial.txt" && fail "a fault on the way"
echo "ok   4 processors run the kernel: pipelines and background jobs across them give the right answers"
# The waits for the kernel as sysinfo says them (system_info's words 8 to
# 13): the pipelines above made processors wait for it, and each wait has
# a holder.
grep -aq '^processors: 4$' "$work/serial.txt" || fail "sysinfo does not say 4 processors"
grep -aEq '^waits for the kernel: [0-9]+\.[0-9]{3} ms in all; the longest [0-9]+\.[0-9]{3} ms \(behind [^)]*\); in the last 10 to 20 s [0-9]+\.[0-9]{3} ms \(behind [^)]*\)$' "$work/serial.txt" || fail "sysinfo does not say the waits for the kernel and their holders"
grep -aEq "^idle processors' waits for the kernel: [0-9]+\.[0-9]{3} ms\$" "$work/serial.txt" || fail "sysinfo does not say the idle processors' waits"
echo "ok   sysinfo says how long processors waited for the kernel, and behind what"
echo PASS
