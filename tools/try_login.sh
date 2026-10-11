#!/usr/bin/env bash
# Tries mlx-login on this machine for a few minutes, from the running
# graphical session, and keeps every log of the try:
#
#   1. ends your mlx-compositor (the graphical session; save your work)
#   2. switches to VT 8, where no shell reads the keyboard (without a seat
#      manager the greeter and the console both get every key)
#   3. runs mlx-login with its greeter (as the mlx-greeter account) for
#      --time seconds, then systemd ends it and every session it started
#   4. saves the logs into ~/mlx-login-test/DATE-TIME: the greeter's
#      compositor log (copied every two seconds, its directory goes with
#      the greeter), the session's compositor log and launcher trace, the
#      journal of mlx-login and mlx-ipcd, the processes with their groups
#      while it ran, the device permissions
#   5. switches back to the VT you were on and starts the display manager
#      again if one ran before
#
#   tools/try_login.sh [--time SECONDS]     (default 180)
#
# Run it as yourself from a terminal of the session; it asks for sudo once.
# The work happens in a transient systemd service (mlx-login-try-*), so it
# goes on when the session it ends takes the terminal with it.
#
# Needs the greeter account and the device access of the greeter
# (mlx-greeter, its udev ACL rule) set up before.
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
seconds=180
inner=false
user=""
out=""
while [[ $# -gt 0 ]]; do
    case $1 in
    --time) seconds=$2; shift ;;
    --inner) inner=true ;;
    --user) user=$2; shift ;;
    --out) out=$2; shift ;;
    -h|--help) sed -n '2,/^set -uo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
    *) echo "try_login.sh: unknown option $1 (see --help)" >&2; exit 2 ;;
    esac
    shift
done
[[ "$seconds" =~ ^[0-9]+$ && "$seconds" -ge 30 ]] || { echo "try_login.sh: --time wants at least 30 seconds" >&2; exit 2; }

bindir=/usr/bin/mlx
test_vt=8

if ! $inner; then
    if [[ $(id -u) -eq 0 ]]; then
        echo "try_login.sh: run it as yourself (it asks for sudo itself)" >&2
        exit 2
    fi
    id mlx-greeter > /dev/null 2>&1 || { echo "try_login.sh: there is no mlx-greeter account yet" >&2; exit 1; }
    [[ -x $bindir/mlx-login && -x $bindir/mlx-compositor && -x $bindir/mlx-greeter ]] || { echo "try_login.sh: $bindir lacks mlx-login, mlx-compositor or mlx-greeter" >&2; exit 1; }
    stamp=$(date +%Y-%m-%d_%H-%M-%S)
    out="$HOME/mlx-login-test/$stamp"
    mkdir -p "$out"
    sudo -v || exit 1
    sudo systemd-run --quiet --collect --unit="mlx-login-try-$stamp" \
        /bin/bash "$repo_root/tools/try_login.sh" --inner --user "$USER" --time "$seconds" --out "$out" || exit 1
    echo "The try runs now: in 5 seconds your session ends, for $seconds seconds"
    echo "mlx-login and its greeter run on VT $test_vt, then you are back on this VT."
    echo "Type no real password in the greeter. The logs go to:"
    echo "    $out"
    exit 0
fi

# The inner part: root, in its own service. Whatever happens, the logs
# end up yours.
trap 'chown -R "$user:" "$out"' EXIT
log="$out/try.log"
say() { echo "$(date +%T) $*" >> "$log"; }
home=$(getent passwd "$user" | cut -d: -f6)
user_id=$(id -u "$user")
greeter_uid=$(id -u mlx-greeter)
greeter_gid=$(getent group mlx-greeter | cut -d: -f3)
greeter_log=/run/mlx/user/greeter/.config/mlx/compositor.log
started=$(date +%s)
vt_open=false
say "try of mlx-login for $seconds s; user $user ($user_id), greeter account $greeter_uid:$greeter_gid"

# What there was before.
original_vt=$(fgconsole 2> /dev/null || echo 1)
display_manager=$(systemctl is-active display-manager 2> /dev/null || true)
say "VT $original_vt, display-manager.service $display_manager"
for file in compositor.log compositor.log.old; do
    [[ -f "$home/.config/mlx/$file" ]] && cp "$home/.config/mlx/$file" "$out/before-session-$file"
done
id "$user" > "$out/id.txt" 2>&1
{
    ls -l /dev/dri/
    getfacl -p /dev/dri/card* /dev/input/event* 2> /dev/null | grep -E '^# file|mlx-greeter'
} > "$out/devices.txt" 2>&1

# End the session's compositor (only this user's).
sleep 5
pkill -TERM -u "$user" -x mlx-compositor && say "ended $user's mlx-compositor"
for _ in $(seq 20); do
    pgrep -u "$user" -x mlx-compositor > /dev/null || break
    sleep 0.5
done
if pgrep -u "$user" -x mlx-compositor > /dev/null; then
    pkill -KILL -u "$user" -x mlx-compositor
    say "mlx-compositor did not end on TERM: killed"
fi
[[ "$display_manager" == active ]] && systemctl stop display-manager && say "stopped the display manager"

# The cards must be free: the greeter has to set the mode.
busy=""
for _ in $(seq 20); do
    busy=$(fuser /dev/dri/card* 2> /dev/null | tr -s ' ')
    [[ -z "$busy" ]] && break
    sleep 0.5
done
if [[ -n "$busy" ]]; then
    say "the cards are still open (pids:$busy); no try"
    ps -o pid,user,cmd -p "$(echo "$busy" | tr ' ' ',' | sed 's/^,//')" >> "$log" 2>&1
else
    systemctl is-active --quiet mlx-ipcd || { systemctl start mlx-ipcd; say "started mlx-ipcd"; }
    # The test VT shows nothing of what is typed. The console resets a VT's
    # settings when the last file on it closes, so `stty -F` alone would
    # not last: this script keeps the VT open on descriptor 3 to the end.
    exec 3<> "/dev/tty$test_vt" && vt_open=true
    $vt_open && stty -echo <&3 2> /dev/null
    timeout 10 chvt "$test_vt" || say "chvt $test_vt did not finish in 10 s"
    systemctl reset-failed mlx-login-test 2> /dev/null
    if systemd-run --quiet --collect --unit=mlx-login-test -p RuntimeMaxSec="$seconds" \
        "$bindir/mlx-login" --greeter-uid "$greeter_uid" --greeter-gid "$greeter_gid" \
        --greeter "$bindir/mlx-compositor" "$bindir/mlx-greeter"; then
        say "mlx-login runs (mlx-login-test.service)"
        tick=0
        while systemctl is-active --quiet mlx-login-test; do
            [[ -f "$greeter_log" ]] && cp -f "$greeter_log" "$out/greeter-compositor.log"
            if (( tick % 5 == 0 )); then
                {
                    echo "== $(date +%T)"
                    ps -eo pid,ppid,user,group,cmd:72,supgrp | grep -E 'mlx-|gnome|PID' | grep -v grep
                } >> "$out/processes.txt"
            fi
            tick=$((tick + 1))
            sleep 2
        done
        say "mlx-login ended"
    else
        say "mlx-login did not start"
    fi
fi

# The logs, first: nothing after this may lose them.
journalctl --since "@$started" -u mlx-login-test -u mlx-ipcd --no-pager -o short-precise > "$out/journal-mlx.txt" 2>&1
journalctl --since "@$started" --no-pager -o short-precise -p warning > "$out/journal-warnings.txt" 2>&1
for file in compositor.log compositor.log.old; do
    [[ -f "$home/.config/mlx/$file" ]] && cp "$home/.config/mlx/$file" "$out/after-session-$file"
done
[[ -f "/tmp/mlx-session-$user_id.log" ]] && cp "/tmp/mlx-session-$user_id.log" "$out/session-launcher.log"
say "saved the logs"

# Back to where it was.
timeout 10 chvt "$original_vt" || say "chvt $original_vt did not finish in 10 s"
[[ "$display_manager" == active ]] && systemctl start display-manager && say "started the display manager again"

# What the console got while the greeter had the keys: thrown away, so no
# later reader of the VT sees it. The read has a time limit: a VT that
# waits for a whole line would block it for ever.
if $vt_open; then
    stty -icanon min 0 time 0 <&3 2> /dev/null && timeout 2 cat <&3 > /dev/null 2>&1
    stty sane <&3 2> /dev/null
    printf '\033c' >&3 2> /dev/null
    exec 3<&-
fi
say "done; the logs are in $out"
