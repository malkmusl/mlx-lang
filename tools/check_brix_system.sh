#!/usr/bin/env bash
# brixOS booted from its disk: a brixfs volume formatted in brixOS,
# `install` copies the system onto it (its programs, settings, /users,
# /temporary) and commits; booted again, the kernel takes the volume as
# the system (/system, /users and /temporary are the disk's, the same
# nodes as /volumes/disk0's; the boot archive's programs at /volumes/boot),
# programs run from the disk, a file written in /users/root is there after
# yet another boot. Users and permissions: files keep their owner and
# group on the disk, and the kernel checks modes against the user a
# process runs as (`as USER PROGRAM` runs one as another user).
#
#   tools/check_brix_system.sh [compiler]
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
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/mlx-sh"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix projects/brix/tools/as/main.mlx -o "$work/as"
for program in ls cat cp mkdir chmod chown sync stat uname echo seq wc rm; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$work/mlx-sh" "brixfs=$work/brixfs" "as=$work/as" "install=projects/brix/tools/install.sh" "$work/bin/"
echo "ok   the kernel and a boot image with install build"

head -c 8388608 /dev/zero > "$work/disk.img"
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 -m 256M -display none -monitor none -no-reboot -cpu max \
        -serial stdio -kernel "$work/brix.img" -drive "file=$work/disk.img,format=raw,if=virtio" < "$work/input" > "$work/serial.txt" 2>&1 &
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

# The first boot: from the archive; the volume formatted, the system
# installed.
boot
wait_for '^brixOS: /devices/disk0, 8192 KiB'
type_line 'brixfs format /devices/disk0 && install'
wait_for '^installed on /volumes/disk0: it is the system from the next boot on$'
type_line 'ls /volumes/disk0; ls /volumes/disk0/system/programs | wc -l; sync; echo synced'
wait_for '^system[[:space:]]*temporary[[:space:]]*users$'
wait_for '^synced$'
stop
echo "ok   brixfs formatted in brixOS, install copies the system onto it"

# The second: from the disk.
boot
wait_for '^brixOS: / from /volumes/disk0 (the boot archive at /volumes/boot)$'
type_line 'echo $(stat -c %i /system) $(stat -c %i /volumes/disk0/system); ls /volumes/boot; cat /system/settings/users'
wait_for '^root:x:0:0:root:/users/root:/system/programs/mlx-sh$'
type_line 'uname -o; echo kept > /users/root/note; cd /temporary && seq 3 > t && cat t | wc -l; sync; echo written'
wait_for '^brixOS$'
wait_for '^3$'
wait_for '^written$'
output | grep -q '^programs  settings$' || fail "the boot archive is not at /volumes/boot"
inodes=$(output | grep -E '^[0-9]+ [0-9]+$' | head -n 1)
[[ -n "$inodes" && ${inodes% *} == "${inodes#* }" ]] || fail "/system and /volumes/disk0/system are not the same node ($inodes)"
# Users: a file of user 7, mode 600; user 7 reads and writes it, user 8
# may not; a directory of user 7, mode 700, user 8 cannot list; the
# owner and group reach the disk.
type_line 'echo secret > /temporary/s && chmod 600 /temporary/s && chown 7:3 /temporary/s && stat -c %u:%g /temporary/s && as 7 cat /temporary/s; as 8 cat /temporary/s; echo denied $(( $? != 0 ))'
wait_for '^7:3$'
wait_for '^secret$'
wait_for '^denied 1$'
type_line 'as 7 mlx-sh -c "echo changed > /temporary/s"; cat /temporary/s; as 8 mlx-sh -c "echo again > /temporary/s"; echo refused $(( $? != 0 )); cat /temporary/s'
wait_for '^changed$' 2
wait_for '^refused 1$'
type_line 'mkdir /temporary/d && chmod 700 /temporary/d && chown 7 /temporary/d && as 7 mlx-sh -c "echo in > /temporary/d/f" && as 7 cat /temporary/d/f; as 8 ls /temporary/d; echo listed $(( $? != 0 )); as 8 chmod 777 /temporary/d; echo chmod $(( $? != 0 )); as 8 as 7 cat /temporary/d/f; echo became $(( $? != 0 )); sync; echo synced'
wait_for '^in$'
wait_for '^listed 1$'
wait_for '^chmod 1$'
wait_for '^became 1$'
wait_for '^synced$'
stop
echo "ok   booted from the disk: /system is the volume's, programs run from it, the boot archive at /volumes/boot; users and modes are checked"

# The third: what was written is there.
boot
wait_for '^brixOS: / from /volumes/disk0'
type_line 'cat /users/root/note; ls /temporary; stat -c %u:%g /temporary/s /temporary/d'
wait_for '^kept$'
wait_for '^d  s  t$'
wait_for '^7:3$'
wait_for '^7:0$'
stop
echo "ok   a file written in /users/root is there after another boot, with its owner and group"
echo PASS
