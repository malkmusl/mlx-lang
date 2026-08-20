#!/bin/bash
set -e

# Usage: ./tests/run_fns.sh tests/07_functions.zin [expected_exit_code]

ZIN_FILE=${1:-tests/07_functions.zin}
EXPECTED=${2:-15}

echo "Building compiler..."
zig build

echo "Compiling $ZIN_FILE to ASM..."
./zig-out/bin/zin0 "$ZIN_FILE" > out.s

echo "=== Generated ASM ==="
cat out.s
echo "====================="

echo "Assembling out.s..."
nasm -f elf64 out.s -o out.o

echo "Linking..."
ld out.o -o out

echo "Executing..."
set +e
./out
EXIT_CODE=$?
set -e

echo "Program exited with code: $EXIT_CODE"

if [ "$EXIT_CODE" -eq "$EXPECTED" ]; then
    echo "SUCCESS: Program returned $EXPECTED as expected!"
else
    echo "ERROR: Expected $EXPECTED but got $EXIT_CODE."
    exit 1
fi
