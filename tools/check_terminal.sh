#!/usr/bin/env bash
# The terminal (std.terminal, std.ui.terminal, projects/desktop/terminal)
# without a compositor:
#   - tests/301_terminal_core_runtime.mlx: the screen's emulation (printing,
#     UTF-8 and wide characters, cursor movement, erasing, inserting,
#     scrolling regions, SGR, the alternate screen, the history and its
#     view, a resize, the title, the reports);
#   - tests/support/terminal_render.mlx: a screen drawn with the TrueType
#     font on a CPU canvas, read back pixel by pixel (skipped without a
#     monospaced font on this machine);
#   - tests/support/terminal_pty.mlx: a shell on a pty, its output through
#     the screen, the size it was told;
#   - mlx-terminal builds (its window needs a compositor:
#     tools/check_desktop_clients.sh runs it there).
#
#   tools/check_terminal.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet tests/301_terminal_core_runtime.mlx -o "$work/core"
set +e
"$work/core"
status=$?
set -e
[[ $status -eq 13 ]] || fail "the screen's emulation: check $((status - 18)) failed (exit $status)"
echo "ok   std.terminal: printing, UTF-8, movement, erasing, scrolling regions, SGR, the alternate screen, the history, resize, title, reports"

"$compiler" --quiet tests/support/terminal_render.mlx -o "$work/render"
set +e
"$work/render"
status=$?
set -e
if [[ $status -eq 2 ]]; then
    echo "skip std.ui.terminal: no monospaced TrueType font here"
else
    [[ $status -eq 0 ]] || fail "std.ui.terminal: check $status failed (the red cell, the cursor, ink in a glyph, inverse video)"
    echo "ok   std.ui.terminal: cells, colours, inverse video and the cursor drawn with the TrueType font"
fi

"$compiler" --quiet tests/support/terminal_pty.mlx -o "$work/pty"
set +e
"$work/pty"
status=$?
set -e
[[ $status -eq 0 ]] || fail "std.terminal.pty: check $status failed (a shell's output through the screen, its size)"
echo "ok   std.terminal.pty: a shell on a pty, its output and its size"

"$compiler" --quiet projects/desktop/terminal/main.mlx -o "$work/mlx-terminal"
echo "ok   mlx-terminal builds"
echo "PASS terminal"
