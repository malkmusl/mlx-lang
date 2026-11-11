#!/usr/bin/env bash
# The desktop's one notion of an app on the host (Linux):
# tests/454_desktop_apps_runtime.mlx runs over data directories made
# here, the repository's bundles (projects/desktop/apps: Vector.app,
# Polaris.app, Control.app) beside desktop entries of foreign software,
# and finds each app by the id its program gives its windows, by its
# entry's file name and by its StartupWMClass, its name and its icon's
# file, and what a dock's pin stands for; the pins file is written into a
# home that has no .config yet. The same on brixOS, on its screen:
# tools/check_brix_apps.sh.
#
#   tests/run_desktop_apps.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

mkdir -p "$work/share/apps" "$work/share/applications" "$work/share/icons/hicolor/64x64/apps" "$work/home"
for app in Vector Polaris Control; do cp -r "projects/desktop/apps/$app.app" "$work/share/apps/"; done
printf '[Desktop Entry]\nType=Application\nName=Files\nExec=nautilus --new-window %%U\nIcon=org.gnome.Nautilus\n' > "$work/share/applications/org.gnome.Nautilus.desktop"
printf '[Desktop Entry]\nType=Application\nName=Firefox\nExec=firefox %%u\nIcon=firefox\nStartupWMClass=Navigator\n' > "$work/share/applications/firefox.desktop"
printf '[Desktop Entry]\nType=Application\nName=Polaris again\nExec=/usr/bin/polaris\n' > "$work/share/applications/polaris.desktop"
# Two apps of one program (both count), and a bundle (its app.ini with
# carriage returns, and blanks around its id) beside a desktop entry of
# another program that goes by the bundle's name.
printf '[Desktop Entry]\nType=Application\nName=Office Writer\nExec=office --writer %%U\n' > "$work/share/applications/office-writer.desktop"
printf '[Desktop Entry]\nType=Application\nName=Office Calc\nExec=office --calc %%U\n' > "$work/share/applications/office-calc.desktop"
mkdir -p "$work/share/apps/Marker.app"
printf '[Desktop Entry]\r\nType=Application\r\nName=Marker\r\nExec=marker\r\nId= org.example.marker \r\n' > "$work/share/apps/Marker.app/app.ini"
printf '[Desktop Entry]\nType=Application\nName=Marker Pen\nExec=marker-pen\n' > "$work/share/applications/marker.desktop"
cp projects/desktop/apps/Voyager.app/icon.png "$work/share/icons/hicolor/64x64/apps/firefox.png"

if ! "$compiler" --quiet tests/454_desktop_apps_runtime.mlx -o "$work/test"; then
    echo "FAIL 454_desktop_apps_runtime did not compile"
    exit 1
fi
status=0
env -i HOME="$work/home" XDG_DATA_DIRS="$work/share" "$work/test" || status=$?
if [[ $status -ne 13 ]]; then
    echo "FAIL 454_desktop_apps_runtime exited $status"
    exit 1
fi
[[ $(cat "$work/home/.config/mlx/dock") == $'terminal\nPolaris\norg.mlx.settings' ]] || { echo "FAIL the pins file is not what was pinned: $(cat "$work/home/.config/mlx/dock" 2>&1)"; exit 1; }
echo "ok   454_desktop_apps_runtime: an app found by the id its program gives its windows, by its entry's file name and by its StartupWMClass; two desktop entries of one program both count; its name; its icon's file (a bundle's own, a theme's); what a pin and a dropped entry stand for (the entry at the dropped path, where two answer to its name); unpinning takes every pin of an app; the pins file written into a home without .config"
