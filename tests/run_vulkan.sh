#!/usr/bin/env bash
# Runs the std.json / std.vulkan tests with the self-hosted compiler.
#
#   tests/run_vulkan.sh [compiler]     (default: mlx-out/bin/compiler/mlx4,
#                                       then zig-out/bin/mlx1)
#
# The driver tests need a Vulkan driver; they use lavapipe (the Mesa CPU
# driver, Debian/Ubuntu package mesa-vulkan-drivers) when its manifest is
# installed, else they are skipped. The Wayland interop check also needs
# xkbcli (libxkbcommon-tools). The layout check needs gcc and the
# Vulkan headers (libvulkan-dev). Every test exits 13 on success.
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-}}
if [[ -z "$compiler" ]]; then
    for candidate in mlx-out/bin/compiler/mlx4 zig-out/bin/mlx1; do
        if [[ -x "$candidate" ]]; then compiler=$candidate; break; fi
    done
fi
if [[ -z "$compiler" || ! -x "$compiler" ]]; then
    echo "run_vulkan.sh: no Mlx compiler found (build mlx1 with 'zig build mlx1')" >&2
    exit 1
fi

lavapipe=""
for manifest in /usr/share/vulkan/icd.d/lvp_icd.json /usr/share/vulkan/icd.d/lvp_icd.x86_64.json; do
    if [[ -f "$manifest" ]]; then lavapipe=$manifest; break; fi
done

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0

# run TEST [VAR=value ...] [-- ARGUMENT ...]
run() {
    local test=$1
    shift
    local environment=()
    while [[ $# -gt 0 && "$1" != "--" ]]; do environment+=("$1"); shift; done
    [[ "${1:-}" == "--" ]] && shift
    if ! "$compiler" --quiet "$test" -o "$work/test" 2> "$work/errors"; then
        echo "FAIL (compile) $test"
        cat "$work/errors"
        failures=$((failures + 1))
        return
    fi
    set +e
    env "${environment[@]}" timeout 120 "$work/test" "$@" > "$work/output" 2>&1
    local status=$?
    set -e
    if [[ $status -ne 13 ]]; then
        echo "FAIL (exit $status) $test"
        cat "$work/output"
        failures=$((failures + 1))
    else
        echo "ok   $test"
    fi
}

run tests/248_json_runtime.mlx
run tests/251_vulkan_api_runtime.mlx
if [[ -n "$lavapipe" ]]; then
    run tests/253_vulkan_loader_runtime.mlx VK_DRIVER_FILES="$lavapipe"
    run tests/254_spirv_compute_runtime.mlx VK_DRIVER_FILES="$lavapipe"
    run tests/256_vulkan_icd_runtime.mlx -- "$work/manifest.json"
    run tests/257_vulkan_sharing_runtime.mlx VK_DRIVER_FILES="$lavapipe"
    run tests/258_vulkan_swapchain_runtime.mlx VK_DRIVER_FILES="$lavapipe"
else
    echo "skip tests/253, 254, 256, 257 and 258 (no lavapipe manifest)"
fi
run tests/255_spirv_module_runtime.mlx -- "$work/doubler.spv"

# The builder's module passes the Khronos validator, and modules from
# glslang pass std.spirv.module.
if command -v spirv-val > /dev/null && [[ -f "$work/doubler.spv" ]]; then
    if spirv-val --target-env vulkan1.1 "$work/doubler.spv"; then
        echo "ok   spirv-val accepts the std.spirv.builder module"
    else
        echo "FAIL spirv-val rejects the std.spirv.builder module"
        failures=$((failures + 1))
    fi
fi
if command -v glslangValidator > /dev/null; then
    "$compiler" --quiet tests/255_spirv_module_runtime.mlx -o "$work/validate"
    for shader in tests/support/spirv/*.comp tests/support/spirv/*.frag tests/support/spirv/*.vert; do
        for environment in vulkan1.0 vulkan1.3; do
            glslangValidator -V --target-env "$environment" "$shader" -o "$work/shader.spv" > /dev/null
            set +e
            "$work/validate" --validate "$work/shader.spv" > "$work/output" 2>&1
            status=$?
            set -e
            if [[ $status -eq 13 ]]; then
                echo "ok   std.spirv.module accepts $shader ($environment)"
            else
                echo "FAIL std.spirv.module rejects $shader ($environment)"
                cat "$work/output"
                failures=$((failures + 1))
            fi
        done
    done
fi

for example in examples/vulkan-info/main.mlx examples/vulkan-wayland-client/main.mlx examples/wayland-compositor/main.mlx; do
    if "$compiler" --quiet "$example" -o "$work/example" 2> "$work/errors"; then
        echo "ok   $example (builds)"
    else
        echo "FAIL (compile) $example"
        cat "$work/errors"
        failures=$((failures + 1))
    fi
done

# The examples' shaders (examples/vulkan-shared) pass the Khronos validator.
if "$compiler" --quiet examples/vulkan-shared/check_shaders.mlx -o "$work/check-shaders" 2> "$work/errors" && "$work/check-shaders" "$work" > "$work/output" 2>&1; then
    echo "ok   examples/vulkan-shared shaders build and pass std.spirv.module"
    if command -v spirv-val > /dev/null; then
        for shader in pattern blit; do
            if spirv-val --target-env vulkan1.1 "$work/$shader.spv"; then
                echo "ok   spirv-val accepts the $shader shader"
            else
                echo "FAIL spirv-val rejects the $shader shader"
                failures=$((failures + 1))
            fi
        done
    fi
else
    echo "FAIL examples/vulkan-shared/check_shaders.mlx"
    cat "$work/errors" "$work/output" 2> /dev/null
    failures=$((failures + 1))
fi

# The Vulkan Wayland client inside the compositor (both renderers, shm and
# dma-buf paths), checked pixel by pixel.
if [[ -n "$lavapipe" ]] && command -v xkbcli > /dev/null; then
    if tools/check_vulkan_wayland.sh "$compiler" "$lavapipe" > "$work/output" 2>&1; then
        sed 's/^/     /' "$work/output"
        echo "ok   tools/check_vulkan_wayland.sh"
    else
        echo "FAIL tools/check_vulkan_wayland.sh"
        cat "$work/output"
        failures=$((failures + 1))
    fi
else
    echo "skip tools/check_vulkan_wayland.sh (needs lavapipe and xkbcli)"
fi

if std/registry/vulkan/materialize.sh --check "$compiler" > "$work/check" 2>&1; then
    echo "ok   std/src/vulkan.mlx is current"
else
    echo "FAIL std/src/vulkan.mlx is stale"
    cat "$work/check"
    failures=$((failures + 1))
fi
if std/registry/spirv/materialize.sh --check "$compiler" > "$work/check" 2>&1; then
    echo "ok   std/src/spirv/core.mlx is current"
else
    echo "FAIL std/src/spirv/core.mlx is stale"
    cat "$work/check"
    failures=$((failures + 1))
fi

if command -v gcc > /dev/null && [[ -f /usr/include/vulkan/vulkan.h ]]; then
    if python3 tools/check_vulkan_layout.py "$compiler" > "$work/layout" 2>&1; then
        echo "ok   $(tail -1 "$work/layout")"
    else
        echo "FAIL layout check"
        cat "$work/layout"
        failures=$((failures + 1))
    fi
else
    echo "skip layout check (needs gcc and vulkan/vulkan.h)"
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures failure(s)"
    exit 1
fi
