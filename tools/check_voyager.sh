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
#   8. Ctrl+L, an address typed and Enter load it;
#   9. the toolbar (std.ui's widgets) is drawn in the desktop's colour
#      scheme: light in the scenarios above (appearance.conf), dark with
#      MLX_COLOR_SCHEME=dark, where projects/voyager/tests/scheme.html,
#      which allows dark (color-scheme), is drawn dark: its canvas, its
#      prefers-color-scheme box, its text field (the theme's), its title
#      "scheme:dark";
#  10. the desktop switched from light to dark while Voyager runs
#      (appearance.conf rewritten): the toolbar and the page follow, the
#      page without a reload;
#  11. a URL of more than 512 bytes (std.ui.field has no fixed size) is
#      loaded whole, and the address field, focused by Ctrl+L, holds all
#      of it, scrolled so that its end is in view;
#  12. tabs (projects/voyager/tests/tabs.html): a `target=_blank` link and
#      window.open each open second.html in a new tab, shown; a middle
#      click on the link opens it in a tab behind, which loads while
#      another is shown; Ctrl+1, Ctrl+Tab, Ctrl+Shift+Tab and Ctrl+9 show
#      tabs, Ctrl+T opens one with the address field taking the keys,
#      Ctrl+W closes them, the last one closing the window; Ctrl+Shift+B
#      folds the sidebar to its narrow strip and opens it again (the page
#      moves left by the difference and back);
#  13. forced dark mode, which follows the desktop (Voyager has no
#      setting for it): on a dark desktop second.html, which has no dark
#      design of its own, is painted dark (its white square dark, its blue
#      kept a blue); the desktop switched to light while it runs, it is
#      painted as it is again (the square white);
#  14. Voyager's internal pages (the voyager: scheme): voyager://about,
#      started with, is "About Voyager" on the theme's light background,
#      with Voyager's version and the engine's; about:version typed into
#      the address field is voyager://version, "Voyager Version", whose
#      text has the versions and the whole user agent (with VOYAGER_TRACE
#      the engine writes an internal page's text); about:about is
#      voyager://about again, an unknown voyager://nothing a 404 page
#      ("Not Found"), voyager:// the index ("Voyager Pages"); on a dark
#      desktop voyager://about is in its own dark design (the theme's dark
#      background), not forced dark; voyager://version shows the
#      profile Voyager keeps its data in ($XDG_DATA_HOME/voyager, here
#      ~/.local/share/voyager of the check's home) and its cache;
#  15. the profile: projects/voyager/tests/storage.html counts its visits
#      in localStorage and notes one in sessionStorage; Voyager closed and
#      started again on it, the count goes on ("visits:2", read from the
#      profile's local storage database) and the session's note is gone;
#  16. downloads: projects/voyager/tests/download.html saves a Blob and a
#      file's link (download attributes) into the download folder
#      (XDG_DOWNLOAD_DIR, a temporary one), each kept with what it holds;
#      the sidebar lists them under "Downloads", and voyager://downloads
#      (typed into the address field) shows each one done, its size and
#      where it is;
#  17. an upload: a click on projects/voyager/tests/upload.html's
#      <input type=file> opens the file chooser (std.ui.chooser) over the
#      page in the home folder; Down chooses the only file there, Enter
#      opens it, and the page reads it (FileReader: "uploaded:NAME:TEXT").
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
# The desktop's scheme: light (std.ui.appearance reads it; without the
# file it would be dark).
echo "color-scheme = light" > "$XDG_CONFIG_HOME/mlx/appearance.conf"
# RUNNER: Voyager on PAGE (a file of projects/voyager/tests), with the
# assignments after it in its environment.
write_runner() {
    local runner=$1 page=$2
    shift 2
    # (A URL with a scheme, voyager://about, as it is.)
    local target=$repo_root/projects/voyager/tests/$page
    [[ $page == *:* ]] && target=$page
    cat > "$runner" << EOF
#!/bin/sh
export HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf"
export VOYAGER_TRACE=1 $*
exec "$voyager" "$target" 2>> "$work/voyager.log"
EOF
    chmod +x "$runner"
}
write_runner "$work/voyager-run" start.html

run_scenario() {
    local name=$1 script=$2 runner=${3:-$work/voyager-run}
    : > "$work/voyager.log"
    "$work/test-host" "host-$name" "$work/us.xkb" "$script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 180 "$work/mlx-compositor" --verbose --renderer cpu --socket "nested-$name" --run "$runner" > "$work/$name-compositor.log" 2>&1
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

# The colour of pixel (X, Y) of a PPM screenshot, as hex ("1e1e1e").
pixel_of() {
    python3 -I - "$1" "$2" "$3" << 'EOF'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(maxsplit=4)
width = int(parts[1])
x, y = int(sys.argv[2]), int(sys.argv[3])
at = (y * width + x) * 3
print(parts[4][at:at + 3].hex())
EOF
}

# Whether a hex colour is light (every channel above 0xc0) or dark (every
# channel below 0x40): "light", "dark" or "neither".
lightness_of() {
    local r=$((16#${1:0:2})) g=$((16#${1:2:2})) b=$((16#${1:4:2}))
    if (( r > 0xc0 && g > 0xc0 && b > 0xc0 )); then echo light
    elif (( r < 0x40 && g < 0x40 && b < 0x40 )); then echo dark
    else echo neither; fi
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
page_x0=$x0
page_y0=$y0
echo "ok   Voyager shows start.html (the page at $x0,$y0 of the screen)"
# The toolbar above the page: its background (under the window's top,
# right of the buttons, above the address field) light.
toolbar=$(pixel_of "$work/start.ppm" $(( x0 + 300 )) $(( y0 - 38 )))
[[ $(lightness_of "$toolbar") == light ]] || fail "the toolbar is not drawn light for a light desktop (found #$toolbar)"
echo "ok   the toolbar is drawn in the desktop's light scheme"

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
[[ "${page_x:-}" == "$x0" && "${page_y:-}" == "$y0" && "${page_width:-0}" -ge 730 && "${page_height:-0}" -ge 590 ]] || fail "the screenshot does not show second.html's blue page at $x0,$y0 (found: ${page_x:-} ${page_y:-} ${page_width:-}x${page_height:-})"
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

# 11: a URL longer than 512 bytes: second.html with a long query.
long_query=$(printf 'segment%04d/' $(seq 1 64))end
long_url="file://$repo_root/projects/voyager/tests/second.html?$long_query"
cat > "$work/voyager-long" << EOF
#!/bin/sh
export HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf" VOYAGER_TRACE=1
exec "$voyager" "$long_url" 2>> "$work/voyager.log"
EOF
chmod +x "$work/voyager-long"
cat > "$work/long.script" << EOF
wait 10000
down 29
down 38
up 38
up 29
wait 1500
shot $work/long.ppm
close
EOF
run_scenario long "$work/long.script" "$work/voyager-long"
log=$work/voyager.log
grep -qxF "voyager: url $long_url" "$log" || { cat "$log" >&2; fail "the URL of ${#long_url} bytes was not loaded whole"; }
grep -qxF "voyager: address focused, ${#long_url} bytes, its end in view" "$log" || { cat "$log" >&2; fail "the address field does not hold the URL of ${#long_url} bytes with its end in view"; }
echo "ok   a URL of ${#long_url} bytes: loaded whole, the address field holds it and shows its end"

# 12: tabs.
write_runner "$work/voyager-tabs" tabs.html
x0=$page_x0
y0=$page_y0
ctrl() { printf 'down 29\n'; for key in "$@"; do printf 'down %s\n' "$key"; done; for key in "$@"; do printf 'up %s\n' "$key"; done; printf 'up 29\nwait 1500\n'; }
{
    echo "wait 10000"
    echo "pointer $(( x0 + 120 )) $(( y0 + 40 ))"
    echo "press 272"; echo "release 272"; echo "wait 5000"
    ctrl 2
    echo "pointer $(( x0 + 120 )) $(( y0 + 100 ))"
    echo "press 272"; echo "release 272"; echo "wait 5000"
    ctrl 2
    echo "pointer $(( x0 + 120 )) $(( y0 + 40 ))"
    echo "press 274"; echo "release 274"; echo "wait 5000"
    echo "shot $work/tabs.ppm"
    ctrl 15
    ctrl 42 15
    ctrl 20
    echo "type file://$repo_root/projects/voyager/tests/second.html?typed"
    echo "enter"; echo "wait 4000"
    ctrl 17
    ctrl 42 48
    echo "shot $work/folded.ppm"
    ctrl 42 48
    echo "shot $work/unfolded.ppm"
    ctrl 10
    ctrl 17; ctrl 17; ctrl 17; ctrl 17
    echo "wait 2000"
    echo "close"
} > "$work/tabs.script"
run_scenario tabs "$work/tabs.script" "$work/voyager-tabs"
log=$work/voyager.log
grep -q "^voyager: tab 2 title second$" "$log" && grep -q "^voyager: tab 2 shown$" "$log" || { cat "$log" >&2; fail "a target=_blank link did not open second.html in a new tab, shown"; }
echo "ok   a target=_blank link opens a new tab, shown"
grep -q "^voyager: tab 3 url file://.*/projects/voyager/tests/second.html?opened$" "$log" && grep -q "^voyager: tab 3 shown$" "$log" || { cat "$log" >&2; fail "window.open did not open a new tab, shown"; }
echo "ok   window.open opens a new tab, shown"
loaded=$(grep -n "^voyager: tab 4 title second$" "$log" | head -1 | cut -d: -f1)
shown=$(grep -n "^voyager: tab 4 shown$" "$log" | head -1 | cut -d: -f1)
[[ -n "$loaded" && -n "$shown" && "$loaded" -lt "$shown" ]] || { cat "$log" >&2; fail "a middle click on the link did not open it in a tab behind that loads while another is shown"; }
echo "ok   a middle click opens the link in a tab behind, which loads (background tabs keep running)"
# (A new tab is shown before its page has a title: the empty ones left out.)
titles=$(grep "^voyager: title ." "$log" | sed 's/^voyager: title //' | tr '\n' '|')
[[ "$titles" == "tabs|second|tabs|second|tabs|second|tabs|second|"* ]] || { cat "$log" >&2; fail "Ctrl+1, Ctrl+Tab, Ctrl+Shift+Tab and Ctrl+T did not show the tabs expected (titles: $titles)"; }
grep -q "^voyager: tab 5 url file://.*/projects/voyager/tests/second.html?typed$" "$log" && grep -q "^voyager: tab 5 closed$" "$log" || { cat "$log" >&2; fail "Ctrl+T and an address typed did not load in a new tab, or Ctrl+W did not close it"; }
echo "ok   Ctrl+1, Ctrl+Tab, Ctrl+Shift+Tab show tabs; Ctrl+T opens one for an address, Ctrl+W closes it"
grep -q "^voyager: sidebar width 48$" "$log" && grep -q "^voyager: sidebar width 220$" "$log" || { cat "$log" >&2; fail "Ctrl+Shift+B did not fold the sidebar and open it again"; }
read -r folded_x _ <<< "$(box_of "$work/folded.ppm" 3366cc)"
read -r unfolded_x _ <<< "$(box_of "$work/unfolded.ppm" 3366cc)"
[[ -n "${folded_x:-}" && -n "${unfolded_x:-}" && $(( unfolded_x - folded_x )) -eq 172 ]] || fail "the page did not move with the sidebar (folded at ${folded_x:-?}, open at ${unfolded_x:-?})"
echo "ok   Ctrl+Shift+B folds the sidebar to its strip (the page 172 pixels further left) and opens it again"
grep -q "^voyager: the last tab was closed$" "$log" || { cat "$log" >&2; fail "closing every tab did not close the window"; }
echo "ok   closing the last tab closes the window"

# 9: a dark desktop (MLX_COLOR_SCHEME=dark), a page that allows dark.
write_runner "$work/voyager-dark" scheme.html MLX_COLOR_SCHEME=dark
cat > "$work/dark.script" << EOF
wait 12000
shot $work/dark.ppm
close
EOF
run_scenario dark "$work/dark.script" "$work/voyager-dark"
log=$work/voyager.log
read -r link_x link_y link_width link_height <<< "$(box_of "$work/dark.ppm" 3366cc)"
[[ "${link_width:-0}x${link_height:-0}" == "200x40" ]] || { cat "$log" >&2; fail "the dark screenshot has no 200x40 box of scheme.html (found: ${link_x:-} ${link_y:-} ${link_width:-}x${link_height:-})"; }
x0=$(( link_x - 20 ))
y0=$(( link_y - 20 ))
grep -q "^voyager: title scheme:dark$" "$log" || { cat "$log" >&2; fail "the page does not see prefers-color-scheme: dark"; }
toolbar=$(pixel_of "$work/dark.ppm" $(( x0 + 300 )) $(( y0 - 38 )))
[[ $(lightness_of "$toolbar") == dark ]] || fail "the toolbar is not drawn dark for a dark desktop (found #$toolbar)"
canvas=$(pixel_of "$work/dark.ppm" $(( x0 + 500 )) $(( y0 + 300 )))
[[ $canvas == 1a1c24 ]] || fail "the page's canvas is not the dark Canvas #1a1c24 (std.ui.theme's dark background; found #$canvas)"
scheme=$(pixel_of "$work/dark.ppm" $(( x0 + 70 )) $(( y0 + 100 )))
[[ $scheme == 0080ff ]] || fail "the page's prefers-color-scheme: dark box is not #0080ff (found #$scheme)"
input=$(pixel_of "$work/dark.ppm" $(( x0 + 250 )) $(( y0 + 155 )))
[[ $(lightness_of "$input") == dark ]] || fail "the page's text field is not drawn dark (found #$input)"
echo "ok   a dark desktop: the toolbar and the page (canvas, prefers-color-scheme, text field) are dark"

# 10: the desktop switched to dark while Voyager shows the page.
write_runner "$work/voyager-live" scheme.html
echo "color-scheme = light" > "$XDG_CONFIG_HOME/mlx/appearance.conf"
cat > "$work/live.script" << EOF
wait 12000
shot $work/live-light.ppm
wait 4000
shot $work/live-dark.ppm
close
EOF
( for _ in $(seq 1 600); do [[ -f "$work/live-light.ppm" ]] && break; sleep 0.1; done
  sleep 0.5
  echo "color-scheme = dark" > "$XDG_CONFIG_HOME/mlx/appearance.conf" ) &
switcher=$!
run_scenario live "$work/live.script" "$work/voyager-live"
wait "$switcher"
log=$work/voyager.log
titles=$(grep "^voyager: title scheme:" "$log" | sed 's/^voyager: title //' | tr '\n' ' ')
[[ "$titles" == "scheme:light scheme:dark " ]] || { cat "$log" >&2; fail "the page did not see the scheme change from light to dark (titles: $titles)"; }
[[ $(grep -c "^voyager: url " "$log") -eq 1 ]] || { cat "$log" >&2; fail "the page was loaded again for the scheme change"; }
read -r link_x link_y link_width link_height <<< "$(box_of "$work/live-light.ppm" 3366cc)"
[[ "${link_width:-0}x${link_height:-0}" == "200x40" ]] || fail "the screenshot has no 200x40 box of scheme.html (found: ${link_x:-} ${link_y:-} ${link_width:-}x${link_height:-})"
x0=$(( link_x - 20 ))
y0=$(( link_y - 20 ))
for shot in light dark; do
    file=$work/live-$shot.ppm
    canvas=$(pixel_of "$file" $(( x0 + 500 )) $(( y0 + 300 )))
    toolbar=$(pixel_of "$file" $(( x0 + 300 )) $(( y0 - 38 )))
    scheme=$(pixel_of "$file" $(( x0 + 70 )) $(( y0 + 100 )))
    [[ $(lightness_of "$canvas") == "$shot" && $(lightness_of "$toolbar") == "$shot" ]] || fail "the page's canvas (#$canvas) and the toolbar (#$toolbar) are not $shot"
    expected=$([[ $shot == dark ]] && echo 0080ff || echo ff8000)
    [[ $scheme == "$expected" ]] || fail "the prefers-color-scheme box is #$scheme, not #$expected, when $shot"
done
echo "ok   a switch of the desktop's scheme: the toolbar and the page follow it, without a reload"
# 13: forced dark mode, following the desktop.
write_runner "$work/voyager-forced" second.html
echo "color-scheme = dark" > "$XDG_CONFIG_HOME/mlx/appearance.conf"
x0=$page_x0
y0=$page_y0
cat > "$work/forced.script" << EOF
wait 12000
shot $work/forced.ppm
wait 4000
shot $work/unforced.ppm
close
EOF
( for _ in $(seq 1 600); do [[ -f "$work/forced.ppm" ]] && break; sleep 0.1; done
  sleep 0.5
  echo "color-scheme = light" > "$XDG_CONFIG_HOME/mlx/appearance.conf" ) &
switcher=$!
run_scenario forced "$work/forced.script" "$work/voyager-forced"
wait "$switcher"
log=$work/voyager.log
grep -q "^voyager: tab 1 forced dark yes$" "$log" || { cat "$log" >&2; fail "second.html, which has no dark design, was not painted in forced dark on a dark desktop"; }
square=$(pixel_of "$work/forced.ppm" $(( x0 + 100 )) $(( y0 + 100 )))
blue=$(pixel_of "$work/forced.ppm" $(( x0 + 10 )) $(( y0 + 10 )))
[[ $(lightness_of "$square") == dark ]] || fail "the white square of second.html is not dark in forced dark (#$square)"
r=$((16#${blue:0:2})); b=$((16#${blue:4:2}))
(( b > r + 40 )) || fail "the blue of second.html is not kept a blue in forced dark (#$blue)"
echo "ok   forced dark: on a dark desktop a page without a dark design is painted dark (white #$square, its blue #$blue)"
grep -q "^voyager: tab 1 forced dark no$" "$log" || { cat "$log" >&2; fail "the page stayed in forced dark when the desktop turned light"; }
square=$(pixel_of "$work/unforced.ppm" $(( x0 + 100 )) $(( y0 + 100 )))
[[ $square == ffffff ]] || fail "the square is not white again on a light desktop (#$square)"
echo "ok   the desktop switched to light: the page is painted as it is again"
# 14: the internal pages.
write_runner "$work/voyager-internal" voyager://about
address_script() {
    printf 'down 29\ndown 38\nup 38\nup 29\nwait 300\ntype %s\nenter\nwait 4000\n' "$1"
}
{
    printf 'wait 10000\nshot %s\n' "$work/about.ppm"
    address_script about:version
    address_script about:about
    address_script voyager://nothing
    address_script voyager://
    printf 'close\n'
} > "$work/internal.script"
run_scenario internal "$work/internal.script" "$work/voyager-internal"
log=$work/voyager.log
grep -q "^voyager: title About Voyager$" "$log" && grep -q "^voyager: url voyager://about$" "$log" || { cat "$log" >&2; fail "voyager://about was not shown as About Voyager"; }
grep -q "^voyager: tab 1 text Version 0.1$" "$log" && grep -q "^voyager: tab 1 text WebKit 2.54.1$" "$log" || { cat "$log" >&2; fail "voyager://about does not show Voyager's version and engine"; }
background=$(pixel_of "$work/about.ppm" $(( page_x0 + 5 )) $(( page_y0 + 5 )))
[[ $background == f6f6f8 ]] || fail "voyager://about is not on the theme's light background (found #$background)"
echo "ok   voyager://about: About Voyager, its version and engine, in the theme's light colours"
grep -q "^voyager: url voyager://version$" "$log" && grep -q "^voyager: title Voyager Version$" "$log" || { cat "$log" >&2; fail "about:version did not load voyager://version"; }
user_agent=$(sed -n 's/^voyager: user agent //p' "$log" | head -1)
grep -qxF "voyager: tab 1 text User agent	$user_agent" "$log" || { cat "$log" >&2; fail "voyager://version does not show the user agent ($user_agent)"; }
grep -qx "voyager: tab 1 text Engine	WebKit 2.54.1" "$log" && grep -qx "voyager: tab 1 text Voyager	0.1" "$log" || { cat "$log" >&2; fail "voyager://version does not show Voyager's and the engine's versions"; }
echo "ok   about:version is voyager://version: Voyager Version, the versions and the whole user agent"
grep -qxF "voyager: tab 1 text Profile	$work/home/.local/share/voyager (local storage, IndexedDB, cookies)" "$log" && grep -qxF "voyager: tab 1 text Cache	$work/home/.cache/voyager (the HTTP cache)" "$log" || { cat "$log" >&2; fail "voyager://version does not show the profile's directories"; }
echo "ok   voyager://version shows the profile: ~/.local/share/voyager and ~/.cache/voyager"
[[ $(grep -c "^voyager: url voyager://about$" "$log") -ge 2 ]] || { cat "$log" >&2; fail "about:about did not load voyager://about"; }
echo "ok   about:about is voyager://about"
grep -q "^voyager: url voyager://nothing$" "$log" && grep -q "^voyager: title Not Found$" "$log" || { cat "$log" >&2; fail "an unknown voyager: page is no 404 page"; }
grep -q "^voyager: title Voyager Pages$" "$log" || { cat "$log" >&2; fail "voyager:// is not the index of the pages"; }
echo "ok   voyager://nothing is a 404 page, voyager:// the index"
write_runner "$work/voyager-internal-dark" voyager://about MLX_COLOR_SCHEME=dark
printf 'wait 10000\nshot %s\nclose\n' "$work/about-dark.ppm" > "$work/internal-dark.script"
run_scenario internal-dark "$work/internal-dark.script" "$work/voyager-internal-dark"
grep -q "^voyager: tab 1 forced dark no$" "$work/voyager.log" || { cat "$work/voyager.log" >&2; fail "voyager://about was painted in forced dark"; }
background=$(pixel_of "$work/about-dark.ppm" $(( page_x0 + 5 )) $(( page_y0 + 5 )))
[[ $background == 1a1c24 ]] || fail "voyager://about is not on the theme's dark background on a dark desktop (found #$background)"
echo "ok   a dark desktop: voyager://about in its own dark design (not forced dark)"
# 15: the profile kept between runs.
write_runner "$work/voyager-storage" storage.html
printf 'wait 8000\nclose\n' > "$work/storage.script"
run_scenario storage-first "$work/storage.script" "$work/voyager-storage"
grep -q "^voyager: title visits:1 session:none$" "$work/voyager.log" || { cat "$work/voyager.log" >&2; fail "storage.html did not count its first visit"; }
[[ -f "$work/home/.local/share/voyager/LocalStorage/file__0.localstorage" ]] || fail "no local storage database in the profile (~/.local/share/voyager/LocalStorage)"
run_scenario storage-second "$work/storage.script" "$work/voyager-storage"
grep -q "^voyager: title visits:2 session:none$" "$work/voyager.log" || { cat "$work/voyager.log" >&2; fail "localStorage did not survive Voyager's restart with the same profile"; }
echo "ok   localStorage survives a restart of Voyager with the same profile, sessionStorage does not"
# 16: downloads, into a download folder of the check's.
downloads=$work/downloads
write_runner "$work/voyager-downloads" download.html XDG_DOWNLOAD_DIR="$downloads"
{
    printf 'wait 10000\nshot %s\n' "$work/downloads-sidebar.ppm"
    address_script voyager://downloads
    printf 'wait 1000\nshot %s\nclose\n' "$work/downloads-page.ppm"
} > "$work/downloads.script"
run_scenario downloads "$work/downloads.script" "$work/voyager-downloads"
log=$work/voyager.log
grep -qxF "voyager: downloads go to $downloads" "$log" || { cat "$log" >&2; fail "Voyager's downloads do not go to XDG_DOWNLOAD_DIR ($downloads)"; }
grep -qxF "voyager: download done $downloads/voyager-blob.txt (29 bytes)" "$log" && grep -qxF "voyager: download done $downloads/voyager-note.txt (44 bytes)" "$log" || { cat "$log" >&2; fail "download.html's two downloads were not saved"; }
[[ "$(cat "$downloads/voyager-blob.txt")" == "saved from a Blob by Voyager" ]] && cmp -s "$downloads/voyager-note.txt" projects/voyager/tests/download-note.txt || fail "the downloaded files do not hold what was downloaded"
echo "ok   a Blob and a file's link (download attributes) are saved into XDG_DOWNLOAD_DIR"
grep -q "^voyager: title Voyager Downloads$" "$log" && grep -qxF "voyager: tab 1 text voyager-note.txt	done, 44 bytes" "$log" && grep -qxF "voyager: tab 1 text voyager-blob.txt	done, 29 bytes" "$log" && grep -qxF "voyager: tab 1 text $downloads/voyager-blob.txt" "$log" || { cat "$log" >&2; fail "voyager://downloads does not list the downloads"; }
echo "ok   voyager://downloads lists them: done, their sizes, where they are"
# 17: an upload through the file chooser, from a home of one file.
mkdir -p "$work/upload-home"
echo "hello from the chooser" > "$work/upload-home/upload-me.txt"
write_runner "$work/voyager-upload" upload.html HOME="$work/upload-home"
{
    printf 'wait 10000\npointer %d %d\npress 272\nrelease 272\nwait 2000\nshot %s\n' $(( x0 + 120 )) $(( y0 + 50 )) "$work/chooser.ppm"
    printf 'down 108\nup 108\nwait 500\ndown 28\nup 28\nwait 3000\nclose\n'
} > "$work/upload.script"
run_scenario upload "$work/upload.script" "$work/voyager-upload"
log=$work/voyager.log
grep -qxF "voyager: file chooser in $work/upload-home" "$log" || { cat "$log" >&2; fail "a click on the page's <input type=file> did not open the file chooser in the home folder"; }
grep -qx "voyager: chose 1 files" "$log" && grep -qxF "voyager: title uploaded:upload-me.txt:hello from the chooser" "$log" || { cat "$log" >&2; fail "the file chosen did not reach the page"; }
echo "ok   the file chooser (std.ui.chooser) gives the file chosen to the page, which reads it"
echo "check_voyager.sh: all checks passed"
