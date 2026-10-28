#!/usr/bin/env bash
# The Mlx port of WebKit, M8's sixth part (projects/webkit/PORT.md):
# WebCore's CSS selector JIT (ENABLE_CSS_SELECTOR_JIT). webcore-dump runs
# each of WebKit's layout tests of fast/selectors that
# tests/layout/selectors.txt lists as the storage check runs its own
# (tools/check_webkit_mlx_storage.sh, which this calls with --list
# selectors.txt), and compares what it prints with the expected text: the
# selectors those tests match, querySelector's, matches' and closest's
# answers and the styles they give, are the selector JIT's code.
#
# A test has 180 s, not 60: case-insensitive-attribute-register-requirement-1.html
# checks 973 answers, each after its line was added to the document, so
# each of its getComputedStyle calls resolves the style of a longer
# document (seven tenths of its time in Style::TreeResolver, three
# tenths in libpas' allocation, PORT.md); it passes in about 56 s alone.
#
#   tools/check_webkit_mlx_selectors.sh [--no-build] [-j N] [--keep DIR] [TEST...]
exec "$(dirname "$0")/check_webkit_mlx_storage.sh" --list selectors.txt --timeout 180 "$@"
