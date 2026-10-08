#!/usr/bin/env bash
# MLX Audio's PulseAudio server (projects/desktop/audio/pulse.mlx) end to
# end, with PulseAudio's own programs as the apps (pactl, pacat, paplay,
# parec from pulseaudio-utils; there is no PulseAudio server) on MLXIPC:
#
#   - mlx-ipcd starts mlx-audiod at once (--start org.mlx.Audio), which
#     answers at $XDG_RUNTIME_DIR/pulse/native: pactl info, its sink, the
#     input and the monitor;
#   - streams of each sample format (S16, float, S24, S24_32, S32, U8),
#     mono and stereo, at 22050 to 48000 Hz play at the real pace, and the
#     monitor (parec -d mlx.output.monitor) has them at their frequency
#     and amplitude; parec records the input;
#   - sink inputs are listed with their app, a stream's volume and the
#     sink's volume and mute change (pactl), subscribers hear streams come
#     and go;
#   - a sandboxed app (bwrap with a .flatpak-info) plays, may not record
#     until a rule allows it, may not change the server, and its
#     recording ends when the rule goes;
#   - ALSA programs (aplay, arecord) through alsa-plugins' pulse plugin
#     and asound.conf;
#   - with Firefox (FIREFOX=PATH, or firefox installed): an <audio>
#     element plays through MLX Audio and its clock reaches the end.
#
#   tools/check_pulse.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
for tool in pactl pacat parec; do
    command -v "$tool" > /dev/null || { echo "check_pulse.sh: $tool is not installed (pulseaudio-utils)" >&2; exit 2; }
done

work=$(mktemp -d /tmp/mlx-pulse.XXXXXX)
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
export XDG_STATE_HOME="$work/state"
export HOME="$work/home"
unset PULSE_SERVER PULSE_COOKIE
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services" "$HOME"
: > "$XDG_CONFIG_HOME/mlx/ipc.conf"

"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/ipc/tool.mlx -o "$work/mlx-ipc"
"$compiler" --quiet projects/desktop/audio/main.mlx -o "$work/mlx-audiod"
"$compiler" --quiet projects/desktop/audio/tool.mlx -o "$work/mlx-audio"

fail() {
    echo "FAIL $1" >&2
    for log in bus audiod; do
        [[ -f "$work/$log.log" ]] && { echo "--- $log log" >&2; tail -40 "$work/$log.log" >&2; }
    done
    exit 1
}

expect() {
    # expect NAME WANTED COMMAND...: the command's output contains WANTED.
    local name=$1 wanted=$2
    shift 2
    local output
    output=$(timeout 10 "$@" 2>&1 || true)
    [[ "$output" == *"$wanted"* ]] || fail "$name: wanted \"$wanted\", got: $output"
    echo "ok   $name"
}

# Raw samples: a tone (FILE FORMAT RATE CHANNELS SECONDS HZ AMPLITUDE), and
# what a capture holds (FILE HZ: seconds loud, the frequency's amplitude
# where it is loud).
cat > "$work/sound.py" <<'PY'
import math, struct, sys
if sys.argv[1] == "make":
    path, fmt, rate, channels, seconds, hz, amplitude = sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), float(sys.argv[6]), float(sys.argv[7]), float(sys.argv[8])
    out = bytearray()
    for i in range(int(rate * seconds)):
        v = amplitude * math.sin(2 * math.pi * hz * i / rate) / 32768
        for _ in range(channels):
            if fmt == "s16le": out += struct.pack("<h", int(v * 32767))
            elif fmt == "float32le": out += struct.pack("<f", v)
            elif fmt == "s32le": out += struct.pack("<i", int(v * 2147483647))
            elif fmt == "s24le": out += struct.pack("<i", int(v * 8388607))[:3]
            elif fmt == "s24-32le": out += struct.pack("<i", int(v * 8388607))
            elif fmt == "u8": out += bytes([int(128 + v * 127)])
    open(path, "wb").write(out)
else:
    path, hz = sys.argv[2], float(sys.argv[3])
    data = open(path, "rb").read()
    frames = len(data) // 4
    left = struct.unpack("<%dh" % (frames * 2), data[:frames * 4])[::2]
    loud = [i for i, v in enumerate(left) if abs(v) > 300]
    if not loud:
        print("0 0")
        sys.exit()
    middle = left[loud[0] + 2400:loud[-1] - 2400] or left[loud[0]:loud[-1]]
    w = 2 * math.pi * hz / 48000
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in middle:
        s1, s2 = v + c * s1 - s2, s1
    print("%.2f %d" % ((loud[-1] - loud[0]) / 48000, round(math.sqrt(max(s1 * s1 + s2 * s2 - c * s1 * s2, 0)) / max(len(middle), 1) * 2)))
PY

# expect_played NAME FORMAT RATE CHANNELS: a 1 s tone of 660 Hz at 12000
# plays at the real pace and the monitor has it.
expect_played() {
    local name=$1 format=$2 rate=$3 channels=$4
    python3 "$work/sound.py" make "$work/tone.raw" "$format" "$rate" "$channels" 1 660 12000
    parec -d mlx.output.monitor --format=s16le --rate=48000 --channels=2 > "$work/monitor.raw" &
    local recorder=$!
    sleep 0.4
    local start
    start=$(date +%s%N)
    timeout 10 pacat --playback --format="$format" --rate="$rate" --channels="$channels" "$work/tone.raw" || fail "$name: pacat failed"
    local elapsed=$(( ($(date +%s%N) - start) / 1000000 ))
    sleep 0.3
    kill "$recorder"
    wait "$recorder" 2> /dev/null || true
    local seconds amplitude
    read -r seconds amplitude < <(python3 "$work/sound.py" measure "$work/monitor.raw" 660)
    [[ $elapsed -ge 850 && $elapsed -lt 2500 ]] || fail "$name: 1 s of sound took $elapsed ms"
    python3 -c "import sys; s, a = float('$seconds'), int('$amplitude'); sys.exit(0 if abs(s - 1) < 0.2 and abs(a - 12000) < 1200 else 1)" || fail "$name: the monitor has $seconds s at amplitude $amplitude (wanted 1 s at 12000)"
    echo "ok   $name: $elapsed ms, the monitor has $seconds s of 660 Hz at $amplitude"
}

# The bus starts MLX Audio at once: PulseAudio apps do not come through it.
cat > "$work/services/org.mlx.Audio.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.Audio
Exec=$work/mlx-audiod --verbose --output null --input sine:440
SERVICE
"$work/mlx-ipcd" --verbose --services "$work/services" --start org.mlx.Audio > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/pulse/native" ]] && break; sleep 0.1; done
[[ -S "$XDG_RUNTIME_DIR/pulse/native" ]] || fail "mlx-audiod does not listen at \$XDG_RUNTIME_DIR/pulse/native"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
ln -sf "$work/bus.log" "$work/audiod.log"

expect "pactl info: the server" "Server Name: PulseAudio (on MLX Audio)" pactl info
expect "pactl info: its default sink" "Default Sink: mlx.output" pactl info
expect "the sink" "mlx.output" pactl list sinks short
expect "the input" "mlx.input" pactl list sources short
expect "the monitor" "mlx.output.monitor" pactl list sources short

expect_played "S16, stereo, 44100 Hz" s16le 44100 2
expect_played "float, mono, 48000 Hz" float32le 48000 1
expect_played "S24, stereo, 22050 Hz" s24le 22050 2
expect_played "S24 in 32 bits, stereo, 32000 Hz" s24-32le 32000 2
expect_played "S32, stereo, 48000 Hz" s32le 48000 2
expect_played "U8, mono, 8000 Hz" u8 8000 1

timeout 3 parec --format=s16le --rate=48000 --channels=2 > "$work/input.raw" || true
read -r seconds amplitude < <(python3 "$work/sound.py" measure "$work/input.raw" 440)
python3 -c "import sys; sys.exit(0 if abs(int('$amplitude') - 16384) < 1600 else 1)" || fail "parec: the input's 440 Hz at $amplitude"
echo "ok   parec records the input (440 Hz at $amplitude)"

# ALSA programs, through alsa-plugins' pulse plugin (asound.conf; the
# installer puts it in /etc/alsa/conf.d, here the user's asoundrc, which
# ALSA reads last).
if command -v aplay > /dev/null && ls /usr/lib/*/alsa-lib/libasound_module_pcm_pulse.so > /dev/null 2>&1; then
    mkdir -p "$XDG_CONFIG_HOME/alsa"
    cp projects/desktop/audio/asound.conf "$XDG_CONFIG_HOME/alsa/asoundrc"
    python3 "$work/sound.py" make "$work/alsa.raw" s16le 44100 2 1 660 12000
    parec -d mlx.output.monitor --format=s16le --rate=48000 --channels=2 > "$work/monitor.raw" &
    recorder=$!
    sleep 0.4
    timeout 10 aplay -q -t raw -f S16_LE -r 44100 -c 2 "$work/alsa.raw" || fail "aplay"
    sleep 0.3
    kill "$recorder"
    wait "$recorder" 2> /dev/null || true
    read -r seconds amplitude < <(python3 "$work/sound.py" measure "$work/monitor.raw" 660)
    python3 -c "import sys; s, a = float('$seconds'), int('$amplitude'); sys.exit(0 if abs(s - 1) < 0.2 and abs(a - 12000) < 1200 else 1)" || fail "aplay: the monitor has $seconds s at $amplitude"
    grep -q "aplay plays" "$work/bus.log" || fail "aplay did not play through MLX Audio"
    played=$amplitude
    timeout 5 arecord -q -t raw -f S16_LE -r 48000 -c 2 -d 1 "$work/arecord.raw" || fail "arecord"
    read -r seconds amplitude < <(python3 "$work/sound.py" measure "$work/arecord.raw" 440)
    python3 -c "import sys; sys.exit(0 if abs(int('$amplitude') - 16384) < 1600 else 1)" || fail "arecord: 440 Hz at $amplitude"
    echo "ok   ALSA programs: aplay plays (660 Hz at $played in the monitor), arecord records the input (440 Hz at $amplitude)"
    rm -f "$XDG_CONFIG_HOME/alsa/asoundrc"
else
    echo "skip ALSA programs (no aplay or no alsa-plugins pulse plugin)"
fi

# Streams, volumes, mute, events.
timeout 10 pactl subscribe > "$work/events.txt" 2>&1 &
subscriber=$!
sleep 0.3
python3 "$work/sound.py" make "$work/long.raw" s16le 48000 2 3 660 12000
pacat --playback --client-name=checker --stream-name=long-tone --format=s16le --rate=48000 --channels=2 "$work/long.raw" &
player=$!
sleep 0.6
inputs=$(pactl list sink-inputs)
[[ "$inputs" == *"media.name = \"long-tone\""* && "$inputs" == *"application.name = \"checker\""* ]] || fail "the sink input: $inputs"
echo "ok   the sink input is listed with its app and name"
index=$(pactl list sink-inputs short | awk '{print $1; exit}')
pactl set-sink-input-volume "$index" 50% || fail "set-sink-input-volume"
expect "a stream's volume" "50%" pactl list sink-inputs
pactl set-sink-volume @DEFAULT_SINK@ 50% || fail "set-sink-volume"
pactl set-sink-mute @DEFAULT_SINK@ 1 || fail "set-sink-mute"
expect "the sink's volume" "50%" pactl list sinks
expect "the sink's mute" "Mute: yes" pactl list sinks
expect "MLX Audio's own volume follows" "volume = 8192" "$work/mlx-audio" status
pactl set-sink-volume @DEFAULT_SINK@ 100%
pactl set-sink-mute @DEFAULT_SINK@ 0
wait "$player" || fail "the long tone"
sleep 0.5
kill "$subscriber" 2> /dev/null || true
grep -q "Event 'new' on sink-input" "$work/events.txt" && grep -q "Event 'remove' on sink-input" "$work/events.txt" || fail "events: $(cat "$work/events.txt")"
echo "ok   subscribers hear a sink input come and go"

# A sandboxed app.
if command -v bwrap > /dev/null && bwrap --bind / / true 2> /dev/null; then
    printf '[Application]\nname=org.example.PulsePlayer\n' > "$work/flatpak-info"
    sandboxed=(bwrap --bind / / --ro-bind "$work/flatpak-info" /.flatpak-info)
    python3 "$work/sound.py" make "$work/short.raw" s16le 48000 2 0.5 660 12000
    timeout 10 "${sandboxed[@]}" pacat --playback --format=s16le --rate=48000 --channels=2 "$work/short.raw" || fail "a sandboxed app may play"
    grep -q "org.example.PulsePlayer connected (PulseAudio" "$work/bus.log" || fail "the sandboxed app was not known by its id"
    echo "ok   sandboxed: plays, known as org.example.PulsePlayer"
    if timeout 5 "${sandboxed[@]}" parec --format=s16le > /dev/null 2> "$work/out.txt"; then fail "a sandboxed app recorded"; fi
    grep -qi "denied" "$work/out.txt" || fail "sandboxed parec: $(cat "$work/out.txt")"
    echo "ok   sandboxed: may not record (asked, and there is no agent)"
    expect "sandboxed: may not change the server" "denied" "${sandboxed[@]}" pactl set-sink-volume @DEFAULT_SINK@ 10%
    "$work/mlx-ipc" permit org.example.PulsePlayer audio.record allow > /dev/null || fail "permit allow"
    timeout 2 "${sandboxed[@]}" parec --format=s16le --rate=48000 --channels=2 > "$work/allowed.raw" 2> "$work/out.txt" || true
    [[ $(stat -c %s "$work/allowed.raw") -gt 100000 ]] || fail "the rule did not let it record: $(cat "$work/out.txt")"
    echo "ok   sandboxed: records once a rule allows it"
    "${sandboxed[@]}" parec --format=s16le > /dev/null 2> "$work/taken.txt" &
    recorder=$!
    sleep 1
    "$work/mlx-ipc" permit org.example.PulsePlayer audio.record deny > /dev/null || fail "permit deny"
    start=$(date +%s)
    for _ in $(seq 1 30); do kill -0 "$recorder" 2> /dev/null || break; sleep 0.1; done
    kill -0 "$recorder" 2> /dev/null && { kill "$recorder"; fail "the recording went on after the rule went"; }
    echo "ok   sandboxed: its recording ends when the permission goes ($(tr '\n' ' ' < "$work/taken.txt"))"
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
else
    echo "skip the sandboxed app (no working bwrap)"
fi

# Firefox.
firefox=${FIREFOX:-$(command -v firefox || true)}
if [[ -n "$firefox" && -x "$firefox" ]]; then
    python3 "$work/sound.py" make "$work/tone.pcm" s16le 44100 2 3 523 12000
    python3 - "$work/tone.pcm" "$work/site/tone.wav" <<'PY'
import sys, wave, os
os.makedirs(os.path.dirname(sys.argv[2]), exist_ok=True)
w = wave.open(sys.argv[2], "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100); w.writeframes(open(sys.argv[1], "rb").read()); w.close()
PY
    cat > "$work/site/page.html" <<'HTML'
<!doctype html><title>tone</title>
<script>
const a = new Audio("tone.wav");
a.onended = () => fetch("/ended?" + a.currentTime.toFixed(2));
a.play().then(() => fetch("/playing")).catch(e => fetch("/blocked?" + encodeURIComponent(e)));
</script>
HTML
    mkdir -p "$work/profile"
    # No portals: here they cannot start without a display.
    cat > "$work/profile/user.js" <<'JS'
user_pref("media.autoplay.default", 0);
user_pref("media.autoplay.blocking_policy", 0);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.reportingpolicy.firstRun", false);
user_pref("widget.use-xdg-desktop-portal.settings", 0);
user_pref("widget.use-xdg-desktop-portal.file-picker", 0);
user_pref("widget.use-xdg-desktop-portal.mime-handler", 0);
user_pref("widget.use-xdg-desktop-portal.open-uri", 0);
JS
    (cd "$work/site" && exec python3 -m http.server 8765 --bind 127.0.0.1 > "$work/http.log" 2>&1) &
    server=$!
    parec -d mlx.output.monitor --format=s16le --rate=48000 --channels=2 > "$work/firefox.raw" &
    recorder=$!
    sleep 0.5
    GDK_DEBUG=no-portals timeout 60 "$firefox" --headless --no-remote --profile "$work/profile" "http://127.0.0.1:8765/page.html" > "$work/firefox.log" 2>&1 &
    browser=$!
    for _ in $(seq 1 500); do grep -q "GET /ended" "$work/http.log" 2> /dev/null && break; kill -0 "$browser" 2> /dev/null || break; sleep 0.1; done
    sleep 0.3
    kill "$browser" "$recorder" "$server" 2> /dev/null || true
    wait "$browser" 2> /dev/null || true
    ended=$(grep -o "GET /ended?[0-9.]*" "$work/http.log" || true)
    [[ -n "$ended" ]] || fail "Firefox did not play to the end: $(cat "$work/http.log")"
    read -r seconds amplitude < <(python3 "$work/sound.py" measure "$work/firefox.raw" 523)
    python3 -c "import sys; s, a = float('$seconds'), int('$amplitude'); sys.exit(0 if abs(s - 3) < 0.4 and a > 9000 else 1)" || fail "Firefox's tone: $seconds s at amplitude $amplitude"
    grep -q "firefox connected (PulseAudio" "$work/bus.log" || fail "Firefox was not known as firefox"
    echo "ok   Firefox plays through MLX Audio: $seconds s of 523 Hz at $amplitude; its clock reached ${ended#GET /ended?} s"
else
    echo "skip Firefox (not installed; FIREFOX=PATH names one)"
fi

kill "$bus_pid"
wait "$bus_pid" 2> /dev/null || true
bus_pid=
sleep 0.3
[[ ! -e "$XDG_RUNTIME_DIR/pulse/native" ]] || fail "the PulseAudio socket stayed after the server ended"
echo "ok   the socket goes with the server"
echo "all PulseAudio checks passed"
