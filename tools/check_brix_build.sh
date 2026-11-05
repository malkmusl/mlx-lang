#!/usr/bin/env bash
# brixOS building brixOS: `mlx build` (std.build's runner, compiled and
# run by mlx4 in brixOS) builds the brix group there, the kernel, the
# tools, the coreutils and the boot image among them, from the part of the
# repository the group needs (build.mlx, compiler/selfhost, the assembler,
# std, projects/brix and the programs in the image; WebKit's steps are
# left out without its tree), fetched as a gzip tar archive made by our
# tar on the host. The boot image built in brixOS is byte for byte the
# one the host builds from the same files. QEMU without KVM: minutes.
#
#   tools/check_brix_build.sh [compiler]
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "skip: no qemu-system-x86_64"
    exit 0
fi
if ! command -v python3 > /dev/null; then
    echo "skip: no python3 for the host's HTTP server"
    exit 0
fi
work=$(mktemp -d)
qemu_pid=
server_pid=
cleanup() {
    exec 3>&- 2> /dev/null || true
    [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true
    [[ -n "$server_pid" ]] && kill "$server_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 40 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/www"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/mlx-sh"
"$compiler" --quiet $brix projects/wget/main.mlx -o "$work/wget"
"$compiler" --quiet $brix projects/tar/main.mlx -o "$work/tar"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix compiler/selfhost/main.mlx -o "$work/mlx4"
"$compiler" --quiet projects/tar/main.mlx -o "$work/tar-host"
"$compiler" --quiet projects/brix/tools/brixfs/main.mlx -o "$work/brixfs-host"
for program in cat wc ls sha256sum; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
# (The font mlx-console draws with, so that brixOS builds the image the
# host builds, with the font in it.)
font=()
for name in DejaVuSansMono.ttf DejaVuSans.ttf; do
    if [[ -f /usr/share/fonts/truetype/dejavu/$name ]]; then font+=("fonts/$name=/usr/share/fonts/truetype/dejavu/$name"); fi
done
# (The keymap too, when the host has the XKB data: brixOS's build takes
# the system's own, /system/settings/keyboard/us.xkb, where the host's
# takes the one it compiles.)
if [[ -f /usr/share/X11/xkb/rules/evdev ]]; then
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
    font+=("settings/keyboard/us.xkb=$work/us.xkb")
fi
"$work/brix-image" "$work/kernel" "$work/brix.img" "mlx-sh=$work/mlx-sh" "wget=$work/wget" "tar=$work/tar" "brixfs=$work/brixfs" "mlx4=$work/mlx4" "${font[@]}" "$work/bin/"
echo "ok   the kernel, wget, tar, brixfs, mlx4 for brixOS and a boot image build"

# What the brix group needs of the repository, as an archive (our tar),
# and the same files built on the host for comparison.
parts=(build.mlx VERSION mlx-out/version.mlx compiler/selfhost compiler/cc/asm.mlx compiler/cc/base.mlx std/src std/bootstrap
    projects/brix projects/shell projects/coreutils projects/wget projects/ping projects/tar projects/console projects/init projects/desktop/splash projects/desktop/shared projects/desktop/compositor projects/desktop/terminal projects/desktop/launcher projects/desktop/dock projects/desktop/topbar projects/desktop/ipc projects/desktop/login projects/desktop/greeter tools/build-number)
for list in projects/webkit/*.mlx; do parts+=("$list"); done
"$work/tar-host" -czf "$work/www/mlx.tar.gz" "${parts[@]}"
members=$("$work/tar-host" -tzf "$work/www/mlx.tar.gz" | wc -l)
size=$(stat -c %s "$work/www/mlx.tar.gz")
mkdir -p "$work/host"
"$work/tar-host" -xzf "$work/www/mlx.tar.gz" -C "$work/host"
(cd "$work/host" && MLX_BUILD_CACHE=off "$OLDPWD/$compiler" build brix -j 4 > "$work/host-build.txt" 2>&1) || { cat "$work/host-build.txt" >&2; fail "the host cannot build the brix group from the archive's files"; }
grep -q "WebKit's steps are left out" "$work/host-build.txt" || fail "the host's build did not leave WebKit out"
image_sum=$(sha256sum "$work/host/mlx-out/bin/brix/brix.img" | cut -d' ' -f1)
kernel_sum=$(sha256sum "$work/host/mlx-out/bin/brix/kernel" | cut -d' ' -f1)
steps=$(grep -o '[0-9]* built' "$work/host-build.txt" | tail -n 1)
"$work/brixfs-host" create "$work/disk0.img" 512 > /dev/null
echo "ok   the archive ($members members, $size bytes) builds the brix group on the host ($steps); a 512 MiB brixfs disk"

port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')
(cd "$work/www" && python3 -m http.server --bind 127.0.0.1 "$port" > "$work/server.txt" 2>&1) &
server_pid=$!
for i in $(seq 1 50); do
    python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.2); sys.exit(0 if s.connect_ex(('127.0.0.1',$port))==0 else 1)" && break
    sleep 0.1
done

rm -f "$work/input"
mkfifo "$work/input"
: > "$work/serial.txt"
qemu-system-x86_64 -m 2G -smp 2 -display none -monitor none -no-reboot -cpu max -serial stdio \
    -kernel "$work/brix.img" -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
    -drive "file=$work/disk0.img,format=raw,if=virtio" < "$work/input" > "$work/serial.txt" 2>&1 &
qemu_pid=$!
exec 3> "$work/input"
output() { sed 's/\x1b\[K//g' "$work/serial.txt" | tr '\r' '\n'; }
# wait_for PATTERN [COUNT [TENTHS]]: COUNT lines matching PATTERN within
# TENTHS tenths of a second (a minute unless given).
wait_for() {
    local tries=0 want=${2:-1} most=${3:-600}
    while (( $(output | grep -c -- "$1" || true) < want )); do
        (( tries++ < most )) || fail "no \"$1\" (x$want) on the serial line"
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
wait_for '^brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server 10.0.2.3 (DHCP)$'
wait_for '^brixOS: /devices/disk0, 524288 KiB'
type_line "cd /volumes/disk0 && wget http://10.0.2.2:$port/mlx.tar.gz -O mlx.tar.gz"
wait_for "^wget: mlx.tar.gz: $size bytes$" 1 1800
type_line 'tar -xzf mlx.tar.gz; echo tar $?'
wait_for '^tar 0$' 1 3000
echo "ok   the archive fetched and unpacked in brixOS"
# (Two jobs where they fit: the runner has the memory figures from
# system_info there, and build.mlx says what the big units take.)
type_line 'mlx4 build brix -j 2; echo build $?'
wait_for '^build 0$' 1 36000
wait_for "^mlx build: WebKit's steps are left out" 0 || true
grep -q "WebKit's steps are left out" <(output) || fail "the build in brixOS did not say WebKit is left out"
built=$(output | grep -o 'mlx build: [0-9]* built[^)]*)' | tail -n 1)
type_line 'sha256sum mlx-out/bin/brix/kernel mlx-out/bin/brix/brix.img'
wait_for "^$kernel_sum "
wait_for "^$image_sum "
type_line 'sync; echo synced'
wait_for '^synced$'
echo "ok   mlx build runs in brixOS ($built): the kernel and the boot image it makes are byte for byte the host's"
echo PASS
