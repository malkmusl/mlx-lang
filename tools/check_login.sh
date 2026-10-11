#!/usr/bin/env bash
# Checks mlx-login (projects/desktop/login) against a private MLXIPC
# system bus and private account files. It owns org.mlx.Login, lists the
# synthetic login accounts, authorizes login.session through MLXIPC and
# checks passwords against a private shadow source.
#
# The daemon receives private passwd and shadow paths explicitly, and the
# session step is still a stub, so it neither opens the defaults nor logs
# anybody in.
#
#   tools/check_login.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
command -v gdbus > /dev/null || { echo "check_login.sh: needs gdbus (glib2.0-bin)" >&2; exit 2; }
command -v unshare > /dev/null || { echo "check_login.sh: needs unshare (util-linux)" >&2; exit 2; }

# Both the MLXIPC system bus and mlx-login use root credentials as a
# security boundary. Keep those credentials inside a private user
# namespace so this check never needs or exercises host root.
read -r namespace_uid host_uid namespace_count < /proc/self/uid_map
if [[ $namespace_uid != 0 || $namespace_count != 1 ]]; then
    unshare --user --map-root-user true 2> /dev/null || { echo "check_login.sh: user namespaces are unavailable" >&2; exit 2; }
    exec unshare --user --map-root-user -- "$0" "$compiler"
fi

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

# Private fixtures prove that both file options are accepted and that the
# account list never comes from the host.
# Put the accounts beyond the old 64 KiB read boundary. The loader must
# stream complete records rather than silently truncating a valid file.
awk 'BEGIN { for (i = 0; i < 8000; i++) print "# padding" }' > "$work/passwd"
cat >> "$work/passwd" <<'PASSWD'
mlx-test:x:1000:1000:MLX Test:/home/mlx-test:/bin/sh
mlx-locked:x:1001:1001:MLX Locked:/home/mlx-locked:/bin/sh
mlx-empty:x:1002:1002:MLX Empty:/home/mlx-empty:/bin/sh
PASSWD
cat > "$work/shadow" <<'SHADOW'
mlx-test:$6$mlxlogintestsalt$/D8RVzVywVfzqDWu6Wm7IjBcNOG2nnjZYQF075LGH3qLA9EwBUX5j5nxpAnuoS7QeGMw1xLBsOI3j/1DSu65I0:20000:0:99999:7:::
mlx-locked:!$6$mlxlogintestsalt$/D8RVzVywVfzqDWu6Wm7IjBcNOG2nnjZYQF075LGH3qLA9EwBUX5j5nxpAnuoS7QeGMw1xLBsOI3j/1DSu65I0:20000:0:99999:7:::
mlx-empty::20000:0:99999:7:::
SHADOW
chmod 600 "$work/shadow"

address="unix:path=$work/bus"
export DBUS_SYSTEM_BUS_ADDRESS="$address"
touch "$work/none.conf"
"$work/mlx-ipcd" --system --address "$work/bus" --policy "$work/none.conf" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$work/bus" ]] && break; sleep 0.1; done
[[ -S "$work/bus" ]] || { echo "FAIL the system bus did not come up" >&2; cat "$work/bus.log" >&2; exit 1; }

"$work/mlx-login" --verbose --passwd "$work/passwd" --shadow "$work/shadow" > "$work/login.log" 2>&1 &
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

# ListUsers comes only from the fixture passed with --passwd.
users=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.ListUsers 2>&1)
grep -q "'mlx-test', 'MLX Test', '/home/mlx-test', '/bin/sh'" <<< "$users" || { echo "FAIL ListUsers did not name mlx-test:" >&2; echo "$users" >&2; exit 1; }
grep -q "'mlx-locked', 'MLX Locked', '/home/mlx-locked', '/bin/sh'" <<< "$users" || { echo "FAIL ListUsers did not name mlx-locked:" >&2; echo "$users" >&2; exit 1; }
grep -q "'mlx-empty', 'MLX Empty', '/home/mlx-empty', '/bin/sh'" <<< "$users" || { echo "FAIL ListUsers did not name mlx-empty:" >&2; echo "$users" >&2; exit 1; }
if grep -q "'root'," <<< "$users"; then
    echo "FAIL ListUsers read the machine's /etc/passwd instead of the fixture:" >&2
    echo "$users" >&2
    exit 1
fi
count=$(grep -o "('" <<< "$users" | wc -l)
[[ "$count" == 3 ]] || { echo "FAIL ListUsers returned $count accounts instead of the three fixtures:" >&2; echo "$users" >&2; exit 1; }
echo "ok   ListUsers names only the three accounts from the passwd fixture"

# Permission is decided before mlx-login looks at an account or password.
login_call() {
    gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
        --method org.mlx.Login.Login "$1" "$2" /bin/true 2>&1 || true
}
login_out=$(login_call mlx-test test-password)
grep -q "org.freedesktop.DBus.Error.AccessDenied" <<< "$login_out" || { echo "FAIL Login handled a password without login.session:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login requires login.session before checking a password"

printf 'allow gdbus use login.session\n' > "$work/none.conf"
sleep 0.3

login_out=$(login_call mlx-missing test-password)
grep -q "(false,)" <<< "$login_out" || { echo "FAIL Login did not refuse an unknown account:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login refuses an unknown account"

login_out=$(login_call mlx-locked test-password)
grep -q "(false,)" <<< "$login_out" || { echo "FAIL Login did not refuse a locked account:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login refuses a locked account"

login_out=$(login_call mlx-empty "")
grep -q "(false,)" <<< "$login_out" || { echo "FAIL Login did not refuse an empty password hash:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login refuses an empty password hash"

login_out=$(login_call mlx-test wrong-password)
grep -q "(false,)" <<< "$login_out" || { echo "FAIL Login did not refuse a wrong password:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login refuses a wrong password"

mv "$work/shadow" "$work/shadow-away"
login_out=$(login_call mlx-test test-password)
mv "$work/shadow-away" "$work/shadow"
grep -q "(false,)" <<< "$login_out" || { echo "FAIL Login did not fail closed when shadow was unavailable:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login fails closed when the shadow source is unavailable"

# A correct password reaches only the still-unconfigured session start.
login_out=$(login_call mlx-test test-password)
grep -q "org.mlx.Login.Error.NotConfigured" <<< "$login_out" || { echo "FAIL a correct password did not reach the session stub:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   a correct password reaches the unconfigured session start"

# Unlock with nobody on the seat is a plain no (it never reaches the
# password check): the lock screen only unlocks the user who is logged in.
unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock root x 2>&1 || true)
grep -q "(false,)" <<< "$unlock_out" || { echo "FAIL Unlock on an empty seat was not refused:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   Unlock refuses when nobody holds the seat"

if grep -qE 'test-password|wrong-password|mlxlogintestsalt' "$work/login.log"; then
    echo "FAIL mlx-login logged password material" >&2
    cat "$work/login.log" >&2
    exit 1
fi
echo "ok   mlx-login logs no password material"

# The daemon is still alive (it did not crash on any call).
kill -0 "$login_pid" 2> /dev/null || { echo "FAIL mlx-login ended during the check" >&2; cat "$work/login.log" >&2; exit 1; }
echo "ok   mlx-login stayed up through the calls"
echo "all mlx-login checks passed"
