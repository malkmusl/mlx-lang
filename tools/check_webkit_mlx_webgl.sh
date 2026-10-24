#!/usr/bin/env bash
# The Mlx port of WebKit, M8 (projects/webkit/PORT.md, "WebGL"): WebGL 1
# and 2 on ANGLE's Vulkan backend (libANGLE.a, libGLESv2.a), whose Vulkan
# is libmlxvulkan.so over the system's driver (lavapipe where there is no
# GPU). webcore-dump runs each of WebKit's layout tests that
# tests/layout/webgl.txt lists (fast/canvas/webgl and webgl, single-process
# ones: the GL calls, their errors and state, shaders and programs,
# textures, framebuffers, readPixels and the drawing buffer, context loss)
# as the storage check runs its own (tools/check_webkit_mlx_storage.sh,
# which this calls with --list webgl.txt), and compares what it prints with
# the expected text. Without a Vulkan driver manifest it says so and
# skips them.
#
#   tools/check_webkit_mlx_webgl.sh [--no-build] [-j N] [--keep DIR] [TEST...]
if [[ -z "$(ls /usr/share/vulkan/icd.d/*.json /etc/vulkan/icd.d/*.json 2> /dev/null)" && -z "${VK_DRIVER_FILES:-}" ]]; then
    echo "skip check_webkit_mlx_webgl.sh: no Vulkan driver manifest (install mesa-vulkan-drivers for lavapipe)"
    exit 0
fi
exec "$(dirname "$0")/check_webkit_mlx_storage.sh" --list webgl.txt "$@"
