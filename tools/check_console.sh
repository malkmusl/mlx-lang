#!/usr/bin/env bash
# mlx-console (projects/console) without a framebuffer device, a keyboard
# or a console to take over:
#   - the screen is a file (--framebuffer FILE --size WxH) and the keys a
#     FIFO of struct input_event records (--keyboard FIFO): the typed keys
#     reach the program on the pty through the xkb keymap, its output is
#     drawn with the monospaced TrueType font, and the file's pixels equal
#     an independent rendering of the same bytes
#     (tests/support/console_expected.mlx);
#   - the console ends with its program's status, and on SIGTERM.
#
#   tools/check_console.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
console_pid=
cleanup() {
    [[ -n "$console_pid" ]] && kill "$console_pid" 2> /dev/null || true
    rm -rf -- "$work"
}
trap cleanup EXIT
fail() { echo "FAIL $1" >&2; [[ -f "${2:-}" ]] && cat "$2" >&2; exit 1; }

"$compiler" --quiet projects/console/main.mlx -o "$work/mlx-console"
"$compiler" --quiet tests/support/console_expected.mlx -o "$work/expected"
echo "ok   mlx-console builds"
grep -q mlxCompositorCrashed "$work/mlx-console" || fail "mlx-console has no crash handler"

: > "$work/empty.bytes"
set +e
"$work/expected" 160 48 16 "$work/empty.bytes" "$work/probe.argb"
status=$?
set -e
if [[ $status -eq 2 ]]; then
    echo "skip the drawing: no monospaced TrueType font on this machine"
    echo PASS
    exit 0
fi
[[ $status -eq 0 ]] || fail "console_expected failed with $status"

# --- Typed keys reach the program; its output is drawn as expected.
mkfifo "$work/keys"
"$work/mlx-console" --framebuffer "$work/screen.argb" --size 480x200 --keyboard "$work/keys" --no-tty --font-size 16 --layout us -- /bin/sh -c 'read x; echo "got $x"; sleep 20' > "$work/console.log" 2>&1 &
console_pid=$!
sleep 0.5
kill -0 "$console_pid" 2> /dev/null || fail "mlx-console ended at once" "$work/console.log"
# h e l l o Enter, each pressed and released with a SYN_REPORT after.
timeout 20 python3 -I - "$work/keys" <<'PY' || fail "the key events could not be written" "$work/console.log"
import struct, sys, time
events = open(sys.argv[1], "wb", buffering=0)
def event(kind, code, value):
    return struct.pack("qqHHi", 0, 0, kind, code, value)
for code in (35, 18, 38, 38, 24, 28):
    events.write(event(1, code, 1) + event(0, 0, 0))
    time.sleep(0.03)
    events.write(event(1, code, 0) + event(0, 0, 0))
    time.sleep(0.03)
events.close()
PY
sleep 1.5
printf 'hello\r\ngot hello\r\n' > "$work/typed.bytes"
"$work/expected" 480 200 16 "$work/typed.bytes" "$work/expected.argb"
[[ $(stat -c %s "$work/screen.argb") -eq $((480 * 200 * 4)) ]] || fail "the screen file is not 480x200 pixels" "$work/console.log"
cmp -s "$work/screen.argb" "$work/expected.argb" || fail "the console's screen differs from the expected rendering of 'hello' and 'got hello'" "$work/console.log"
echo "ok   typed keys reach the program through the keymap; its output is drawn as std.ui.terminal draws it"

# --- SIGTERM ends it.
kill -TERM "$console_pid"
set +e
wait "$console_pid"
status=$?
set -e
console_pid=
[[ $status -eq 0 ]] || fail "mlx-console did not end cleanly on SIGTERM (exit $status)" "$work/console.log"
echo "ok   SIGTERM ends the console"

# --- The program's status.
set +e
timeout 10 "$work/mlx-console" --framebuffer "$work/screen2.argb" --size 320x100 --keyboard "$work/keys" --no-tty -- /bin/sh -c 'exit 7' > "$work/console2.log" 2>&1
status=$?
set -e
[[ $status -eq 7 ]] || fail "mlx-console did not end with its program's status 7 (exit $status)" "$work/console2.log"
echo "ok   the console ends with its program's status"

# --- Options.
set +e
"$work/mlx-console" --size 10x10 > "$work/options.log" 2>&1
status=$?
set -e
[[ $status -eq 2 ]] && grep -q -- "--size wants" "$work/options.log" || fail "a bad --size is not refused" "$work/options.log"
"$work/mlx-console" --help | grep -q "^usage: mlx-console" || fail "--help"
echo "ok   options are checked"

echo PASS
