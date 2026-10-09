#!/usr/bin/env bash
# MLX Audio (projects/desktop/audio) end to end, on MLXIPC: mlx-ipcd as the
# session bus, mlx-audiod started by it on the first use (activation),
# playing into a file (--output file:) and recording from a tone or a
# file, and mlx-audio (std.audio) as the app. The mix is checked by
# frequency (Goertzel) and amplitude:
#
#   - a tone plays at the real pace, at its frequency and amplitude;
#   - two streams mix (one at half volume), and a 44100 Hz mono file is
#     resampled to the mix's 48000 Hz;
#   - recordings come back at 48000 Hz stereo and at 16000 Hz mono, and
#     from a looped file;
#   - the permissions are MLXIPC's: recording is asked about for every
#     app (unsandboxed too; with no agent the answer is no) until ipc.conf
#     allows it (`use audio.record`); a sandboxed app (bwrap with a
#     .flatpak-info) may play but not record until then, a deny rule
#     takes playing from an unsandboxed app, and one taking org.mlx.Audio
#     leaves the app no channel;
#   - asking (MLXIPC's `ask`, the default for recording): the bus starts
#     the permission agent (mlx-permissions --answer), a plain answer holds
#     for the session, a remembered one is a rule; SetPermission
#     (mlx-ipc permit) changes it and a recording ends at once; with no
#     agent the answer is no;
#   - an app killed while it plays leaves no stream behind; the server's
#     memory stays the same over a long stream;
#   - virtual sinks: a stream played to one is in its monitor, in the
#     desktop's sound (the default sink's monitor) when the sink goes to
#     the output and not when it goes nowhere; an app's volume and route
#     apply to its streams and are kept in audio.conf.
#
#   tools/check_audio.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
bus_pid=
cleanup() {
    [[ -n "$bus_pid" ]] && kill "$bus_pid" 2> /dev/null || true
    pkill -P $$ 2> /dev/null || true
    [[ -f "$work/audiod.pid" ]] && kill "$(cat "$work/audiod.pid")" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
# Which apps asked for which permission (MLXIPC keeps it there).
export XDG_STATE_HOME="$work/state"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/services"
: > "$XDG_CONFIG_HOME/mlx/ipc.conf"

"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/audio/main.mlx -o "$work/mlx-audiod"
"$compiler" --quiet projects/desktop/audio/tool.mlx -o "$work/mlx-audio"
play="$work/mlx-audio"

fail() {
    echo "FAIL $1" >&2
    for log in bus audiod; do
        [[ -f "$work/$log.log" ]] && { echo "--- $log log" >&2; cat "$work/$log.log" >&2; }
    done
    exit 1
}

# The microphone and the monitors are asked about for every app, sandboxed
# or not; the check has no agent to answer, so a rule lets its own tool
# (mlx-audio, unsandboxed) record.
allow_recording() {
    printf 'allow mlx-audio use audio.record\nallow mlx-audio use audio.monitor\n' > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
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

# Test sounds, and the analysis: the amplitude of each frequency where the
# file is loud.
python3 - "$work" <<'PY'
import sys, wave, struct, math
w = wave.open(sys.argv[1] + '/mono44.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
w.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * 1000 * i / 44100))) for i in range(44100))); w.close()
PY
cat > "$work/analyze.py" <<'PY'
import sys, struct, math
data = open(sys.argv[1], 'rb').read()
at, fmt = 12, None
while at + 8 <= len(data):
    tag, size = data[at:at + 4], struct.unpack('<I', data[at + 4:at + 8])[0]
    if tag == b'fmt ': fmt = struct.unpack('<HHIIHH', data[at + 8:at + 24])
    if tag == b'data':
        body = data[at + 8:]
        break
    at += 8 + size + (size & 1)
channels, rate = fmt[1], fmt[2]
frames = len(body) // (channels * 2)
left = struct.unpack('<%dh' % (frames * channels), body[:frames * channels * 2])[::channels]
block = rate // 10
loud = [i for i in range(0, frames - block, block) if math.sqrt(sum(v * v for v in left[i:i + block]) / block) > 1000]
# Each loud tenth of a second on its own, the mean of what they show.
def block_amplitude(segment, f):
    w = 2 * math.pi * f / rate; c = 2 * math.cos(w); s1 = s2 = 0.0
    for v in segment: s1, s2 = v + c * s1 - s2, s1
    return math.sqrt(max(s1 * s1 + s2 * s2 - c * s1 * s2, 0)) / max(len(segment), 1) * 2
def amplitude(f):
    return sum(block_amplitude(left[i:i + block], f) for i in loud) / max(len(loud), 1)
print(rate, channels, len(loud) / 10.0, *[round(amplitude(float(f))) for f in sys.argv[2:]])
PY
# expect_sound NAME FILE RATE CHANNELS SECONDS FREQUENCY AMPLITUDE [FREQUENCY AMPLITUDE...]
# (amplitudes within 10%; SECONDS of sound within 0.25 s, - for any)
expect_sound() {
    local name=$1 file=$2 rate=$3 channels=$4 seconds=$5
    shift 5
    local frequencies=() amplitudes=()
    while [[ $# -gt 0 ]]; do frequencies+=("$1"); amplitudes+=("$2"); shift 2; done
    local result
    result=$(python3 "$work/analyze.py" "$file" "${frequencies[@]}")
    python3 - "$result" "$rate" "$channels" "$seconds" "${amplitudes[@]}" <<'PY' || fail "$name: got (rate channels seconds amplitudes...) $result"
import sys
got = sys.argv[1].split()
rate, channels, seconds = int(got[0]), int(got[1]), float(got[2])
assert rate == int(sys.argv[2]) and channels == int(sys.argv[3]), 'format'
assert sys.argv[4] == '-' or abs(seconds - float(sys.argv[4])) <= 0.25, 'length'
for value, wanted in zip(map(int, got[3:]), map(int, sys.argv[5:])):
    assert abs(value - wanted) <= max(wanted * 0.1, 200), (value, wanted)
PY
    echo "ok   $name"
}

# The bus; MLX Audio starts on the first use (its service file).
cat > "$work/services/org.mlx.Audio.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.Audio
Exec=$work/mlx-audiod --verbose --output file:$work/mix.wav --input sine:440
SERVICE
"$work/mlx-ipcd" --verbose --services "$work/services" > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/bus" ]] && break; sleep 0.1; done
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"

start=$(date +%s%N)
"$play" tone 440 1 || fail "the first tone"
elapsed=$(( ($(date +%s%N) - start) / 1000000 ))
grep -q "starting org.mlx.Audio" "$work/bus.log" || fail "MLX Audio was not started by the bus"
[[ $elapsed -ge 900 && $elapsed -lt 3000 ]] || fail "a 1 s tone took $elapsed ms"
echo "ok   started on the first use; a 1 s tone takes $elapsed ms"
pgrep -n -x mlx-audiod > "$work/audiod.pid"

"$play" tone 440 2 --volume 50 &
first=$!
"$play" play "$work/mono44.wav" &
second=$!
sleep 0.5
listing=$("$play" streams)
[[ "$listing" == *"mono44.wav"* && "$listing" == *"50%"* ]] || fail "streams: $listing"
echo "ok   streams lists both, with their volume"
wait "$first" "$second"
# Recording is asked about for every app, sandboxed or not: with no agent
# to ask, the answer is no until a rule says otherwise.
if "$play" record "$work/rec.wav" 1 > "$work/out.txt" 2>&1; then fail "an unsandboxed app recorded unasked"; fi
grep -q "refused" "$work/out.txt" || fail "unsandboxed record: $(cat "$work/out.txt")"
grep -q "asking the user whether mlx-audio may use audio.record\|no permission agent to ask" "$work/bus.log" || fail "the bus did not try to ask about mlx-audio"
echo "ok   unsandboxed: recording is asked about too (no agent: refused)"
allow_recording
"$play" record "$work/rec.wav" 1 > /dev/null || fail "recording"
"$play" record "$work/rec16.wav" 1 --rate 16000 --channels 1 > /dev/null || fail "recording at 16000 Hz"
echo "ok   unsandboxed: records once ipc.conf allows it"

# Permissions.
if command -v bwrap > /dev/null && bwrap --bind / / true 2> /dev/null; then
    printf '[Application]\nname=org.example.Player\n' > "$work/flatpak-info"
    sandboxed=(bwrap --bind / / --ro-bind "$work/flatpak-info" /.flatpak-info)
    "${sandboxed[@]}" "$play" tone 440 1 > /dev/null 2>&1 || fail "a sandboxed app may play"
    echo "ok   sandboxed: plays (audio.play by default)"
    if "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > "$work/out.txt" 2>&1; then fail "a sandboxed app recorded"; fi
    grep -q "refused" "$work/out.txt" || fail "sandboxed: $(cat "$work/out.txt")"
    echo "ok   sandboxed: may not record"
    echo "allow org.example.Player use audio.record" > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > /dev/null 2>&1 || fail "the rule did not let it record"
    echo "ok   sandboxed: records once ipc.conf allows it"
    if "${sandboxed[@]}" "$play" record "$work/x.wav" 1 --source monitor > "$work/out.txt" 2>&1; then fail "a sandboxed app recorded what plays"; fi
    grep -q "refused" "$work/out.txt" || fail "sandboxed monitor: $(cat "$work/out.txt")"
    echo "ok   sandboxed: may not record what plays (audio.monitor), recording allowed or not"
    echo "deny org.example.Player talk org.mlx.Audio" > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    if "${sandboxed[@]}" "$play" tone 440 1 > "$work/out.txt" 2>&1; then fail "a denied app got a channel"; fi
    grep -q "may not talk to MLX Audio" "$work/out.txt" || fail "no channel: $(cat "$work/out.txt")"
    echo "ok   sandboxed: no channel when it may not talk to org.mlx.Audio"

    # The permission agent (projects/desktop/permissions), answering at
    # once (--answer): the bus starts it when it has to ask, keeps a plain
    # answer for the session and a remembered one as a rule of ipc.conf.
    "$compiler" --quiet projects/desktop/permissions/main.mlx -o "$work/mlx-permissions"
    "$compiler" --quiet projects/desktop/ipc/tool.mlx -o "$work/mlx-ipc"
    ipc="$work/mlx-ipc"
    agent() {
        pkill -x -P "$bus_pid" mlx-permissions 2> /dev/null || true
        sleep 0.2
        if [[ $1 == none ]]; then
            rm -f "$work/services/org.mlx.PermissionAgent.service"
        else
            printf '[D-BUS Service]\nName=org.mlx.PermissionAgent\nExec=%s --answer %s\n' "$work/mlx-permissions" "$1" > "$work/services/org.mlx.PermissionAgent.service"
        fi
    }
    asked() { grep -c "asking the user whether org.example.Player may use audio.record" "$work/bus.log" || true; }
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    agent deny
    if "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > "$work/out.txt" 2>&1; then fail "it recorded though the user said no"; fi
    grep -q "starting org.mlx.PermissionAgent" "$work/bus.log" && [[ $(asked) -eq 1 ]] || fail "the bus did not start the agent and ask"
    if "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > /dev/null 2>&1; then fail "the second time it recorded"; fi
    [[ $(asked) -eq 1 ]] || fail "the bus asked again within the session"
    expect "the answer holds for the session" "org.example.Player  audio.record  session-deny  sandboxed" "$ipc" permissions
    [[ ! -s "$XDG_CONFIG_HOME/mlx/ipc.conf" ]] || fail "a plain answer became a rule: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")"
    grep -q "org.example.Player	audio.record	1" "$XDG_STATE_HOME/mlx/ipc-seen" || fail "ipc-seen: $(cat "$XDG_STATE_HOME/mlx/ipc-seen")"
    echo "ok   the agent asked once; the no holds for the session, not as a rule"
    "$ipc" permit org.example.Player audio.record default > /dev/null || fail "permit default"
    agent allow-remember
    "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > /dev/null 2>&1 || fail "the user said yes, it did not record"
    grep -qx "allow org.example.Player use audio.record" "$XDG_CONFIG_HOME/mlx/ipc.conf" || fail "no rule remembered: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")"
    expect "the remembered answer" "org.example.Player  audio.record  allow  sandboxed" "$ipc" permissions
    echo "ok   a remembered yes is a rule of ipc.conf"
    # Taken back in the settings (SetPermission): the recording ends
    # (org.mlx.IPC.PermissionsChanged, no recheck asked for).
    "${sandboxed[@]}" "$play" record "$work/taken.wav" 6 > "$work/taken.txt" 2>&1 &
    first=$!
    sleep 1
    "$ipc" permit org.example.Player audio.record deny > /dev/null || fail "permit deny"
    start=$(date +%s)
    wait "$first" || true
    [[ $(( $(date +%s) - start )) -lt 3 ]] && grep -q "ended" "$work/taken.txt" || fail "the recording went on: $(cat "$work/taken.txt")"
    grep -qx "deny org.example.Player use audio.record" "$XDG_CONFIG_HOME/mlx/ipc.conf" && [[ $(wc -l < "$XDG_CONFIG_HOME/mlx/ipc.conf") -eq 1 ]] || fail "SetPermission wrote: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")"
    echo "ok   SetPermission replaces the rule and a recording ends at once"
    expect "a sandboxed app may not change permissions" "AccessDenied" "${sandboxed[@]}" "$ipc" permit org.example.Player audio.record allow
    "$ipc" permit org.example.Player audio.record default > /dev/null || fail "permit default"
    [[ ! -s "$XDG_CONFIG_HOME/mlx/ipc.conf" ]] || fail "default left: $(cat "$XDG_CONFIG_HOME/mlx/ipc.conf")"
    expect "back to the default" "org.example.Player  audio.record  default-ask  sandboxed" "$ipc" permissions
    if command -v dbus-send > /dev/null; then
        expect "only the bus asks the agent" "AccessDenied" dbus-send --session --print-reply --dest=org.mlx.PermissionAgent /org/mlx/PermissionAgent org.mlx.PermissionAgent.Ask string:org.example.Fake string:audio.record boolean:true
    fi
    agent none
    if "${sandboxed[@]}" "$play" record "$work/x.wav" 1 > /dev/null 2>&1; then fail "it recorded with nobody to ask"; fi
    echo "ok   with no agent to ask the answer is no"
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
else
    echo "skip the sandboxed app (no working bwrap)"
fi
echo "deny mlx-audio use audio.play" > "$XDG_CONFIG_HOME/mlx/ipc.conf"
sleep 0.3
if "$play" tone 440 1 > "$work/out.txt" 2>&1; then fail "a denied app played"; fi
echo "ok   a deny rule takes playing from an unsandboxed app"
allow_recording

# An app killed while it plays; a long stream.
timeout 1 "$play" tone 500 30 || true
sleep 0.3
[[ -z "$("$play" streams)" ]] || fail "a killed app's stream stayed: $("$play" streams)"
echo "ok   a killed app leaves no stream"
pid=$(cat "$work/audiod.pid")
before=$(awk '/VmRSS/ {print $2}' "/proc/$pid/status")
"$play" tone 300 8 &
first=$!
"$play" record "$work/long.wav" 8 > /dev/null
wait "$first"
after=$(awk '/VmRSS/ {print $2}' "/proc/$pid/status")
[[ $((after - before)) -lt 512 ]] || fail "the server grew from $before to $after kB"
echo "ok   8 s of playing and recording: the server stays at ${after} kB"

kill "$pid"
for _ in $(seq 1 30); do kill -0 "$pid" 2> /dev/null || break; sleep 0.1; done
python3 - "$work/mix.wav" <<'PY' || fail "the mix's file is not whole"
import sys, struct
data = open(sys.argv[1], 'rb').read()
assert struct.unpack('<I', data[40:44])[0] == len(data) - 44
PY
echo "ok   the mix's file has its final header"
expect_sound "recorded at 48000 Hz stereo" "$work/rec.wav" 48000 2 0.9 440 16384
expect_sound "recorded at 16000 Hz mono" "$work/rec16.wav" 16000 1 0.9 440 16384

# The mix itself, from a fresh server: one tone, then two streams.
"$work/mlx-audiod" --output "file:$work/one.wav" --input "file:$work/mono44.wav" > "$work/audiod.log" 2>&1 &
echo $! > "$work/audiod.pid"
sleep 0.4
"$play" tone 440 1
kill "$(cat "$work/audiod.pid")"; sleep 0.3
expect_sound "a tone: 440 Hz at its amplitude, 1 s" "$work/one.wav" 48000 2 1 440 16000 1000 0
"$work/mlx-audiod" --output "file:$work/two.wav" --input "file:$work/mono44.wav" > "$work/audiod.log" 2>&1 &
echo $! > "$work/audiod.pid"
sleep 0.4
"$play" tone 440 1 --volume 50 &
first=$!
"$play" play "$work/mono44.wav"
wait "$first"
"$play" record "$work/fromfile.wav" 1 --rate 22050 > /dev/null
kill "$(cat "$work/audiod.pid")"; sleep 0.3
expect_sound "mixed: 440 Hz at half volume and the resampled 1000 Hz file" "$work/two.wav" 48000 2 1 440 8000 1000 12000
expect_sound "recorded from a looped file, at 22050 Hz" "$work/fromfile.wav" 22050 2 0.9 1000 12000

# Sinks, monitors and app settings (routing.mlx), on a fresh server whose
# input is an 880 Hz tone (it must never reach a monitor).
"$work/mlx-audiod" --verbose --output "file:$work/routed.wav" --input sine:880 > "$work/audiod.log" 2>&1 &
echo $! > "$work/audiod.pid"
sleep 0.4
"$play" sink add music || fail "adding a sink"
"$play" sink add hidden none || fail "adding a sink that goes nowhere"
sinks=$("$play" sinks)
[[ "$sinks" == *"music"$'\t'"output"* && "$sinks" == *"hidden"$'\t'"none"* ]] || fail "sinks: $sinks"
"$play" tone 660 2 --sink hidden &
first=$!
sleep 0.3
"$play" record "$work/hidden.wav" 1 --source monitor:hidden > /dev/null || fail "recording a sink's monitor"
"$play" record "$work/desktop.wav" 1 --source monitor > /dev/null || fail "recording the desktop's sound"
wait "$first"
expect_sound "a sink's monitor has what plays to it, not the input" "$work/hidden.wav" 48000 2 0.9 660 16000 880 0
expect_sound "a sink to nothing stays out of the output (the default monitor)" "$work/desktop.wav" 48000 2 0 660 0
"$play" app mlx-audio volume 50 || fail "an app's volume"
"$play" tone 440 2 --sink music &
first=$!
sleep 0.3
"$play" record "$work/desktop.wav" 1 --source monitor > /dev/null || fail "recording the desktop's sound"
wait "$first"
expect_sound "a sink to the output: in the desktop's sound, at the app's volume (50%)" "$work/desktop.wav" 48000 2 0.9 440 8000 880 0
"$play" app mlx-audio sink hidden || fail "routing an app"
"$play" tone 440 3 &
first=$!
sleep 0.3
"$play" record "$work/desktop.wav" 1 --source monitor > /dev/null || fail "recording the desktop's sound"
"$play" record "$work/hidden.wav" 1 --source monitor:hidden > /dev/null || fail "recording a sink's monitor"
wait "$first"
expect_sound "the app's route takes its streams to its sink" "$work/hidden.wav" 48000 2 0.9 440 8000
expect_sound "and out of the output" "$work/desktop.wav" 48000 2 0 440 0
for line in "sink = music"$'\t'"output"$'\t'"100"$'\t'"off" "sink = hidden"$'\t'"none"$'\t'"100"$'\t'"off" "app = mlx-audio"$'\t'"50"$'\t'"off"$'\t'"hidden"; do
    grep -qxF "$line" "$XDG_CONFIG_HOME/mlx/audio.conf" || fail "audio.conf lacks \"$line\": $(cat "$XDG_CONFIG_HOME/mlx/audio.conf")"
done
kill "$(cat "$work/audiod.pid")"; sleep 0.3
"$work/mlx-audiod" --output null --input none > "$work/audiod.log" 2>&1 &
echo $! > "$work/audiod.pid"
sleep 0.4
apps=$("$play" apps)
[[ "$("$play" sinks)" == *"hidden"* && "$apps" == *"mlx-audio"$'\t'"32768"$'\t'"0"$'\t'"hidden"* ]] || fail "after a restart: sinks $("$play" sinks), apps $apps"
"$play" app mlx-audio sink default && "$play" app mlx-audio volume 100 && "$play" sink remove music && "$play" sink remove hidden || fail "undoing the routing"
[[ -z "$("$play" sinks)" ]] || fail "sinks left: $("$play" sinks)"
kill "$(cat "$work/audiod.pid")"; sleep 0.3
# The next server takes the card (no output kept from these).
rm -f "$XDG_CONFIG_HOME/mlx/audio.conf"
echo "ok   sinks and app settings are kept in audio.conf and undone"
# A sound card (emulated, tools/fake_alsa.py: the kernel's PCM ioctls as
# frames), busy at first as when PipeWire has it. Playback takes only
# S32_LE, capture only 44100 Hz: the server must negotiate and convert.
python3 tools/fake_alsa.py "$work/card" --busy 2 > "$work/fake.log" 2>&1 &
fake_pid=$!
for _ in $(seq 1 50); do [[ -f "$work/card/ready" ]] && break; sleep 0.1; done
allow_recording
MLX_SND_DIR="$work/card/snd" MLX_ASOUND_DIR="$work/card/asound" "$work/mlx-audiod" --verbose > "$work/audiod.log" 2>&1 &
echo $! > "$work/audiod.pid"
sleep 0.5
expect "a busy card: playing to nobody, and why" "output-problem = busy" "$play" status
sleep 2.5
state=$("$play" status)
[[ "$state" == *"output-problem = "$'\n'* && "$state" == *"device = p"$'\t'"pcmC0D0p"$'\t'"FakeCard: Fake Speakers"* && "$state" == *"device = c"$'\t'"pcmC0D0c"* ]] || fail "the card once free: $state"
grep -q "pcmC0D0p: S32_LE 48000 Hz" "$work/fake.log" && grep -q "pcmC0D0c: S16_LE 44100 Hz" "$work/fake.log" || fail "the card's parameters: $(cat "$work/fake.log")"
echo "ok   the card taken once free (S32_LE playback, 44100 Hz capture negotiated)"
"$play" tone 440 1 || fail "playing to the card"
"$play" record "$work/card-rec.wav" 1 > /dev/null || fail "recording from the card"
expect_sound "recorded from the card (44100 Hz, resampled)" "$work/card-rec.wav" 48000 2 0.9 523 12000

# Controls (mlx-settings uses them): switching while a tone plays,
# remembered in audio.conf.
"$play" tone 440 3 &
first=$!
sleep 0.5
"$play" output null && expect "the output switched" "output = null" "$play" status
"$play" output alsa:pcmC0D0p && expect "and back" "output = alsa:pcmC0D0p" "$play" status
wait "$first" || fail "the tone across the switch"
grep -q "^output = alsa:pcmC0D0p" "$XDG_CONFIG_HOME/mlx/audio.conf" || fail "audio.conf: $(cat "$XDG_CONFIG_HOME/mlx/audio.conf")"
echo "ok   switching outputs while playing; audio.conf keeps the choice"
if command -v bwrap > /dev/null && bwrap --bind / / true 2> /dev/null; then
    expect "sandboxed: no controls (audio.control)" "control = 0" "${sandboxed[@]}" "$play" status
    # Permission taken back while it records.
    echo "allow org.example.Player use audio.record" > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    "${sandboxed[@]}" "$play" record "$work/taken.wav" 6 > "$work/taken.txt" 2>&1 &
    first=$!
    sleep 1
    : > "$XDG_CONFIG_HOME/mlx/ipc.conf"
    sleep 0.3
    "$play" recheck
    start=$(date +%s)
    wait "$first" || true
    [[ $(( $(date +%s) - start )) -lt 3 ]] && grep -q "ended" "$work/taken.txt" || fail "the recording went on: $(cat "$work/taken.txt")"
    echo "ok   a recording ends when its permission is taken back"
fi
kill "$(cat "$work/audiod.pid")"
sleep 0.3
kill "$fake_pid"
sleep 0.5
grep -q FAIL "$work/card/result" && fail "the card saw: $(cat "$work/card/result")"
expect_sound "played to the card" "$work/card/played.wav" 48000 2 - 440 16000
echo "ok   the card saw well-formed ioctls ($(head -1 "$work/card/result"))"
echo "all MLX Audio checks passed"
