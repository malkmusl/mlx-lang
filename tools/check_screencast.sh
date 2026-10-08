#!/usr/bin/env bash
# Screen sharing without PipeWire (projects/desktop/capture: portal.mlx,
# pipewire.mlx, stream.mlx): a nested compositor under
# tools/wayland-test-host, MLXIPC as the session bus, and mlx-capture
# --portal started by it as org.freedesktop.portal.Desktop. Apps ask the
# way browsers do (mlx-capture --share: CreateSession, SelectSources,
# Start, OpenPipeWireRemote) and take the stream with the system's own
# libpipewire (GStreamer's pipewiresrc):
#
#   - the screen: frames at the screen's size that follow the pointer;
#   - a window (the picker's own, as a window to share): frames at its size;
#   - the user says no: nothing is shared;
#   - the picker: Share clicked in the dialog;
#   - with Firefox (FIREFOX=PATH, or firefox installed): a page's
#     getDisplayMedia gets the screen's frames.
#
#   tools/check_screencast.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
command -v xkbcli > /dev/null || { echo "check_screencast.sh: xkbcli is not installed" >&2; exit 2; }
command -v gst-launch-1.0 > /dev/null && gst-inspect-1.0 pipewiresrc > /dev/null 2>&1 || { echo "check_screencast.sh: needs GStreamer with pipewiresrc (gstreamer1.0-tools, gstreamer1.0-pipewire)" >&2; exit 2; }
python3 -c 'import PIL' 2> /dev/null || { echo "check_screencast.sh: needs Python with PIL" >&2; exit 2; }

work=$(mktemp -d)
# A short runtime directory: Unix socket paths are limited.
export XDG_RUNTIME_DIR=$(mktemp -d /tmp/cast.XXXXXX)
export XDG_CONFIG_HOME="$work/config" XDG_STATE_HOME="$work/state"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services"
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    [[ -n "${KEEP:-}" ]] && cp -r "$work" "$KEEP"; rm -rf -- "$work" "$XDG_RUNTIME_DIR"
}
trap cleanup EXIT

for program in ipc/main:mlx-ipcd compositor/main:mlx-compositor capture/main:mlx-capture; do
    "$compiler" --quiet "projects/desktop/${program%%:*}.mlx" -o "$work/${program##*:}"
done
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
xkbcli compile-keymap --layout us > "$work/us.xkb"

fail() {
    echo "FAIL $1" >&2
    for log in bus share compositor host firefox; do
        [[ -f "$work/$log.log" ]] && { echo "--- $log log" >&2; tail -30 "$work/$log.log" >&2; }
    done
    exit 1
}

# share NAME CHOICE HOST-SCRIPT INNER-COMMANDS: a session where the portal
# answers CHOICE (screen, deny, window:NAME, or ask: the picker), the host
# plays HOST-SCRIPT, and INNER-COMMANDS run in the compositor.
share() {
    local name=$1 choice=$2 script=$3 inner=$4
    local choose="--choose $choice"
    [[ "$choice" == ask ]] && choose=
    cat > "$work/services/org.freedesktop.portal.Desktop.service" <<SERVICE
[D-BUS Service]
Name=org.freedesktop.portal.Desktop
Exec=$work/mlx-capture --portal --verbose $choose
SERVICE
    "$work/mlx-ipcd" --verbose --services "$work/services" > "$work/bus.log" 2>&1 &
    bus_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
    printf '%b\n' "$script" > "$work/$name.script"
    printf '#!/bin/sh\nexport DBUS_SESSION_BUS_ADDRESS="unix:path=%s/bus"\n%s\n' "$XDG_RUNTIME_DIR" "$inner" > "$work/$name.sh"
    chmod +x "$work/$name.sh"
    "$work/test-host" "host-$name" "$work/us.xkb" "$work/$name.script" > "$work/host.log" 2>&1 &
    local host=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 120 "$work/mlx-compositor" --renderer cpu --size ${SIZE:-640x400} --no-fps --socket wayland-mlx \
        --dock none --topbar none --launcher none --xwayland none --run "$work/$name.sh" > "$work/compositor.log" 2>&1 || true
    wait "$host" || true
    kill "$bus_pid" 2> /dev/null || true
    wait "$bus_pid" 2> /dev/null || true
    bus_pid=
}

# The pointer moving for `count` tenths of a second, after `wait` ms.
moving() {
    local wait=$1 count=$2
    printf 'wait %s\\n' "$wait"
    for step in $(seq 1 "$count"); do printf 'pointer %s %s\\nwait 100\\n' $((100 + step * 5)) $((100 + step * 3)); done
    printf 'wait 1000\\nclose'
}

# frames DIRECTORY COUNT WIDTH HEIGHT: COUNT PNGs of that size; prints how
# many differ from the first.
frames() {
    python3 - "$@" <<'PY'
import sys, os
from PIL import Image
directory, count, width, height = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
names = sorted(n for n in os.listdir(directory) if n.startswith("frame") and n.endswith(".png"))
if len(names) < count:
    sys.exit(f"{len(names)} frames, wanted {count}")
first = None
changed = 0
for name in names[:count]:
    image = Image.open(os.path.join(directory, name)).convert("RGB")
    if image.size != (width, height):
        sys.exit(f"{name} is {image.size[0]}x{image.size[1]}, wanted {width}x{height}")
    data = image.tobytes()
    if first is None:
        first = data
    elif data != first:
        changed += 1
print(changed)
PY
}

pipeline() {
    echo "timeout 60 $work/mlx-capture --share $1 -- gst-launch-1.0 pipewiresrc fd=3 path=%n num-buffers=$2 ! videoconvert ! pngenc ! multifilesink location=$3/frame%02d.png > $work/share.log 2>&1"
}

# The screen.
mkdir -p "$work/screen"
share screen screen "$(moving 1500 60)" "sleep 0.5; $(pipeline "" 6 "$work/screen")"
changed=$(frames "$work/screen" 6 640 400) || fail "the screen: $changed"
[[ $changed -ge 3 ]] || fail "the screen's frames do not follow the pointer ($changed of 5 changed)"
grep -q "capture: sharing the screen as node" "$work/bus.log" && grep -q "runs with" "$work/bus.log" || fail "the portal's log does not show the stream"
echo "ok   the screen through the portal and libpipewire: 6 frames of 640x400 following the pointer"

# A window: the picker's own (app id org.mlx.capture), as some window
# (it changes little: two frames, the first and one as the pointer passes).
mkdir -p "$work/window"
share window window:org.mlx.capture "$(moving 2500 40)" "$work/mlx-capture --pick someone --types 1 > /dev/null 2>&1 &
sleep 1.5; $(pipeline --windows 2 "$work/window")"
read -r width height < <(python3 -c "
from PIL import Image; import sys
image = Image.open(sys.argv[1]); print(*image.size)" "$work/window/frame00.png" 2> /dev/null) || fail "the window: no frames"
[[ $width -lt 640 && $height -lt 400 && $width -gt 100 ]] || fail "the window's frames are ${width}x$height, not a window's"
frames "$work/window" 2 "$width" "$height" > /dev/null || fail "the window's frames"
echo "ok   a window: frames of ${width}x$height, its own size"

# No.
share deny deny "wait 4000\nclose" "sleep 0.5; timeout 30 $work/mlx-capture --share -- true > $work/share.log 2>&1; echo \$? > $work/deny.status"
grep -q "the user said no" "$work/share.log" && [[ "$(cat "$work/deny.status")" != 0 ]] || fail "a refusal still shared something"
echo "ok   the user says no: nothing is shared"

# The picker: Share clicked (the dialog opens at the top left; one row,
# the screen).
mkdir -p "$work/picked"
share picked ask "wait 4000\npointer 441 212\npress 272\nwait 60\nrelease 272\n$(moving 500 40)" "sleep 0.5; $(pipeline "" 3 "$work/picked")"
grep -q "capture: sharing the screen" "$work/bus.log" || fail "the picker's Share did not share the screen"
frames "$work/picked" 3 640 400 > /dev/null || fail "the picker: frames"
echo "ok   the picker: Share shares the screen"

firefox=${FIREFOX:-$(command -v firefox || true)}
if [[ -n "$firefox" && -x "$firefox" ]]; then
    mkdir -p "$work/profile" "$work/home"
    cat > "$work/page.html" <<'HTML'
<!doctype html><title>cast</title><body style="margin:0;background:#c03">
<video id=v autoplay muted width=320 height=200></video><canvas id=c width=64 height=40></canvas>
<script>
function log(t) { fetch("/log?" + encodeURIComponent(t)); }
let started = false;
async function go() {
  if (started) return;
  started = true;
  try {
    const s = await navigator.mediaDevices.getDisplayMedia({video: true});
    const v = document.getElementById('v'); v.srcObject = s;
    let frames = 0;
    const timer = setInterval(() => {
      if (!v.videoWidth) return;
      log("frame " + (++frames) + " " + v.videoWidth + "x" + v.videoHeight);
      if (frames == 3) { clearInterval(timer); s.getVideoTracks()[0].stop(); }
    }, 500);
  } catch (e) { started = false; log("error " + e.name); }
}
document.addEventListener('click', go);
</script>
HTML
    cat > "$work/profile/user.js" <<'JS'
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
user_pref("toolkit.telemetry.reportingpolicy.firstRun", false);
user_pref("termsofuse.bypassNotification", true);
user_pref("termsofuse.acceptedVersion", 999);
user_pref("termsofuse.acceptedDate", "1700000000000");
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("trailhead.firstrun.didSeeAboutWelcome", true);
user_pref("media.navigator.permission.disabled", true);
JS
    (cd "$work" && exec python3 -m http.server 8767 --bind 127.0.0.1 > "$work/http.log" 2>&1) &
    http=$!
    # Clicks on the page (getDisplayMedia needs one), then the pointer moves.
    script=""
    for click in 1 2 3 4 5 6; do script+="wait 5000\npointer $((500 + click)) 400\npress 272\nwait 60\nrelease 272\n"; done
    SIZE=1024x640 share firefox screen "$script$(moving 0 80)" "export MOZ_ENABLE_WAYLAND=1 XDG_SESSION_TYPE=wayland HOME=$work/home
timeout 70 $firefox --no-remote --profile $work/profile http://127.0.0.1:8767/page.html > $work/firefox.log 2>&1"
    kill "$http" 2> /dev/null || true
    grep -q "GET /log?frame%203%201024x640" "$work/http.log" || fail "Firefox's getDisplayMedia got no frames ($(grep -o 'log?[^ ]*' "$work/http.log" | tail -1))"
    grep -q "capture: firefox asks to share the screen" "$work/bus.log" || fail "the portal did not know Firefox"
    echo "ok   Firefox: getDisplayMedia shows the screen (1024x640) through the portal"
else
    echo "skip Firefox (not installed; FIREFOX=PATH names one)"
fi
echo "all screen-sharing checks passed"
