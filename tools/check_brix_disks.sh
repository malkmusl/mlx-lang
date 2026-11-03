#!/usr/bin/env bash
# brixOS with several disks: every virtio disk is /devices/diskN and its
# volume /volumes/diskN (brixfs, or ext2 read-only); the first with a
# system on it is the system. Booted with three: a brixfs volume the
# system is installed on, an empty disk formatted and written in brixOS
# (what was written is there after another boot, on that disk), and an
# ext2 image read beside them; a snapshot of the second volume; `sync`
# commits every volume.
#
#   tools/check_brix_disks.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/source"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet projects/brix/tools/ext2/main.mlx -o "$work/ext2"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/mlx-sh"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix projects/brix/tools/snapshot/main.mlx -o "$work/snapshot"
for program in ls cat cp mkdir chmod sync stat echo; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$work/mlx-sh" "brixfs=$work/brixfs" "snapshot=$work/snapshot" "install=projects/brix/tools/install.sh" "$work/bin/"
echo "ok   the kernel and a boot image build"

head -c 8388608 /dev/zero > "$work/disk0.img"
head -c 4194304 /dev/zero > "$work/disk1.img"
echo "from ext2" > "$work/source/hello.txt"
"$work/ext2" make "$work/disk2.img" 2 "$work/source"
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
        -serial stdio -kernel "$work/brix.img" \
        -drive "file=$work/disk0.img,format=raw,if=virtio" \
        -drive "file=$work/disk1.img,format=raw,if=virtio" \
        -drive "file=$work/disk2.img,format=raw,if=virtio" < "$work/input" > "$work/serial.txt" 2>&1 &
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
wait_for() {
    local tries=0 want=${2:-1}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < 300 )) || fail "no \"$1\" (x$want) on the serial line"
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

# The first boot: three devices, the ext2 one mounted; disk0 and disk1
# formatted, the system installed on disk0, a file written on disk1.
boot
wait_for '^brixOS: /devices/disk0, 8192 KiB'
wait_for '^brixOS: /devices/disk1, 4096 KiB'
wait_for '^brixOS: /devices/disk2, 2048 KiB'
wait_for '^brixOS: /volumes/disk2 (ext2, read-only)$'
type_line 'ls /devices; cat /volumes/disk2/hello.txt; brixfs format /devices/disk0 && brixfs format /devices/disk1 && install'
wait_for 'disk0[[:space:]]*disk1[[:space:]]*disk2'
wait_for '^from ext2$'
wait_for '^brixfs: mounted at /volumes/disk1$'
wait_for '^installed on /volumes/disk0: it is the system from the next boot on$'
type_line 'echo second > /volumes/disk1/note; mkdir /volumes/disk1/d; sync; ls /volumes/disk1; echo synced'
wait_for '^d  note$'
wait_for '^synced$'
stop
echo "ok   three disks: /devices/disk0..2, the ext2 one at /volumes/disk2, two brixfs volumes formatted and written"

# The second: the system from disk0, disk1's file there, a snapshot of
# disk1, the ext2 volume still read.
boot
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: /volumes/disk1 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: / from /volumes/disk0 (the boot archive at /volumes/boot)$'
type_line 'cat /volumes/disk1/note; cat /volumes/disk2/hello.txt; echo $(stat -c %i /system) $(stat -c %i /volumes/disk0/system); snapshot -r /volumes/disk1 /volumes/kept && echo third > /volumes/disk1/note; ls /volumes; cat /volumes/kept/note; snapshot -d /volumes/kept; sync; echo done'
wait_for '^second$' 2
wait_for '^from ext2$'
wait_for '^boot  disk0  disk1  disk2  kept$'
wait_for '^done$'
inodes=$(output | grep -E '^[0-9]+ [0-9]+$' | head -n 1)
[[ -n "$inodes" && ${inodes% *} == "${inodes#* }" ]] || fail "/system is not /volumes/disk0/system ($inodes)"
stop
echo "ok   booted from disk0 with disk1's file kept; a snapshot of disk1 made and removed; ext2 read beside them"

# The third: disk1's change committed by the sync.
boot
wait_for '^brixOS: / from /volumes/disk0'
type_line 'cat /volumes/disk1/note; ls /volumes'
wait_for '^third$'
wait_for '^boot  disk0  disk1  disk2$'
stop
echo "ok   disk1's change is there after another boot"
echo PASS
