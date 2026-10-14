#!/usr/bin/env bash
# End-to-end check of mlx-lock against the nested compositor.  A small
# org.mlx.Login stand-in accepts the second password only; neither it nor
# the locker may print either password.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
command -v xkbcli > /dev/null || { echo "check_lock.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
bus_pid=
login_pid=
host_pid=
cleanup() {
    [[ -z "$host_pid" ]] || kill "$host_pid" 2> /dev/null || true
    [[ -z "$login_pid" ]] || kill "$login_pid" 2> /dev/null || true
    [[ -z "$bus_pid" ]] || kill "$bus_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT

export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx"
printf 'color-scheme = dark\n' > "$XDG_CONFIG_HOME/mlx/appearance.conf"

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet projects/desktop/lock/main.mlx -o "$work/mlx-lock"
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet tools/lock-login-test/main.mlx -o "$work/lock-login-test"
"$compiler" --quiet tools/lock-login-test/client.mlx -o "$work/lock-login-client"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

"$work/mlx-ipcd" --address "$work/system-bus" > "$work/ipc.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$work/system-bus" ]] && break; sleep 0.1; done
[[ -S "$work/system-bus" ]] || { echo "mlx-ipcd did not open its socket" >&2; exit 1; }

DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" "$work/lock-login-test" > "$work/login.log" 2>&1 &
login_pid=$!
for _ in $(seq 1 50); do grep -q '^lock-login-test: ready$' "$work/login.log" 2> /dev/null && break; sleep 0.1; done
grep -q '^lock-login-test: ready$' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }

DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" "$work/lock-login-client" > "$work/client.log" 2>&1
grep -qx 'lock-login-client: allowed and wiped' "$work/client.log" || { cat "$work/client.log" >&2; exit 1; }

cat > "$work/lock.script" <<SCRIPT
wait 2500
shot $work/dark.ppm
wait 2200
shot $work/light.ppm
type wrong
enter
wait 800
shot $work/denied.ppm
type right-pass
enter
wait 1200
shot $work/after.ppm
close
SCRIPT

( sleep 3.5; printf 'color-scheme = light\n' > "$XDG_CONFIG_HOME/mlx/appearance.conf" ) &
"$work/test-host" host-lock "$work/us.xkb" "$work/lock.script" > "$work/host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-lock" ]] && break; sleep 0.1; done

USER=tester LANG=de_DE.UTF-8 DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" \
    WAYLAND_DISPLAY=host-lock timeout 30 "$work/mlx-compositor" --verbose --renderer cpu \
    --socket nested-lock --no-fps --run "$work/mlx-lock" > "$work/compositor.log" 2>&1
wait "$host_pid" || { cat "$work/host.log" "$work/compositor.log" >&2; exit 1; }
host_pid=

grep -qx 'unlock tester 5 bytes: denied' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }
grep -qx 'unlock tester 10 bytes: allowed' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }
for line in 'mlx-lock: locked' 'mlx-lock: authentication failed' 'mlx-lock: unlocked' 'lock: locked' 'lock: unlocked'; do
    grep -qx "$line" "$work/compositor.log" || { echo "check_lock: missing $line" >&2; cat "$work/compositor.log" >&2; exit 1; }
done
! grep -Fq 'wrong' "$work/login.log" "$work/compositor.log" "$work/ipc.log" || { echo 'check_lock: a password reached a log' >&2; exit 1; }
! grep -Fq 'right-pass' "$work/login.log" "$work/compositor.log" "$work/ipc.log" || { echo 'check_lock: a password reached a log' >&2; exit 1; }
grep -aFq 'Could not unlock. Please try again.' "$work/mlx-lock"
grep -aFq 'Entsperren fehlgeschlagen. Bitte erneut versuchen.' "$work/mlx-lock"

python3 - "$work" <<'PY'
import pathlib, sys

work = pathlib.Path(sys.argv[1])
def load(name):
    data = (work / f"{name}.ppm").read_bytes()
    _, size, _, pixels = data.split(b"\n", 3)
    width, height = map(int, size.split())
    return width, height, pixels

dark = load("dark")
light = load("light")
denied = load("denied")
after = load("after")
assert dark[:2] == light[:2] == denied[:2] == after[:2]
w, h = dark[:2]
def pixel(image, x, y):
    data = image[2]
    at = (y * w + x) * 3
    return data[at:at + 3]

# The appearance watcher repaints the full-screen lock, the rejected
# attempt leaves it in place, and success reveals the compositor again.
assert pixel(dark, 8, 8) != pixel(light, 8, 8), (pixel(dark, 8, 8), pixel(light, 8, 8))
assert pixel(light, 8, 8) == pixel(denied, 8, 8)
assert pixel(after, 8, 8) != pixel(denied, 8, 8)
PY

echo 'ok   mlx-lock masks and wipes passwords, fails closed, follows appearance, and unlocks through org.mlx.Login'
