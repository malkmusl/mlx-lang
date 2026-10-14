#!/usr/bin/env bash
# Checks the PREFIX/bin/mlx layout (tools/lib/mlx_layout.sh) inside
# temporary staged roots only: the PATH additions and their removal,
# tools/remove_old_mlx_layout.sh on a tree with files of the layouts
# before, and mlx-session finding its prefix and putting its directory on
# PATH. Nothing here builds a program.
#
#   tools/check_mlx_layout.sh
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
[[ $# -eq 0 ]] || { echo "usage: tools/check_mlx_layout.sh" >&2; exit 2; }

work=$(mktemp -d)
cleanup() {
    local status=$?
    trap - EXIT
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT

fail() {
    echo "FAIL $*" >&2
    exit 1
}

bash -n tools/lib/mlx_layout.sh tools/remove_old_mlx_layout.sh
source tools/lib/mlx_layout.sh
[[ "$(mlx_bindir /usr)" == /usr/bin/mlx && "$(mlx_bindir /opt/mlx/)" == /opt/mlx/bin/mlx ]] || fail "mlx_bindir"

# The PATH additions, in a root with sudo, login.defs and a PATH in
# /etc/environment as Debian and Ubuntu have them (made up here: the
# machine's own files are never read).
stage="$work/path-root"
install -d "$stage/etc/fish" "$stage/etc/sudoers.d"
printf 'Defaults\tenv_reset\nDefaults\tsecure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin"\n@includedir /etc/sudoers.d\n' > "$stage/etc/sudoers"
printf '# login.defs\nMAIL_DIR\t/var/mail\nENV_SUPATH\tPATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\nENV_PATH\tPATH=/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games\n#ENV_PATH\tPATH=/commented\n' > "$stage/etc/login.defs"
printf 'PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin"\nLANG=C.UTF-8\n' > "$stage/etc/environment"
chmod 600 "$stage/etc/login.defs"
cp -a "$stage/etc/login.defs" "$work/login.defs.before"
cp -a "$stage/etc/environment" "$work/environment.before"
mlx_install_path_files "$stage" /usr/bin/mlx > "$work/path.log"
mlx_install_path_files "$stage" /usr/bin/mlx > "$work/path-again.log"
profile="$stage/etc/profile.d/mlx.sh"
[[ "$(stat -c '%a' "$profile")" == 644 ]] || fail "the profile script is not mode 0644"
got=$(env -i PATH=/usr/bin:/bin sh -c ". '$profile'; . '$profile'; printf '%s' \"\$PATH\"")
[[ "$got" == /usr/bin:/bin:/usr/bin/mlx ]] || fail "the profile script gives PATH=$got"
got=$(env -i PATH=/usr/bin/mlx:/usr/bin sh -c ". '$profile'; printf '%s' \"\$PATH\"")
[[ "$got" == /usr/bin/mlx:/usr/bin ]] || fail "the profile script added a directory already on PATH: $got"
grep -qx 'PATH=${PATH}:/usr/bin/mlx' "$stage/etc/environment.d/50-mlx.conf" || fail "environment.d has no PATH line"
grep -qx '\[Manager\]' "$stage/etc/systemd/system.conf.d/50-mlx-path.conf" && \
    grep -qx 'DefaultEnvironment="PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/bin/mlx"' "$stage/etc/systemd/system.conf.d/50-mlx-path.conf" || \
    fail "the system manager's PATH drop-in is wrong"
grep -qx 'contains -- /usr/bin/mlx $PATH; or set -gx PATH $PATH /usr/bin/mlx' "$stage/etc/fish/conf.d/mlx.fish" || fail "fish's PATH line is wrong"
sudo_file="$stage/etc/sudoers.d/50-mlx-path"
if command -v visudo > /dev/null; then
    grep -qx 'Defaults secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/usr/bin/mlx"' "$sudo_file" || fail "sudo's secure_path drop-in is wrong"
    [[ "$(stat -c '%a' "$sudo_file")" == 440 ]] || fail "the sudoers drop-in is not mode 0440"
    visudo -c -q -f "$sudo_file" || fail "visudo rejects the sudoers drop-in"
    # sudoers without quotes (Fedora's way) and without a secure_path.
    other="$work/other-root"
    install -d "$other/etc/sudoers.d"
    printf 'Defaults    secure_path = /sbin:/bin:/usr/sbin:/usr/bin\n' > "$other/etc/sudoers"
    mlx_install_path_files "$other" /usr/bin/mlx > /dev/null
    grep -qx 'Defaults secure_path="/sbin:/bin:/usr/sbin:/usr/bin:/usr/bin/mlx"' "$other/etc/sudoers.d/50-mlx-path" || fail "an unquoted secure_path was not kept"
    : > "$other/etc/sudoers"
    mlx_install_path_files "$other" /usr/bin/mlx > /dev/null
    grep -qx 'Defaults secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/usr/bin/mlx"' "$other/etc/sudoers.d/50-mlx-path" || fail "without a secure_path the default was not taken"
else
    [[ ! -e "$sudo_file" ]] || fail "a sudoers drop-in was installed without visudo to check it"
fi
grep -qx $'ENV_SUPATH\tPATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/bin/mlx' "$stage/etc/login.defs" && \
    grep -qx $'ENV_PATH\tPATH=/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/usr/bin/mlx' "$stage/etc/login.defs" && \
    grep -qx $'#ENV_PATH\tPATH=/commented' "$stage/etc/login.defs" || { cat "$stage/etc/login.defs" >&2; fail "login.defs' PATH values are wrong"; }
[[ "$(stat -c '%a' "$stage/etc/login.defs")" == 600 ]] || fail "login.defs lost its mode"
grep -qx 'PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/usr/bin/mlx"' "$stage/etc/environment" && \
    grep -qx 'LANG=C.UTF-8' "$stage/etc/environment" || { cat "$stage/etc/environment" >&2; fail "/etc/environment's PATH is wrong"; }
if grep -q 'changed' "$work/path-again.log"; then fail "a second install changed the edited files again"; fi
no_fish="$work/no-fish-root"
mlx_install_path_files "$no_fish" /usr/bin/mlx > /dev/null
[[ ! -e "$no_fish/etc/fish" ]] || fail "a fish setting was written without fish"
echo "ok   PATH gets /usr/bin/mlx once, after the system's directories (login shells, user session, system services, sudo, su and console logins, PAM, fish)"

# Removed only with the last program.
install -d "$stage/usr/bin/mlx"
touch "$stage/usr/bin/mlx/mlx-netd"
mlx_remove_path_files "$stage" /usr/bin/mlx > /dev/null
[[ -f "$profile" ]] || fail "the PATH additions went while a program was left"
rm -f "$stage/usr/bin/mlx/mlx-netd"
mlx_remove_path_files "$stage" /usr/bin/mlx > /dev/null
for path in "${mlx_path_files[@]}"; do
    [[ ! -e "$stage$path" ]] || fail "$path remained after the last program"
done
[[ ! -e "$stage/usr/bin/mlx" ]] || fail "the empty /usr/bin/mlx remained"
cmp -s "$stage/etc/login.defs" "$work/login.defs.before" && cmp -s "$stage/etc/environment" "$work/environment.before" || fail "login.defs or /etc/environment differ from before the install"
[[ "$(stat -c '%a' "$stage/etc/login.defs")" == 600 ]] || fail "login.defs lost its mode on removal"
echo "ok   the PATH additions go with the last program, the edited files as they were"

# A machine with the layouts before: the root installer's /usr/bin, the
# other installers' /usr/local, partly reinstalled.
old="$work/old-root"
put() {
    install -d "$(dirname "$old$1")"
    printf '%s\n' "${2:-old}" > "$old$1"
}
put /usr/local/bin/mlx-compositor
put /usr/local/bin/mlx-netd
put /usr/bin/mlx-terminal
put /usr/bin/ls "not ours"
ln -s mlx-observatory "$old/usr/local/bin/mlx-codemap"
put /usr/local/share/applications/org.mlx.files.desktop
put /usr/local/share/applications/someone-else.desktop "not ours"
put /usr/local/share/mlx/dbus-1/services/org.mlx.Audio.service
put /usr/local/lib/mlx-audio/libasound_module_pcm_pulse.so
put /usr/local/share/mlx/sound-servers-replaced pipewire.socket
put /etc/alsa/conf.d/99-zz-mlx-audio.conf 'pcm.mlx { type pulse lib "/usr/local/lib/mlx-audio/libasound_module_pcm_pulse.so" }'
put /etc/systemd/system/mlx-login.service 'ExecStart=/usr/local/bin/mlx-login --greeter /usr/local/bin/mlx-compositor /usr/local/bin/mlx-greeter'
put /etc/systemd/system/mlx-netd.service 'ExecStart=/usr/bin/mlx/mlx-netd'
for program in mlx-compositor mlx-terminal mlx-codemap mlx-observatory; do put "/usr/bin/mlx/$program" new; done
put /usr/share/applications/org.mlx.files.desktop new
put /usr/share/mlx/dbus-1/services/org.mlx.Audio.service new
put /usr/lib/mlx-audio/libasound_module_pcm_pulse.so new

tools/remove_old_mlx_layout.sh --destdir "$old" > "$work/dry.log"
grep -qx 'old     /usr/local/bin/mlx-compositor (now /usr/bin/mlx/mlx-compositor)' "$work/dry.log" || { cat "$work/dry.log" >&2; fail "the dry run did not list the old compositor"; }
grep -qx 'kept    /usr/local/bin/mlx-netd (no /usr/bin/mlx/mlx-netd yet: reinstall first)' "$work/dry.log" || fail "the dry run did not keep mlx-netd"
grep -q '^stale   /etc/systemd/system/mlx-login.service ' "$work/dry.log" || fail "the stale unit was not reported"
if grep -q 'mlx-netd.service' "$work/dry.log"; then fail "a unit of the new layout was reported stale"; fi
[[ -f "$old/usr/local/bin/mlx-compositor" && -L "$old/usr/local/bin/mlx-codemap" ]] || fail "the dry run removed something"
echo "ok   remove_old_mlx_layout.sh lists without --remove and changes nothing"

tools/remove_old_mlx_layout.sh --destdir "$old" --remove > "$work/remove.log"
for path in /usr/local/bin/mlx-compositor /usr/local/bin/mlx-codemap /usr/bin/mlx-terminal \
    /usr/local/share/applications/org.mlx.files.desktop /usr/local/share/mlx/dbus-1/services/org.mlx.Audio.service \
    /usr/local/lib/mlx-audio/libasound_module_pcm_pulse.so /usr/local/share/mlx/sound-servers-replaced; do
    [[ ! -e "$old$path" && ! -L "$old$path" ]] || { cat "$work/remove.log" >&2; fail "$path was not removed"; }
done
[[ ! -e "$old/usr/local/share/mlx" && ! -e "$old/usr/local/lib/mlx-audio" ]] || fail "empty old directories remained"
[[ -f "$old/usr/local/bin/mlx-netd" ]] || fail "mlx-netd went without a new copy"
[[ "$(< "$old/usr/bin/ls")" == "not ours" && "$(< "$old/usr/local/share/applications/someone-else.desktop")" == "not ours" ]] || fail "a file that is not ours was touched"
for program in mlx-compositor mlx-terminal mlx-codemap; do
    [[ "$(< "$old/usr/bin/mlx/$program")" == new ]] || fail "the new $program was touched"
done
[[ "$(< "$old/usr/share/mlx/sound-servers-replaced")" == pipewire.socket ]] || fail "the sound servers' marker did not move"
grep -qF 'lib "/usr/lib/mlx-audio/libasound_module_pcm_pulse.so"' "$old/etc/alsa/conf.d/99-zz-mlx-audio.conf" || fail "the ALSA setting still names the old library"
tools/remove_old_mlx_layout.sh --destdir "$old" --remove > "$work/again.log"
grep -qx 'kept    /usr/local/bin/mlx-netd (no /usr/bin/mlx/mlx-netd yet: reinstall first)' "$work/again.log" && \
    grep -qx '1 kept until their new counterparts are installed' "$work/again.log" || { cat "$work/again.log" >&2; fail "a second run did more than keep mlx-netd"; }
echo "ok   --remove takes the old copies with new counterparts, moves the sound marker, fixes ALSA's path, keeps the rest"

# mlx-session in PREFIX/bin/mlx: its MLXIPC services under PREFIX/share
# and its directory on PATH. A stand-in mlx-ipcd records how it was run.
session="$work/session-root/usr/bin/mlx"
install -d "$session"
install -m 755 projects/desktop/compositor/session/mlx-session "$session/mlx-session"
printf '#!/bin/sh\nprintf "%%s\\n" "$*" > "%s/ipcd-args"\nprintf "%%s\\n" "$PATH" > "%s/ipcd-path"\n' "$work" "$work" > "$session/mlx-ipcd"
printf '#!/bin/sh\nexit 0\n' > "$session/mlx-compositor"
chmod 755 "$session/mlx-ipcd" "$session/mlx-compositor"
env -i HOME="$work/home" PATH=/usr/bin:/bin MLX_SESSION_INSIDE=1 "$session/mlx-session"
grep -q -- "--services $work/session-root/usr/share/mlx/dbus-1/services --start org.mlx.Audio --run $session/mlx-compositor " "$work/ipcd-args" || { cat "$work/ipcd-args" >&2; fail "mlx-session gave mlx-ipcd the wrong services"; }
[[ "$(< "$work/ipcd-path")" == "/usr/bin:/bin:$session" ]] || fail "mlx-session's PATH is $(< "$work/ipcd-path")"
echo "ok   mlx-session in PREFIX/bin/mlx runs MLXIPC on PREFIX/share and puts its directory on PATH"

echo "all mlx layout checks passed"
