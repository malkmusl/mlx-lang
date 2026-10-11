#!/usr/bin/env bash
# Checks that the compositor's devices read without waiting whatever way
# their descriptors came (projects/desktop/compositor/device.mlx init):
# a blocking descriptor, as a session manager other than systemd-logind
# may hand over, becomes nonblocking, and an empty read gives EAGAIN.
#
# Usage: tools/check_compositor_device.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"$compiler" --quiet tools/compositor-test/device-nonblocking.mlx -o "$work/device-nonblocking"
if ! result=$(timeout 10 "$work/device-nonblocking"); then
    echo "check_compositor_device.sh: FAILED: ${result:-the read waited (no answer in 10 s)}" >&2
    exit 1
fi
[[ $result == ok ]] || { echo "check_compositor_device.sh: FAILED: $result" >&2; exit 1; }
echo "check_compositor_device.sh: a blocking device descriptor reads without waiting"
