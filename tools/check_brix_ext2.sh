#!/usr/bin/env bash
# brixOS reads ext2, the format it exchanges disks with Linux in:
# projects/brix/tools/ext2 makes an image of a directory (e2fsck, when
# the host has it, finds nothing to say about it), brixOS boots with it as
# its disk and mounts it read-only at /volumes/disk0: a directory listed,
# files read (one past its direct blocks), a symbolic link followed (one
# in the inode, one in a block), a directory deeper down, a write refused.
#
#   tools/check_brix_ext2.sh [compiler]
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

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/source/docs/deep"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet projects/brix/tools/ext2/main.mlx -o "$work/ext2"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
for program in ls cat wc tail readlink; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "$work/bin/"
echo "ok   the kernel, the ext2 tool and a boot image build"

echo "hello ext2" > "$work/source/hello.txt"
seq 1 30000 > "$work/source/docs/numbers"
echo deep > "$work/source/docs/deep/file"
ln -s hello.txt "$work/source/link"
ln -s "docs/deep/$(printf 'x%.0s' $(seq 1 70))" "$work/source/longlink"
"$work/ext2" make "$work/disk.img" 4 "$work/source"
if command -v e2fsck > /dev/null || [[ -x /usr/sbin/e2fsck ]]; then
    fsck=$(command -v e2fsck || echo /usr/sbin/e2fsck)
    "$fsck" -fn "$work/disk.img" > "$work/fsck.txt" 2>&1 || fail "e2fsck finds something: $(cat "$work/fsck.txt")"
    echo "ok   ext2 makes an image of a directory, e2fsck finds nothing to say"
else
    echo "ok   ext2 makes an image of a directory (no e2fsck here to ask)"
fi

{
    sleep 4
    printf 'ls /volumes/disk0; cat /volumes/disk0/hello.txt; wc -l /volumes/disk0/docs/numbers; tail -1 /volumes/disk0/docs/numbers; cat /volumes/disk0/link; readlink /volumes/disk0/longlink; cat /volumes/disk0/docs/deep/file; echo x > /volumes/disk0/new; echo done\r'
    sleep 5
} | timeout 20 qemu-system-x86_64 -m 256M -cpu max -display none -serial stdio \
    -kernel "$work/brix.img" -drive "file=$work/disk.img,format=raw,if=virtio" 2>&1 | tr -d '\r' > "$work/serial.txt" || true
for expected in \
    '^brixOS: /volumes/disk0 (ext2, read-only)$' \
    '^docs  hello.txt  link  longlink  lost+found$' \
    '^hello ext2$' \
    '^30000 /volumes/disk0/docs/numbers$' \
    '^30000$' \
    "^docs/deep/x\\{70\\}\$" \
    '^deep$' \
    'cannot open /volumes/disk0/new' \
    '^done$'; do
    grep -q "$expected" "$work/serial.txt" || fail "no \"$expected\" on the serial line"
done
[[ $(grep -c '^hello ext2$' "$work/serial.txt") == 2 ]] || fail "the link does not lead to hello.txt"
echo "ok   brixOS mounts it read-only: a directory, files (past the direct blocks), links, a write refused"
echo PASS
