#!/usr/bin/env bash
# Focused checks for mlx-init service identities, dependency-aware shutdown,
# and persistent rotating logs. The main init check invokes the same behavior
# in its broader boot scenario; keeping this file focused makes failures much
# easier to diagnose while developing these three features.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
init_pid=
cleanup() {
    [[ -n "$init_pid" ]] && kill "$init_pid" 2> /dev/null || true
    sleep 0.1
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet projects/init/main.mlx -o "$work/mlx-init"
"$compiler" --quiet projects/init/tool.mlx -o "$work/mlx-initctl"

wait_for() {
    local description=$1
    local command=$2
    local tries=0
    until eval "$command"; do
        tries=$((tries + 1))
        [[ $tries -lt 100 ]] || fail "$description" "$work/init.err"
        sleep 0.05
    done
}

mkdir -p "$work/services" "$work/run" "$work/log"
current_user=$(id -un)
current_group=$(id -gn)
current_uid=$(id -u)
current_gid=$(id -g)
cat > "$work/services/identity.service" <<SERVICE
type = oneshot
user = $current_user
group = $current_group
exec = /bin/sh -c "echo uid=\$(id -u) gid=\$(id -g) home=\$HOME user=\$USER"
SERVICE
cat > "$work/services/refused.service" <<SERVICE
type = oneshot
user = mlx-init-account-that-must-not-exist
exec = /bin/sh -c "touch $work/ran-as-the-wrong-user"
SERVICE
"$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/log" --verbose 2> "$work/init.err" &
init_pid=$!
wait_for "the identity service did not finish" "grep -q '^uid=' '$work/log/identity.log' 2>/dev/null"
wait_for "the unknown service account was not diagnosed" "grep -q 'refused: has an unknown or unavailable user/group' '$work/init.err' 2>/dev/null"
grep -q "^uid=$current_uid gid=$current_gid .* user=$current_user$" "$work/log/identity.log" || fail "user/group were not applied to the service" "$work/log/identity.log"
[[ ! -e "$work/ran-as-the-wrong-user" ]] || fail "a service with an unknown user was run under the init identity"
if grep -q 'unknown key' "$work/init.err"; then fail "user/group were treated as unknown service keys" "$work/init.err"; fi
kill -TERM "$init_pid"
wait "$init_pid"
init_pid=
echo "ok   service user/group names select its uid, gid and environment"

# When subordinate-id mappings are available, exercise the privileged path:
# namespace root must clear its groups and really become a different account.
if unshare --map-root-user --map-users 1:100000:65534 --map-groups 1:100000:65534 true 2>/dev/null; then
    rm -rf "$work/services" "$work/run" "$work/log"
    mkdir -p "$work/services" "$work/run" "$work/log"
    cat > "$work/services/drop.service" <<SERVICE
type = oneshot
user = nobody
group = nobody
exec = /bin/sh -c "echo uid=\$(id -u) gid=\$(id -g) groups=\$(id -G)"
SERVICE
    unshare --map-root-user --map-users 1:100000:65534 --map-groups 1:100000:65534 "$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/log" 2> "$work/init.err" &
    init_pid=$!
    wait_for "the namespace service did not drop privileges" "grep -q '^uid=' '$work/log/drop.log' 2>/dev/null"
    grep -q '^uid=65534 gid=65534 groups=65534$' "$work/log/drop.log" || fail "setgroups/setgid/setuid did not apply the configured account" "$work/log/drop.log"
    kill -TERM "$init_pid"
    wait "$init_pid"
    init_pid=
    echo "ok   namespace root clears supplementary groups and drops to the configured account"
else
    echo "skip service privilege drop (subordinate uid/gid mappings are unavailable)"
fi

rm -rf "$work/services" "$work/run" "$work/log"
mkdir -p "$work/services" "$work/run" "$work/log"
cat > "$work/services/base.service" <<SERVICE
exec = /bin/sh -c "trap 'echo base-term >> $work/shutdown; sleep 0.25; echo base-end >> $work/shutdown; exit 0' TERM; echo base-ready >> $work/shutdown; while :; do sleep 1; done"
stop-timeout = 2s
SERVICE
cat > "$work/services/dependent.service" <<SERVICE
after = base
exec = /bin/sh -c "trap 'echo dependent-term >> $work/shutdown; sleep 0.25; echo dependent-end >> $work/shutdown; exit 0' TERM; echo dependent-ready >> $work/shutdown; while :; do sleep 1; done"
stop-timeout = 2s
SERVICE
"$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/log" --verbose 2> "$work/init.err" &
init_pid=$!
wait_for "the dependent service did not start" "grep -q '^dependent-ready$' '$work/shutdown' 2>/dev/null"
kill -TERM "$init_pid"
wait "$init_pid"
init_pid=
dependent_end=$(grep -n '^dependent-end$' "$work/shutdown" 2>/dev/null | cut -d: -f1 || true)
base_term=$(grep -n '^base-term$' "$work/shutdown" 2>/dev/null | cut -d: -f1 || true)
[[ -n "$dependent_end" && -n "$base_term" && "$dependent_end" -lt "$base_term" ]] || fail "shutdown did not wait for the dependent before stopping its dependency" "$work/shutdown"
echo "ok   shutdown stops and waits for dependents before their dependencies"

rm -rf "$work/services" "$work/run" "$work/log" "$work/persistent"
mkdir -p "$work/services" "$work/run" "$work/runtime" "$work/blocked"
# A non-directory at the persistent location makes opening it fail even when
# this check runs as root. Replacing it later models /var becoming writable.
touch "$work/blocked/persistent"
cat > "$work/services/writer.service" <<SERVICE
exec = /bin/sh -c "n=0; while :; do echo record-\$n-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; n=\$((n+1)); done"
stop-timeout = 1s
SERVICE
"$work/mlx-init" --services "$work/services" --control "$work/run/init" --log "$work/blocked/persistent" --runtime-log "$work/runtime" --log-size 32768 2> "$work/init.err" &
init_pid=$!
wait_for "the service did not write its temporary log" "test -s '$work/runtime/writer.log'"
rm "$work/blocked/persistent"
mkdir "$work/blocked/persistent"
wait_for "logging did not move to the persistent directory" "test -s '$work/blocked/persistent/writer.log'"
wait_for "the persistent log was not rotated" "test -s '$work/blocked/persistent/writer.log.1'"
kill -TERM "$init_pid"
wait "$init_pid"
init_pid=
grep -q '^record-' "$work/blocked/persistent"/writer.log* || fail "the rotated logs have no service output"
echo "ok   logs move to writable persistent storage and rotate"

echo "PASS mlx-init identities, shutdown order and logs"
