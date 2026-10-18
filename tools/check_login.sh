#!/usr/bin/env bash
# Checks mlx-login (projects/desktop/login) against a private MLXIPC
# system bus and private account files. It owns org.mlx.Login, lists the
# synthetic login accounts, authorizes login.session through MLXIPC and
# checks passwords against a private shadow source.
#
# The daemon receives private account paths explicitly and starts only a
# short fixture session after dropping privileges inside the namespace.
#
#   tools/check_login.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
command -v gdbus > /dev/null || { echo "check_login.sh: needs gdbus (glib2.0-bin)" >&2; exit 2; }
command -v unshare > /dev/null || { echo "check_login.sh: needs unshare (util-linux)" >&2; exit 2; }
# The login1 bridge hands out seat devices through /dev/char/MAJOR:MINOR
# (the test takes 1:3, /dev/null); a container's minimal /dev has no
# /dev/char, and the check cannot run there.
[[ -e /dev/char/1:3 ]] || { echo "check_login.sh: needs /dev/char/1:3 (devtmpfs's device links; a minimal /dev has none)" >&2; exit 2; }

# Both the MLXIPC system bus and mlx-login use root credentials as a
# security boundary. Keep those credentials inside a private user
# namespace so this check never needs or exercises host root. The second
# mapping gives the session a real non-root uid and keeps setgroups usable.
mapped_user=$(awk '$1 <= 1000 && 1000 < $1 + $3 { found = 1 } END { print found + 0 }' /proc/self/uid_map)
setgroups_state=$(cat /proc/self/setgroups 2> /dev/null || true)
if [[ $(id -u) != 0 || $mapped_user != 1 || $setgroups_state != allow ]]; then
    unshare --user --map-root-user --map-users 1:100000:65535 --map-groups 1:100000:65535 true 2> /dev/null || {
        echo "check_login.sh: user namespaces with subordinate uid/gid maps are unavailable" >&2
        exit 2
    }
    exec unshare --user --map-root-user --map-users 1:100000:65535 --map-groups 1:100000:65535 -- "$0" "$compiler"
fi

work=$(mktemp -d)
chmod 755 "$work"
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
"$compiler" --quiet tools/login-test/logind-client.mlx -o "$work/login1-client"

# Private fixtures prove that both file options are accepted and that the
# account list never comes from the host.
# Put the accounts beyond the old 64 KiB read boundary. The loader must
# stream complete records rather than silently truncating a valid file.
awk 'BEGIN { for (i = 0; i < 8000; i++) print "# padding" }' > "$work/passwd"
cat >> "$work/passwd" <<'PASSWD'
mlx-test:x:1000:1000:MLX Test:HOME_PLACEHOLDER:/bin/sh
mlx-locked:x:1001:1001:MLX Locked:/home/mlx-locked:/bin/sh
mlx-empty:x:1002:1002:MLX Empty:/home/mlx-empty:/bin/sh
PASSWD
mkdir "$work/home" "$work/runtime"
chown 1000:1000 "$work/home"
sed -i "s|HOME_PLACEHOLDER|$work/home|" "$work/passwd"
cat > "$work/group" <<'GROUP'
mlx-test:x:1000:mlx-test
video:x:983:mlx-test
GROUP
cat > "$work/shadow" <<'SHADOW'
mlx-test:$6$mlxlogintestsalt$/D8RVzVywVfzqDWu6Wm7IjBcNOG2nnjZYQF075LGH3qLA9EwBUX5j5nxpAnuoS7QeGMw1xLBsOI3j/1DSu65I0:20000:0:99999:7:::
mlx-locked:!$6$mlxlogintestsalt$/D8RVzVywVfzqDWu6Wm7IjBcNOG2nnjZYQF075LGH3qLA9EwBUX5j5nxpAnuoS7QeGMw1xLBsOI3j/1DSu65I0:20000:0:99999:7:::
mlx-empty::20000:0:99999:7:::
SHADOW
chmod 600 "$work/shadow"
cat > "$work/session.sh" <<SESSION
#!/bin/sh
{
    printf 'pid=%s\n' "\$\$"
    printf 'uid=%s gid=%s groups=%s\n' "\$(id -u)" "\$(id -g)" "\$(id -G)"
    printf 'HOME=%s USER=%s LOGNAME=%s SHELL=%s\n' "\$HOME" "\$USER" "\$LOGNAME" "\$SHELL"
    printf 'XDG_RUNTIME_DIR=%s XDG_SESSION_TYPE=%s XDG_SEAT=%s XDG_VTNR=%s\n' "\$XDG_RUNTIME_DIR" "\$XDG_SESSION_TYPE" "\$XDG_SEAT" "\$XDG_VTNR"
} > "$work/home/session.out"
touch "$work/home/ready"
while [[ ! -e "$work/home/finish" ]]; do sleep 0.1; done
SESSION
chmod 755 "$work/session.sh"

# A stand-in for `mlx-compositor --greeter mlx-greeter`. mlx-login must
# launch it without root credentials, stop it before the user's session,
# and start a fresh one after that session ends.
cat > "$work/greeter-compositor" <<'GREETER'
#!/bin/sh
log="$XDG_RUNTIME_DIR/lifecycle.log"
printf 'start pid=%s uid=%s gid=%s args=%s\n' "$$" "$(id -u)" "$(id -g)" "$*" >> "$log"
trap 'printf "stop pid=%s\n" "$$" >> "$log"; exit 0' TERM INT
while :; do sleep 0.1; done
GREETER
cat > "$work/mlx-greeter" <<'GREETER_APP'
#!/bin/sh
exit 0
GREETER_APP
chmod 755 "$work/greeter-compositor" "$work/mlx-greeter"

address="unix:path=$work/bus"
export DBUS_SYSTEM_BUS_ADDRESS="$address"
touch "$work/none.conf"
"$work/mlx-ipcd" --system --address "$work/bus" --policy "$work/none.conf" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$work/bus" ]] && break; sleep 0.1; done
[[ -S "$work/bus" ]] || { echo "FAIL the system bus did not come up" >&2; cat "$work/bus.log" >&2; exit 1; }

"$work/mlx-login" --verbose --passwd "$work/passwd" --shadow "$work/shadow" --group "$work/group" --runtime-dir "$work/runtime" --tty none --test-device-null \
    --greeter "$work/greeter-compositor" "$work/mlx-greeter" --greeter-uid 65534 --greeter-gid 65534 > "$work/login.log" 2>&1 &
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

greeter_log="$work/runtime/greeter/lifecycle.log"
for _ in $(seq 1 50); do [[ -s "$greeter_log" ]] && break; sleep 0.1; done
[[ -s "$greeter_log" ]] || { echo "FAIL mlx-login did not start its greeter" >&2; cat "$work/login.log" >&2; exit 1; }
grep -q "^start pid=[0-9]* uid=65534 gid=65534 args=--greeter $work/mlx-greeter$" "$greeter_log" || { echo "FAIL greeter did not have the dropped identity and expected arguments:" >&2; cat "$greeter_log" >&2; exit 1; }
[[ $(stat -c '%u:%g:%a' "$work/runtime/greeter") == 65534:65534:700 ]] || { echo "FAIL greeter runtime directory has wrong ownership or mode" >&2; stat -c '%u:%g:%a' "$work/runtime/greeter" >&2; exit 1; }
greeter_pid=$(sed -n 's/^start pid=\([0-9]*\).*/\1/p' "$greeter_log" | head -1)
greeter_session=$(gdbus call --address "$address" --dest org.freedesktop.login1 --object-path /org/freedesktop/login1 \
    --method org.freedesktop.login1.Manager.GetSessionByPID "uint32 $greeter_pid" 2>&1 || true)
grep -q '/org/freedesktop/login1/session/seat0' <<< "$greeter_session" || { echo "FAIL login1 did not expose the greeter compositor as the seat" >&2; echo "$greeter_session" >&2; exit 1; }
echo "ok   mlx-login starts the greeter compositor unprivileged and exposes its seat"

# ListUsers comes only from the fixture passed with --passwd.
users=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.ListUsers 2>&1)
grep -q "'mlx-test', 'MLX Test', '$work/home', '/bin/sh'" <<< "$users" || { echo "FAIL ListUsers did not name mlx-test:" >&2; echo "$users" >&2; exit 1; }
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
        --method org.mlx.Login.Login "$1" "$2" "${3:-/bin/true}" 2>&1 || true
}
login_out=$(login_call mlx-test test-password)
grep -q "org.freedesktop.DBus.Error.AccessDenied" <<< "$login_out" || { echo "FAIL Login handled a password without login.session:" >&2; echo "$login_out" >&2; exit 1; }
echo "ok   Login requires login.session before checking a password"

printf 'allow gdbus use login.session\nallow login1-client use login.device\n' > "$work/none.conf"
sleep 0.3

# The unprivileged greeter compositor uses its distinct session id before a
# user session exists. It must receive devices through mlx-login's login1
# bridge instead of depending on systemd/udev ACLs that mlx-init does not run.
XDG_SESSION_ID=mlx-greeter XDG_SEAT=seat0 "$work/login1-client" > "$work/greeter-login1.log" 2>&1 || {
    echo "FAIL the greeter compositor could not control the login seat" >&2
    cat "$work/greeter-login1.log" "$work/login.log" >&2
    exit 1
}
grep -q 'login1 client passed' "$work/greeter-login1.log" || { echo "FAIL greeter login1 client did not finish" >&2; cat "$work/greeter-login1.log" >&2; exit 1; }
echo "ok   the greeter session receives devices through mlx-login login1"

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

# Unlock with nobody on the seat is a plain no (it never reaches the
# password check): the lock screen only unlocks the user who is logged in.
unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock mlx-test test-password 2>&1 || true)
grep -q "(false,)" <<< "$unlock_out" || { echo "FAIL Unlock on an empty seat was not refused:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   Unlock refuses when nobody holds the seat"

# A correct password starts the fixture as uid/gid 1000 with only the
# requested supplementary groups and a deliberately small environment.
login_out=$(login_call mlx-test test-password "$work/session.sh")
grep -q "(true,)" <<< "$login_out" || { echo "FAIL a correct password did not start the session:" >&2; echo "$login_out" >&2; cat "$work/login.log" >&2; exit 1; }
for _ in $(seq 1 50); do [[ -e "$work/home/ready" ]] && break; sleep 0.1; done
[[ -e "$work/home/ready" ]] || { echo "FAIL the session did not become ready" >&2; cat "$work/login.log" >&2; exit 1; }
grep -q "^stop pid=$greeter_pid$" "$greeter_log" || { echo "FAIL the greeter was not stopped before the user session" >&2; cat "$greeter_log" "$work/login.log" >&2; exit 1; }
if kill -0 "$greeter_pid" 2> /dev/null; then
    echo "FAIL the old greeter compositor still runs beside the user session" >&2
    exit 1
fi
grep -q '^uid=1000 gid=1000 groups=.*983.*1000\|^uid=1000 gid=1000 groups=.*1000.*983' "$work/home/session.out" || { echo "FAIL the session kept the wrong ids or groups:" >&2; cat "$work/home/session.out" >&2; exit 1; }
grep -q "HOME=$work/home USER=mlx-test LOGNAME=mlx-test SHELL=/bin/sh" "$work/home/session.out" || { echo "FAIL the session environment has wrong account values:" >&2; cat "$work/home/session.out" >&2; exit 1; }
grep -q "XDG_RUNTIME_DIR=$work/runtime/1000 XDG_SESSION_TYPE=wayland XDG_SEAT=seat0 XDG_VTNR=1" "$work/home/session.out" || { echo "FAIL the session environment has wrong seat values:" >&2; cat "$work/home/session.out" >&2; exit 1; }
[[ $(stat -c '%u:%g:%a' "$work/runtime/1000") == 1000:1000:700 ]] || { echo "FAIL the runtime directory has wrong ownership or mode" >&2; stat -c '%u:%g:%a' "$work/runtime/1000" >&2; exit 1; }
echo "ok   Login starts a uid/gid-dropped session with groups, environment and private runtime directory"
echo "ok   Login stops the greeter before giving the seat to the user session"

login1_owned=$(gdbus call --address "$address" --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
    --method org.freedesktop.DBus.NameHasOwner org.freedesktop.login1 2>&1)
grep -q true <<< "$login1_owned" || { echo "FAIL mlx-login did not own org.freedesktop.login1" >&2; cat "$work/login.log" >&2; exit 1; }
session_pid=$(sed -n 's/^pid=//p' "$work/home/session.out")
session_path=$(gdbus call --address "$address" --dest org.freedesktop.login1 --object-path /org/freedesktop/login1 \
    --method org.freedesktop.login1.Manager.GetSessionByPID "uint32 $session_pid" 2>&1 || true)
grep -q '/org/freedesktop/login1/session/seat0' <<< "$session_path" || { echo "FAIL login1 did not resolve the active session" >&2; echo "$session_path" >&2; exit 1; }
unauthorized_control=$(gdbus call --address "$address" --dest org.freedesktop.login1 --object-path /org/freedesktop/login1/session/seat0 \
    --method org.freedesktop.login1.Session.TakeControl false 2>&1 || true)
grep -q 'org.freedesktop.DBus.Error.AccessDenied' <<< "$unauthorized_control" || { echo "FAIL login1 granted device control without login.device" >&2; echo "$unauthorized_control" >&2; exit 1; }
XDG_SESSION_ID=mlx XDG_SEAT=seat0 "$work/login1-client" abandon > "$work/login1-abandon.log" 2>&1 || {
    echo "FAIL the first compositor could not take control" >&2
    cat "$work/login1-abandon.log" >&2
    exit 1
}
sleep 0.2
XDG_SESSION_ID=mlx XDG_SEAT=seat0 "$work/login1-client" > "$work/login1-client.log" 2>&1 || {
    echo "FAIL the compositor login1 client could not control the active session" >&2
    cat "$work/login1-client.log" >&2
    cat "$work/login.log" >&2
    exit 1
}
grep -q 'login1 client passed' "$work/login1-client.log" || { echo "FAIL the login1 client did not finish" >&2; cat "$work/login1-client.log" >&2; exit 1; }
echo "ok   login1 replaces a disconnected controller and provides devices, pause/resume and VT switching"

unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock mlx-test test-password 2>&1 || true)
grep -q "(true,)" <<< "$unlock_out" || { echo "FAIL Unlock refused the active user's password:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   Unlock accepts the active user's password"

unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock mlx-test wrong-password 2>&1 || true)
grep -q "(false,)" <<< "$unlock_out" || { echo "FAIL Unlock accepted a wrong password:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   Unlock refuses a wrong password"

# No bus request wakes the daemon here. SIGCHLD itself must reap the
# child and release the seat, or a greeter could never be restarted.
touch "$work/home/finish"
for _ in $(seq 1 70); do
    grep -q 'session ended; the seat is free' "$work/login.log" && break
    sleep 0.1
done
grep -q 'session ended; the seat is free' "$work/login.log" || { echo "FAIL SIGCHLD did not release the seat without bus traffic" >&2; cat "$work/login.log" >&2; exit 1; }
for _ in $(seq 1 50); do [[ $(grep -c '^start ' "$greeter_log" 2> /dev/null || true) -ge 2 ]] && break; sleep 0.1; done
[[ $(grep -c '^start ' "$greeter_log" 2> /dev/null || true) -ge 2 ]] || { echo "FAIL mlx-login did not restart the greeter after logout" >&2; cat "$greeter_log" "$work/login.log" >&2; exit 1; }
unlock_out=$(gdbus call --address "$address" --dest org.mlx.Login --object-path /org/mlx/Login \
    --method org.mlx.Login.Unlock mlx-test test-password 2>&1 || true)
grep -q "(false,)" <<< "$unlock_out" || { echo "FAIL the seat stayed occupied after the session ended:" >&2; echo "$unlock_out" >&2; exit 1; }
echo "ok   the seat is released when the session ends"
echo "ok   the greeter starts again after the user session ends"

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
