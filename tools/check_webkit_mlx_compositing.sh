#!/usr/bin/env bash
# The Mlx port of WebKit, M8 (projects/webkit/PORT.md, "Compositing"):
# composited layers on the CPU. webcore-dump runs each of WebKit's layout
# tests of compositing/ that tests/layout/compositing.txt lists (tests
# that dump their layer tree with internals.layerTreeAsText, which
# webcore-dump --test gives without WebCore's testing library) as the
# storage check runs its own (tools/check_webkit_mlx_storage.sh, which
# this calls with --list compositing.txt and WebKitTestRunner's fonts),
# and compares what it prints with the expected text (platform/wpe's,
# else platform/glib's, else the generic one).
#
#   tools/check_webkit_mlx_compositing.sh [--no-build] [-j N] [--keep DIR] [TEST...]
exec "$(dirname "$0")/check_webkit_mlx_storage.sh" --list compositing.txt --fonts "$@"
