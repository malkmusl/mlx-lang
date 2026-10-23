#!/usr/bin/env bash
# The Mlx port of WebKit, M4 (projects/webkit/PORT.md): a page painted into
# a buffer. webcore-dump --png paints each page of projects/webkit/tests/paint
# (pages.txt) at 800x600 into a Skia raster surface (CPU) through
# GraphicsContextSkia, writes it with std.png, and compares it with the
# page's reference painting, PAGE-expected.png (made by make_references.py
# from what the page draws: boxes, borders and outlines, Ahem text, linear
# gradients, a PNG image decoded by the port's std.png decoder, at its
# size and scaled, the desktop's colour scheme: a page that allows dark
# painted light and, as PAGE@dark, with --dark: its canvas, a
# prefers-color-scheme rule, system colours and a text field of the
# theme in the dark variant). A page passes when no more pixels than pages.txt
# allows differ from the reference by more than its tolerance in a channel;
# the check fails when a page fails that pages.txt does not list as known
# to (with its cause).
#
#   tools/check_webkit_mlx_paint.sh [--no-build] [-j N] [--keep DIR] [PAGE...]
#     (--no-build: the program as it is; -j: the build's jobs, 2 by
#     default; --keep: the paintings, kept in DIR)
set -uo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
pages=projects/webkit/tests/paint
work=$(mktemp -d /tmp/mlx-webkit-paint.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

build=1
jobs=2
keep=""
chosen=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build) build=0 ;;
        -j) jobs=$2; shift ;;
        --keep) keep=$2; shift ;;
        -*) fail "unknown argument $1" ;;
        *) chosen+=("$1") ;;
    esac
    shift
done

targets="libc webkit-mlx-port webkit-mlx-webcore-dump"
if [[ $build -eq 1 ]]; then
    ./mlx build $targets -j "$jobs" > "$work/build.log" 2>&1 || { tail -20 "$work/build.log"; fail "./mlx build $targets"; }
    echo "ok   ./mlx build $targets"
fi
program=$repo_root/mlx-out/webkit/mlx/bin/webcore-dump
[[ -x "$program" ]] || fail "$program is missing"

# The fonts: Ahem as a test font; Liberation for the families a page
# does not use but the settings name.
serif=$(find /usr/share/fonts /usr/local/share/fonts -name LiberationSerif-Regular.ttf 2> /dev/null | head -1)
[[ -n "$serif" ]] || fail "no Liberation fonts (LiberationSerif-Regular.ttf) in /usr/share/fonts"
cat > "$work/fonts.conf" << EOF2
<?xml version="1.0"?>
<fontconfig>
    <dir>$(dirname "$serif")</dir>
    <include>$repo_root/projects/webkit/tests/layout/fonts.conf</include>
</fontconfig>
EOF2
mkdir -p "$work/home"
ahem=$repo_root/projects/webkit/tests/layout/LayoutTests/resources/Ahem.ttf
[[ -n "$keep" ]] && mkdir -p "$keep"

passed=0
failed=0
known=0
while read -r page tolerance allow cause; do
    case $page in "#"*|"") continue ;; esac
    if [[ ${#chosen[@]} -gt 0 ]] && [[ ! " ${chosen[*]} " == *" $page "* ]]; then continue; fi
    # (PAGE@dark: PAGE with the dark appearance.)
    file=${page%@dark}
    name=${file%.*}
    flags=()
    if [[ "$page" == *@dark ]]; then
        name=$name-dark
        flags=(--dark)
    fi
    out=$work/$name.png
    status=0
    env -i HOME="$work/home" FONTCONFIG_FILE="$work/fonts.conf" timeout 300 "$program" --size 800x600 --font "$ahem" "${flags[@]}" \
        --png "$out" --reference "$pages/$name-expected.png" --tolerance "$tolerance" --allow "$allow" \
        "$repo_root/$pages/$file" > /dev/null 2> "$work/err.txt" || status=$?
    [[ -n "$keep" && -f "$out" ]] && cp "$out" "$keep/"
    result=$(grep "pixels differ" "$work/err.txt" | sed 's/^webcore-dump: //')
    if [[ $status -eq 0 ]]; then
        passed=$(( passed + 1 ))
        echo "PASS $page: $result"
        [[ -z "$cause" ]] || echo "     (listed as known to fail in $pages/pages.txt: take that off)"
    else
        echo "FAIL $page (exit $status): ${result:-$(grep -v '^  ' "$work/err.txt" | tail -1)}"
        if [[ -n "$cause" ]]; then
            known=$(( known + 1 ))
            echo "     ${cause}"
        else
            failed=$(( failed + 1 ))
        fi
    fi
done < "$pages/pages.txt"

echo "check_webkit_mlx_paint.sh: $passed of $(( passed + failed + known )) pages pass, $known known to fail"
[[ $failed -eq 0 ]] || fail "$failed pages are not painted as their references"
echo "check_webkit_mlx_paint.sh: all checks passed"
