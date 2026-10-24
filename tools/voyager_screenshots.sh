#!/usr/bin/env bash
# The pictures of Voyager's README (projects/voyager/screenshots): Voyager
# (mlx-out/webkit/mlx/bin/voyager, already built: ./mlx build
# webkit-mlx-voyager) on the nested compositor (projects/desktop/compositor)
# under tools/wayland-test-host, which plays keys and the pointer from a
# script and saves the frames it is shown (`shot`), as
# tools/check_voyager.sh runs it. The pages are projects/voyager/demo's,
# served by tools/http-test-server (whose /download/slow fills the
# downloads ring and popover), and Voyager's own (voyager://about,
# voyager://version). The desktop's scheme is appearance.conf's (rewritten
# while Voyager runs for the tour's switch to dark) or MLX_COLOR_SCHEME.
# Python with Pillow turns the frames into PNGs and the tour into an
# animated GIF (one palette for all its frames, as
# tools/observatory_screenshots.sh makes Observatory's).
#
# With VOYAGER_SCREENSHOTS_WEB=1 also pictures of real sites, Wikipedia's
# and Hacker News's: wikipedia.png and hn-forced-dark.png. Voyager's
# environment is this script's (MLXWEB_CA_FILE, a CA bundle to trust,
# reaches it); the port's HTTP does not go through HTTPS_PROXY, so the
# sites must be reachable from here.
#
#   tools/voyager_screenshots.sh [output directory]
#     (default projects/voyager/screenshots; VOYAGER_SHOTS_ONLY=NAME runs
#     only that scenario: tour, light, dark, chooser, downloads, web;
#     KEEP_WORK=1 keeps the work directory)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
out=${1:-projects/voyager/screenshots}
compiler=${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}
voyager=$repo_root/mlx-out/webkit/mlx/bin/voyager
say() { echo "== $*" >&2; }
fail() { echo "voyager_screenshots.sh: $*" >&2; exit 1; }
python3 -c "import PIL" 2> /dev/null || fail "needs Python with Pillow (pip install pillow)"
command -v xkbcli > /dev/null || fail "xkbcli is not installed"
[[ -x "$voyager" ]] || fail "$voyager is missing (./mlx build webkit-mlx-voyager)"

# (Short: the Wayland sockets' paths are at most 107 bytes.)
work=$(mktemp -d -t voyager.XXXXXX)
server_pid=""
converter_pid=""
cleanup() {
    [[ -n "$server_pid" ]] && kill -- -"$server_pid" 2> /dev/null
    [[ -n "$converter_pid" ]] && kill "$converter_pid" 2> /dev/null
    [[ -n "${KEEP_WORK:-}" ]] || rm -rf -- "$work"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR="$work/runtime"
export XDG_CONFIG_HOME="$work/config"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$XDG_CONFIG_HOME/mlx" "$work/frames" "$work/pictures" "$out"
appearance=$XDG_CONFIG_HOME/mlx/appearance.conf

say "building"
"$compiler" --quiet projects/desktop/compositor/main.mlx -o "$work/mlx-compositor"
"$compiler" --quiet tools/wayland-test-host/main.mlx -o "$work/test-host"
"$compiler" --quiet tools/http-test-server/main.mlx -o "$work/http-test-server"
xkbcli compile-keymap --layout us > "$work/us.xkb"

setsid "$work/http-test-server" "$repo_root/projects/voyager/demo" > "$work/server.port" 2> "$work/server.log" &
server_pid=$!
for _ in $(seq 1 50); do [[ -s "$work/server.port" ]] && break; sleep 0.1; done
port=$(head -1 "$work/server.port")
[[ -n "$port" ]] || fail "the test server did not start: $(cat "$work/server.log")"
site=127.0.0.1:$port

# The compositor's output is 1024x768, Voyager's window (960x640) at
# (24, 24) of it; the page under the toolbar (40) right of the open
# sidebar (220).
wx=24; wy=24
x0=$(( wx + 220 )); y0=$(( wy + 40 ))

# A home for each scenario (Voyager's profile, its downloads), the font
# cache shared.
new_home() {
    local home=$work/home-$1
    mkdir -p "$home/.cache"
    [[ -d "$work/font-cache" ]] || mkdir -p "$work/font-cache"
    ln -sfn "$work/font-cache" "$home/.cache/mlx-fontconfig"
    echo "$home"
}

# RUNNER HOME URL [ASSIGNMENT]...: Voyager on URL with the assignments in
# its environment.
write_runner() {
    local runner=$1 home=$2 url=$3
    shift 3
    cat > "$runner" << EOF
#!/bin/sh
export HOME="$home" XDG_STATE_HOME="$home/.local/state" XDG_DOWNLOAD_DIR="$home/Downloads" VOYAGER_TRACE=1 $*
exec "$voyager" "$url" 2>> "$work/voyager-\$\$.log"
EOF
    chmod +x "$runner"
}

# NAME SCRIPT RUNNER: the test host plays SCRIPT against the compositor,
# which runs Voyager.
run_scenario() {
    local name=$1 script=$2 runner=$3
    "$work/test-host" "host-$name" "$work/us.xkb" "$script" > "$work/$name-host.log" 2>&1 &
    local host_pid=$!
    for _ in $(seq 1 50); do [[ -S "$XDG_RUNTIME_DIR/host-$name" ]] && break; sleep 0.1; done
    WAYLAND_DISPLAY="host-$name" timeout 300 "$work/mlx-compositor" --verbose --renderer cpu --xwayland none --socket "nested-$name" --run "$runner" > "$work/$name-compositor.log" 2>&1 || true
    wait "$host_pid" || { cat "$work/$name-host.log" >&2; fail "the test host failed ($name)"; }
    grep -q "^window: 960x640 at $wx,$wy$" "$work/$name-compositor.log" || { cat "$work/$name-compositor.log" >&2; fail "Voyager's window is not 960x640 at $wx,$wy ($name)"; }
}

# Script lines: keys with Ctrl (and more), an address typed into the
# address field (Ctrl+L), a click.
ctrl() { printf 'down 29\n'; for key in "$@"; do printf 'down %s\n' "$key"; done; for key in "$@"; do printf 'up %s\n' "$key"; done; printf 'up 29\n'; }
address() { ctrl 38; printf 'wait 300\ntype %s\nenter\n' "$1"; }
new_tab() { ctrl 20; printf 'wait 300\ntype %s\nenter\n' "$1"; }
click() { printf 'pointer %d %d\nwait 150\npress 272\nrelease 272\n' "$1" "$2"; }
# (Each picture is checked for at the end.)
expected=()
picture() { printf 'shot %s\n' "$work/pictures/$1.ppm"; }
expect() { expected+=("$@"); }

only() { [[ -z "${VOYAGER_SHOTS_ONLY:-}" || "$VOYAGER_SHOTS_ONLY" == "$1" ]]; }
long_url="http://$site/journal.html?probe=voyager-1&from=1977-09-05&to=2012-08-25&instruments=imaging,magnetometer,plasma-waves,cosmic-rays,low-energy-particles&targets=jupiter,io,saturn,titan&view=timeline&highlight=interstellar-space&units=astronomical&lang=en"

# The light desktop: voyager://about, voyager://version, tabs, the folded
# sidebar, a long URL.
if only light; then
    say "light"
    echo "color-scheme = light" > "$appearance"
    home=$(new_home light)
    write_runner "$work/run-light" "$home" voyager://about
    {
        printf 'wait 10000\n'; picture about-light
        address voyager://version; printf 'wait 3000\n'; picture version
        address "$site/welcome.html"; printf 'wait 3000\n'
        new_tab "$site/journal.html"; printf 'wait 2500\n'
        new_tab "$site/orbits.html"; printf 'wait 2500\n'
        new_tab "$site/form.html"; printf 'wait 2500\n'
        ctrl 2; printf 'wait 1500\n'; picture tabs-light
        ctrl 4; printf 'wait 1000\n'
        ctrl 42 48; printf 'wait 1200\n'; picture sidebar-folded
        ctrl 42 48; printf 'wait 800\n'
        new_tab "$long_url"; printf 'wait 2500\n'
        ctrl 38; printf 'wait 800\n'; picture long-url
        printf 'close\n'
    } > "$work/light.script"
    run_scenario light "$work/light.script" "$work/run-light"
    expect about-light version tabs-light sidebar-folded long-url
fi

# The dark desktop (MLX_COLOR_SCHEME=dark): voyager://about in its own dark
# design, a page with one (welcome.html) and the form in the port's theme,
# a page without one (journal.html) in forced dark.
if only dark; then
    say "dark"
    echo "color-scheme = dark" > "$appearance"
    home=$(new_home dark)
    write_runner "$work/run-dark" "$home" voyager://about MLX_COLOR_SCHEME=dark
    {
        printf 'wait 10000\n'; picture about-dark
        address "$site/welcome.html"; printf 'wait 3000\n'
        new_tab "$site/journal.html"; printf 'wait 2500\n'
        new_tab "$site/orbits.html"; printf 'wait 2500\n'
        new_tab "$site/form.html"; printf 'wait 3000\n'; picture form-dark
        ctrl 2; printf 'wait 1500\n'; picture tabs-dark
        ctrl 3; printf 'wait 1500\n'; picture forced-dark
        printf 'close\n'
    } > "$work/dark.script"
    run_scenario dark "$work/dark.script" "$work/run-dark"
    expect about-dark form-dark tabs-dark forced-dark
fi

# The file chooser over postcard.html, from a home of folders and files.
if only chooser; then
    say "chooser"
    echo "color-scheme = light" > "$appearance"
    home=$(new_home chooser)
    mkdir -p "$home/Documents" "$home/Downloads" "$home/Music" "$home/Pictures" "$home/Projects"
    echo "Greetings from the third planet." > "$home/postcard.txt"
    echo "Jupiter, Saturn, Uranus, Neptune." > "$home/flybys.txt"
    cp projects/voyager/demo/saturn.svg "$home/saturn.svg"
    write_runner "$work/run-chooser" "$home" "http://$site/postcard.html"
    {
        printf 'wait 10000\n'
        click $(( x0 + 192 )) $(( y0 + 144 )); printf 'wait 2000\n'
        for _ in 1 2 3 4 5 6 7; do printf 'down 108\nup 108\nwait 120\n'; done
        printf 'pointer %d %d\nwait 600\n' $(( x0 + 560 )) $(( y0 + 400 )); picture chooser
        printf 'down 1\nup 1\nwait 500\nclose\n'
    } > "$work/chooser.script"
    run_scenario chooser "$work/chooser.script" "$work/run-chooser"
    expect chooser
fi

# Downloads: a slow one (tools/http-test-server's /download/slow) and a
# small one; the popover while the slow one comes in.
if only downloads; then
    say "downloads"
    echo "color-scheme = light" > "$appearance"
    home=$(new_home downloads)
    write_runner "$work/run-downloads" "$home" "http://$site/archive.html"
    {
        printf 'wait 10000\n'
        click $(( x0 + 120 )) $(( y0 + 216 + 23 )); printf 'wait 1200\n'
        click $(( x0 + 120 )) $(( y0 + 150 + 23 )); printf 'wait 3500\n'
        click $(( x0 + 720 )) $(( y0 - 21 )); printf 'wait 1200\n'; picture downloads-popover
        printf 'wait 6000\nclose\n'
    } > "$work/downloads.script"
    run_scenario downloads "$work/downloads.script" "$work/run-downloads"
    expect downloads-popover
fi

# Real sites, with VOYAGER_SCREENSHOTS_WEB=1 (Voyager's environment is
# this script's: HTTPS_PROXY, MLXWEB_CA_FILE): Wikipedia on a light
# desktop, Hacker News, which has no dark design, forced dark on a dark one.
if [[ -n "${VOYAGER_SCREENSHOTS_WEB:-}" ]] && only web; then
    say "the web"
    echo "color-scheme = light" > "$appearance"
    home=$(new_home web)
    write_runner "$work/run-web" "$home" https://example.com
    {
        printf 'wait 12000\n'
        new_tab https://news.ycombinator.com; printf 'wait 12000\n'
        new_tab https://en.wikipedia.org/wiki/WebKit; printf 'wait 45000\n'; picture wikipedia
        printf 'close\n'
    } > "$work/web.script"
    run_scenario web "$work/web.script" "$work/run-web"
    expect wikipedia
    echo "color-scheme = dark" > "$appearance"
    home=$(new_home web-dark)
    write_runner "$work/run-web-dark" "$home" https://news.ycombinator.com MLX_COLOR_SCHEME=dark
    printf 'wait 15000\n%sclose\n' "$(picture hn-forced-dark)
" > "$work/web-dark.script"
    run_scenario web-dark "$work/web-dark.script" "$work/run-web-dark"
    expect hn-forced-dark
fi

# The tour: frames every 100 ms (the test host's `shot`s), each made
# smaller as it comes (the converter: 800 pixels wide), then a GIF.
# FRAMES N [MS]: N frames, MS apart (shown as long, or as DURATION).
frame_count=0
frames() {
    local count=$1 interval=${2:-100} duration=${3:-${2:-100}} i
    for (( i = 0; i < count; i++ )); do
        frame_count=$(( frame_count + 1 ))
        printf 'shot %s/tour-%04d.ppm\nwait %d\n' "$work/frames" "$frame_count" "$interval"
        printf 'tour-%04d %d\n' "$frame_count" "$duration" >> "$work/tour.list"
    done
}
# Keys typed a few at a time, a frame after each few.
type_slowly() {
    local text=$1 step=${2:-2} i
    for (( i = 0; i < ${#text}; i += step )); do
        printf 'type %s\n' "${text:i:step}"
        frames 1
    done
}
# The pointer moved to (X, Y) over N frames.
pointer_x=$(( wx + 480 )); pointer_y=$(( wy + 400 ))
glide() {
    local x=$1 y=$2 count=${3:-4} i
    for (( i = 1; i <= count; i++ )); do
        printf 'pointer %d %d\n' $(( pointer_x + (x - pointer_x) * i / count )) $(( pointer_y + (y - pointer_y) * i / count ))
        frames 1
    done
    pointer_x=$x; pointer_y=$y
}
press() { printf 'press 272\nrelease 272\n'; }

if only tour; then
    say "the tour"
    echo "color-scheme = light" > "$appearance"
    home=$(new_home tour)
    write_runner "$work/run-tour" "$home" voyager://about
    : > "$work/tour.list"
    python3 -I - "$work/frames" "$work/tour" "$wx" "$wy" << 'PY' &
import sys, os, time, glob
from PIL import Image
frames, tour, wx, wy = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
os.makedirs(tour, exist_ok=True)
# Voyager's window (960x640 at (wx, wy)); its bottom corners, which the
# compositor rounds over the window's own pixels, as the 14 rows above them.
def window(image):
    image = image.crop((wx, wy, wx + 960, wy + 640))
    for left in (0, 960 - 14):
        image.paste(image.crop((left, 640 - 28, left + 14, 640 - 14)), (left, 640 - 14))
    return image
def size_of(path):
    # A whole PPM: its header and width * height * 3 bytes.
    with open(path, 'rb') as f:
        head = f.read(32)
    parts = head.split(maxsplit=4)
    if len(parts) < 4:
        return None
    return len(b'%s\n%s %s\n%s\n' % tuple(parts[:4])) + int(parts[1]) * int(parts[2]) * 3
while True:
    done = os.path.exists(os.path.join(frames, 'done'))
    for path in sorted(glob.glob(os.path.join(frames, 'tour-*.ppm'))):
        if os.path.getsize(path) != size_of(path):
            continue
        image = window(Image.open(path).convert('RGB'))
        image.resize((800, 533), Image.LANCZOS).save(os.path.join(tour, os.path.basename(path)[:-4] + '.png'), compress_level=1)
        os.remove(path)
    if done and not glob.glob(os.path.join(frames, 'tour-*.ppm')):
        break
    time.sleep(0.05)
PY
    converter_pid=$!
    # The switch to dark: appearance.conf rewritten once the script gets
    # there (its marker shot).
    ( for _ in $(seq 1 3000); do [[ -f "$work/frames/dark-now.ppm" ]] && break; sleep 0.05; done
      echo "color-scheme = dark" > "$appearance" ) &
    switcher=$!
    {
        printf 'wait 9500\n'
        frames 8
        # An address typed into the field: a page of text, an image, a form.
        glide $(( wx + 330 )) $(( wy + 19 )) 5; press; frames 3
        type_slowly "$site/welcome.html" 2; printf 'enter\n'; frames 22
        # A new tab: the long page, scrolled by the wheel.
        ctrl 20; frames 3
        type_slowly "$site/journal.html" 5; printf 'enter\n'; frames 9
        glide $(( x0 + 400 )) $(( y0 + 320 )) 4
        for _ in $(seq 1 12); do printf 'scroll 1280\n'; frames 1; done
        frames 4
        # Another: an animation (the compositor thread's).
        ctrl 20; frames 2
        type_slowly "$site/orbits.html" 5; printf 'enter\n'; frames 20
        # The sidebar folded and opened (Ctrl+Shift+B).
        ctrl 42 48; frames 7
        ctrl 42 48; frames 6
        # Back to the first page; the desktop switched to dark: the
        # toolbar fades, the page follows in a dark design of its own.
        # (Not recorded while a tab is shown or a page loaded: Voyager
        # shows the page's canvas until its first frame is composed.)
        ctrl 2; printf 'wait 500\n'; frames 8
        printf 'shot %s\n' "$work/frames/dark-now.ppm"
        frames 16
        # Its link to the journal, which has no dark design: forced dark.
        glide $(( x0 + 120 )) $(( y0 + 518 )) 5; press; printf 'wait 1500\n'; frames 10
        # A download: the toolbar's button and its ring, the popover until
        # Done (frames 250 ms apart, shown at twice the speed).
        glide $(( wx + 330 )) $(( wy + 19 )) 5; press; frames 2
        type_slowly "$site/archive.html" 5; printf 'enter\nwait 1200\n'; frames 6
        glide $(( x0 + 120 )) $(( y0 + 173 )) 5; press; frames 10
        glide $(( x0 + 720 )) $(( y0 - 21 )) 5; press
        frames 32 250 125
        frames 6
        # voyager://version.
        printf 'down 1\nup 1\n'; frames 2
        ctrl 38; frames 2
        type_slowly voyager://version 4; printf 'enter\n'; frames 20
        printf 'close\n'
    } > "$work/tour.script"
    run_scenario tour "$work/tour.script" "$work/run-tour"
    wait "$switcher" 2> /dev/null || true
    touch "$work/frames/done"
    wait "$converter_pid"
    converter_pid=""
    say "the tour's GIF ($frame_count frames)"
    python3 -I - "$work/tour" "$work/tour.list" "$out/tour.gif" << 'PY'
import sys, os
from PIL import Image
tour, listing, gif_path = sys.argv[1], sys.argv[2], sys.argv[3]
names, durations = [], []
for line in open(listing):
    name, duration = line.split()
    if os.path.exists(os.path.join(tour, name + '.png')):
        names.append(name)
        durations.append(int(duration))
frames = [Image.open(os.path.join(tour, name + '.png')).convert('RGB') for name in names]
# One palette for all the frames, from a sample of light and dark ones.
sample = frames[::max(1, len(frames) // 12)]
sheet = Image.new('RGB', (frames[0].width, frames[0].height * len(sample)))
for index, frame in enumerate(sample):
    sheet.paste(frame, (0, index * frame.height))
palette = sheet.quantize(colors=255, method=Image.MEDIANCUT)
gif = [frame.quantize(palette=palette, dither=Image.FLOYDSTEINBERG) for frame in frames]
gif[0].save(gif_path, save_all=True, append_images=gif[1:], duration=durations, loop=0, optimize=True)
print('%d frames, %.1f s, %d bytes' % (len(gif), sum(durations) / 1000, os.path.getsize(gif_path)))
PY
fi

for name in "${expected[@]}"; do
    [[ -s "$work/pictures/$name.ppm" ]] || fail "no picture $name (see the logs: KEEP_WORK=1 keeps $work)"
done
say "pictures"
python3 - "$work/pictures" "$out" "$wx" "$wy" << 'PY'
import sys, os, glob
from PIL import Image
pictures, out, wx, wy = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
# Voyager's window (960x640 at (wx, wy)); its bottom corners, which the
# compositor rounds over the window's own pixels, as the 14 rows above them.
def window(image):
    image = image.crop((wx, wy, wx + 960, wy + 640))
    for left in (0, 960 - 14):
        image.paste(image.crop((left, 640 - 28, left + 14, 640 - 14)), (left, 640 - 14))
    return image
for path in sorted(glob.glob(os.path.join(pictures, '*.ppm'))):
    name = os.path.basename(path)[:-4]
    image = window(Image.open(path).convert('RGB'))
    image.save(os.path.join(out, name + '.png'), optimize=True)
for name in sorted(os.listdir(out)):
    print('%-28s %8d bytes' % (name, os.path.getsize(os.path.join(out, name))))
PY
