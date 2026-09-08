#!/bin/bash
set -e

# Usage: ./tests/run_error.sh tests/09_type_errors.mlx MLX-E4001

MLX_FILE=$1
EXPECTED_CODE=$2

TMP_DIR=$(mktemp -d)
OUT_PATH="$TMP_DIR/out"
LOG_PATH="$TMP_DIR/compiler.log"
trap 'rm -rf -- "$TMP_DIR"' EXIT

echo "Building compiler..."
zig build

echo "Checking that $MLX_FILE is rejected with $EXPECTED_CODE..."
set +e
./zig-out/bin/mlx0 "$MLX_FILE" "-o$OUT_PATH" >"$LOG_PATH" 2>&1
EXIT_CODE=$?
set -e

if [ "$EXIT_CODE" -eq 0 ]; then
    echo "ERROR: Compilation unexpectedly succeeded."
    cat "$LOG_PATH"
    exit 1
fi

if ! grep -q "$EXPECTED_CODE" "$LOG_PATH"; then
    echo "ERROR: Expected diagnostic $EXPECTED_CODE was not emitted."
    cat "$LOG_PATH"
    exit 1
fi

if [ -e "$OUT_PATH" ]; then
    echo "ERROR: Compiler produced an executable for an invalid program."
    exit 1
fi

echo "SUCCESS: Compiler rejected the program with $EXPECTED_CODE."
