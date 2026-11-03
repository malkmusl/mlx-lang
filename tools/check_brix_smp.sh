#!/usr/bin/env bash
# brixOS on more than one processor: QEMU with -smp 4; the kernel finds
# them in ACPI's MADT and starts the other three through the local APIC
# (INIT, startup IPIs, the trampoline at layout.Ap.START); they reach long
# mode and wait there, halted, while the first one goes on (the shell
# answers).
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
fail() { echo "FAIL $1" >&2; cat -v "$work/serial.txt" >&2 || true; exit 1; }
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet --target=x86_64-brixos projects/shell/main.mlx -o "$work/mlx-sh"
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$work/mlx-sh" "$work/bin/"
echo "ok   the kernel and the image (with the trampoline) build"
{
    sleep 5
    printf 'echo the first one goes on\r'
    sleep 2
} | timeout 20 qemu-system-x86_64 -m 256M -smp 4 -cpu max -display none -serial stdio \
    -kernel "$work/brix.img" 2>&1 | tr -d '\r' > "$work/serial.txt" || true
grep -aq '^brixOS: 4 processors, 3 more started (halted)$' "$work/serial.txt" || fail "the other processors did not start"
grep -aq '^the first one goes on$' "$work/serial.txt" || fail "the shell does not answer"
echo "ok   4 processors in the MADT, the other 3 started into long mode, the shell answers"
echo PASS
