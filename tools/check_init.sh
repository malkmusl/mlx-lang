#!/usr/bin/env bash
# mlx-init (projects/init) end to end:
#   - as a supervisor (not PID 1): services start in their order (`after`,
#     `ready`), a oneshot runs once, a failing service is started again
#     with a growing pause, a disabled one is not started; mlx-initctl
#     shows the status and stops, starts, restarts and reloads; SIGTERM
#     stops every service and ends the init;
#   - as PID 1 of a user, PID, mount and UTS namespace (unshare): the
#     hostname, the mounts file, a service looking around, and a poweroff
#     from mlx-initctl that stops the service and ends the namespace.
#
#   tools/check_init.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
init_pid=
cleanup() {
    [[ -n "$init_pid" ]] && kill "$init_pid" 2> /dev/null || true
    sleep 0.2
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet projects/init/main.mlx -o "$work/mlx-init"
"$compiler" --quiet projects/init/tool.mlx -o "$work/mlx-initctl"
echo "ok   mlx-init and mlx-initctl build"

ctl() { "$work/mlx-initctl" --control "$work/run/init" "$@"; }
# The state of a service from the status table.
state_of() { ctl status | awk -v name="$1" '$1 == name { print $2 }'; }
pid_of() { ctl status | awk -v name="$1" '$1 == name { print $3 }'; }
# Waits up to $2 tenths of a second for service $1 to be in state $3.
wait_state() {
    local tries=0
    while [[ "$(state_of "$1")" != "$3" ]]; do
        tries=$((tries + 1))
        [[ $tries -lt $2 ]] || return 1
        sleep 0.1
    done
}
wait_socket() {
    local tries=0
    while [[ ! -S "$1" ]]; do
        tries=$((tries + 1))
        [[ $tries -lt 50 ]] || fail "the init did not listen at $1" "$2"
        sleep 0.1
    done
}

# --- As a supervisor.
mkdir -p "$work/services" "$work/run" "$work/log"
cat > "$work/services/a.service" <<SERVICE
description = service A
exec = /bin/sh -c "echo started; exec sleep 1000"
SERVICE
cat > "$work/services/b.service" <<SERVICE
description = service B, after A, ready by a file
after = a
ready = $work/run/b.ready
exec = /bin/sh -c "sleep 0.3; touch $work/run/b.ready; exec sleep 1000"
SERVICE
cat > "$work/services/once.service" <<SERVICE
description = runs once
type = oneshot
exec = /bin/sh -c "echo once >> $work/run/once.txt"
SERVICE
cat > "$work/services/crasher.service" <<SERVICE
description = exits with 3
exec = /bin/sh -c "exit 3"
restart = on-failure
SERVICE
cat > "$work/services/off.service" <<SERVICE
description = disabled
disabled = yes
exec = /bin/sleep 1000
SERVICE
"$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/log" --verbose 2> "$work/init.err" &
init_pid=$!
wait_socket "$work/run/init" "$work/init.err"
wait_state b 50 running || fail "b did not come to run (after a, ready by its file)" "$work/init.err"
[[ "$(state_of a)" == running ]] || fail "a is not running" "$work/init.err"
[[ "$(state_of once)" == done ]] || fail "the oneshot is not done" "$work/init.err"
[[ "$(state_of off)" == disabled ]] || fail "the disabled service is not disabled" "$work/init.err"
[[ "$(grep -c . "$work/run/once.txt")" == 1 ]] || fail "the oneshot did not run exactly once"
grep -q '^started$' "$work/log/a.log" || fail "a's output is not in its log" "$work/init.err"
a_line=$(grep -n '^mlx-init: a: started' "$work/init.err" | head -1 | cut -d: -f1)
b_line=$(grep -n '^mlx-init: b: started' "$work/init.err" | head -1 | cut -d: -f1)
[[ -n "$a_line" && -n "$b_line" && "$a_line" -lt "$b_line" ]] || fail "b did not start after a" "$work/init.err"
grep -q '^mlx-init: b: ready' "$work/init.err" || fail "b's ready path was not seen" "$work/init.err"
echo "ok   services start in order (after, ready), a oneshot runs once, a disabled one stays off, output goes to the logs"

sleep 2.5
restarts=$(ctl status | awk '$1 == "crasher" { print $4 }')
[[ "$restarts" -ge 2 ]] || fail "the failing service was not started again ($restarts restarts)" "$work/init.err"
grep -q 'crasher: starting again in ms: 1000' "$work/init.err" || fail "no first pause of 1 s" "$work/init.err"
grep -q 'crasher: starting again in ms: 2000' "$work/init.err" || fail "the pause did not grow" "$work/init.err"
echo "ok   a failing service is started again after a growing pause ($restarts restarts so far)"

a_pid=$(pid_of a)
ctl stop a > /dev/null || fail "stop a failed"
wait_state a 30 stopped || fail "a did not stop" "$work/init.err"
kill -0 "$a_pid" 2> /dev/null && fail "a's process $a_pid is still there after stop"
ctl start a > /dev/null || fail "start a failed"
wait_state a 30 running || fail "a did not start again" "$work/init.err"
[[ "$(pid_of a)" != "$a_pid" ]] || fail "a kept its old pid"
b_pid=$(pid_of b)
ctl restart b > /dev/null || fail "restart b failed"
sleep 1
wait_state b 50 running || fail "b did not come back after restart" "$work/init.err"
[[ "$(pid_of b)" != "$b_pid" ]] || fail "b kept its old pid after restart"
kill -0 "$b_pid" 2> /dev/null && fail "b's old process $b_pid is still there after restart"
if ctl start nothere 2> "$work/nothere.err"; then fail "starting an unknown service succeeded"; fi
grep -q 'no such service: nothere' "$work/nothere.err" || fail "the unknown service was not named" "$work/nothere.err"
echo "ok   mlx-initctl stops, starts and restarts a service, and names an unknown one"

cat > "$work/services/c.service" <<SERVICE
description = added later
exec = /bin/sleep 1000
SERVICE
rm "$work/services/a.service"
ctl reload > /dev/null || fail "reload failed"
wait_state c 30 running || fail "the added service did not start on reload" "$work/init.err"
wait_state a 30 stopped || fail "the removed service did not stop on reload" "$work/init.err"
echo "ok   reload starts added services and stops removed ones"

b_pid=$(pid_of b)
c_pid=$(pid_of c)
kill -TERM "$init_pid"
set +e
wait "$init_pid"
status=$?
set -e
init_pid=
[[ $status -eq 0 ]] || fail "the init exited $status on SIGTERM" "$work/init.err"
for pid in $b_pid $c_pid; do
    kill -0 "$pid" 2> /dev/null && fail "service process $pid is still there after the init ended"
done
grep -q '^mlx-init: services stopped, ending' "$work/init.err" || fail "the init did not say it ended" "$work/init.err"
echo "ok   SIGTERM stops every service and ends the init (not PID 1)"

# --- As PID 1 of a namespace.
if unshare --user --map-root-user --pid --fork --mount-proc --uts true 2> /dev/null; then
    ns="$work/ns"
    mkdir -p "$ns/services" "$ns/run" "$ns/log" "$ns/mnt"
    echo mlxtest > "$ns/hostname"
    echo "tmpfs $ns/mnt tmpfs mode=755,size=1m" > "$ns/mounts"
    cat > "$ns/services/probe.service" <<SERVICE
description = looks around inside the namespace
exec = /bin/sh -c "echo comm=\$(cat /proc/1/comm); echo host=\$(cat /proc/sys/kernel/hostname); echo mnt=\$(grep ' $ns/mnt ' /proc/self/mounts | cut -d' ' -f3); exec sleep 1000"
SERVICE
    unshare --user --map-root-user --pid --fork --mount-proc --uts "$work/mlx-init" --services "$ns/services" --mounts "$ns/mounts" --hostname "$ns/hostname" --control "$ns/run/init" --log "$ns/log" --verbose 2> "$ns/init.err" &
    init_pid=$!
    wait_socket "$ns/run/init" "$ns/init.err"
    nsctl() { "$work/mlx-initctl" --control "$ns/run/init" "$@"; }
    tries=0
    while ! grep -q '^mnt=' "$ns/log/probe.log" 2> /dev/null; do
        tries=$((tries + 1))
        [[ $tries -lt 50 ]] || fail "the probe service did not write its log" "$ns/init.err"
        sleep 0.1
    done
    [[ "$(nsctl status | awk '$1 == "probe" { print $2 }')" == running ]] || fail "the probe is not running under PID 1" "$ns/init.err"
    grep -q '^comm=mlx-init$' "$ns/log/probe.log" || fail "PID 1 of the namespace is not mlx-init" "$ns/log/probe.log"
    grep -q '^host=mlxtest$' "$ns/log/probe.log" || fail "the hostname was not set from the file" "$ns/log/probe.log"
    grep -q '^mnt=tmpfs$' "$ns/log/probe.log" || fail "the mounts file was not applied" "$ns/log/probe.log"
    echo "ok   as PID 1: the hostname, the mounts file and the services"
    nsctl poweroff > /dev/null || fail "poweroff was refused" "$ns/init.err"
    tries=0
    while kill -0 "$init_pid" 2> /dev/null; do
        tries=$((tries + 1))
        [[ $tries -lt 150 ]] || fail "the namespace did not end on poweroff" "$ns/init.err"
        sleep 0.1
    done
    init_pid=
    grep -q '^mlx-init: probe: stopped' "$ns/init.err" || fail "the service was not stopped for the poweroff" "$ns/init.err"
    grep -q '^mlx-init: powering off' "$ns/init.err" || fail "the init did not get to the power off" "$ns/init.err"
    echo "ok   as PID 1: poweroff stops the services, unmounts and hands over to the kernel (the namespace ends)"
else
    echo "skip as PID 1 (unshare with a user and PID namespace does not work here)"
fi

echo "PASS mlx-init"
