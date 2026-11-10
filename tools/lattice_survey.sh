#!/usr/bin/env bash
# Lattice (projects/webkit/LATTICE.md): how far the C++ to Mlx translator
# is on ANGLE as a whole. Runs `mlx-cc --emit-mlx` over every unit of the
# build's ANGLE lists (projects/webkit/lists/angle*.txt) and counts, per
# directory: the units that translate, the functions written as Mlx, the
# ones held back as comments (a type or a call the Mlx lacks), the marks
# left to the hand and the calls still declared extern. With --compile,
# each translation is compiled with mlx4 as well (a module of its own).
#
#   tools/lattice_survey.sh [--compile] [--jobs N] [--out DIR] [LIST...]
#
# Needs mlx-out/bin/compiler/mlx4 and the WebKit checkout
# (projects/webkit/upstream). Prints the table; DIR (a temporary directory
# by default) keeps the translations and `survey.tsv`, a line per unit.
set -euo pipefail
cd "$(dirname "$0")/.."

compile=0
jobs=4
out=
lists=()
while [[ $# -gt 0 ]]; do
    case $1 in
    --compile) compile=1 ;;
    --jobs) jobs=$2; shift ;;
    --out) out=$2; shift ;;
    *) lists+=("$1") ;;
    esac
    shift
done
[[ ${#lists[@]} -gt 0 ]] || lists=(angle angle-vulkan angle-glesv2)

compiler=mlx-out/bin/compiler/mlx4
angle=projects/webkit/upstream/Source/ThirdParty/ANGLE
[[ -x $compiler ]] || { echo "lattice_survey.sh: $compiler is missing"; exit 1; }
[[ -d $angle/src ]] || { echo "lattice_survey.sh: the WebKit checkout is missing ($angle)"; exit 1; }
if [[ -z $out ]]; then
    out=$(mktemp -d)
    trap 'rm -rf -- "$out"' EXIT
fi
mkdir -p "$out/units"

"$compiler" --quiet compiler/cc/main.mlx -o "$out/mlx-cc" > "$out/cc.log" 2>&1 || { echo "lattice_survey.sh: mlx-cc does not build: $(tail -3 "$out/cc.log")"; exit 1; }
export MLX_CC_INCLUDE="$PWD/compiler/cc/include" MLX_CC_LIB="$PWD/mlx-out/lib" MLX_LD="$PWD/mlx-out/bin/mlx-ld"
# (The build's one define with a string in it, kept out of the word-split flags.)
echo '#define ANGLE_VK_MOCK_ICD_JSON "angledata/VkICD_mock_icd.json"' > "$out/defines.h"

flags="-DANGLE_ENABLE_ESSL -DANGLE_ENABLE_GLSL -DANGLE_ENABLE_VULKAN -DANGLE_PLATFORM_LINUX -DANGLE_ENABLE_SHARE_CONTEXT_LOCK=1 -DANGLE_ENABLE_CONTEXT_MUTEX=1 -DEGL_EGL_PROTOTYPES=0 -DEGL_NO_PLATFORM_SPECIFIC_TYPES -DGL_GLES_PROTOTYPES=0 -DNDEBUG -DLIBANGLE_IMPLEMENTATION -I$angle/WebKit -I$angle/include -I$angle/include/KHR -I$angle/src -I$angle/src/common/base -I$angle/src/common/third_party/xxhash -I$angle/third_party/zlib/google -Imlx-out/include/mlx-zlib -Imlx-out/include/mlx-vulkan -Imlx-out/include/mlx-spirv"

# One unit: its translation and a line `path lines status functions held marks externs compiled`.
survey_one() {
    local path=$1
    local name=${path//\//__}
    local file=$out/units/${name%.*}.mlx
    local source=$angle/$path
    local standard="-std=c++23 -include $angle/ANGLEPrefix.h -include $out/defines.h"
    [[ $path == *.c ]] && standard="-std=c11"
    # (As projects/webkit/angle.mlx's sourceFlags: the port's header first.)
    [[ $path == src/libANGLE/renderer/vulkan/vk_helpers.cpp ]] && standard="$standard -include projects/webkit/port/angle/VkHelpersMlx.h"
    local lines status=ok functions=0 held=0 marks=0 externs=0 compiled=-
    lines=$(wc -l < "$source")
    # shellcheck disable=SC2086
    if ! timeout 600 "$out/mlx-cc" $flags $standard --emit-mlx-runtime=./c.mlx --emit-mlx-std=./libcpp.mlx --emit-mlx-std-file=projects/lattice/runtime/libcpp.mlx --emit-mlx="$file" "$source" > "$file.log" 2>&1; then
        status=fail
    else
        functions=$(grep -c '^\(pub \)\?fn ' "$file" || true)
        held=$(grep -c '^// LATTICE: this function' "$file" || true)
        marks=$(grep -c 'LATTICE:' "$file" || true)
        externs=$(grep -c '^extern("c") fn ' "$file" || true)
        if [[ $compile == 1 ]]; then
            compiled=no
            # (A module has no main: that one error is the success.)
            if ! timeout 300 "$compiler" --quiet "$file" -o /dev/null 2>&1 | grep -v 'unresolved backend symbol' | grep -q 'error:'; then compiled=yes; fi
        fi
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$path" "$lines" "$status" "$functions" "$held" "$marks" "$externs" "$compiled"
}
export -f survey_one
export out angle flags compile compiler

# (The runtime and the std module beside the translations, for --compile.)
cp projects/lattice/runtime/c.mlx projects/lattice/runtime/libcpp.mlx "$out/units/"

for list in "${lists[@]}"; do grep -E '\.(cpp|cc|c)$' "projects/webkit/lists/$list.txt"; done | sort -u > "$out/units.txt"
xargs -P "$jobs" -I{} bash -c 'survey_one "$@"' _ {} < "$out/units.txt" > "$out/survey.tsv"

summarize() {
    awk -F'\t' -v compile="$compile" -v total="$1" '
    function group(path,    parts, n) {
        n = split(path, parts, "/")
        if (total == 1) return "all"
        if (parts[1] != "src") return parts[1]
        if (parts[2] == "libANGLE" && parts[3] == "renderer") return "src/libANGLE/renderer" (n > 4 ? "/" parts[4] : "")
        if (parts[2] == "compiler") return "src/compiler/" parts[3]
        return "src/" parts[2]
    }
    {
        g = group($1)
        units[g]++; lines[g] += $2
        if ($3 == "ok") { ok[g]++; functions[g] += $4; held[g] += $5; marks[g] += $6; externs[g] += $7 }
        if ($8 == "yes") compiled[g]++
    }
    END {
        for (g in units) {
            sum = functions[g] + held[g]
            printf "%-34s %5d %8d %6d %9d %6d %5.1f %6d %7d", g, units[g], lines[g], ok[g], functions[g], held[g], (sum ? 100 * functions[g] / sum : 0), marks[g], externs[g]
            if (compile == 1) printf " %8d", compiled[g]
            printf "\n"
        }
    }' "$out/survey.tsv"
}
printf "%-34s %5s %8s %6s %9s %6s %5s %6s %7s" "part" "units" "lines" "transl" "functions" "held" "live%" "marks" "externs"
[[ $compile == 1 ]] && printf " %8s" "compiles"
printf "\n"
summarize 0 | sort
summarize 1
failed=$(awk -F'\t' '$3 != "ok"' "$out/survey.tsv" | wc -l)
if [[ $failed -gt 0 ]]; then
    echo "units the translator fails on:"
    awk -F'\t' '$3 != "ok" { print "  " $1 }' "$out/survey.tsv" | head -40
fi
