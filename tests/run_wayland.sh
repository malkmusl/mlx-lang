#!/usr/bin/env bash
# Runs the std.xml / std.wayland conformance tests with the self-hosted
# compiler and checks that the materialized modules match the canonical XML.
#
#   tests/run_wayland.sh [compiler]     (default: mlx-out/bin/compiler/mlx4,
#                                        then zig-out/bin/mlx1)
#
# Every test exits 13 on success. Run from any directory.
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
    echo "run_wayland.sh: no Mlx compiler found (build mlx1 with 'zig build mlx1')" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
failures=0
for test in tests/240_*.mlx tests/241_*.mlx tests/242_*.mlx tests/243_*.mlx tests/244_*.mlx tests/245_*.mlx tests/246_*.mlx; do
    if ! "$compiler" --quiet "$test" -o "$work/test" 2> "$work/errors"; then
        echo "FAIL (compile) $test"
        cat "$work/errors"
        failures=$((failures + 1))
        continue
    fi
    set +e
    timeout 60 "$work/test" > /dev/null
    status=$?
    set -e
    if [[ $status -eq 13 ]]; then
        echo "ok   $test"
    else
        echo "FAIL (exit $status) $test"
        failures=$((failures + 1))
    fi
done

# The compositor's damage tracking: frames composed in what changed must
# equal frames composed from scratch.
for test in tests/268_compositor_damage_runtime.mlx; do
    if ! "$compiler" --quiet "$test" -o "$work/test" 2> "$work/errors"; then
        echo "FAIL (compile) $test"
        cat "$work/errors"
        failures=$((failures + 1))
        continue
    fi
    set +e
    timeout 120 "$work/test" > "$work/output"
    status=$?
    set -e
    if [[ $status -eq 13 ]]; then
        echo "ok   $test"
    else
        echo "FAIL (exit $status) $test"
        cat "$work/output"
        failures=$((failures + 1))
    fi
done

for example in examples/wayland-client/main.mlx examples/wayland-server/main.mlx examples/wayland-terminal/main.mlx examples/wayland-compositor/main.mlx tools/wayland-test-host/main.mlx; do
    if "$compiler" --quiet "$example" -o "$work/example"; then
        echo "ok   $example (builds)"
    else
        echo "FAIL (compile) $example"
        failures=$((failures + 1))
    fi
done

if std/protocols/wayland/materialize.sh --check "$compiler" > /dev/null 2>&1; then
    echo "ok   std/src/wayland/generated is current"
else
    echo "FAIL std/src/wayland/generated is stale; run std/protocols/wayland/materialize.sh"
    failures=$((failures + 1))
fi

if [[ $failures -ne 0 ]]; then
    echo "$failures failure(s)"
    exit 1
fi
