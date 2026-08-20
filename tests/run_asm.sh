#!/bin/bash
set -e

# Usage: ./tests/run_asm.sh tests/06_control_flow.zin

ZIN_FILE=$1

echo "Building compiler..."
zig build

echo "Compiling $ZIN_FILE to LIR/ASM..."
# Run the compiler, which will output to out.s
./zig-out/bin/zin0 $ZIN_FILE > out.s

echo "Assembling out.s to out.o..."
nasm -f elf64 out.s -o out.o

echo "Linking out.o to executable out..."
ld out.o -o out

echo "Executing out..."
set +e
./out
EXIT_CODE=$?
set -e

echo "Program exited with code: $EXIT_CODE"

if [ "$EXIT_CODE" -eq 5 ]; then
    echo "SUCCESS: Program returned 5 as expected!"
else
    echo "ERROR: Program did not return 5. Returned $EXIT_CODE."
    exit 1
fi
