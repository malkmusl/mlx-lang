#!/usr/bin/env bash
# brixfs-host: the host-side brixfs reader (tools/brixfs-host.py) reads a
# volume brixOS wrote. brixOS formats a disk as brixfs, writes files and a
# copy of the kernel log, and syncs; the reader then lists them and reads
# them back byte for byte on the host -- so the files brixOS keeps (the
# boot log, anything) are reachable from Linux without brixOS speaking a
# foreign filesystem.
#
#   tools/check_brixfs_host.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"; exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3"; exit 0
fi
work=$(mktemp -d)
qemu_pid=
cleanup() { exec 3>&- 2>/dev/null || true; [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2>/dev/null || true; rm -rf -- "$work"; }
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && tr -d '\r' < "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
for p in cat echo mkdir sync; do
    "$compiler" --quiet $brix "projects/coreutils/$p/main.mlx" -o "$work/bin/$p"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "brixfs=$work/brixfs" "$work/bin/"
echo "ok   the kernel, brixfs and a boot image build"

head -c 16777216 /dev/zero > "$work/vol.img"
rm -f "$work/input"; mkfifo "$work/input"; : > "$work/serial.txt"
qemu-system-x86_64 -m 512M -display none -monitor none -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -drive "file=$work/vol.img,format=raw,if=virtio" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
wait_for() {
    local tries=0
    while ! grep -q -- "$1" <(output); do
        (( tries++ < 400 )) || fail "no \"$1\" on the serial line"
        kill -0 "$qemu_pid" 2>/dev/null || fail "QEMU ended"
        sleep 0.1
    done
}
type_line() { wait_for '^root@brix:[^#]*# $'; printf '%s\r' "$1" >&3; sleep 0.3; }
wait_for '^root@brix:'
type_line 'brixfs format /devices/disk0'
wait_for '^brixfs: mounted at /volumes/disk0$'
type_line 'mkdir /volumes/disk0/logs'
type_line 'cat /devices/klog > /volumes/disk0/logs/boot.log'
type_line 'echo brixfs-host-ok > /volumes/disk0/marker.txt'
type_line 'sync'
type_line 'echo SYNCED'
wait_for '^SYNCED$'
exec 3>&-; kill "$qemu_pid" 2>/dev/null || true; wait "$qemu_pid" 2>/dev/null || true; qemu_pid=
echo "ok   brixOS formatted a brixfs volume and wrote files and the kernel log"

reader="python3 -I tools/brixfs-host.py $work/vol.img"
$reader ls / | grep -q 'marker.txt' || fail "the reader did not list marker.txt"
$reader ls / | grep -q 'logs' || fail "the reader did not list the logs directory"
[[ "$($reader cat /marker.txt)" == "brixfs-host-ok" ]] || fail "marker.txt content mismatch"
$reader ls /logs | grep -q 'boot.log' || fail "the reader did not find logs/boot.log"
$reader cat /logs/boot.log | grep -q '^brixOS: ' || fail "boot.log does not look like the kernel log"
echo "ok   the host reader lists and reads back the files, including the kernel log"
echo "PASS brixfs-host: a brixfs volume brixOS wrote is read from the host"
