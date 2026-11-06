#!/usr/bin/env bash
# See brixOS without a display: boot the built image headless and write PNG
# screenshots of the screen through QEMU's monitor (no window, no VNC, so
# it works over SSH on a headless box). The greeter and, after the password
# is typed, the desktop are captured.
#
#   ./mlx build brix          # builds mlx-out/bin/brix/brix.img
#   tools/brix-shots.sh [OUTDIR] [IMAGE]
#
# Writes OUTDIR/01-greeter.png and OUTDIR/02-desktop.png (default OUTDIR:
# mlx-out/shots). The account is brix, its password "brix".
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
out=${1:-mlx-out/shots}
image=${2:-mlx-out/bin/brix/brix.img}
mkdir -p "$out"
if [[ ! -f $image ]]; then
    echo "no image at $image — run ./mlx build brix first" >&2
    exit 1
fi
if ! command -v qemu-system-x86_64 > /dev/null; then echo "need qemu-system-x86_64" >&2; exit 1; fi
if ! command -v python3 > /dev/null; then echo "need python3 for the monitor" >&2; exit 1; fi

work=$(mktemp -d)
qemu_pid=
cleanup() { [[ -n "$qemu_pid" ]] && kill "$qemu_pid" 2> /dev/null || true; rm -rf -- "$work"; }
trap cleanup EXIT
truncate -s 64M "$work/disk.img"

qemu-system-x86_64 -m 1G -cpu max -display none -serial file:"$work/serial.txt" \
    -monitor "unix:$work/mon,server,nowait" -no-reboot \
    -kernel "$image" -drive file="$work/disk.img",format=raw,if=virtio &
qemu_pid=$!

python3 - "$work/mon" "$out" <<'PY'
import socket, sys, time, os
mon, out = sys.argv[1], sys.argv[2]
for _ in range(100):
    if os.path.exists(mon): break
    time.sleep(0.1)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect(mon); s.settimeout(1)
def drain():
    try:
        while s.recv(4096): pass
    except Exception: pass
drain()
def cmd(c):
    s.sendall((c + "\n").encode()); time.sleep(0.3); drain()
def dump(name):
    p = os.path.abspath(os.path.join(out, name + ".ppm"))
    cmd("screendump %s" % p); time.sleep(0.4)
    png = p[:-4] + ".png"
    try:
        from PIL import Image
        Image.open(p).save(png); os.remove(p)
        print("wrote", png)
    except Exception:
        print("wrote", p, "(install python3-pil for PNG)")
# The greeter.
time.sleep(14)
dump("01-greeter")
# Log in (the account brix, password brix), then the desktop.
for key in ["b", "r", "i", "x", "ret"]:
    cmd("sendkey %s" % key); time.sleep(0.3)
time.sleep(10)
dump("02-desktop")
PY
echo "done: screenshots in $out/"
