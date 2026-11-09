#!/usr/bin/env bash
# Lattice L0 (projects/webkit/LATTICE.md): mlx-cc's C++ to Mlx translator.
# Builds mlx-cc from the sources, translates the pilot units of ANGLE's
# common/ with `--emit-mlx` and compares the output with the files kept in
# projects/lattice/angle/common (regenerate them with --update), compiles
# the translated modules with mlx4 and runs their tests: ANGLE's
# aligned_memory test as Mlx, and the mathutil differential test (the Mlx
# module and the C++ built by mlx-cc print the same lines for the same
# inputs). Needs mlx-out/bin/compiler/mlx4 and the WebKit checkout
# (projects/webkit/upstream).
#
#   tools/check_lattice_emit.sh [--update]
set -u
cd "$(dirname "$0")/.."

update=0
[[ ${1:-} == --update ]] && update=1
compiler=mlx-out/bin/compiler/mlx4
[[ -x "$compiler" ]] || { echo "check_lattice_emit.sh: $compiler is missing (run tools/bootstrap.sh)"; exit 1; }
angle=projects/webkit/upstream/Source/ThirdParty/ANGLE
[[ -d "$angle/src/common" ]] || { echo "check_lattice_emit.sh: $angle is missing (tools/fetch_webkit.sh)"; exit 1; }
out=projects/lattice/angle/common

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "FAIL $1"; exit 1; }

"$compiler" --quiet compiler/cc/main.mlx -o "$work/mlx-cc" > "$work/cc.log" 2>&1 || fail "mlx-cc does not build: $(tail -5 "$work/cc.log")"
export MLX_CC_INCLUDE="$PWD/compiler/cc/include"
export MLX_CC_LIB="$PWD/mlx-out/lib"
[[ -x mlx-out/bin/mlx-ld ]] || { echo "check_lattice_emit.sh: mlx-out/bin/mlx-ld is missing (./mlx build mlx-ld)"; exit 1; }
export MLX_LD="$PWD/mlx-out/bin/mlx-ld"
echo "ok   mlx-cc builds"

# ANGLE's flags for the Vulkan backend on Linux, as projects/webkit/build.mlx compiles it.
flags=(-DANGLE_ENABLE_ESSL -DANGLE_ENABLE_GLSL -DANGLE_ENABLE_VULKAN -DANGLE_PLATFORM_LINUX
    -DANGLE_ENABLE_SHARE_CONTEXT_LOCK=1 -DANGLE_ENABLE_CONTEXT_MUTEX=1 -DEGL_EGL_PROTOTYPES=0
    -DEGL_NO_PLATFORM_SPECIFIC_TYPES -DGL_GLES_PROTOTYPES=0 -DNDEBUG -DLIBANGLE_IMPLEMENTATION
    -I"$angle/WebKit" -I"$angle/include" -I"$angle/include/KHR" -I"$angle/src" -I"$angle/src/common/base"
    -I"$angle/src/common/third_party/xxhash" -I"$angle/third_party/zlib/google"
    -Imlx-out/include/mlx-zlib -Imlx-out/include/mlx-vulkan -Imlx-out/include/mlx-spirv
    -std=c++23 -include "$angle/ANGLEPrefix.h")

units=(aligned_memory mathutil Float16ToFloat32 utilities)
for unit in "${units[@]}"; do
    "$work/mlx-cc" "${flags[@]}" --emit-mlx="$work/$unit.mlx" "$angle/src/common/$unit.cpp" 2> "$work/$unit.err" || fail "$unit.cpp does not translate: $(head -5 "$work/$unit.err")"
    if [[ $update -eq 1 ]]; then
        cp "$work/$unit.mlx" "$out/$unit.mlx"
    elif ! cmp -s "$work/$unit.mlx" "$out/$unit.mlx"; then
        fail "$out/$unit.mlx is not what the translator writes now (tools/check_lattice_emit.sh --update): $(diff "$out/$unit.mlx" "$work/$unit.mlx" | head -12)"
    fi
    marks=$(grep -c '^// LATTICE:\|// LATTICE: ' "$out/$unit.mlx" || true)
    echo "ok   $unit.cpp translated ($(grep -c '^pub fn\|^fn' "$out/$unit.mlx") functions, $marks marks for the hand)"
done

# ANGLE's aligned_memory test (aligned_memory_unittest.cpp) as Mlx on the translated module.
"$compiler" --quiet "$out/tests/aligned_memory_test.mlx" -o "$work/aligned_test" 2> "$work/aligned.err" || fail "aligned_memory_test.mlx does not compile: $(head -5 "$work/aligned.err")"
"$work/aligned_test" > "$work/aligned.out" 2>&1 || fail "the aligned_memory test: $(cat "$work/aligned.out")"
echo "ok   $(cat "$work/aligned.out")"

# A differential test: the C++ reference (built by mlx-cc against the C++
# unit) and the Mlx driver (on the translated module) print the same.
differential() { # UNIT TEST WORDS [OTHER C++ UNITS THE REFERENCE LINKS...]
    local unit=$1 test=$2 words=$3
    shift 3
    local others=()
    local other
    for other in "$@"; do others+=("$angle/src/common/$other.cpp"); done
    "$work/mlx-cc" "${flags[@]}" -o "$work/${test}_ref" "$out/tests/${test}_ref.cpp" "$angle/src/common/$unit.cpp" "${others[@]}" 2> "$work/${test}_ref.err" || fail "$test: the C++ reference does not build: $(head -5 "$work/${test}_ref.err")"
    "$work/${test}_ref" > "$work/${test}_ref.out" || fail "$test: the C++ reference does not run"
    "$compiler" --quiet "$out/tests/${test}_test.mlx" -o "$work/${test}_test" 2> "$work/${test}.err" || fail "${test}_test.mlx does not compile: $(head -5 "$work/${test}.err")"
    "$work/${test}_test" > "$work/${test}_mlx.out" || fail "$test: the Mlx test does not run"
    cmp -s "$work/${test}_ref.out" "$work/${test}_mlx.out" || fail "$test: the Mlx and the C++ differ (C++ < > Mlx): $(diff "$work/${test}_ref.out" "$work/${test}_mlx.out" | head -12)"
    echo "ok   $unit: $words, $(wc -l < "$work/${test}_ref.out") lines the same in Mlx and C++"
}
differential mathutil mathutil "RGB9E5 packing and unpacking"
differential Float16ToFloat32 float16 "every half-precision pattern to a float"
differential utilities utilities "the GL type and target predicates, sizes, counts and orders over the enum range" string_utils
echo "check_lattice_emit.sh: all checks passed"
