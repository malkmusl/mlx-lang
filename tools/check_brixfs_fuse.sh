#!/usr/bin/env bash
# brixfs-fuse: the Linux driver (projects/brix/tools/brixfs-fuse) mounts a
# brixfs volume that brixOS wrote and serves it read-only over /dev/fuse,
# our own FUSE implementation, no libfuse or C. brixOS formats a disk as
# brixfs, writes files and a copy of the kernel log, and syncs; the driver
# then mounts the volume on the host and ls/cat read the files back byte
# for byte -- so the files brixOS keeps (the boot log, anything) are
# reachable from Linux without brixOS speaking a foreign filesystem.
#
#   tools/check_brixfs_fuse.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"; exit 0
fi
if [[ ! -w /dev/fuse ]]; then
    echo "skip: no writable /dev/fuse (need FUSE and privileges to mount)"; exit 0
fi
work=$(mktemp -d)
qemu_pid=
fuse_pid=
cleanup() {
    exec 3>&- 2>/dev/null || true
    [[ -n "$fuse_pid" ]] && kill -9 "$fuse_pid" 2>/dev/null || true
    umount "$work/mnt" 2>/dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2>/dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && tr -d '\r' < "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet projects/brix/tools/brixfs-fuse/main.mlx -o "$work/brixfs-fuse"
for p in cat echo mkdir sync; do
    "$compiler" --quiet $brix "projects/coreutils/$p/main.mlx" -o "$work/bin/$p"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "brixfs=$work/brixfs" "$work/bin/"
echo "ok   the kernel, brixfs, the FUSE driver and a boot image build"

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
type_line 'echo brixfs-fuse-ok > /volumes/disk0/marker.txt'
type_line 'sync'
type_line 'echo SYNCED'
wait_for '^SYNCED$'
exec 3>&-; kill "$qemu_pid" 2>/dev/null || true; wait "$qemu_pid" 2>/dev/null || true; qemu_pid=
echo "ok   brixOS formatted a brixfs volume and wrote files and the kernel log"

mkdir -p "$work/mnt"
"$work/brixfs-fuse" "$work/vol.img" "$work/mnt" &
fuse_pid=$!
mounted=
for _ in $(seq 1 50); do
    if mountpoint -q "$work/mnt" 2>/dev/null || grep -q "$work/mnt" /proc/mounts 2>/dev/null; then mounted=1; break; fi
    kill -0 "$fuse_pid" 2>/dev/null || fail "the driver exited before the mount came up"
    sleep 0.1
done
[[ -n "$mounted" ]] || fail "the driver did not mount the volume"
ls "$work/mnt" | grep -q 'marker.txt' || fail "the driver did not list marker.txt"
ls "$work/mnt" | grep -q 'logs' || fail "the driver did not list the logs directory"
[[ "$(cat "$work/mnt/marker.txt")" == "brixfs-fuse-ok" ]] || fail "marker.txt content mismatch"
ls "$work/mnt/logs" | grep -q 'boot.log' || fail "the driver did not find logs/boot.log"
cat "$work/mnt/logs/boot.log" | grep -q '^brixOS: ' || fail "boot.log does not look like the kernel log"
umount "$work/mnt" 2>/dev/null || true
sleep 0.3
kill -9 "$fuse_pid" 2>/dev/null || true; fuse_pid=
echo "ok   the Linux driver mounts the volume and reads the files back, the kernel log included"
echo "PASS brixfs-fuse: a brixfs volume brixOS wrote is mounted and read on Linux"
