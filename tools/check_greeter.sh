#!/usr/bin/env bash
# Checks mlx-greeter's account/session model without a real password or
# the host's account database. The UI itself is compiled here; the
# compositor check exercises it as a Wayland client.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-$(tools/ensure_compiler.sh)}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/greeter/main.mlx -o "$work/mlx-greeter"
"$compiler" --quiet tools/greeter-test/model.mlx -o "$work/model"

mkdir -p "$work/home/wayland-sessions" "$work/system/wayland-sessions"
cat > "$work/home/wayland-sessions/mlx.desktop" <<'ENTRY'
[Desktop Entry]
Name=Mlx Desktop
Name[de]=Mlx Arbeitsfläche
Exec=/opt/mlx/bin/mlx-session
Type=Application
ENTRY
cat > "$work/system/wayland-sessions/other.desktop" <<'ENTRY'
[Desktop Entry]
Name=Other Desktop
Exec=/opt/other/session
Type=Application
ENTRY
cat > "$work/system/wayland-sessions/mlx.desktop" <<'ENTRY'
[Desktop Entry]
Name=Duplicate must lose
Exec=/wrong/session
Type=Application
ENTRY
cat > "$work/system/wayland-sessions/unsafe.desktop" <<'ENTRY'
[Desktop Entry]
Name=Has arguments
Exec=/bin/sh -c something
Type=Application
ENTRY
cat > "$work/system/wayland-sessions/hidden.desktop" <<'ENTRY'
[Desktop Entry]
Name=Hidden
Exec=/hidden/session
Type=Application
Hidden=true
ENTRY

model=$(env -i HOME="$work/empty-home" XDG_DATA_HOME="$work/home" XDG_DATA_DIRS="$work/system" LANG=de_DE.UTF-8 "$work/model")
expected=$(printf 'session Mlx Arbeitsfläche /opt/mlx/bin/mlx-session\nsession Other Desktop /opt/other/session\nselected Mlx Arbeitsfläche /opt/mlx/bin/mlx-session\nselected-next Other Desktop /opt/other/session\nselected-wrap Mlx Arbeitsfläche /opt/mlx/bin/mlx-session\nusers alice Alice Example /home/alice /bin/mlx-sh\nsecret ******\nwiped yes')
[[ "$model" == "$expected" ]] || {
    echo "FAIL greeter model:" >&2
    printf '%s\n' "$model" >&2
    exit 1
}
echo "ok   greeter discovers localized sessions, ignores duplicates and unsafe commands"
echo "ok   greeter selects the preferred session and cycles through every discovered session"
echo "ok   greeter parses ListUsers data and masks then wipes its password field"
echo "all mlx-greeter checks passed"
