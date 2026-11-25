#!/usr/bin/env bash
# Builds and runs nebula's tests (projects/nebula/tests/NN_name.mlx: each a
# program that exits 13 when all is well, another number says which check
# failed), and builds each for brixOS too, so that nothing in the engine
# needs a system the other does not have.
#
#   projects/nebula/test.sh [-c COMPILER] [NAME...]    (NAME: 01, 01_device, ...; none: all)
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
compiler=${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}
if [[ ${1:-} == -c ]]; then
    compiler=$2
    shift 2
fi
if [[ ! -x $compiler ]]; then
    echo "test.sh: no compiler at $compiler (run ./mlx build once, or tools/ensure_compiler.sh)" >&2
    exit 2
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

tests=()
if [[ $# -eq 0 ]]; then
    for file in projects/nebula/tests/[0-9]*.mlx; do tests+=("$file"); done
else
    for name in "$@"; do
        found=0
        for file in projects/nebula/tests/"$name"*.mlx; do
            if [[ -f $file ]]; then
                tests+=("$file")
                found=1
            fi
        done
        if [[ $found -eq 0 ]]; then
            echo "test.sh: no test named $name" >&2
            exit 2
        fi
    done
fi

failed=0
for file in "${tests[@]}"; do
    name=$(basename "$file" .mlx)
    if ! "$compiler" --quiet "$file" -o "$work/$name" > "$work/$name.log" 2>&1; then
        echo "FAIL $name: does not compile"
        head -n 20 "$work/$name.log"
        failed=1
        continue
    fi
    if ! "$compiler" --quiet --target=x86_64-brixos "$file" -o "$work/$name.brixos" > "$work/$name.brixos.log" 2>&1; then
        echo "FAIL $name: does not compile for brixOS"
        head -n 20 "$work/$name.brixos.log"
        failed=1
        continue
    fi
    status=0
    (cd "$work" && timeout 300 "./$name" > "$name.out" 2>&1) || status=$?
    if [[ $status -ne 13 ]]; then
        echo "FAIL $name: exit status $status"
        head -n 20 "$work/$name.out"
        failed=1
        continue
    fi
    echo "ok   $name"
done
if [[ $failed -ne 0 ]]; then
    echo FAIL
    exit 1
fi
echo PASS
