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
#   tools/check_webkit_mlx_selectors.sh [--no-build] [-j N] [--keep DIR] [TEST...]
exec "$(dirname "$0")/check_webkit_mlx_storage.sh" --list selectors.txt "$@"
