#!/usr/bin/env bash
# Checks mlx-vulkan (compiler/libc/vulkan) and mlx-spirv's headers
# (compiler/libc/spirv): the generated modules are what the registry and
# the grammars make (generate.sh --check), the headers compile in C and
# C++ (spirv.hpp in C++), and a C program (compiler/libc/vulkan/tests/fill.c) built
# by mlx-cc on mlxlibc makes an instance and a device on the system's
# Vulkan driver (lavapipe), fills a buffer on the GPU and reads it back.
# Without a Vulkan driver manifest it says so and skips the last part.
#
#   tools/check_mlx_vulkan.sh
set -euo pipefail
cd "$(dirname "$0")/.."
fail() { echo "FAIL $1"; exit 1; }
compiler/libc/vulkan/generate.sh --check || fail "compiler/libc/vulkan's generated modules are stale (run generate.sh)"
compiler/libc/spirv/generate.sh --check || fail "compiler/libc/spirv's generated modules are stale (run generate.sh)"
echo "ok   api/vulkan_core.mlx, commands.mlx, api/spirv.mlx and api/glsl_std_450.mlx are the registry's and the grammars'"
./mlx build -j 4 vulkan spirv libc cc > /dev/null 2>&1 || fail "mlx-vulkan or mlx-spirv does not build"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc=mlx-out/bin/mlx-cc
printf '#include <vulkan/vulkan.h>\nint main(void) { return VK_SUCCESS; }\n' > "$work/c.c"
cp "$work/c.c" "$work/cpp.cpp"
"$cc" -c -Imlx-out/include/mlx-vulkan -o "$work/c.o" "$work/c.c" || fail "vulkan.h does not compile as C"
"$cc" -std=c++23 -c -Imlx-out/include/mlx-vulkan -o "$work/cpp.o" "$work/cpp.cpp" || fail "vulkan.h does not compile as C++"
echo "ok   vulkan/vulkan.h compiles in C and C++"
printf '#include <spirv/unified1/spirv.hpp>\nnamespace spv {\n#include <spirv/unified1/GLSL.std.450.h>\n}\nint main() { spv::ImageOperandsMask m = spv::ImageOperandsMaskNone; m = m | spv::ImageOperandsLodMask; return spv::MagicNumber == 0x07230203 && m == spv::ImageOperandsLodMask && spv::OpSDotKHR == spv::OpSDot && spv::GLSLstd450Round == 1 ? 0 : 1; }\n' > "$work/spirv.cpp"
"$cc" -std=c++23 -Imlx-out/include/mlx-spirv -o "$work/spirv" "$work/spirv.cpp" || fail "spirv.hpp does not compile"
"$work/spirv" || fail "spirv.hpp's values are not SPIR-V's"
echo "ok   spirv/unified1/spirv.hpp and GLSL.std.450.h compile in C++, with SPIR-V's values"
"$cc" -Imlx-out/include/mlx-vulkan -o "$work/fill" compiler/libc/vulkan/tests/fill.c --dynamic-linker="$PWD/mlx-out/libc/libmlxc.so.1" -l:"$PWD/mlx-out/lib/libmlxvulkan.so" || fail "fill.c does not build"
manifests=$(ls /usr/share/vulkan/icd.d/*.json /etc/vulkan/icd.d/*.json 2>/dev/null || true)
if [[ -z "$manifests" && -z "${VK_DRIVER_FILES:-}" ]]; then
    echo "skip the GPU part: no Vulkan driver manifest (install mesa-vulkan-drivers for lavapipe)"
    exit 0
fi
out=$(timeout 120 "$work/fill" 2>&1) || fail "fill.c: $out"
grep -q "filled 12345678" <<< "$out" || fail "the buffer was not filled: $out"
echo "ok   a buffer filled on $(grep '^device ' <<< "$out" | cut -c8-) through libmlxvulkan.so"
echo "PASS mlx-vulkan, mlx-spirv"
