#!/usr/bin/env bash
# WebCore's code generators in Mlx (projects/webkit/generators), for the
# WPE port:
#
#   - `./mlx build webkit-webcore-derived` makes what WebCore's build
#     generates into mlx-out/webkit/generated/WebCore;
#   - with WEBCORE_REFERENCE naming the WebCore/DerivedSources directory of
#     a WPE build by WebKit's own generators (CMake, Perl, Python, Ruby,
#     gperf, gdbus-codegen) of the same tree and configuration, the two
#     directories are compared: every file of the reference must be made,
#     the same byte for byte, and nothing else.
#
#   WEBCORE_REFERENCE=.../WebCore/DerivedSources tools/check_webcore_generators.sh
#
# Two files name where they are, so they differ by that path alone, and
# are compared with the reference's DerivedSources path (as it is in them)
# made the one of mlx-out/webkit/generated/WebCore: ColorData.cpp, whose
# header repeats gperf's command line (with its output file), and the
# bindings' supplemental_dependency.tmp, which lists the IDL files by path
# (the generated CSSStyleProperties+PropertyNames.idl among them).
set -euo pipefail
cd "$(dirname "$0")/.."

fail() { echo "check_webcore_generators: $*" >&2; exit 1; }

./mlx build webkit-webcore-derived >/dev/null || fail "webkit-webcore-derived does not build"
made=mlx-out/webkit/generated/WebCore
count=$(find "$made" -type f | wc -l)

if [ -n "${WEBCORE_REFERENCE:-}" ]; then
    reference=${WEBCORE_REFERENCE%/}
    referencePath=$(cd "$reference" && pwd)
    compared=0
    paths=0
    for name in $(cd "$reference" && find . -type f | sed 's|^\./||' | sort); do
        [ -f "$made/$name" ] || fail "$name: not made"
        case "$name" in
            ColorData.cpp|supplemental_dependency.tmp)
                sed "s|$referencePath|$made|g" "$reference/$name" | cmp -s - "$made/$name" \
                    || fail "$name differs from $reference/$name (but for its paths)"
                paths=$((paths + 1)) ;;
            *)
                cmp -s "$made/$name" "$reference/$name" || fail "$name differs from $reference/$name" ;;
        esac
        compared=$((compared + 1))
    done
    for name in $(cd "$made" && find . -type f | sed 's|^\./||' | sort); do
        [ -f "$reference/$name" ] || fail "$name: made, but not in $reference"
    done
    echo "check_webcore_generators: $compared files the same as WebKit's generators make them ($paths of them but for the paths they name)"
else
    echo "check_webcore_generators: $count files made (WEBCORE_REFERENCE not set: not compared)"
fi
