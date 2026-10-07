#!/usr/bin/env bash
# End-to-end check of mlx-capture's OBS plugin (projects/desktop/capture/obs.mlx,
# built with `mlx4 --plugin`): OBS runs as a Wayland client of the nested
# compositor (tools/wayland-test-host plays the session) with a scene whose
# only source is "MLX Capture" on the screen, and records. The plugin must
# load, connect to the compositor and hand OBS frames; the recording must
# grow, and OBS's preview (in the host's screenshot) must show the screen,
# OBS's own window included.
#
#   tools/check_obs_plugin.sh [compiler]
#
# Needs obs (OBS Studio 30 or later) and xkbcli; without obs it says so and
# passes.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
command -v obs > /dev/null || { echo "skip the OBS plugin (obs is not installed)"; exit 0; }
command -v xkbcli > /dev/null || { echo "check_obs_plugin.sh: xkbcli is not installed" >&2; exit 2; }

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
trap 'rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet projects/desktop/settings/main.mlx -o "$work/mlx-settings"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet --plugin projects/desktop/capture/obs.mlx -o "$work/mlx-capture.so"
xkbcli compile-keymap --layout us > "$work/us.xkb"

# OBS's configuration: the plugin, a profile that records, and a scene
# collection with the source (no first-run wizard).
config="$work/obs/config"
mkdir -p "$config/obs-studio/plugins/mlx-capture/bin/64bit" "$config/obs-studio/basic/profiles/mlx" "$config/obs-studio/basic/scenes" "$work/obs/recordings"
cp "$work/mlx-capture.so" "$config/obs-studio/plugins/mlx-capture/bin/64bit/"
cat > "$config/obs-studio/global.ini" <<INI
[General]
FirstRun=true
[Basic]
Profile=mlx
ProfileDir=mlx
SceneCollection=mlx
SceneCollectionFile=mlx
INI
cat > "$config/obs-studio/basic/profiles/mlx/basic.ini" <<INI
[General]
Name=mlx
[Video]
BaseCX=1024
BaseCY=768
OutputCX=1024
OutputCY=768
FPSType=0
FPSCommon=30
[Output]
Mode=Simple
[SimpleOutput]
FilePath=$work/obs/recordings
RecFormat2=mkv
RecQuality=Stream
RecEncoder=x264
VBitrate=2500
INI
cat > "$config/obs-studio/basic/scenes/mlx.json" <<'JSON'
{"current_scene":"Main","current_program_scene":"Main","scene_order":[{"name":"Main"}],"name":"mlx","sources":[{"id":"mlx_capture_source","versioned_id":"mlx_capture_source","name":"MLX Screen","settings":{"target":""},"enabled":true},{"id":"scene","versioned_id":"scene","name":"Main","settings":{"items":[{"name":"MLX Screen","visible":true,"pos":{"x":0.0,"y":0.0},"scale":{"x":1.0,"y":1.0},"id":1,"bounds_type":0,"align":5}],"id_counter":1}}]}
JSON
cat > "$work/obs.sh" <<SCRIPT
#!/bin/sh
export HOME="$work/obs"
export XDG_CONFIG_HOME="$config"
export QT_QPA_PLATFORM=wayland
exec obs --multi --disable-shutdown-check --disable-missing-files-check --collection mlx --profile mlx --scene Main --startrecording > "$work/obs.log" 2>&1
SCRIPT
chmod +x "$work/obs.sh"
cat > "$work/obs.script" <<SCRIPT
wait 25000
shot $work/obs.ppm
close
SCRIPT

"$work/test-host" host-obs "$work/us.xkb" "$work/obs.script" > "$work/host.log" 2>&1 &
host_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-obs" ]] && break; sleep 0.1; done
WAYLAND_DISPLAY=host-obs XDG_CONFIG_HOME="$work/config" timeout 90 "$work/mlx-compositor" --verbose --renderer cpu --socket nested-obs --no-fps --launcher none --dock none --topbar none --xwayland none --size 1600x1000 --run "$work/mlx-settings" --run "$work/obs.sh" > "$work/compositor.log" 2>&1 || true
wait "$host_pid" || { echo "test host failed:" >&2; cat "$work/host.log" >&2; exit 1; }

log=$(ls "$config"/obs-studio/logs/*.txt 2> /dev/null | head -1)
[[ -n "$log" ]] || { echo "OBS wrote no log" >&2; cat "$work/obs.log" "$work/compositor.log" >&2; exit 1; }
for line in "\[mlx-capture\] loaded" "\[mlx-capture\] connected to the compositor" "\[mlx-capture\] first frame handed to OBS" "Recording Start"; do
    grep -q "$line" "$log" || { echo "OBS's log lacks \"$line\"" >&2; cat "$log" >&2; exit 1; }
done
recording=$(ls "$work"/obs/recordings/*.mkv 2> /dev/null | head -1)
[[ -n "$recording" && $(stat -c %s "$recording") -gt 100000 ]] || { echo "OBS recorded nothing" >&2; ls -la "$work/obs/recordings" >&2; exit 1; }
python3 - "$work/obs.ppm" <<'PY'
import sys
data = open(sys.argv[1], 'rb').read()
_, size, _, pixels = data.split(b'\n', 3)
width, height = map(int, size.split())
def at(x, y):
    i = (y * width + x) * 3
    return pixels[i:i + 3]
# OBS's window (at 64, 56) with its preview: the captured screen shows the
# compositor's blue title bars inside the preview, not only OBS's dark
# background.
blue = sum(1 for y in range(100, 500, 2) for x in range(300, 1000, 2) if at(x, y)[2] > 200 and at(x, y)[0] < 140)
assert blue > 200, blue
PY
echo "ok   OBS loads mlx-capture's plugin, records the screen through it, and previews it"
