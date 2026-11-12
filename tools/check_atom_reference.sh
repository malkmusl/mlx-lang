#!/usr/bin/env bash
# Holds the kernel's interpreter for a Radeon's firmware tables
# (projects/brix/kernel/atom.mlx) against Linux's own (atom.c), which no
# card is needed for: both run the same tables on the same card that is a
# table, and what each did to it is held side by side, line for line:
# every register read and written in order with its word, the waits, the
# parameter space afterwards, the interpreter's state, how the run ended.
#
#   tools/check_atom_reference.sh LINUX [IMAGES [SEED]]
#       thousands of generated tables (IMAGES ROM images, 60 by default,
#       from SEED, 1 by default: the same numbers give the same tables)
#   tools/check_atom_reference.sh LINUX --mistakes
#       the comparison's own proof: deliberate mistakes put into copies
#       of the kernel's interpreter, each of which the comparison must
#       find
#   tools/check_atom_reference.sh LINUX ROM TABLE[:WORD,WORD,...] ...
#       a real ROM's tables (the user's own card's), each with its
#       parameter block (32-bit words as atombios.h lays the table's
#       parameters out, decimal or 0x...), through both; add -v for
#       Linux's own account of every instruction. (A table that waits
#       for the card, for a PLL to lock say, does not end on a card
#       that is a table: both interpreters give up, Linux's after 20000
#       jumps to one place and the kernel's after 200000 instructions,
#       and what both did up to there is held.)
#
# LINUX is a directory with these files of Linux 6.6 (they are not in
# this repository; from drivers/gpu/drm/amd/ of the kernel's tree):
#   amdgpu/atom.c amdgpu/atom.h
#   include/atom-names.h atom-bits.h atom-types.h atombios.h
#   include/atomfirmware.h atomfirmwareid.h pptable.h
#   amdgpu/ObjectID.h
# Linux's atom.c is built with the host's C compiler as an ordinary
# program around tools/atom_reference/kernel.h (what it asks of a kernel)
# and reference.c (the card); the kernel's interpreter with this
# repository's compiler around tools/atom_reference/ours.mlx. A
# development comparison in the way of tools/check_gpu_registers.py:
# nothing of what brixOS is built from comes from here, and no file of
# Linux is kept.
#
# The tables come from tools/atom_reference/tables.py, which says what
# they cover. Where the kernel's interpreter stops and Linux goes on (the
# head of atom.mlx lists the places), a table made for that place must
# stop there in the kernel's, having done exactly what Linux did up to
# there.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
here="$root/tools/atom_reference"
if [[ $# -lt 1 || ! -f "$1/atom.c" ]]; then
    sed -n '2,44p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
linux=$(cd "$1" && pwd)
shift
compiler=${MLX_COMPILER:-$root/mlx-out/bin/compiler/mlx4}
if [[ ! -x "$compiler" ]]; then
    echo "check_atom_reference: no compiler at $compiler (build it, or name one in MLX_COMPILER)" >&2
    exit 2
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/atom-reference.XXXXXX")
trap 'rm -rf "$work"' EXIT

# Linux's atom.c as a program: its ten files beside one another (and no
# other file of Linux's that LINUX may hold), and every header it asks
# the kernel for is kernel.h.
mkdir -p "$work/src/linux" "$work/src/asm" "$work/src/drm"
for name in atom.c atom.h atom-names.h atom-bits.h atom-types.h atombios.h atomfirmware.h atomfirmwareid.h pptable.h ObjectID.h; do
    if [[ ! -f "$linux/$name" ]]; then
        echo "check_atom_reference: $linux/$name is not there" >&2
        exit 2
    fi
    cp "$linux/$name" "$work/src/$name"
done
for name in linux/module.h linux/sched.h linux/slab.h linux/string_helpers.h linux/types.h asm/unaligned.h drm/drm_util.h amdgpu.h; do
    echo '#include "kernel.h"' > "$work/src/$name"
done
cc -O1 -w -I "$here" -I "$work/src" -o "$work/reference" "$here/reference.c" "$work/src/atom.c"

ours() {
    # The kernel's interpreter from the tree at $1, as the program $2.
    (cd "$1" && "$compiler" --quiet tools/atom_reference/ours.mlx -o "$2")
}

# (Python is not put in the shell's place: the work directory, with the
# two programs in it, is removed when the shell ends.)
if [[ "${1:-}" == "--mistakes" ]]; then
    python3 "$here/tables.py" mistakes "$root" "$work" "$compiler"
    exit $?
fi

ours "$root" "$work/ours"

if [[ $# -ge 2 && -f "${1:-}" ]]; then
    # A real ROM's tables.
    image=$1
    shift
    verbose=""
    : > "$work/runs"
    for run in "$@"; do
        if [[ "$run" == "-v" ]]; then
            verbose="-v"
            continue
        fi
        python3 "$here/tables.py" run-line "$run" >> "$work/runs"
    done
    "$work/reference" "$image" "$work/runs" $verbose > "$work/reference.txt"
    "$work/ours" "$image" "$work/runs" > "$work/ours.txt"
    # What the kernel's interpreter did, for the reader: all of a table
    # that ends, the beginning of one that waits for ever.
    head -n 400 "$work/ours.txt"
    lines=$(wc -l < "$work/ours.txt")
    if [[ $lines -gt 400 ]]; then
        echo "... ($((lines - 400)) lines more)"
        grep -E '^(run|end|ps|state|fb|#) ' "$work/ours.txt" | tail -n +2 || true
    fi
    python3 "$here/tables.py" compare "$work/ours.txt" "$work/reference.txt"
    exit $?
fi

images=${1:-60}
seed=${2:-1}
python3 "$here/tables.py" check "$work" "$images" "$seed"
exit $?
