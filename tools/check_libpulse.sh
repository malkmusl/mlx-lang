#!/usr/bin/env bash
# MLX Audio's client libraries (projects/desktop/libpulse, built by
# tools/build_libpulse.sh) as the stand-ins for PulseAudio's:
#
#   - each library exports what PulseAudio's does (when those are installed
#     to compare with), under its soname and symbol version PULSE_0;
#   - libpulse's functions on values (sample specs, channel maps, volumes,
#     property lists, formats, UTF-8, times, error texts) return what
#     PulseAudio's return (tools/libpulse_values.py, both called the same);
#   - against mlx-audiod on MLXIPC: libpulse-simple plays (the monitor has
#     the tone) and records (the input's tone); a GLib main loop
#     (libpulse-mainloop-glib) connects and reads the server's info; a
#     signal (pa_signal_new) ends pacat's main loop;
#   - the Debian package (dpkg-deb): its files, and that it provides and
#     replaces libpulse0, libpulse-mainloop-glib0 and libasound2-plugins.
#
# tools/check_pulse.sh runs PulseAudio's programs, ALSA's and Firefox on
# them against the server.
#
#   tools/check_libpulse.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d /tmp/mlx-libpulse.XXXXXX)
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

fail() {
    echo "FAIL $1" >&2
    [[ -f "$work/bus.log" ]] && { echo "--- bus log" >&2; tail -30 "$work/bus.log" >&2; }
    exit 1
}

deb=()
command -v dpkg-deb > /dev/null && deb=(--deb)
tools/build_libpulse.sh --compiler "$compiler" --out "$work/lib" "${deb[@]}" > /dev/null
lib="$work/lib"
system=$(dirname "$(ldconfig -p 2> /dev/null | awk '/libpulse.so.0 \(/ {print $NF; exit}')" 2> /dev/null || true)

# Exports, sonames, versions.
for name in libpulse.so.0 libpulse-simple.so.0 libpulse-mainloop-glib.so.0; do
    if command -v readelf > /dev/null; then
        # (Whole outputs: grep -q leaving early would fail the pipe.)
        dynamic=$(readelf -d "$lib/$name")
        versions=$(readelf -V "$lib/$name")
        [[ "$dynamic" == *"Library soname: [$name]"* ]] || fail "$name: no soname $name"
        [[ "$versions" == *PULSE_0* ]] || fail "$name: no symbol version PULSE_0"
    fi
    if [[ -n "$system" && -f "$system/$name" ]]; then
        ours=$(nm -D --defined-only "$lib/$name" | awk '{print $3}' | grep '^pa_' | sed 's/@.*//' | sort)
        theirs=$(nm -D --defined-only "$system/$name" | awk '{print $3}' | grep '^pa_' | sed 's/@.*//' | sort)
        [[ "$ours" == "$theirs" ]] || fail "$name exports otherwise than PulseAudio's: $(diff <(echo "$ours") <(echo "$theirs") | head -5)"
        echo "ok   $name: PulseAudio's $(echo "$theirs" | wc -l) functions, soname, version PULSE_0"
    else
        echo "ok   $name: soname, version PULSE_0 (no PulseAudio library here to compare its exports with)"
    fi
done
plugin=$(nm -D --defined-only "$lib/libasound_module_pcm_pulse.so")
[[ "$plugin" == *" _snd_pcm_pulse_open"* ]] || fail "the ALSA plugin opens no pulse PCM"
[[ "$plugin" == *" _snd_ctl_pulse_open"* ]] || fail "the ALSA plugin opens no pulse control"
[[ "$(readelf -d "$lib/libasound_module_pcm_pulse.so")" != *libpulse* ]] || fail "the ALSA plugin needs a libpulse"
echo "ok   the ALSA plugin: pulse PCM and control, no libpulse needed"

# Values.
if [[ -n "$system" && -f "$system/libpulse.so.0" ]]; then
    result=$(python3 tools/libpulse_values.py "$lib/libpulse.so.0" "$system/libpulse.so.0") || fail "values: $result"
    echo "ok   values: $(echo "$result" | tail -1) as PulseAudio's"
else
    echo "skip values (no PulseAudio libpulse here to compare with)"
fi

# Against the server.
"$compiler" --quiet projects/desktop/ipc/main.mlx -o "$work/mlx-ipcd"
"$compiler" --quiet projects/desktop/audio/main.mlx -o "$work/mlx-audiod"
cat > "$work/services/org.mlx.Audio.service" <<SERVICE
[D-BUS Service]
Name=org.mlx.Audio
Exec=$work/mlx-audiod --verbose --output null --input sine:440
SERVICE
"$work/mlx-ipcd" --verbose --services "$work/services" --start org.mlx.Audio > "$work/bus.log" 2>&1 &
bus_pid=$!
for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/pulse/native" ]] && break; sleep 0.1; done
[[ -S "$XDG_RUNTIME_DIR/pulse/native" ]] || fail "mlx-audiod does not listen"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"

# Tone measurements, and the libraries driven through ctypes.
cat > "$work/clients.py" <<'PY'
import ctypes, math, os, struct, sys, time
lib = sys.argv[2]
mode = sys.argv[1]

class Spec(ctypes.Structure):
    _fields_ = [("format", ctypes.c_int), ("rate", ctypes.c_uint32), ("channels", ctypes.c_uint8)]

def amplitude(data, hz, rate=48000):
    frames = len(data) // 4
    left = struct.unpack("<%dh" % (frames * 2), data[:frames * 4])[::2]
    middle = left[len(left) // 4: len(left) * 3 // 4]
    w = 2 * math.pi * hz / rate
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in middle:
        s1, s2 = v + c * s1 - s2, s1
    return round(math.sqrt(max(s1 * s1 + s2 * s2 - c * s1 * s2, 0)) / max(len(middle), 1) * 2)

if mode in ("play", "record"):
    simple = ctypes.CDLL(os.path.join(lib, "libpulse-simple.so.0"))
    simple.pa_simple_new.restype = ctypes.c_void_p
    simple.pa_simple_new.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    for name in ("pa_simple_write", "pa_simple_read"):
        getattr(simple, name).argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_int)]
    simple.pa_simple_drain.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    simple.pa_simple_get_latency.restype = ctypes.c_uint64
    simple.pa_simple_get_latency.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    simple.pa_simple_free.argtypes = [ctypes.c_void_p]
    error = ctypes.c_int(0)
    spec = Spec(3, 48000, 2)
    direction = 1 if mode == "play" else 2
    s = simple.pa_simple_new(None, b"simple-check", direction, None, mode.encode(), ctypes.byref(spec), None, None, ctypes.byref(error))
    if not s:
        sys.exit(f"pa_simple_new: error {error.value}")
    if mode == "play":
        tone = bytearray()
        for i in range(48000):
            v = int(12000 * math.sin(2 * math.pi * 660 * i / 48000))
            tone += struct.pack("<hh", v, v)
        start = time.time()
        if simple.pa_simple_write(s, bytes(tone), len(tone), ctypes.byref(error)) < 0:
            sys.exit(f"pa_simple_write: error {error.value}")
        latency = simple.pa_simple_get_latency(s, ctypes.byref(error))
        if simple.pa_simple_drain(s, ctypes.byref(error)) < 0:
            sys.exit(f"pa_simple_drain: error {error.value}")
        print("%d %d" % (round((time.time() - start) * 1000), latency))
    else:
        buffer = ctypes.create_string_buffer(48000 * 4 // 2)
        if simple.pa_simple_read(s, buffer, len(buffer), ctypes.byref(error)) < 0:
            sys.exit(f"pa_simple_read: error {error.value}")
        print(amplitude(buffer.raw, 440))
    simple.pa_simple_free(s)
elif mode == "glib":
    pulse = ctypes.CDLL(os.path.join(lib, "libpulse.so.0"))
    glue = ctypes.CDLL(os.path.join(lib, "libpulse-mainloop-glib.so.0"))
    glib = ctypes.CDLL("libglib-2.0.so.0")
    glue.pa_glib_mainloop_new.restype = ctypes.c_void_p
    glue.pa_glib_mainloop_get_api.restype = ctypes.c_void_p
    glue.pa_glib_mainloop_get_api.argtypes = [ctypes.c_void_p]
    glue.pa_glib_mainloop_free.argtypes = [ctypes.c_void_p]
    pulse.pa_context_new.restype = ctypes.c_void_p
    pulse.pa_context_new.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    pulse.pa_context_connect.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p]
    pulse.pa_context_get_state.argtypes = [ctypes.c_void_p]
    pulse.pa_context_get_server_info.restype = ctypes.c_void_p
    pulse.pa_context_get_server_info.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    pulse.pa_context_disconnect.argtypes = [ctypes.c_void_p]
    pulse.pa_context_unref.argtypes = [ctypes.c_void_p]
    pulse.pa_operation_unref.argtypes = [ctypes.c_void_p]
    loop = glue.pa_glib_mainloop_new(None)
    context = pulse.pa_context_new(glue.pa_glib_mainloop_get_api(loop), b"glib-check")
    pulse.pa_context_connect(context, None, 0, None)
    names = []
    INFO = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
    callback = INFO(lambda c, info, u: names.append(ctypes.cast(ctypes.c_void_p.from_address(info + 24).value, ctypes.c_char_p).value.decode()))
    asked = False
    deadline = time.time() + 10
    while not names and time.time() < deadline and pulse.pa_context_get_state(context) <= 4:
        glib.g_main_context_iteration(None, 1)
        if pulse.pa_context_get_state(context) == 4 and not asked:
            pulse.pa_operation_unref(pulse.pa_context_get_server_info(context, callback, None))
            asked = True
    pulse.pa_context_disconnect(context)
    pulse.pa_context_unref(context)
    glue.pa_glib_mainloop_free(loop)
    print(names[0] if names else "no server info")
elif mode == "measure":
    print(amplitude(open(sys.argv[3], "rb").read(), 660))
PY

# libpulse-simple: a second of 660 Hz, at the real pace, into the monitor.
preload="$lib/libpulse.so.0"
if command -v parec > /dev/null; then
    LD_PRELOAD="$preload" parec -d mlx.output.monitor --format=s16le --rate=48000 --channels=2 > "$work/monitor.raw" &
    recorder=$!
    sleep 0.4
fi
read -r elapsed latency < <(LD_LIBRARY_PATH="$lib" timeout 20 python3 "$work/clients.py" play "$lib") || fail "libpulse-simple: playing"
[[ $elapsed -ge 850 && $elapsed -lt 2500 ]] || fail "libpulse-simple: a second of sound took $elapsed ms"
if [[ -n "${recorder:-}" ]]; then
    sleep 0.3
    kill "$recorder"
    wait "$recorder" 2> /dev/null || true
    level=$(python3 "$work/clients.py" measure "$lib" "$work/monitor.raw")
    [[ $level -gt 10000 && $level -lt 14000 ]] || fail "libpulse-simple: the monitor has 660 Hz at $level (wanted 12000)"
    echo "ok   libpulse-simple plays: $elapsed ms for a second, latency $latency us, the monitor has 660 Hz at $level"
else
    echo "ok   libpulse-simple plays: $elapsed ms for a second, latency $latency us"
fi
level=$(LD_LIBRARY_PATH="$lib" timeout 20 python3 "$work/clients.py" record "$lib") || fail "libpulse-simple: recording"
[[ $level -gt 15000 && $level -lt 18000 ]] || fail "libpulse-simple: the input's 440 Hz at $level (wanted 16384)"
echo "ok   libpulse-simple records the input (440 Hz at $level)"

# libpulse-mainloop-glib.
name=$(LD_LIBRARY_PATH="$lib" timeout 20 python3 "$work/clients.py" glib "$lib") || fail "the GLib main loop"
[[ "$name" == "PulseAudio (on MLX Audio)" ]] || fail "the GLib main loop: $name"
echo "ok   a GLib main loop connects and reads the server's info ($name)"

# Signals: pacat's SIGINT handler (pa_signal_new) ends its main loop.
if command -v parec > /dev/null; then
    LD_PRELOAD="$preload" parec -v --format=s16le > /dev/null 2> "$work/parec.txt" &
    recorder=$!
    sleep 1
    kill -INT "$recorder"
    status=0
    wait "$recorder" || status=$?
    [[ $status -eq 0 ]] && grep -q "Got signal, exiting" "$work/parec.txt" || fail "SIGINT did not end parec's loop (status $status: $(cat "$work/parec.txt"))"
    echo "ok   a signal ends a main loop (parec, SIGINT)"
fi

# The package.
if [[ ${#deb[@]} -gt 0 ]]; then
    package=$(ls "$lib"/mlx-audio-libs_*.deb)
    # (The multiarch directory as MULTIARCH.)
    listed=$(dpkg-deb -c "$package" | awk '{print $NF}' | sed -E 's|^\./||; s|lib/[a-z0-9_]+-linux-[a-z0-9_]+/|lib/MULTIARCH/|')
    for path in usr/lib/MULTIARCH/libpulse.so.0 usr/lib/MULTIARCH/libpulse-simple.so.0 usr/lib/MULTIARCH/libpulse-mainloop-glib.so.0 usr/lib/MULTIARCH/alsa-lib/libasound_module_pcm_pulse.so usr/lib/MULTIARCH/alsa-lib/libasound_module_ctl_pulse.so usr/share/alsa/alsa.conf.d/50-pulseaudio.conf; do
        grep -qx "$path" <<< "$listed" || fail "the package lacks $path"
    done
    for field in Provides Replaces Conflicts; do
        value=$(dpkg-deb -f "$package" "$field")
        [[ "$value" == *libpulse0* && "$value" == *libpulse-mainloop-glib0* && "$value" == *libasound2-plugins* ]] || fail "the package's $field: $value"
    done
    echo "ok   the package $(basename "$package"): the libraries and the plugin, replacing libpulse0, libpulse-mainloop-glib0 and libasound2-plugins"
else
    echo "skip the package (no dpkg-deb)"
fi
echo "all libpulse checks passed"
