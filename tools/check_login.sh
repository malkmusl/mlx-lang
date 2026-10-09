#!/usr/bin/env bash
# Checks mlx-login (projects/desktop/login) against a private MLXIPC
# system bus: it owns org.mlx.Login, lists this machine's accounts (the
# ones in /etc/passwd with a login shell), and answers Login and Unlock
# with NotConfigured until the two privileged steps (authenticate,
# startSession) are filled in on the machine that runs it.
#
# Nothing here reads /etc/shadow or starts a session: the two steps that
# would are stubs, so the check needs no root and logs nobody in.
#
#   tools/check_login.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
command -v gdbus > /dev/null || { echo "check_login.sh: needs gdbus (glib2.0-bin)" >&2; exit 2; }

work=$(mktemp -d)
bus_pid=""
login_pid=""
cleanup() {
    [[ -n "$login_pid" ]] && kill "$login_pid" 2> /dev/null || true
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT

"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/login/main.mlx -o "$work/mlx-login"

address="unix:path=$work/bus"
export DBUS_SYSTEM_BUS_ADDRESS="$address"
touch "$work/none.conf"
"$work/mlx-ipcd" --system --address "$work/bus" --policy "$work/none.conf" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$work/bus" ]] && break; sleep 0.1; done
[[ -S "$work/bus" ]] || { echo "FAIL the system bus did not come up" >&2; cat "$work/bus.log" >&2; exit 1; }

"$work/mlx-login" --verbose > "$work/login.log" 2>&1 &
login_pid=$!
# Wait until it owns the name.
owned=""
for _ in $(seq 1 50); do
    if gdbus call --address "$address" --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner org.mlx.Login 2> /dev/null | grep -q true; then
        owned=yes
        break
    fi
    sleep 0.1
done
[[ -n "$owned" ]] || { echo "FAIL mlx-login did not own org.mlx.Login" >&2; cat "$work/login.log" >&2; exit 1; }
echo "ok   mlx-login owns org.mlx.Login on the system bus"

# ListUsers names this machine's own account (the one running the check),
# unless it is a system account; root is always a login account.
users=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.ListUsers 2>&1)
grep -q "'root', 'root', '/root'" <<< "$users" || { echo "FAIL ListUsers did not name root:" >&2; echo "$users" >&2; exit 1; }
count=$(grep -o "', '/" <<< "$users" | wc -l)
echo "ok   ListUsers names the login accounts of /etc/passwd ($count of them, including root)"

# Login reaches the password check (authenticate), which is a stub, so it
# answers NotConfigured -- not a crash and not a wrong "true".
login_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Login root x mlx-compositor 2>&1 || true)
grep -q "org.mlx.Login.Error.NotConfigured" <<< "$login_out" || { echo "FAIL Login did not answer NotConfigured:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login answers NotConfigured until authenticate and startSession are filled in"

# Unlock with nobody on the seat is a plain no (it never reaches the
# password check): the lock screen only unlocks the user who is logged in.
unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock root x 2>&1 || true)
grep -q "(false,)" <<< "$unlock_out" || { echo "FAIL Unlock on an empty seat was not refused:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   Unlock refuses when nobody holds the seat"

# The daemon is still alive (it did not crash on any call).
kill -0 "$login_pid" 2> /dev/null || { echo "FAIL mlx-login ended during the check" >&2; cat "$work/login.log" >&2; exit 1; }
echo "ok   mlx-login stayed up through the calls"
echo "all mlx-login checks passed"
