#!/usr/bin/env bash
# brixOS building its own compiler: the repository's compiler and std
# (compiler/selfhost, std/src, std/bootstrap, the version module) go into
# brixOS as a gzip tar archive made by our tar on the host and served by
# an HTTP server there; in brixOS wget fetches it onto a brixfs volume,
# tar unpacks it, mlx4 (built for brixOS, in the boot archive) compiles a
# program that runs, then compiles itself; the compiler it made is byte
# for byte the one the host built for brixOS, and compiles the program
# again. QEMU without KVM: the self-build takes a few minutes.
#
#   tools/check_brix_selfhost.sh [compiler]
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
fail() { echo "FAIL $1" >&2; [[ -f "$work/serial.txt" ]] && cat -v "$work/serial.txt" | tail -n 30 >&2; exit 1; }

brix=--target=x86_64-brixos
mkdir -p "$work/bin" "$work/www"
"$compiler" --quiet --target=x86_64-freestanding projects/brix/kernel/main.mlx -o "$work/kernel"
"$compiler" --quiet projects/brix/image/main.mlx -o "$work/brix-image"
"$compiler" --quiet $brix projects/shell/main.mlx -o "$work/flux"
"$compiler" --quiet $brix projects/wget/main.mlx -o "$work/wget"
"$compiler" --quiet $brix projects/tar/main.mlx -o "$work/tar"
"$compiler" --quiet $brix projects/brix/tools/brixfs/main.mlx -o "$work/brixfs"
"$compiler" --quiet $brix compiler/selfhost/main.mlx -o "$work/mlx4"
"$compiler" --quiet projects/tar/main.mlx -o "$work/tar-host"
"$compiler" --quiet projects/brix/tools/brixfs/main.mlx -o "$work/brixfs-host"
for program in cat wc ls sha256sum; do
    "$compiler" --quiet $brix "projects/coreutils/$program/main.mlx" -o "$work/bin/$program"
done
"$work/brix-image" "$work/kernel" "$work/brix.img" "flux=$work/flux" "wget=$work/wget" "tar=$work/tar" "brixfs=$work/brixfs" "mlx4=$work/mlx4" "$work/bin/"
echo "ok   the kernel, wget, tar, brixfs, mlx4 for brixOS and a boot image build"

# The compiler's sources as an archive (our tar), a program to compile,
# and what to compare in brixOS.
"$work/tar-host" -czf "$work/www/mlx.tar.gz" compiler/selfhost std/src std/bootstrap mlx-out/version.mlx
members=$("$work/tar-host" -tzf "$work/www/mlx.tar.gz" | wc -l)
size=$(stat -c %s "$work/www/mlx.tar.gz")
cat > "$work/www/hello.mlx" <<'PROGRAM'
const std = @import("std")
pub fn main(arguments: [][*]const u8) -> u8 {
    if std.process.count(arguments) == 0 { return 1 }
    return if std.io.print("hello from brixOS\n") { 0 } else { 1 }
}
PROGRAM
sample=$(find std/src -type f -printf '%s %p\n' | sort -n | tail -n 1 | cut -d' ' -f2)
sample_sum=$(sha256sum "$sample" | cut -d' ' -f1)
compiler_sum=$(sha256sum "$work/mlx4" | cut -d' ' -f1)
"$work/brixfs-host" create "$work/disk0.img" 256 > /dev/null
echo "ok   the archive ($members members, $size bytes), a 256 MiB brixfs disk"

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
qemu-system-x86_64 -m 1G -smp 2 -display none -monitor none -no-reboot -cpu max -serial stdio \
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
wait_for '^brixOS: /devices/disk0, 262144 KiB'
type_line "cd /volumes/disk0 && wget http://10.0.2.2:$port/mlx.tar.gz -O mlx.tar.gz"
wait_for "^wget: mlx.tar.gz: $size bytes$" 1 1800
type_line 'tar -xzf mlx.tar.gz; echo tar $?'
wait_for '^tar 0$' 1 3000
type_line "tar -tzf mlx.tar.gz | wc -l; sha256sum $sample"
wait_for "^$members$" 1 1800
wait_for "^$sample_sum "
echo "ok   wget fetches the archive onto the brixfs volume, tar unpacks it (the members listed, a file's bytes as on the host)"
type_line "wget http://10.0.2.2:$port/hello.mlx -O hello.mlx && mlx4 --quiet -o hello hello.mlx; echo mlx4 \$?"
wait_for '^mlx4 0$' 1 1800
type_line './hello'
wait_for '^hello from brixOS$'
echo "ok   mlx4 compiles a program in brixOS (its default target: brixOS), which runs"
type_line 'mlx4 --quiet -o mlx4-2 compiler/selfhost/main.mlx; echo self $?'
wait_for '^self 0$' 1 9000
type_line './mlx4-2 --version; sha256sum mlx4-2'
wait_for '^mlx4 0\.[0-9]*\.[0-9]*'
wait_for "^$compiler_sum "
type_line './mlx4-2 --quiet -o hello2 hello.mlx && ./hello2'
wait_for '^hello from brixOS$' 2 1800
type_line 'sync; echo synced'
wait_for '^synced$'
echo "ok   mlx4 builds itself in brixOS, byte for byte the host's build for brixOS; the compiler it made compiles the program"
echo PASS
