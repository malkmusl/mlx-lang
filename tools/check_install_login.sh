#!/usr/bin/env bash
# Checks tools/install_login.sh only inside temporary staged roots. A
# compiler fixture keeps the repeated enable/disable cases fast; the last
# case uses the real compiler and stages all three actual programs.
#
#   tools/check_install_login.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
[[ $# -le 1 ]] || { echo "usage: tools/check_install_login.sh [compiler]" >&2; exit 2; }

work=$(mktemp -d)
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT

bash -n tools/install_login.sh

# Behaves only like the compiler's `SOURCE -o OUTPUT` boundary: each output
# is a distinct executable fixture. The installer must still select all
# three source paths and install the resulting files correctly.
fake_compiler="$work/mlx4-fixture"
cat > "$fake_compiler" <<'COMPILER'
#!/usr/bin/env bash
set -euo pipefail
source_file=""
output=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -o)
            output=$2
            shift 2
            ;;
        *.mlx)
            source_file=$1
            shift
            ;;
        *) shift ;;
    esac
done
[[ -n "$source_file" && -n "$output" ]]
printf '#!/bin/sh\n# compiled from %s\nexit 0\n' "$source_file" > "$output"
chmod 755 "$output"
COMPILER
chmod 755 "$fake_compiler"

fail() {
    echo "FAIL $*" >&2
    exit 1
}

run_installer() {
    local log=$1
    shift
    if ! tools/install_login.sh "$@" > "$log" 2>&1; then
        cat "$log" >&2
        fail "install_login.sh failed for: $*"
    fi
}

assert_link() {
    local path=$1 expected=$2
    [[ -L "$path" ]] || fail "$path is not a symbolic link"
    [[ "$(readlink -- "$path")" == "$expected" ]] || \
        fail "$path points to $(readlink -- "$path"), expected $expected"
}

stage="$work/root"
system="$stage/etc/systemd/system"
install -d "$system/graphical.target.wants"

# Model an existing SDDM selection, a local SDDM override, and an unrelated
# pre-existing link at the place systemctl enable would use for sentinel.
ln -s /usr/lib/systemd/system/sddm.service "$system/display-manager.service"
ln -s /usr/lib/systemd/system/sddm.service "$system/sddm.service"
ln -s /custom/preexisting-login.service "$system/graphical.target.wants/sentinel.service"

if tools/install_login.sh --destdir "$stage" --enable --disable --compiler "$fake_compiler" > "$work/conflict.log" 2>&1; then
    fail "--enable and --disable together were accepted"
fi

# Installing without --enable must leave every display-manager link alone.
run_installer "$work/install.log" --destdir "$stage" --prefix /opt/mlx \
    --compiler "$fake_compiler"
assert_link "$system/display-manager.service" /usr/lib/systemd/system/sddm.service
assert_link "$system/sddm.service" /usr/lib/systemd/system/sddm.service
assert_link "$system/graphical.target.wants/sentinel.service" /custom/preexisting-login.service
[[ ! -e "$stage/var/lib/mlx/login-installer" ]] || fail "a plain install recorded enable state"

run_installer "$work/enable.log" --destdir "$stage" --prefix /opt/mlx --enable \
    --compiler "$fake_compiler"

for program in sentinel aurora umbra; do
    path="$stage/opt/mlx/bin/mlx/$program"
    [[ -x "$path" ]] || fail "$program was not installed executable"
    source_name=${program#mlx-}
    grep -q "projects/desktop/$source_name/main.mlx" "$path" || fail "$program came from the wrong Mlx source"
done
[[ "$(stat -c '%a' "$stage/opt/mlx/bin/mlx/sentinel")" == 755 ]] || fail "installed programs are not mode 0755"

unit="$system/sentinel.service"
[[ -f "$unit" ]] || fail "sentinel.service was not installed"
grep -q '^Requires=synapse.service$' "$unit" || fail "sentinel.service does not require synapse"
grep -q '^Conflicts=getty@tty1.service$' "$unit" || fail "sentinel.service does not reserve tty1 from the getty"
grep -q '^After=synapse.service getty@tty1.service$' "$unit" || fail "sentinel.service does not order the tty1 handover"
grep -q '^ExecStart=/opt/mlx/bin/mlx/sentinel --greeter /opt/mlx/bin/mlx/kepler /opt/mlx/bin/mlx/aurora$' "$unit" || fail "sentinel.service has the wrong ExecStart"
grep -q '^Documentation=file:///opt/mlx/share/doc/mlx/login/README.md$' "$unit" || fail "sentinel.service has the wrong documentation path"
[[ "$(stat -c '%a' "$unit")" == 644 ]] || fail "sentinel.service is not mode 0644"

init_services="$stage/etc/mlx/init/services"
grep -q '^exec = /opt/mlx/bin/mlx/synapse --system$' "$init_services/synapse.service" || fail "impulse IPC service was not rendered"
grep -q '^ready = socket:/run/mlx/bus$' "$init_services/synapse.service" || fail "impulse IPC service does not use socket readiness"
grep -q '^exec = /opt/mlx/bin/mlx/nexus$' "$init_services/nexus.service" || fail "impulse network service was not rendered"
grep -q '^exec = /opt/mlx/bin/mlx/sentinel --greeter /opt/mlx/bin/mlx/kepler /opt/mlx/bin/mlx/aurora$' "$init_services/sentinel.service" || fail "impulse login service was not rendered"
grep -q '^after = synapse nexus$' "$init_services/sentinel.service" || fail "impulse login ordering is wrong"
[[ ! -e "$init_services/console.service" ]] || fail "the greeter boot set installed a competing tty1 console"
grep -qx 'disabled = yes' projects/init/examples/services/console.service || fail "the optional tty1 console is enabled beside the greeter by default"
[[ -f "$stage/opt/mlx/share/doc/mlx/login/README.md" ]] || fail "login README was not installed"
grep -qx 'PATH=${PATH}:/opt/mlx/bin/mlx' "$stage/etc/environment.d/50-mlx.conf" && [[ -f "$stage/etc/profile.d/mlx.sh" ]] || fail "the programs' directory was not put on PATH"
if grep -R -q '@BINDIR@\|@DOCDIR@' "$unit" "$init_services"; then
    fail "an installation placeholder remained in a staged service"
fi

assert_link "$system/display-manager.service" /etc/systemd/system/sentinel.service
assert_link "$system/graphical.target.wants/sentinel.service" /etc/systemd/system/sentinel.service
assert_link "$system/sddm.service" /dev/null
state="$stage/var/lib/mlx/login-installer"
[[ "$(< "$state/previous-name")" == sddm.service ]] || fail "the previous display manager name was not saved"
[[ "$(< "$state/display-manager.target")" == /usr/lib/systemd/system/sddm.service ]] || fail "the previous display-manager alias was not saved"
[[ "$(< "$state/previous-unit.target")" == /usr/lib/systemd/system/sddm.service ]] || fail "the previous unit override was not saved"

# Enabling twice must retain the original snapshot instead of recording the
# links made by the first enable.
run_installer "$work/enable-again.log" --destdir "$stage" --prefix /opt/mlx --enable \
    --compiler "$fake_compiler"
[[ "$(< "$state/display-manager.target")" == /usr/lib/systemd/system/sddm.service ]] || fail "a repeated enable replaced the saved alias"
[[ "$(< "$state/previous-unit.target")" == /usr/lib/systemd/system/sddm.service ]] || fail "a repeated enable replaced the saved unit override"
assert_link "$system/sddm.service" /dev/null

run_installer "$work/disable.log" --destdir "$stage" --disable
assert_link "$system/display-manager.service" /usr/lib/systemd/system/sddm.service
assert_link "$system/sddm.service" /usr/lib/systemd/system/sddm.service
assert_link "$system/graphical.target.wants/sentinel.service" /custom/preexisting-login.service
[[ ! -e "$state" ]] || fail "saved enable state remained after disable"
[[ -x "$stage/opt/mlx/bin/mlx/sentinel" ]] || fail "disable removed installed programs"

# A second disable is a no-op and must not remove anything it just restored.
run_installer "$work/disable-again.log" --destdir "$stage" --disable
assert_link "$system/display-manager.service" /usr/lib/systemd/system/sddm.service
assert_link "$system/sddm.service" /usr/lib/systemd/system/sddm.service

# A pre-existing display-manager mask is data too: enabling may temporarily
# replace it, but disabling must put that exact mask back.
masked_stage="$work/masked-root"
masked_system="$masked_stage/etc/systemd/system"
install -d "$masked_system"
ln -s /dev/null "$masked_system/display-manager.service"
run_installer "$work/masked-enable.log" --destdir "$masked_stage" --enable --compiler "$fake_compiler"
assert_link "$masked_system/display-manager.service" /etc/systemd/system/sentinel.service
run_installer "$work/masked-disable.log" --destdir "$masked_stage" --disable
assert_link "$masked_system/display-manager.service" /dev/null

# Compile and install the actual programs once. This is deliberately after
# the fixture-heavy rollback matrix so those checks do not rebuild three UI
# programs for every state transition.
real_compiler=${1:-$(tools/ensure_compiler.sh)}
real_stage="$work/real-root"
run_installer "$work/real-install.log" --destdir "$real_stage" --compiler "$real_compiler"
for program in sentinel aurora umbra; do
    [[ -s "$real_stage/usr/bin/mlx/$program" && -x "$real_stage/usr/bin/mlx/$program" ]] || \
        fail "the real $program build was not staged executable"
done

echo "ok   install_login stages all programs, units, impulse services and documentation"
echo "ok   --enable is idempotent and masks the previously selected display manager"
echo "ok   --disable restores prior aliases, overrides and masks and is idempotent"
echo "ok   the real compiler builds and stages sentinel, aurora and umbra"
echo "all sentinel installer checks passed"
