#!/usr/bin/env bash
# End-to-end check of umbra against the nested compositor.  A small
# org.mlx.Login stand-in accepts the second password only; neither it nor
# the locker may print either password.  umbra asks in MLXIPC's native
# format (std.ipc; the bus says so), the stand-in answers in D-Bus's: the
# bus carries the one to the other.  While the wrong password is asked,
# another program on the bus (tools/lock-login-test/forger.mlx) sends
# umbra replies "true" with the serials its calls have: only the answer
# of the connection that owns org.mlx.Login may count.
#
# Needs an xkb keymap for the host: xkbcli's, or brix-keymap's from the
# host's XKB data. Usage: tools/check_lock.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
if ! command -v xkbcli > /dev/null && [[ ! -f /usr/share/X11/xkb/rules/evdev ]]; then
    echo "check_lock.sh: no xkb keymap for the test host (neither xkbcli nor the XKB data)" >&2
    exit 2
fi

work=$(mktemp -d)
bus_pid=
login_pid=
host_pid=
forger_pid=
cleanup() {
    [[ -z "$host_pid" ]] || kill "$host_pid" 2> /dev/null || true
    [[ -z "$forger_pid" ]] || kill "$forger_pid" 2> /dev/null || true
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

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/kepler"
"$compiler" --quiet projects/desktop/lock/main.mlx -o "$work/umbra"
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/synapse"
"$compiler" --quiet tools/lock-login-test/main.mlx -o "$work/lock-login-test"
"$compiler" --quiet tools/lock-login-test/client.mlx -o "$work/lock-login-client"
"$compiler" --quiet tools/lock-login-test/forger.mlx -o "$work/forger"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
if command -v xkbcli > /dev/null; then
    xkbcli compile-keymap --layout us > "$work/us.xkb"
else
    "$compiler" --quiet projects/brix/tools/keymap/main.mlx -o "$work/brix-keymap"
    "$work/brix-keymap" us "$work/us.xkb"
fi

"$work/synapse" --verbose --address "$work/system-bus" > "$work/ipc.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$work/system-bus" ]] && break; sleep 0.1; done
[[ -S "$work/system-bus" ]] || { echo "synapse did not open its socket" >&2; exit 1; }

DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" "$work/lock-login-test" > "$work/login.log" 2>&1 &
login_pid=$!
for _ in $(seq 1 50); do grep -q '^lock-login-test: ready$' "$work/login.log" 2> /dev/null && break; sleep 0.1; done
grep -q '^lock-login-test: ready$' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }

DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" "$work/lock-login-client" > "$work/client.log" 2>&1
grep -qx 'lock-login-client: allowed and wiped' "$work/client.log" || { cat "$work/client.log" >&2; exit 1; }

# From here on a program on the bus answers "true" to every connection
# that comes, until umbra's has (with its first password, the wrong one).
DBUS_SYSTEM_BUS_ADDRESS="unix:path=$work/system-bus" "$work/forger" - umbra > "$work/forger.log" 2>&1 &
forger_pid=$!
for _ in $(seq 1 50); do grep -q '^forger: watching$' "$work/forger.log" 2> /dev/null && break; sleep 0.1; done
grep -q '^forger: watching$' "$work/forger.log" || { cat "$work/forger.log" >&2; exit 1; }

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
    WAYLAND_DISPLAY=host-lock timeout 30 "$work/kepler" --verbose --renderer cpu \
    --socket nested-lock --no-fps --run "$work/umbra" > "$work/compositor.log" 2>&1
wait "$host_pid" || { cat "$work/host.log" "$work/compositor.log" >&2; exit 1; }
host_pid=

# The forged answers reached umbra with the wrong password (the forger
# ended once the bus named that connection umbra's); the stand-in's
# refusal counted all the same.
grep -qx 'forger: answered umbra' "$work/forger.log" || { echo 'check_lock: the forger did not answer umbra' >&2; cat "$work/forger.log" >&2; exit 1; }
forger_pid=
grep -qx 'unlock tester 5 bytes: denied' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }
grep -qx 'unlock tester 10 bytes: allowed' "$work/login.log" || { cat "$work/login.log" >&2; exit 1; }
for line in 'umbra: locked' 'umbra: authentication failed' 'umbra: unlocked' 'lock: locked' 'lock: unlocked'; do
    grep -qx "$line" "$work/compositor.log" || { echo "check_lock: missing $line" >&2; cat "$work/compositor.log" >&2; exit 1; }
done
# The locker and the memory-side client spoke the native wire to the bus,
# the stand-in D-Bus.
grep -Eqx 'ipc: :1\.[0-9]+ is umbra \(mlx\)' "$work/ipc.log" || { echo 'check_lock: umbra did not speak the native wire to the bus' >&2; cat "$work/ipc.log" >&2; exit 1; }
grep -Eqx 'ipc: :1\.[0-9]+ is lock-login-client \(mlx\)' "$work/ipc.log" || { echo 'check_lock: the client did not speak the native wire to the bus' >&2; cat "$work/ipc.log" >&2; exit 1; }
grep -Eqx 'ipc: :1\.[0-9]+ is lock-login-test' "$work/ipc.log" || { echo 'check_lock: the stand-in is not the D-Bus service the bus carries to' >&2; cat "$work/ipc.log" >&2; exit 1; }
! grep -Fq 'wrong' "$work/login.log" "$work/compositor.log" "$work/ipc.log" || { echo 'check_lock: a password reached a log' >&2; exit 1; }
! grep -Fq 'right-pass' "$work/login.log" "$work/compositor.log" "$work/ipc.log" || { echo 'check_lock: a password reached a log' >&2; exit 1; }
grep -aFq 'Could not unlock. Please try again.' "$work/umbra"
grep -aFq 'Entsperren fehlgeschlagen. Bitte erneut versuchen.' "$work/umbra"

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

echo 'ok   umbra masks and wipes passwords, fails closed (a forged answer among it), follows appearance, and unlocks through org.mlx.Login'
