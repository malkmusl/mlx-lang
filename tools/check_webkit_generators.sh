#!/usr/bin/env bash
# WebKit's code generators in Mlx (projects/webkit/generators):
#
#   - `./mlx build webkit-jsc-derived` makes what JavaScriptCore's build
#     generates into mlx-out/webkit/generated/JavaScriptCore (the
#     interpreter's headers through the extractors mlx-cc compiles);
#   - with JSC_REFERENCE naming the DerivedSources directory of a build by
#     WebKit's own generators (CMake, Perl, Python, Ruby) of the same
#     tree, every file made is compared with that one byte for byte.
#
#   JSC_REFERENCE=.../JavaScriptCore/DerivedSources tools/check_webkit_generators.sh
set -euo pipefail
cd "$(dirname "$0")/.."

fail() { echo "check_webkit_generators: $*" >&2; exit 1; }

./mlx build webkit-jsc-derived >/dev/null || fail "webkit-jsc-derived does not build"
made=mlx-out/webkit/generated/JavaScriptCore
count=$(ls "$made"/*.lut.h | wc -l)
[ "$count" -eq 102 ] || fail "$count hash tables made, not 102"

if [ -n "${JSC_REFERENCE:-}" ]; then
    compared=0
    # Every file made (the bundles in unified-sources/ too), but the
    # forwarding headers, which WebKit's build makes as links.
    for file in $(cd "$made" && find . -type f -not -path "./Headers/*" -not -path "./PrivateHeaders/*" | sed 's|^\./||' | sort); do
        name=$file
        [ -f "$JSC_REFERENCE/$name" ] || fail "$name: not in $JSC_REFERENCE"
        cmp -s "$made/$file" "$JSC_REFERENCE/$name" || fail "$name differs from $JSC_REFERENCE/$name"
        compared=$((compared + 1))
    done
    echo "check_webkit_generators: $compared files the same as WebKit's generators make them"
else
    echo "check_webkit_generators: $count hash tables made (JSC_REFERENCE not set: not compared)"
fi
