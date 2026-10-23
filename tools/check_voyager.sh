#!/usr/bin/env bash
# Voyager, M5 of the Mlx port of WebKit (projects/webkit/PORT.md): a page in
# a window on our compositor. Voyager (projects/voyager: the engine,
# WebCore in the process, and its std.ui.host window, drawn into wl_shm
# buffers) runs as the nested compositor's client under
# tools/wayland-test-host, which plays keys and the pointer from a script
# and takes screenshots (as tools/check_wayland_compositor.sh does).
# Voyager writes each new title and URL of its page to standard error;
# projects/voyager/tests/start.html shows in its title what was done:
#
#   1. the page is shown: the screenshot has the page's blue link box
#      (which also says where the page is on the screen), its user agent
#      WebKit's with "Voyager/0.1 (WebKit 2.54.1; Wayland)" after the platform;
#   2. the wheel scrolls it ("scrolled:N", then back to "scrolled:0");
#   3. a click focuses the text field and typing goes in ("typed:hello");
#   4. a double click selects the word, Ctrl+C copies it to the
#      compositor's clipboard, End and Ctrl+V paste it ("typed:hellohello");
#   5. a click on the link loads second.html (its title, its file: URL);
#   6. the screenshot shows second.html as webcore-dump paints it: blue
#      with a white square of 100 pixels at (50, 50) of the page;
#   7. Alt+Left goes back to start.html, Alt+Right forward to second.html;
#   8. Ctrl+L, an address typed and Enter load it.
#
#   tools/check_voyager.sh [--no-build] [--keep DIR]
#     (--keep: the screenshots and Voyager's log, kept in DIR)
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
fail() { echo "FAIL $1" >&2; exit 1; }
build=1
keep=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        --keep) keep=$2; shift ;;
        *) echo "FAIL unknown argument $1" >&2; exit 1 ;;
    esac
    shift
done
command -v xkbcli > /dev/null || fail "xkbcli is not installed"

if [[ $build -eq 1 ]]; then
    ./mlx build libc webkit-mlx-voyager > /dev/null 2>&1 || fail "./mlx build webkit-mlx-voyager"
    echo "ok   ./mlx build webkit-mlx-voyager"
fi
voyager=$repo_root/mlx-out/webkit/mlx/bin/voyager
[[ -x "$voyager" ]] || fail "$voyager is missing"
compiler=${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}

work=$(mktemp -d)
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
export XDG_STATE_HOME="$work/state"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/home"
trap '[[ -n "$keep" ]] && mkdir -p "$keep" && cp "$work"/*.ppm "$work"/voyager.log "$keep"/ 2> /dev/null; rm -rf -- "$work"' EXIT

"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor" || fail "the compositor does not compile"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host" || fail "the test host does not compile"
xkbcli compile-keymap --layout us > "$work/us.xkb"

# The fonts: Liberation (the settings' families), Ahem.
serif=$(find /usr/share/fonts /usr/local/share/fonts -name LiberationSerif-Regular.ttf 2> /dev/null | head -1)
[[ -n "$serif" ]] || fail "no Liberation fonts (LiberationSerif-Regular.ttf) in /usr/share/fonts"
cat > "$work/fonts.conf" << EOF
<?xml version="1.0"?>
<fontconfig>
    <dir>$(dirname "$serif")</dir>
    <include>$repo_root/projects/webkit/tests/layout/fonts.conf</include>
</fontconfig>
EOF
cat > "$work/voyager-run" << EOF
#!/bin/sh
export HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf"
export VOYAGER_TRACE=1
exec "$voyager" "$repo_root/projects/voyager/tests/start.html" 2>> "$work/voyager.log"
EOF
chmod +x "$work/voyager-run"

run_scenario() {
    local name=$1 script=$2
    : > "$work/voyager.log"
    "$work/test-host" "host-$name" "$work/us.xkb" "$script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 180 "$work/mlx-compositor" --verbose --renderer cpu --socket "nested-$name" --run "$work/voyager-run" > "$work/$name-compositor.log" 2>&1
    wait "$host_pid" || { cat "$work/$name-host.log" >&2; fail "the test host failed ($name)"; }
}

# The bounding box of the pixels of one colour in a PPM screenshot:
# "X Y WIDTH HEIGHT", or nothing.
box_of() {
    python3 -I - "$1" "$2" << 'EOF'
import sys
path, colour = sys.argv[1], bytes.fromhex(sys.argv[2])
data = open(path, "rb").read()
fields, at = [], 0
while len(fields) < 4:
    while data[at:at + 1].isspace():
        at += 1
    if data[at:at + 1] == b"#":
        at = data.index(b"\n", at)
        continue
    end = at
    while not data[end:end + 1].isspace():
        end += 1
    fields.append(data[at:end])
    at = end
width, height = int(fields[1]), int(fields[2])
pixels = data[at + 1:]
xs, ys = [], []
for y in range(height):
    row = pixels[y * width * 3:(y + 1) * width * 3]
    start = row.find(colour)
    while start >= 0:
        if start % 3 == 0:
            xs.append(start // 3)
            ys.append(y)
        start = row.find(colour, start + 1)
if xs:
    print(min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)
EOF
}

# 1: where the page is (its link box, 200x40 at (20, 20) of the page).
cat > "$work/show.script" << EOF
wait 12000
shot $work/start.ppm
close
EOF
run_scenario show "$work/show.script"
grep -q "^voyager: title start$" "$work/voyager.log" || { cat "$work/voyager.log" >&2; fail "Voyager did not show start.html"; }
grep -q "^voyager: user agent Mozilla/5.0 (X11; Linux [^)]*) Voyager/0.1 (WebKit 2.54.1; Wayland) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15$" "$work/voyager.log" || { cat "$work/voyager.log" >&2; fail "Voyager's user agent is not WebKit's with Voyager/0.1 (WebKit 2.54.1; Wayland) after the platform"; }
read -r link_x link_y link_width link_height <<< "$(box_of "$work/start.ppm" 3366cc)"
[[ "${link_width:-0}x${link_height:-0}" == "200x40" ]] || fail "the screenshot has no 200x40 link box of start.html (found: ${link_x:-} ${link_y:-} ${link_width:-}x${link_height:-})"
x0=$(( link_x - 20 ))
y0=$(( link_y - 20 ))
echo "ok   Voyager shows start.html (the page at $x0,$y0 of the screen)"

# 2-6: the session.
field_x=$(( x0 + 60 ))
field_y=$(( y0 + 115 ))
cat > "$work/session.script" << EOF
wait 12000
pointer $(( x0 + 400 )) $(( y0 + 300 ))
scroll 2560
wait 1500
scroll -2560
wait 1500
pointer $field_x $field_y
press 272
release 272
wait 800
type hello
wait 1500
pointer $(( x0 + 40 )) $field_y
press 272
release 272
press 272
release 272
wait 800
down 29
down 46
up 46
up 29
wait 800
down 107
up 107
wait 300
down 29
down 47
up 47
up 29
wait 2000
pointer $(( x0 + 120 )) $(( y0 + 40 ))
press 272
release 272
wait 6000
pointer $(( x0 + 700 )) $(( y0 + 550 ))
wait 500
shot $work/second.ppm
down 56
down 105
up 105
up 56
wait 4000
down 56
down 106
up 106
up 56
wait 4000
down 29
down 38
up 38
up 29
wait 300
type file://$repo_root/projects/voyager/tests/start.html?typed
enter
wait 5000
close
EOF
run_scenario session "$work/session.script"
log=$work/voyager.log
grep -q "^voyager: title scrolled:[1-9]" "$log" || { cat "$log" >&2; fail "the wheel did not scroll the page"; }
grep -q "^voyager: title scrolled:0$" "$log" || { cat "$log" >&2; fail "the wheel did not scroll the page back"; }
echo "ok   the wheel scrolls the page"
grep -q "^voyager: title typed:hello$" "$log" || { cat "$log" >&2; fail "typing did not reach the text field"; }
echo "ok   a click focuses the text field and the keys type into it"
grep -q "^voyager: title typed:hellohello$" "$log" || { cat "$log" >&2; fail "the copied word was not pasted (Ctrl+C, Ctrl+V)"; }
echo "ok   Ctrl+C copies the selection to the clipboard, Ctrl+V pastes it"
grep -q "^voyager: title second$" "$log" && grep -q "^voyager: url file://.*/projects/voyager/tests/second.html$" "$log" || { cat "$log" >&2; fail "the link was not followed"; }
echo "ok   a click on the link loads second.html"
read -r page_x page_y page_width page_height <<< "$(box_of "$work/second.ppm" 3366cc)"
[[ "${page_x:-}" == "$x0" && "${page_y:-}" == "$y0" && "${page_width:-0}" -ge 790 && "${page_height:-0}" -ge 590 ]] || fail "the screenshot does not show second.html's blue page at $x0,$y0 (found: ${page_x:-} ${page_y:-} ${page_width:-}x${page_height:-})"
python3 -I - "$work/second.ppm" "$(( x0 + 50 ))" "$(( y0 + 50 ))" << 'EOF' || fail "the screenshot does not show second.html's white square at (50, 50)"
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(maxsplit=4)
width = int(parts[1])
pixels = parts[4]
x0, y0 = int(sys.argv[2]), int(sys.argv[3])
def pixel(x, y):
    at = (y * width + x) * 3
    return pixels[at:at + 3]
inside = all(pixel(x, y) == b"\xff\xff\xff" for y in range(y0, y0 + 100) for x in range(x0, x0 + 100))
around = all(pixel(x, y) == b"\x33\x66\xcc" for x, y in ((x0 - 1, y0), (x0 + 100, y0), (x0, y0 - 1), (x0, y0 + 100)))
sys.exit(0 if inside and around else 1)
EOF
echo "ok   the screenshot shows second.html as it is painted"
titles=$(grep "^voyager: title \(start\|second\)$" "$log" | sed 's/^voyager: title //' | tr '\n' ' ')
[[ "$titles" == "start second start second start "* ]] || { cat "$log" >&2; fail "back and forward did not go back to start.html and forward to second.html (titles: $titles)"; }
echo "ok   Alt+Left goes back, Alt+Right forward"
grep -q "^voyager: url file://.*/projects/voyager/tests/start.html?typed$" "$log" || { cat "$log" >&2; fail "an address typed into the address field did not load"; }
echo "ok   an address typed into the address field loads"
echo "check_voyager.sh: all checks passed"
