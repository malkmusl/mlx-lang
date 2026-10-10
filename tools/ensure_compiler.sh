#!/usr/bin/env bash
# Prints the path of an Mlx compiler that builds this checkout, building one
# when needed.
#
# The canonical compiler is mlx-out/bin/compiler/mlx4, the self-hosted
# compiler's fixed point (it builds the same binary from the same sources on
# every machine, so a crash address from one machine finds its place on
# another). When only the bootstrap-built zig-out/bin/mlx1 is there (or
# nothing: then `zig build mlx1`), mlx4 is built from it in two stages
# (mlx1 builds the compiler, which builds itself). When mlx4 is older than
# the compiler's sources and
# lacks something they added since (each feature below has a probe), it is
# rebuilt in two stages (mlx4 builds the new compiler, which builds itself)
# and installed once it has every feature; the message names what was
# missing. A rebuild that does not help (the old compiler cannot build the
# sources, or the result still lacks a feature) falls back to the bootstrap
# chain, and failing that leaves the old compiler in place and exits 1
# with the reason, so a check does not rebuild on every run in silence.
# Progress and messages go to stderr; stdout is the path alone.
#
#   compiler=$(tools/ensure_compiler.sh)
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
mkdir -p mlx-out/bin/compiler
canonical=mlx-out/bin/compiler/mlx4

# The probes: each returns 0 when the compiler ($1) has the feature. They
# build and run small programs in a directory under mlx-out (a /tmp mounted
# noexec would fail every one of them, and the compiler would be rebuilt
# on every run).

# Whether $1 writes shared objects (ET_DYN) with --shared.
writes_shared_objects() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet tests/293_signed_result_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 gives a plugin a soname and its exports a symbol version
# (--soname, --symbol-version; projects/desktop/libpulse needs them).
writes_symbol_versions() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet --plugin --soname=libmlxprobe.so.1 --symbol-version=MLXPROBE_1 tests/support/plugin_library.mlx -o "$probe/probe.so" > /dev/null 2>&1 \
        && grep -qa MLXPROBE_1 "$probe/probe.so" && grep -qa libmlxprobe.so.1 "$probe/probe.so"; then
        ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 gives an array whose length is a named constant that many
# elements (tests/294_array_length_constant_runtime.mlx exits 13); older
# compilers took a written-out number only and left such a binding with
# no size, so writing into it went over the frame.
resolves_named_array_lengths() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet tests/294_array_length_constant_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
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
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet tests/274_slice_assignment_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 makes an `export const` a data symbol of a plugin
# (tests/support/plugin_library.mlx's mlx_plugin_level, read as a C global
# from python3; projects/desktop/libpipewire needs it for pw_log_level).
exports_data() {
    command -v python3 > /dev/null || return 0
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet --plugin tests/support/plugin_library.mlx -o "$probe/probe.so" > /dev/null 2>&1 \
        && python3 -c 'import ctypes, sys; sys.exit(0 if ctypes.c_uint32.in_dll(ctypes.CDLL(sys.argv[1]), "mlx_plugin_level").value == 7 else 1)' "$probe/probe.so" > /dev/null 2>&1; then
        ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether the compiler lowers the atomic builtins (spec atomics-tls.xml,
# CallShapes): @atomicRmw, which std's general-purpose allocator and every
# lock use.
lowers_atomics() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    printf 'pub fn main() -> u8 {\n    var word: u32 = 1\n    if @atomicRmw(u32, &word, add, 2, seq_cst) != 1 || word != 3 { return 1 }\n    return 0\n}\n' > "$probe/probe.mlx"
    local ok=1
    if "$1" --quiet "$probe/probe.mlx" -o "$probe/probe" > /dev/null 2>&1 && "$probe/probe"; then ok=0; fi
    rm -rf -- "$probe"
    return $ok
}

# Whether the compiler lowers module-level `var` globals (slots of the
# image's data area), which mlxlibc and every stateful library need.
lowers_globals() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    printf 'var counter: u32 = 5\n\nfn bump() -> u32 {\n    counter += 1\n    return counter\n}\n\npub fn main() -> u8 {\n    if bump() != 6 || counter != 6 { return 1 }\n    return 0\n}\n' > "$probe/probe.mlx"
    local ok=1
    if "$1" --quiet "$probe/probe.mlx" -o "$probe/probe" > /dev/null 2>&1 && "$probe/probe"; then ok=0; fi
    rm -rf -- "$probe"
    return $ok
}

# Whether the compiler builds a plugin without a C library whose exports
# run on stack arenas (--plugin --stack-arena --no-libc), what mlxlibc is.
builds_freestanding_plugins() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    printf 'const Pair = struct { a: i64, b: i64 }\n\nfn pair(v: i64) -> Pair { return Pair.{ .a = v, .b = v } }\n\nexport fn probe_twice(v: i64) -> i64 {\n    const made = pair(v)\n    return made.a + made.b\n}\n' > "$probe/probe.mlx"
    local ok=1
    # (An older compiler takes the options and still names libc.so.6.)
    if "$1" --quiet --plugin --stack-arena --no-libc "$probe/probe.mlx" -o "$probe/probe.so" > /dev/null 2>&1 && ! grep -q "libc.so.6" "$probe/probe.so"; then ok=0; fi
    rm -rf -- "$probe"
    return $ok
}

# Whether the compiler has the thread builtins (spec atomics-tls.xml,
# <Threads>): @threadPointer and @spawnThread, which mlxlibc's threads need.
lowers_thread_builtins() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    printf 'fn entry(argument: usize) -> usize { return argument }\n\npub fn main() -> u8 {\n    var stack: [512]usize = undefined\n    const started = @spawnThread(0, @intFromPtr(&stack), 0, 0, 0, entry, 1)\n    if started == 1 || @threadPointer() == 1 { return 1 }\n    return 0\n}\n' > "$probe/probe.mlx"
    local ok=1
    if "$1" --quiet "$probe/probe.mlx" -o "$probe/probe" > /dev/null 2>&1; then ok=0; fi
    rm -rf -- "$probe"
    return $ok
}

# What mlxlibc (projects/libc) needs: @frameAddress, a narrow signed value
# read through a pointer kept signed, a module var with an array
# initializer, and a plugin with --entry and --init that runs as a program.
builds_mlxlibc() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    printf 'var table: [3]u16 = [3]u16{ 7, 8, 9 }\n\nfn load(address: usize) -> i32 {\n    unsafe { return @ptrFromInt(*i32, address).* }\n}\n\npub fn main() -> u8 {\n    var value: i32 = -1\n    if load(@intFromPtr(&value)) >= 0 { return 1 }\n    if table[2] != 9 { return 2 }\n    if @frameAddress() <= @intFromPtr(&value) { return 3 }\n    return 0\n}\n' > "$probe/probe.mlx"
    printf 'extern("syscall") fn syscall1(number: usize, a1: usize) -> isize {}\n\nexport fn start() -> void {\n    const exited = syscall1(231, 7)\n}\n\nexport fn init() -> void {}\n' > "$probe/entry.mlx"
    local ok=1
    if "$1" --quiet "$probe/probe.mlx" -o "$probe/probe" > /dev/null 2>&1 && "$probe/probe" && "$1" --quiet --plugin --stack-arena --no-libc --entry=start --init=init "$probe/entry.mlx" -o "$probe/entry.so" > /dev/null 2>&1; then
        chmod +x "$probe/entry.so"
        local status=0
        "$probe/entry.so" || status=$?
        if [[ $status -eq 7 ]]; then ok=0; fi
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether $1 decodes \xHH and \u{H..H} in string and character literals
# (tests/302_string_escapes_runtime.mlx exits 13); older compilers took
# "\x1b" as the bytes 'x', '1', 'b', and std.terminal needs ESC.
decodes_escapes() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet tests/302_string_escapes_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# Whether a slice stored through its address (`into.* = text` in a callee,
# `&local` passed on) keeps its length when the local reads it again
# (tests/303_slice_address_runtime.mlx exits 13); older compilers kept
# the words of such a local apart, so the store reached the pointer only,
# and loaded one word of a slice through a pointer.
stores_through_slice_addresses() {
    local probe
    probe=$(mktemp -d mlx-out/probe.XXXXXX)
    local ok=1
    if "$1" --quiet tests/303_slice_address_runtime.mlx -o "$probe/probe" > /dev/null 2>&1; then
        local status=0
        "$probe/probe" > /dev/null 2>&1 || status=$?
        [[ $status -eq 13 ]] && ok=0
    fi
    rm -rf -- "$probe"
    return $ok
}

# The probes with what each one stands for.
probes=(
    "writes_shared_objects|--shared objects"
    "writes_symbol_tables|symbol tables"
    "accepts_discards|\`_ = value\`"
    "keeps_slice_lengths|slice lengths kept on assignment"
    "orders_import_cycles|import cycles broken in order"
    "has_large_arena|the 1 GiB arena"
    "keeps_match_widths|match values stored at their width"
    "extends_signed_results|signed results sign-extended"
    "writes_symbol_versions|symbol versions"
    "resolves_named_array_lengths|array lengths from named constants"
    "exports_data|data symbols for export const"
    "lowers_atomics|the atomic builtins"
    "lowers_globals|module-level var globals"
    "builds_freestanding_plugins|plugins without libc on stack arenas"
    "lowers_thread_builtins|the thread builtins"
    "builds_mlxlibc|what mlxlibc needs (@frameAddress, signed loads through pointers, array initializers of module vars, --entry and --init)"
    "decodes_escapes|\\xHH escapes"
    "stores_through_slice_addresses|slices stored through their address keeping their length"
)

# What compiler $1 lacks, one feature per line (nothing when it has all).
missing_features() {
    local compiler=$1 entry probe what
    for entry in "${probes[@]}"; do
        probe=${entry%%|*}
        what=${entry#*|}
        "$probe" "$compiler" || echo "$what"
    done
}

# Builds the compiler from the current sources with $1 into $2, saying $3.
build_stage() {
    echo "  $3" >&2
    "$1" --quiet compiler/selfhost/main.mlx -o "$2"
}

# The compiler rebuilt from the current sources by $1 in two stages (it
# builds the new compiler, which builds itself), installed as the canonical
# one when it has every feature. Fails with the old compiler untouched when
# a stage fails or the result still lacks something.
rebuild_with() {
    local seed=$1
    local next=$canonical.next fixed=$canonical.fixed
    rm -f "$next" "$fixed"
    if ! build_stage "$seed" "$next" "stage 1 of 2: $seed builds the compiler from the current sources"; then
        echo "ensure_compiler.sh: $seed cannot build the current compiler sources" >&2
        rm -f "$next"
        return 1
    fi
    if ! build_stage "$next" "$fixed" "stage 2 of 2: the new compiler builds itself"; then
        echo "ensure_compiler.sh: the compiler $seed built cannot build the sources again" >&2
        rm -f "$next" "$fixed"
        return 1
    fi
    local still
    still=$(missing_features "$fixed")
    if [[ -n "$still" ]]; then
        echo "ensure_compiler.sh: the compiler rebuilt by $seed still lacks:" >&2
        sed 's/^/    /' <<< "$still" >&2
        rm -f "$next" "$fixed"
        return 1
    fi
    mv "$fixed" "$canonical"
    rm -f "$next"
    return 0
}

# The bootstrap compiler zig-out/bin/mlx1 (built with zig when missing);
# its path on stdout, or failure.
bootstrap_seed() {
    if [[ ! -x zig-out/bin/mlx1 ]]; then
        if ! command -v zig > /dev/null; then
            echo "ensure_compiler.sh: no bootstrap compiler (zig-out/bin/mlx1) and no zig to build one" >&2
            return 1
        fi
        echo "building the bootstrap compiler (zig build mlx1)" >&2
        zig build mlx1 >&2
    fi
    echo zig-out/bin/mlx1
}

if [[ ! -x $canonical ]]; then
    seed=$(bootstrap_seed) || exit 1
    echo "building the canonical compiler $canonical from $seed" >&2
    rebuild_with "$seed" || exit 1
else
    missing=$(missing_features "$canonical")
    if [[ -n "$missing" ]]; then
        echo "rebuilding $canonical from the current compiler sources: it lacks $(paste -sd '|' <<< "$missing" | sed 's/|/, /g')" >&2
        if ! rebuild_with "$canonical"; then
            echo "rebuilding $canonical from the bootstrap compiler instead" >&2
            seed=$(bootstrap_seed) || { echo "ensure_compiler.sh: $canonical stays as it is; build the bootstrap compiler (zig build mlx1) and run again" >&2; exit 1; }
            rebuild_with "$seed" || { echo "ensure_compiler.sh: $canonical stays as it is" >&2; exit 1; }
        fi
    fi
fi
echo "$canonical"
