#!/usr/bin/env bash
# brixfs on a large disk: a sparse 500 GB image as the kernel's disk
# (virtio; then the same image on AHCI, with a small one on NVMe).
#
#   - formatted in brixOS, the kernel mounts it within a bound, by its own
#     line (how long the mount took, how many blocks it read), and the
#     image stays small on the host;
#   - the size and the free room asked a thousand times, by volume_info
#     and by statfs, within a bound a walk over the disk's map misses by
#     orders of magnitude (the count of free blocks was such a walk once:
#     122 million bits for each question);
#   - the free count is the one the host's brixfs check finds, after
#     files are written, changed and removed, with a snapshot and after
#     it is taken out; commits walk nothing (the kernel's count of walks
#     stays at the mount's), and the kernel's own check of its map against
#     a walk over every root finds no block that differs;
#   - a small second disk is filled until a commit fails, a file is
#     removed, and what is written then takes the blocks that became free;
#     a page changed in a commit that failed is on the disk after the
#     next one, and so is a file made after it (it does not get the inode
#     number of a file made and removed before: that number waits to be
#     freed, and the commit that frees it would free the new file);
#   - what is on the disk does not stop the kernel: six thousand files
#     made in one go and committed at once, a snapshot of them (a walk
#     over two full inode tables, again at the next mount), a file of
#     600 MiB read after a restart. (Each round of those loops made
#     records in the kernel's arena, which nothing freed before the
#     kernel was left: 4 MiB of them, and the kernel stopped.)
#   - a file written and synced is there after the machine is cut off and
#     started again, the volumes are sound by the host's check.
#
#   tools/check_brix_bigdisk.sh [compiler]
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | grep -v '^root@brix' | tail -n 30 >&2; exit 1; }

# The bounds (wide: the host is shared, and QEMU here has no KVM). A
# mount of the empty 500 GB volume takes about 50 ms, a thousand
# questions 30 ms; a walk over the map for each question took 7 s.
# Mounted again it holds six thousand files and a snapshot of them: two
# inode tables walked, 750 blocks read, about 300 ms.
MOUNT_MS=3000
AGAIN_MS=15000
ASK_US=2000000
BLOCKS=131072000

brix=--target=x86_64-brixos
mkdir -p "$work/bin"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet projects/brix/tools/brixfs/main.mlx -o "$work/brixfs-host"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix projects/brix/tools/volume/main.mlx -o "$work/volume"
"$compiler" --quiet $brix projects/brix/tools/snapshot/main.mlx -o "$work/snapshot"
for program in ls cat cp rm mkdir sync echo seq wc tail head truncate; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "brixfs=$work/brixfs" "volume=$work/volume" "snapshot=$work/snapshot" "$work/bin/"
echo "ok   the kernel and a boot image with brixfs and volume build"

# The disks: sparse (nothing of the 500 GB is written but what the
# volume holds).
truncate -s 500G "$work/big.img"
truncate -s 2M "$work/small.img"
boot() {
    rm -f "$work/input"
    mkfifo "$work/input"
    : > "$work/serial.txt"
    qemu-system-x86_64 "$@" -display none -monitor none -no-reboot -cpu max -serial stdio -kernel "$work/brix.img" < "$work/input" > "$work/serial.txt" 2>&1 &
    qemu_pid=$!
    exec 3> "$work/input"
    prompts=0
}
virtio=(-m 512M -drive "file=$work/big.img,format=raw,if=virtio" -drive "file=$work/small.img,format=raw,if=virtio")
q35=(-machine q35 -m 1G -smp 2 -drive "id=d0,file=$work/big.img,format=raw,if=none" -device ide-hd,drive=d0,bus=ide.0 -drive "id=nvm,file=$work/small.img,format=raw,if=none" -device nvme,serial=brixnvme,drive=nvm)
# The machine cut off: no shutdown, nothing committed but what was.
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
        (( tries++ < 3000 )) || fail "no \"$1\" (x$want) on the serial line"
        kill -0 "$qemu_pid" 2> /dev/null || fail "QEMU ended"
        if output | grep -q '^brixOS: the kernel stopped'; then fail "the kernel stopped"; fi
        sleep 0.1
    done
}
prompts=0
type_line() {
    prompts=$((prompts + 1))
    wait_for '^root@brix:[^#]*# $' "$prompts"
    printf '%s\r' "$1" >&3
}
# The kernel's line about a volume it mounted: "brixOS: disk0: a brixfs
# volume of N blocks, F free, read in T ms (...)"; its last one.
mounted() { output | grep "^brixOS: $1: a brixfs volume of " | tail -n 1; }
mount_ms() { mounted "$1" | sed -n 's/.* read in \([0-9]*\) ms .*/\1/p'; }
mount_free() { mounted "$1" | sed -n 's/.* blocks, \([0-9]*\) free, .*/\1/p'; }
# A figure of the line "TAG kind=1 blocks=... free=..." (volume --figures).
figure() { output | grep "^$1 kind=" | tail -n 1 | tr ' ' '\n' | sed -n "s/^$2=//p"; }
# The figures of a volume under TAG, once what changed is committed.
figures() {
    type_line "sync; echo $1 \$(volume --figures $2)"
    wait_for "^$1 kind=1 "
}
# The blocks in use the host's check finds on an image; the volume sound.
in_use() {
    "$work/brixfs-host" check "$1" > "$work/check.txt" 2>&1 || fail "the host's brixfs check of $1 finds something: $(cat "$work/check.txt")"
    grep -q '^brixfs: the volume is sound$' "$work/check.txt" || fail "the host's brixfs check does not say $1 is sound"
    sed -n 's/^brixfs: \([0-9]*\) of [0-9]* blocks in use$/\1/p' "$work/check.txt"
}
# The kernel's free count under TAG is what the host's check leaves.
same_as_host() {
    local free used
    free=$(figure "$1" free)
    used=$(in_use "$2")
    [[ -n "$free" && -n "$used" && $((free + used)) -eq $3 ]] || fail "$4: the kernel counts $free blocks free, the host's check $used in use, of $3"
}

# The first boot: both disks formatted in brixOS.
boot "${virtio[@]}"
wait_for '^brixOS: /devices/disk0, 524288000 KiB (virtio)'
wait_for '^brixOS: /devices/disk1, 2048 KiB (virtio)'
type_line 'brixfs format /devices/disk0 && brixfs format /devices/disk1 && echo formatted'
wait_for '^brixfs: mounted at /volumes/disk0$'
wait_for '^formatted$'
took=$(mount_ms disk0)
[[ -n "$took" ]] || fail "the kernel does not say how long the mount of disk0 took"
(( took <= MOUNT_MS )) || fail "the kernel took $took ms to mount the empty 500 GB volume (the bound: $MOUNT_MS ms)"
[[ $(mount_free disk0) -eq $((BLOCKS - 4)) ]] || fail "a fresh volume of $BLOCKS blocks does not have all but 4 free: $(mounted disk0)"
kib=$(du -k "$work/big.img" | cut -f 1)
(( kib <= 1024 )) || fail "formatting wrote $kib KiB of the 500 GB image"
echo "ok   a sparse 500 GB disk formatted in brixOS ($kib KiB of it written): the kernel mounts it in $took ms"

# A thousand status questions, each way.
type_line 'volume --ask 1000 /volumes/disk0'
wait_for '^statfs: asked [0-9]* of 1000 times in [0-9]* us$\|^volume: '
asked=$(output | sed -n 's/^volume_info: asked \([0-9]*\) of 1000 times in \([0-9]*\) us$/\1 \2/p' | tail -n 1)
answered=$(output | sed -n 's/^statfs: asked \([0-9]*\) of 1000 times in \([0-9]*\) us$/\1 \2/p' | tail -n 1)
[[ -n "$asked" && -n "$answered" ]] || fail "volume --ask did not answer"
[[ ${asked% *} -eq 1000 && ${asked#* } -le $ASK_US ]] || fail "volume_info: ${asked% *} of 1000 questions in ${asked#* } us (the bound: 1000 in $ASK_US us): the free count is a walk over the disk's map again?"
[[ ${answered% *} -eq 1000 && ${answered#* } -le $ASK_US ]] || fail "statfs: ${answered% *} of 1000 questions in ${answered#* } us (the bound: 1000 in $ASK_US us): the free count is a walk over the disk's map again?"
echo "ok   the size and the free room asked 1000 times: ${asked#* } us by volume_info, ${answered#* } us by statfs"

# The free count against the host's check: fresh, files written, changed
# and removed. Commits walk nothing: the walks stay at the mount's one.
figures F0 /volumes/disk0
same_as_host F0 "$work/big.img" $BLOCKS "the fresh volume"
type_line 'mkdir /volumes/disk0/d && seq 1 200000 > /volumes/disk0/d/big && cp -r /system/programs /volumes/disk0/d/p && echo one > /volumes/disk0/note && echo written'
wait_for '^written$'
figures F1 /volumes/disk0
same_as_host F1 "$work/big.img" $BLOCKS "files written"
(( $(figure F1 free) < $(figure F0 free) - 1000 )) || fail "writing the programs and a file of 1.3 MB took fewer than 1000 blocks"
type_line 'echo more >> /volumes/disk0/d/big; rm -r /volumes/disk0/d/p; echo changed'
wait_for '^changed$'
figures F2 /volumes/disk0
same_as_host F2 "$work/big.img" $BLOCKS "a file appended to, a directory removed"
(( $(figure F2 free) > $(figure F1 free) + 500 )) || fail "removing the programs freed fewer than 500 blocks"
[[ $(figure F2 exact) -eq 1 && $(figure F2 walks) -eq 1 ]] || fail "after $(figure F2 commits) commits the kernel walked the volume $(figure F2 walks) times (exact=$(figure F2 exact)): a commit should free what it replaced, without a walk"
(( $(figure F2 read) <= 200 )) || fail "the kernel read $(figure F2 read) blocks for the mount and $(figure F2 commits) commits"
echo "ok   the free count is the host's check's, fresh ($(figure F0 free)), after files are written ($(figure F1 free)) and removed ($(figure F2 free)); $(figure F2 commits) commits, the longest $(figure F2 longest_commit_us) us, no walk over the volume but the mount's, $(figure F2 read) blocks read in all"

# A snapshot keeps what the current state gives up; taken out, it is free.
type_line 'snapshot -r /volumes/disk0 /volumes/kept && rm /volumes/disk0/d/big && echo two > /volumes/disk0/note && echo snapped'
wait_for '^snapped$'
figures F3 /volumes/disk0
same_as_host F3 "$work/big.img" $BLOCKS "a snapshot made, its file removed from the current state"
(( $(figure F3 free) < $(figure F2 free) + 100 )) || fail "the snapshot does not keep the removed file's blocks"
[[ $(figure F3 exact) -eq 1 ]] || fail "with a snapshot the kernel walks the volume after each commit (no second map?)"
type_line 'cat /volumes/kept/d/big | wc -l; cat /volumes/kept/note; snapshot -d /volumes/kept; echo unsnapped'
wait_for '^200001$'
wait_for '^one$'
wait_for '^unsnapped$'
figures F4 /volumes/disk0
same_as_host F4 "$work/big.img" $BLOCKS "the snapshot taken out"
(( $(figure F4 free) > $(figure F3 free) + 300 )) || fail "taking the snapshot out freed fewer than 300 blocks"
type_line 'volume --check /volumes/disk0'
wait_for '^checked: \|^volume: '
output | grep -q "^checked: 0 blocks held that no root reaches, 0 reached and not held; $(figure F4 free) of $BLOCKS free\$" || fail "the kernel's check of its map against a walk: $(output | grep '^checked: \|^volume: ' | tail -n 1)"
echo "ok   a snapshot keeps the blocks of a file removed after it ($(figure F3 free) free), taken out they are free ($(figure F4 free)); the kernel's map is what a walk over every root marks"

# The small disk filled until a commit fails; a file removed; what is
# written then takes the blocks that became free.
type_line 'seq 1 100000 > /volumes/disk1/a; sync; cp /volumes/disk1/a /volumes/disk1/b; sync; cp /volumes/disk1/a /volumes/disk1/c; echo three'
wait_for '^three$'
figures S1 /volumes/disk1
same_as_host S1 "$work/small.img" 512 "three files on the small disk"
"$work/brixfs-host" blocks "$work/small.img" /b | sort > "$work/b.blocks"
(( $(wc -l < "$work/b.blocks") == 144 )) || fail "the file b does not have 144 blocks on the small disk"
type_line ': > /volumes/disk1/x; rm /volumes/disk1/x; echo tail >> /volumes/disk1/a; cp /volumes/disk1/c /volumes/disk1/d; sync; echo filled'
wait_for '^brixOS: the commit to disk1 failed'
wait_for '^filled$'
type_line 'echo new > /volumes/disk1/y; rm /volumes/disk1/d /volumes/disk1/b; echo removed'
wait_for '^removed$'
figures S2 /volumes/disk1
same_as_host S2 "$work/small.img" 512 "a file removed from the full disk"
(( $(figure S2 free) >= $(figure S1 free) + 140 )) || fail "removing a file of 144 blocks left $(figure S2 free) free where $(figure S1 free) were"
type_line 'cp /volumes/disk1/c /volumes/disk1/e; echo again'
wait_for '^again$'
figures S3 /volumes/disk1
same_as_host S3 "$work/small.img" 512 "a file written into the room"
"$work/brixfs-host" blocks "$work/small.img" /e | sort > "$work/e.blocks"
again=$(comm -12 "$work/b.blocks" "$work/e.blocks" | wc -l)
(( again >= 100 )) || fail "the new file took $again of the 144 blocks the removed one had"
type_line 'volume --check /volumes/disk1'
wait_for '^checked: ' 2
output | grep -q "^checked: 0 blocks held that no root reaches, 0 reached and not held; $(figure S3 free) of 512 free\$" || fail "the kernel's check of the small disk's map: $(output | grep '^checked: ' | tail -n 1)"
echo "ok   a disk of 512 blocks filled until a commit fails ($(figure S1 free) free), a file removed ($(figure S2 free) free), another written: it took $again of the removed one's 144 blocks"

# What is on the disk: many files in one commit, a snapshot of them, a
# large file (a hole but for its first block).
type_line 'mkdir /volumes/disk0/many; cd /volumes/disk0/many; for d in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do mkdir $d; for i in $(seq 1 400); do : > $d/f$i; done; done; cd /; echo x > /volumes/disk0/large; truncate -s 600M /volumes/disk0/large; echo many'
wait_for '^many$'
figures M1 /volumes/disk0
type_line 'snapshot -r /volumes/disk0 /volumes/held && rm -r /volumes/disk0/many/15 && echo held'
wait_for '^held$'
figures M2 /volumes/disk0
same_as_host M2 "$work/big.img" $BLOCKS "six thousand files, a snapshot of them, a folder removed"
[[ $(figure M2 exact) -eq 1 ]] || fail "after the snapshot of six thousand files the kernel walks the volume after each commit"
echo "ok   6000 files made in one go are committed (in $(figure M1 commit_us) us), a snapshot holds them, a folder of 400 is removed: the kernel's count is the host's check's ($(figure M2 free) free)"

# A file written and synced, then the machine cut off.
type_line 'echo kept-after-reset > /volumes/disk0/reset-note'
figures F5 /volumes/disk0
stop
used=$(in_use "$work/big.img")
[[ $((used + $(figure F5 free))) -eq $BLOCKS ]] || fail "after the cut the host's check finds $used blocks in use, the kernel had $(figure F5 free) free"
in_use "$work/small.img" > /dev/null
free=$(figure F5 free)
kib=$(du -k "$work/big.img" | cut -f 1)
(( kib <= 65536 )) || fail "$kib KiB of the 500 GB image are written"

# The second boot: mounted again, the files there.
boot "${virtio[@]}"
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: /volumes/disk1 (brixfs, generation [0-9]*)$'
took=$(mount_ms disk0)
(( took <= AGAIN_MS )) || fail "the kernel took $took ms to mount the 500 GB volume again (the bound: $AGAIN_MS ms)"
[[ $(mount_free disk0) -eq $free ]] || fail "mounted again the volume has $(mount_free disk0) blocks free, it had $free"
type_line 'cat /volumes/disk0/reset-note /volumes/disk0/note /volumes/disk1/y; tail -n 1 /volumes/disk1/a; cat /volumes/disk1/a | wc -c; cat /volumes/disk1/e | wc -c; ls /volumes/disk1; head -c 1 /volumes/disk0/large; echo; echo files $(ls /volumes/disk0/many/7 | wc -l) $(ls /volumes/held/many/15 | wc -l) $(ls /volumes/disk0/many | wc -l)'
wait_for '^kept-after-reset$'
wait_for '^two$'
wait_for '^tail$'
wait_for '^588900$'
wait_for '^588895$'
wait_for '^new$'
wait_for '^a  c  e  y$'
wait_for '^x$'
wait_for '^files 400 400 14$'
stop
in_use "$work/big.img" > /dev/null
in_use "$work/small.img" > /dev/null
echo "ok   the machine cut off and started again: the volume mounts in $took ms with $free blocks free as before, the synced file is there, the page changed in the failed commit and the file made after it too, the 600 MiB file is read, the snapshot has its 6000 files; both volumes are sound by the host's check ($kib KiB of the 500 GB written)"

# The same disks on AHCI and NVMe.
boot "${q35[@]}"
wait_for '^brixOS: /devices/disk0, 524288000 KiB (AHCI)'
wait_for '^brixOS: /volumes/disk0 (brixfs, generation [0-9]*)$'
wait_for '^brixOS: /volumes/disk1 (brixfs, generation [0-9]*)$'
took=$(mount_ms disk0)
(( took <= AGAIN_MS )) || fail "the kernel took $took ms to mount the 500 GB volume on AHCI (the bound: $AGAIN_MS ms)"
[[ $(mount_free disk0) -eq $free ]] || fail "on AHCI the volume has $(mount_free disk0) blocks free, it had $free"
type_line 'volume --ask 1000 /volumes/disk0; cat /volumes/disk0/reset-note; tail -n 1 /volumes/disk1/a; echo q35 > /volumes/disk0/note; echo nvme > /volumes/disk1/note'
wait_for '^statfs: asked 1000 of 1000 times in [0-9]* us$'
wait_for '^kept-after-reset$'
wait_for '^tail$'
figures Q0 /volumes/disk0
figures Q1 /volumes/disk1
same_as_host Q0 "$work/big.img" $BLOCKS "the volume written on AHCI"
same_as_host Q1 "$work/small.img" 512 "the small volume written on NVMe"
stop
echo "ok   the 500 GB disk on AHCI (mounted in $took ms) and the small one on NVMe: read, written, counted as the host's check counts"
echo PASS
