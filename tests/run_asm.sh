#!/bin/bash
set -e

# Usage: ./tests/run_asm.sh tests/06_control_flow.zin [expected_exit_code]

ZIN_FILE=$1
EXPECTED=${2:-5}

TMP_DIR=$(mktemp -d)
OUT_PATH="$TMP_DIR/out"
trap 'rm -rf -- "$TMP_DIR"' EXIT

echo "Building compiler..."
zig build

echo "Compiling $ZIN_FILE to ELF..."
./zig-out/bin/zin0 "$ZIN_FILE" "-o$OUT_PATH"

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
