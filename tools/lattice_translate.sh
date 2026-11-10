#!/usr/bin/env bash
# tools/lattice_translate.sh [--jobs N] [--out DIR] [--only REGEX] [--compile] [LIST...]
#
# ANGLE translated to Mlx as one program (projects/webkit/LATTICE.md): every
# unit the build compiles of it, each a module, with every record, variable
# and function the headers define written once, in the unit that is its
# home. Three steps, as compiler/cc/emitmlx.mlx describes them ("One
# program of many units"):
#
#   1. each unit lists what it defines (mlx-cc --emit-mlx-index),
#   2. the lists make the manifest: a thing's home, its name, whether a
#      function is held back anywhere in the program
#      (mlx-cc --lattice-manifest),
#   3. each unit is translated with the manifest (--emit-mlx-manifest).
#
# DIR/units has the modules and `angle.mlx`, which imports them all;
# --compile gives that to mlx4. LIST names lists of projects/webkit/lists
# (angle, angle-vulkan and angle-glesv2 without one); --only keeps the
# units whose path matches.
set -u
cd "$(dirname "$0")/.."

jobs=4
out=
only=.
compile=0
lists=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --jobs) jobs=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --only) only=$2; shift 2 ;;
        --compile) compile=1; shift ;;
        -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
        *) lists+=("$1"); shift ;;
    esac
done
[[ ${#lists[@]} -gt 0 ]] || lists=(angle angle-vulkan angle-glesv2)

compiler=mlx-out/bin/compiler/mlx4
angle=projects/webkit/upstream/Source/ThirdParty/ANGLE
[[ -x $compiler ]] || { echo "lattice_translate.sh: $compiler is missing"; exit 1; }
[[ -d $angle/src ]] || { echo "lattice_translate.sh: the WebKit checkout is missing ($angle)"; exit 1; }
if [[ -z $out ]]; then
    out=$(mktemp -d)
    trap 'rm -rf -- "$out"' EXIT
fi
mkdir -p "$out/units" "$out/index" "$out/log"

"$compiler" --quiet compiler/cc/main.mlx -o "$out/mlx-cc" > "$out/cc.log" 2>&1 || { echo "lattice_translate.sh: mlx-cc does not build: $(tail -3 "$out/cc.log")"; exit 1; }
export MLX_CC_INCLUDE="$PWD/compiler/cc/include" MLX_CC_LIB="$PWD/mlx-out/lib" MLX_LD="$PWD/mlx-out/bin/mlx-ld"
# (The build's one define with a string in it, kept out of the word-split flags.)
echo '#define ANGLE_VK_MOCK_ICD_JSON "angledata/VkICD_mock_icd.json"' > "$out/defines.h"

flags="-DANGLE_ENABLE_ESSL -DANGLE_ENABLE_GLSL -DANGLE_ENABLE_VULKAN -DANGLE_PLATFORM_LINUX -DANGLE_ENABLE_SHARE_CONTEXT_LOCK=1 -DANGLE_ENABLE_CONTEXT_MUTEX=1 -DEGL_EGL_PROTOTYPES=0 -DEGL_NO_PLATFORM_SPECIFIC_TYPES -DGL_GLES_PROTOTYPES=0 -DNDEBUG -DLIBANGLE_IMPLEMENTATION -I$angle/WebKit -I$angle/include -I$angle/include/KHR -I$angle/src -I$angle/src/common/base -I$angle/src/common/third_party/xxhash -I$angle/third_party/zlib/google -Imlx-out/include/mlx-zlib -Imlx-out/include/mlx-vulkan -Imlx-out/include/mlx-spirv"

# A unit's module name: its path, what is no letter, digit or underscore
# an underscore.
module_of() {
    local path=${1%.*}
    path=${path//\//__}
    echo "${path//[^A-Za-z0-9_]/_}"
}

# One unit through mlx-cc: STEP is `index` or `emit`.
translate_one() {
    local step=$1 path=$2
    local name
    name=$(module_of "$path")
    local source=$angle/$path
    local standard="$flags -std=c++23 -include $angle/ANGLEPrefix.h -include $out/defines.h"
    [[ $path == *.c ]] && standard="$flags -std=c11"
    # (As projects/webkit/angle.mlx's sourceFlags: the port's header first.)
    [[ $path == src/libANGLE/renderer/vulkan/vk_helpers.cpp ]] && standard="$standard -include projects/webkit/port/angle/VkHelpersMlx.h"
    # (The C++ library's own units, mlxlibcpp: string's and the streams'
    # functions live in them.)
    if [[ $path == libcpp/* ]]; then
        source=compiler/$path
        standard="-std=c++23"
    fi
    local common="--emit-mlx-runtime=./c.mlx --emit-mlx-unit=$name"
    if [[ $step == index ]]; then
        # shellcheck disable=SC2086
        timeout 900 "$out/mlx-cc" $standard $common --emit-mlx-index="$out/index/$name.idx" --emit-mlx=/dev/null "$source" > "$out/log/$name.index.log" 2>&1 || echo "$path"
    else
        # shellcheck disable=SC2086
        timeout 900 "$out/mlx-cc" $standard $common --emit-mlx-manifest="$out/manifest.tsv" --emit-mlx="$out/units/$name.mlx" "$source" > "$out/log/$name.emit.log" 2>&1 || echo "$path"
    fi
}
export -f translate_one module_of
export out angle flags

{
    for list in "${lists[@]}"; do grep -E '\.(cpp|cc|c)$' "projects/webkit/lists/$list.txt"; done | sort -u
    for unit in compiler/libcpp/*.cpp; do echo "libcpp/$(basename "$unit")"; done
} | grep -E -- "$only" > "$out/units.txt"
units=$(wc -l < "$out/units.txt")
[[ $units -gt 0 ]] || { echo "lattice_translate.sh: no unit matches"; exit 1; }

echo "1. the units' lists ($units units)"
xargs -P "$jobs" -I{} bash -c 'translate_one index "$@"' _ {} < "$out/units.txt" > "$out/index.failed"
if [[ -s $out/index.failed ]]; then
    echo "lattice_translate.sh: $(wc -l < "$out/index.failed") units do not translate: $(head -3 "$out/index.failed" | tr '\n' ' ')"
    exit 1
fi

echo "2. the manifest"
while read -r path; do
    name=$(module_of "$path")
    printf '%s\t%s\n' "$name" "$out/index/$name.idx"
done < "$out/units.txt" > "$out/index.list"
"$out/mlx-cc" --lattice-units="$out/index.list" --lattice-manifest="$out/manifest.tsv" || { echo "lattice_translate.sh: the manifest was not made"; exit 1; }

echo "3. the units, each with what lives in it"
xargs -P "$jobs" -I{} bash -c 'translate_one emit "$@"' _ {} < "$out/units.txt" > "$out/emit.failed"
if [[ -s $out/emit.failed ]]; then
    echo "lattice_translate.sh: $(wc -l < "$out/emit.failed") units do not translate with the manifest: $(head -3 "$out/emit.failed" | tr '\n' ' ')"
    exit 1
fi

# The runtime and the std module beside the modules, and the program: a
# module that imports every unit.
# (The runtime imports mlxlibc's printf engine by its place in the
# repository: the copy names it from where the copy is.)
sed "s#\"../../../compiler/libc/#\"$(realpath --relative-to="$out/units" compiler/libc)/#" projects/lattice/runtime/c.mlx > "$out/units/c.mlx"
{
    echo "// ANGLE as one Mlx program: every unit a module (tools/lattice_translate.sh)."
    while read -r path; do
        name=$(module_of "$path")
        echo "const $name = @import(\"./$name.mlx\")"
    done < "$out/units.txt"
    echo
    echo "pub fn main() -> u8 {"
    echo "    return 0"
    echo "}"
} > "$out/units/angle.mlx"

modules=$(while read -r path; do echo "$out/units/$(module_of "$path").mlx"; done < "$out/units.txt")
# shellcheck disable=SC2086
functions=$(cat $modules | grep -c '^\(pub \)\?fn ')
# shellcheck disable=SC2086
held=$(cat $modules | grep -c '^// LATTICE: this function')
# shellcheck disable=SC2086
externs=$(cat $modules | grep -h '^extern("c") fn ' | sort -u | wc -l)
# shellcheck disable=SC2086
bytes=$(cat $modules | wc -c)
echo "$units modules, $functions functions written, $held held back, $externs functions declared extern, $bytes bytes of Mlx"

if [[ $compile == 1 ]]; then
    echo "4. the program through mlx4"
    if timeout 3600 "$compiler" --quiet "$out/units/angle.mlx" -o "$out/angle" > "$out/compile.log" 2>&1; then
        echo "ok   ANGLE compiles as one Mlx program"
    else
        echo "FAIL $(grep -c 'error:' "$out/compile.log") errors ($out/compile.log); the first:"
        grep 'error:' "$out/compile.log" | head -5
        exit 1
    fi
fi
