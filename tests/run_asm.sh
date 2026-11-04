#!/bin/bash
set -e

# Usage: ./tests/run_asm.sh tests/06_control_flow.mlx [expected_exit_code]

MLX_FILE=$1
EXPECTED=${2:-5}
if [[ $# -lt 1 ]]; then
    echo "usage: tests/run_asm.sh FILE.mlx [EXPECTED_EXIT_CODE]" >&2
    exit 2
fi

TMP_DIR=$(mktemp -d)
OUT_PATH="$TMP_DIR/out"
trap 'rm -rf -- "$TMP_DIR"' EXIT

echo "Building the stage-0 compiler (zig-out/bin/mlx0, with zig; only when its sources changed)..."
# Without zig an mlx0 built before serves as it is; without either there
# is nothing to test with.
if command -v zig > /dev/null; then
    ./mlx build mlx0
elif [[ -x zig-out/bin/mlx0 ]]; then
    echo "(no zig: zig-out/bin/mlx0 as it was built)"
else
    echo "skip: no zig and no zig-out/bin/mlx0"
    exit 0
fi

echo "Compiling $MLX_FILE to ELF..."
./zig-out/bin/mlx0 "$MLX_FILE" "-o$OUT_PATH"

echo "Executing..."
set +e
"$OUT_PATH"
EXIT_CODE=$?
set -e

echo "Program exited with code: $EXIT_CODE"

if [ "$EXIT_CODE" -eq "$EXPECTED" ]; then
    echo "SUCCESS: Program returned $EXPECTED as expected!"
else
    echo "ERROR: Expected $EXPECTED but got $EXIT_CODE."
    exit 1
fi
