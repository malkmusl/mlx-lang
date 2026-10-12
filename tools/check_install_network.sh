#!/usr/bin/env bash
# Checks tools/install_network.sh --enable and --disable, only inside
# temporary staged roots (--destdir): --enable masks NetworkManager,
# systemd-networkd and wpa_supplicant and keeps what was there; --disable
# puts exactly that back, leaves mlx-ipcd enabled and says what to start;
# a root enabled by an older installer (masks, no saved state) is unmasked.
# A compiler fixture stands in for the real compiler (only its
# `SOURCE -o OUTPUT` boundary matters here).
#
#   tools/check_install_network.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $*" >&2; exit 1; }

bash -n tools/install_network.sh

fake_compiler="$work/mlx4-fixture"
cat > "$fake_compiler" <<'COMPILER'
#!/usr/bin/env bash
set -euo pipefail
source_file=""
output=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -o) output=$2; shift 2 ;;
        *.mlx) source_file=$1; shift ;;
        *) shift ;;
    esac
done
[[ -n "$source_file" && -n "$output" ]]
printf '#!/bin/sh\n# compiled from %s\nexit 0\n' "$source_file" > "$output"
chmod 755 "$output"
COMPILER
chmod 755 "$fake_compiler"

run_installer() {
    local log=$1
    shift
    tools/install_network.sh "$@" > "$log" 2>&1 || { cat "$log" >&2; fail "install_network.sh failed for: $*"; }
}
assert_link() {
    [[ -L "$1" ]] || fail "$1 is not a symbolic link"
    [[ "$(readlink -- "$1")" == "$2" ]] || fail "$1 points to $(readlink -- "$1"), expected $2"
}
assert_absent() {
    [[ ! -e "$1" && ! -L "$1" ]] || fail "$1 is still there"
}

# A machine on NetworkManager, systemd-networkd masked by its owner, a
# local override file for wpa_supplicant.
stage="$work/root"
system="$stage/etc/systemd/system"
install -d "$system/multi-user.target.wants"
ln -s /usr/lib/systemd/system/NetworkManager.service "$system/multi-user.target.wants/NetworkManager.service"
ln -s /dev/null "$system/systemd-networkd.service"
printf '[Service]\nExecStart=/bin/true\n' > "$system/wpa_supplicant.service"

run_installer "$work/enable.log" --destdir "$stage" --enable "$fake_compiler"
for unit in NetworkManager.service systemd-networkd.service wpa_supplicant.service; do
    assert_link "$system/$unit" /dev/null
done
assert_link "$system/multi-user.target.wants/mlx-netd.service" /etc/systemd/system/mlx-netd.service
assert_link "$system/multi-user.target.wants/mlx-ipcd.service" /etc/systemd/system/mlx-ipcd.service
[[ -x "$stage/usr/bin/mlx/mlx-netd" ]] || fail "mlx-netd was not installed in /usr/bin/mlx"
grep -q '^ExecStart=/usr/bin/mlx/mlx-netd' "$system/mlx-netd.service" || fail "mlx-netd.service does not start /usr/bin/mlx/mlx-netd"
grep -qx 'Type=notify' "$system/mlx-ipcd.service" || fail "mlx-ipcd.service does not wait for the bus's READY=1"
grep -qx 'Documentation=file:///usr/share/doc/mlx/ipc/README.md' "$system/mlx-ipcd.service" && [[ -f "$stage/usr/share/doc/mlx/ipc/README.md" ]] || fail "mlx-ipcd.service does not point at its installed README"
grep -qx 'Documentation=file:///usr/share/doc/mlx/network/README.md' "$system/mlx-netd.service" || fail "mlx-netd.service does not point at its installed README"
grep -qx 'PATH=${PATH}:/usr/bin/mlx' "$stage/etc/environment.d/50-mlx.conf" && [[ -f "$stage/etc/profile.d/mlx.sh" && -f "$stage/etc/systemd/system.conf.d/50-mlx-path.conf" ]] || fail "/usr/bin/mlx was not put on PATH"
# A second --enable keeps the state of before the first.
run_installer "$work/enable-again.log" --destdir "$stage" --enable "$fake_compiler"
echo "ok   --enable masks the replaced services and keeps what was there"

run_installer "$work/disable.log" --destdir "$stage" --disable
assert_absent "$system/NetworkManager.service"
assert_link "$system/systemd-networkd.service" /dev/null
[[ -f "$system/wpa_supplicant.service" && ! -L "$system/wpa_supplicant.service" ]] || fail "wpa_supplicant's override file was not restored"
grep -qx 'ExecStart=/bin/true' "$system/wpa_supplicant.service" || fail "wpa_supplicant's override file changed"
assert_absent "$system/multi-user.target.wants/mlx-netd.service"
assert_link "$system/multi-user.target.wants/mlx-ipcd.service" /etc/systemd/system/mlx-ipcd.service
assert_link "$system/multi-user.target.wants/NetworkManager.service" /usr/lib/systemd/system/NetworkManager.service
grep -q 'to start after daemon-reload: NetworkManager.service' "$work/disable.log" || fail "--disable did not name NetworkManager to start: $(cat "$work/disable.log")"
assert_absent "$stage/var/lib/mlx/network-installer"
[[ -x "$stage/usr/bin/mlx/mlx-netd" ]] || fail "--disable removed the installed program"
run_installer "$work/disable-again.log" --destdir "$stage" --disable
assert_link "$system/systemd-networkd.service" /dev/null
grep -q 'was not enabled; nothing changed' "$work/disable-again.log" || fail "a second --disable changed something: $(cat "$work/disable-again.log")"
echo "ok   --disable restores exactly what --enable masked, keeps mlx-ipcd, names what to start"

# A root an older installer enabled: masks, no saved state.
legacy="$work/legacy"
lsystem="$legacy/etc/systemd/system"
install -d "$lsystem/multi-user.target.wants"
ln -s /etc/systemd/system/mlx-netd.service "$lsystem/multi-user.target.wants/mlx-netd.service"
ln -s /etc/systemd/system/mlx-ipcd.service "$lsystem/multi-user.target.wants/mlx-ipcd.service"
ln -s /usr/lib/systemd/system/NetworkManager.service "$lsystem/multi-user.target.wants/NetworkManager.service"
for unit in NetworkManager.service systemd-networkd.service wpa_supplicant.service; do ln -s /dev/null "$lsystem/$unit"; done
run_installer "$work/legacy.log" --destdir "$legacy" --disable
for unit in NetworkManager.service systemd-networkd.service wpa_supplicant.service; do assert_absent "$lsystem/$unit"; done
assert_absent "$lsystem/multi-user.target.wants/mlx-netd.service"
assert_link "$lsystem/multi-user.target.wants/mlx-ipcd.service" /etc/systemd/system/mlx-ipcd.service
echo "ok   --disable on an older install unmasks the replaced services"

if tools/install_network.sh --destdir "$stage" --enable --disable > "$work/both.log" 2>&1; then fail "--enable with --disable was accepted"; fi
echo "check_install_network.sh: all checks passed"
