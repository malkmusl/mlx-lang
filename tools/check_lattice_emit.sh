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

units=(aligned_memory mathutil)
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

# The mathutil differential test: the C++ (built by mlx-cc) and the Mlx print the same.
"$work/mlx-cc" "${flags[@]}" -o "$work/mathutil_ref" "$out/tests/mathutil_ref.cpp" "$angle/src/common/mathutil.cpp" 2> "$work/ref.err" || fail "the C++ reference does not build: $(head -5 "$work/ref.err")"
"$work/mathutil_ref" > "$work/ref.out" || fail "the C++ reference does not run"
"$compiler" --quiet "$out/tests/mathutil_test.mlx" -o "$work/mathutil_test" 2> "$work/mathutil.err" || fail "mathutil_test.mlx does not compile: $(head -5 "$work/mathutil.err")"
"$work/mathutil_test" > "$work/mlx.out" || fail "the Mlx mathutil test does not run"
cmp -s "$work/ref.out" "$work/mlx.out" || fail "mathutil: the Mlx and the C++ differ (C++ < > Mlx): $(diff "$work/ref.out" "$work/mlx.out" | head -12)"
echo "ok   mathutil: RGB9E5 packing and unpacking, $(wc -l < "$work/ref.out") inputs the same in Mlx and C++"
echo "check_lattice_emit.sh: all checks passed"
