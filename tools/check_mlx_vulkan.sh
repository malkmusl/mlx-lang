#!/usr/bin/env bash
# Checks mlx-vulkan (compiler/libc/vulkan): its API module and dispatch
# are what the registry makes (generate.sh --check), its headers compile
# in C and C++, and a C program (compiler/libc/vulkan/tests/fill.c) built
# by mlx-cc on mlxlibc makes an instance and a device on the system's
# Vulkan driver (lavapipe), fills a buffer on the GPU and reads it back.
# Without a Vulkan driver manifest it says so and skips the last part.
#
#   tools/check_mlx_vulkan.sh
set -euo pipefail
cd "$(dirname "$0")/.."
fail() { echo "FAIL $1"; exit 1; }
compiler/libc/vulkan/generate.sh --check || fail "compiler/libc/vulkan's generated modules are stale (run generate.sh)"
echo "ok   api/vulkan_core.mlx and commands.mlx are the registry's"
./mlx build -j 4 vulkan libc cc > /dev/null 2>&1 || fail "mlx-vulkan does not build"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc=mlx-out/bin/mlx-cc
printf '#include <vulkan/vulkan.h>\nint main(void) { return VK_SUCCESS; }\n' > "$work/c.c"
cp "$work/c.c" "$work/cpp.cpp"
"$cc" -c -Imlx-out/include/mlx-vulkan -o "$work/c.o" "$work/c.c" || fail "vulkan.h does not compile as C"
"$cc" -std=c++23 -c -Imlx-out/include/mlx-vulkan -o "$work/cpp.o" "$work/cpp.cpp" || fail "vulkan.h does not compile as C++"
echo "ok   vulkan/vulkan.h compiles in C and C++"
"$cc" -Imlx-out/include/mlx-vulkan -o "$work/fill" compiler/libc/vulkan/tests/fill.c --dynamic-linker="$PWD/mlx-out/libc/libmlxc.so.1" -l:"$PWD/mlx-out/lib/libmlxvulkan.so" || fail "fill.c does not build"
manifests=$(ls /usr/share/vulkan/icd.d/*.json /etc/vulkan/icd.d/*.json 2>/dev/null || true)
if [[ -z "$manifests" && -z "${VK_DRIVER_FILES:-}" ]]; then
    echo "skip the GPU part: no Vulkan driver manifest (install mesa-vulkan-drivers for lavapipe)"
    exit 0
fi
out=$(timeout 120 "$work/fill" 2>&1) || fail "fill.c: $out"
grep -q "filled 12345678" <<< "$out" || fail "the buffer was not filled: $out"
echo "ok   a buffer filled on $(grep '^device ' <<< "$out" | cut -c8-) through libmlxvulkan.so"
echo "PASS mlx-vulkan"
