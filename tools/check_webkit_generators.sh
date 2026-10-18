#!/usr/bin/env bash
# WebKit's code generators in Mlx (projects/webkit/generators), for a port
# (jsconly, the default, or wpe):
#
#   - `./mlx build webkit-jsc-derived` (webkit-wpe-jsc-derived) makes what
#     JavaScriptCore's build generates into
#     mlx-out/webkit/generated/JavaScriptCore
#     (mlx-out/webkit/wpe/generated/JavaScriptCore), the interpreter's
#     headers through the extractors mlx-cc compiles;
#   - with JSC_REFERENCE naming the DerivedSources directory of a build by
#     WebKit's own generators (CMake, Perl, Python, Ruby) of the same
#     tree and port, every file made is compared with that one byte for
#     byte.
#
#   JSC_REFERENCE=.../JavaScriptCore/DerivedSources tools/check_webkit_generators.sh [jsconly|wpe]
set -euo pipefail
cd "$(dirname "$0")/.."

fail() { echo "check_webkit_generators: $*" >&2; exit 1; }

port=${1:-jsconly}
case "$port" in
    jsconly) step=webkit-jsc-derived; made=mlx-out/webkit/generated/JavaScriptCore; objects=mlx-out/webkit/obj/jsc-tools ;;
    wpe) step=webkit-wpe-jsc-derived; made=mlx-out/webkit/wpe/generated/JavaScriptCore; objects=mlx-out/webkit/wpe/obj/jsc-tools ;;
    *) fail "unknown port $port (jsconly or wpe)" ;;
esac

./mlx build "$step" >/dev/null || fail "$step does not build"
count=$(ls "$made"/*.lut.h | wc -l)
[ "$count" -eq 102 ] || fail "$count hash tables made, not 102"

if [ -n "${JSC_REFERENCE:-}" ]; then
    compared=0
    # Every file made (the bundles in unified-sources/ too), but the
    # forwarding headers, which WebKit's build makes as links.
    for file in $(cd "$made" && find . -type f -not -path "./Headers/*" -not -path "./PrivateHeaders/*" | sed 's|^\./||' | sort); do
        name=$file
        [ -f "$JSC_REFERENCE/$name" ] || fail "$name: not in $JSC_REFERENCE"
        if ! cmp -s "$made/$file" "$JSC_REFERENCE/$name"; then
            # The interpreter's offsets are those of the compiler that
            # compiled the extractor: a reference build by another
            # compiler lays some structures out otherwise (GCC's
            # [[no_unique_address]]). Then the reference is what WebKit's
            # offlineasm (Ruby) makes of mlx-cc's extractor.
            [ "$name" = LLIntAssembly.h ] && command -v ruby >/dev/null || fail "$name differs from $JSC_REFERENCE/$name"
            source=${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}/Source/JavaScriptCore
            work=$(mktemp -d)
            env CMAKE_CXX_COMPILER_ID=GNU GCC_OFFLINEASM_SOURCE_MAP=OFF ruby "$source/offlineasm/asm.rb" -I"$JSC_REFERENCE/" "$source/llint/LowLevelInterpreter.asm" \
                "$objects/LLIntOffsetsExtractor.cpp.o" "$work/LLIntAssembly.h" normal --binary-format=ELF >/dev/null
            cmp -s "$made/$file" "$work/LLIntAssembly.h" || { rm -rf "$work"; fail "$name differs from what WebKit's offlineasm makes of $objects/LLIntOffsetsExtractor.cpp.o"; }
            rm -rf "$work"
            echo "check_webkit_generators: $name the same as WebKit's offlineasm makes it of mlx-cc's extractor (the reference's compiler lays structures out otherwise)"
        fi
        compared=$((compared + 1))
    done
    # And nothing the reference has is missing.
    for file in $(cd "$JSC_REFERENCE" && find . -type f | sed 's|^\./||' | sort); do
        [ -f "$made/$file" ] || fail "$file: made by WebKit's build, not here"
    done
    # WPE: JavaScriptCore's GLib API headers, beside DerivedSources in
    # the reference build (JavaScriptCoreGLib/DerivedSources).
    if [ "$port" = wpe ]; then
        glib_made=mlx-out/webkit/wpe/generated/JavaScriptCoreGLib/DerivedSources
        glib_reference=${JSC_GLIB_REFERENCE:-$JSC_REFERENCE/../../JavaScriptCoreGLib/DerivedSources}
        for file in $(cd "$glib_reference" && find . -type f | sed 's|^\./||' | sort); do
            [ -f "$glib_made/$file" ] || fail "JavaScriptCoreGLib: $file made by WebKit's build, not here"
            cmp -s "$glib_made/$file" "$glib_reference/$file" || fail "JavaScriptCoreGLib: $file differs from $glib_reference/$file"
            compared=$((compared + 1))
        done
        [ "$(readlink mlx-out/webkit/wpe/generated/JavaScriptCoreGLib/Headers/jsc)" = "${WEBKIT_SOURCE:-/home/user/csrc/wpe/wpewebkit-2.54.1}/Source/JavaScriptCore/API/glib" ] || fail "JavaScriptCoreGLib/Headers/jsc is not a link to API/glib"
    fi
    echo "check_webkit_generators: $port: $compared files the same as WebKit's generators make them"
else
    echo "check_webkit_generators: $port: $count hash tables made (JSC_REFERENCE not set: not compared)"
fi
