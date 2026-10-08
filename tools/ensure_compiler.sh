#!/usr/bin/env bash
# Prints the path of an Mlx compiler that builds this checkout, building one
# when needed.
#
# The canonical compiler is mlx-out/bin/compiler/mlx4, the self-hosted
# compiler's fixed point (it builds the same binary from the same sources on
# every machine, so a crash address from one machine finds its place on
# another). When only the bootstrap-built zig-out/bin/mlx1 is there (or
# nothing: then `zig build mlx1`), the chain mlx1 -> mlx2 -> mlx3 is built
# and mlx3 kept as mlx4. When mlx4 is older than the compiler's sources and
# lacks what they added since (it cannot write --shared objects, or writes
# no symbol table, or knows no `_ = value`, or loses a slice's length when
# it is assigned again, or stores a match's arms at their own widths, or
# loads a signed if/match result unsigned), it builds them twice (mlx4 ->
# new mlx -> mlx4 again).
# Messages go to stderr.
#
#   compiler=$(tools/ensure_compiler.sh)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
mkdir -p mlx-out/bin/compiler

# Whether $1 writes shared objects (ET_DYN) with --shared.
writes_shared_objects() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet --shared tests/support/shared_object_library.mlx -o "$probe/probe.so" > /dev/null 2>&1 \
        && [[ "$(od -An -tx1 -j16 -N2 "$probe/probe.so" | tr -d ' ')" == "0300" ]]; then
        ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 names the functions in a symbol table (.symtab), which the
# crash reports of the desktop programs read.
writes_symbol_tables() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/support/crash_report.mlx -o "$probe/probe" > /dev/null 2>&1 \
        && grep -q 'crash_report.mlx:[0-9]*:middle' "$probe/probe"; then
        ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 accepts `_ = value` (dropping a value).
accepts_discards() {
    "$1" --quiet tests/support/discard.mlx > /dev/null 2>&1
}

# Whether $1 analyzes an import cycle reached through a module that uses it
# (tests/275_import_cycle_order_runtime.mlx compiles; the programs split
# into parts need it).
orders_import_cycles() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/275_import_cycle_order_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether programs $1 builds have the 1 GiB aggregate arena
# (tests/281_arena_capacity_runtime.mlx exits 13); older compilers mapped
# 256 MiB, too little for the compiler to build the largest programs.
has_large_arena() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/281_arena_capacity_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether programs $1 builds store a match's arms at the match's width
# (tests/282_match_result_width_runtime.mlx exits 13); older compilers
# stored a narrower arm's value in part and loaded the whole slot.
keeps_match_widths() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/282_match_result_width_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether programs $1 builds load a narrow signed value an `if` or `match`
# made (or a `for` took from an array) sign-extended
# (tests/293_signed_result_runtime.mlx exits 13); older compilers loaded
# it as an unsigned word, so a negative i32 came back large and positive.
extends_signed_results() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/293_signed_result_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 keeps a slice's length when it is assigned again, stored in a
# field or held in an optional (tests/274_slice_assignment_runtime.mlx
# exits 13); older compilers stored the pointer only.
keeps_slice_lengths() {
    local probe
    probe=$(mktemp -d)
    local ok=1
    if "$1" --quiet tests/274_slice_assignment_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

if [[ ! -x mlx-out/bin/compiler/mlx4 ]]; then
    if [[ ! -x zig-out/bin/mlx1 ]]; then
        if command -v zig > /dev/null; then
            echo "building the Mlx compiler (zig build mlx1)" >&2
            zig build mlx1 >&2
        else
            echo "ensure_compiler.sh: no Mlx compiler; build one (zig build mlx1)" >&2
            exit 1
        fi
    fi
    echo "building the canonical compiler (mlx1 -> mlx2 -> mlx3, kept as mlx-out/bin/compiler/mlx4)" >&2
    zig-out/bin/mlx1 --quiet compiler/selfhost/main.mlx -o mlx-out/bin/compiler/mlx2
    mlx-out/bin/compiler/mlx2 --quiet compiler/selfhost/main.mlx -o mlx-out/bin/compiler/mlx3
    cp mlx-out/bin/compiler/mlx3 mlx-out/bin/compiler/mlx4
elif ! writes_shared_objects mlx-out/bin/compiler/mlx4 || ! writes_symbol_tables mlx-out/bin/compiler/mlx4 || ! accepts_discards mlx-out/bin/compiler/mlx4 || ! keeps_slice_lengths mlx-out/bin/compiler/mlx4 || ! orders_import_cycles mlx-out/bin/compiler/mlx4 || ! has_large_arena mlx-out/bin/compiler/mlx4 || ! keeps_match_widths mlx-out/bin/compiler/mlx4 || ! extends_signed_results mlx-out/bin/compiler/mlx4; then
    echo "rebuilding mlx-out/bin/compiler/mlx4 from the current compiler sources (it lacks --shared objects, symbol tables, \`_ = value\`, slice lengths kept on assignment, import cycles broken in order, the 1 GiB arena, match values stored at their width or signed results sign-extended)" >&2
    mlx-out/bin/compiler/mlx4 --quiet compiler/selfhost/main.mlx -o mlx-out/bin/compiler/mlx4.next
    mlx-out/bin/compiler/mlx4.next --quiet compiler/selfhost/main.mlx -o mlx-out/bin/compiler/mlx4.fixed
    mv mlx-out/bin/compiler/mlx4.fixed mlx-out/bin/compiler/mlx4
    rm -f mlx-out/bin/compiler/mlx4.next
fi
echo mlx-out/bin/compiler/mlx4
