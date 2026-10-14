#!/usr/bin/env bash
# mlx-cc, the C compiler in Mlx (projects/cc):
#
#   - it builds with the Mlx compiler, and finds its own headers next to
#     the program (include/);
#   - -E: projects/cc/tests/preprocess.c (C11 6.10.3.5's examples and
#     more directives) preprocesses to preprocess.expected;
#   - every test program in projects/cc/tests (NAME.c, with NAME2.c as its
#     second file when there is one, and the options of NAME.flags, such
#     as -std=c23) compiles, and runs printing what NAME.expected says
#     (stdout and stderr, then the exit status), as the program gcc made
#     of it did when the file was written;
#     (the tests directory is an include directory, for #embed <...>);
#   - -std=: each variant from c89 to gnu23 predefines what gcc does
#     (tests/standards.c prints __STDC_VERSION__, __STRICT_ANSI__,
#     `linux` and `unix`);
#   - the same programs run under mlxlibc as their dynamic loader and C
#     library (`libmlxc.so.1 PROGRAM`, an empty environment but for the
#     path to the libm built next to it): no GNU code
#     between the C source and the running process; and one built with
#     --dynamic-linker naming mlxlibc starts on it by itself.
#
#   tools/check_cc.sh [compiler]
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo_root"
compiler=${1:-${MLX_COMPILER:-mlx-out/bin/compiler/mlx4}}
work=$(mktemp -d /tmp/mlx-cc.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
fail() { echo "FAIL $1" >&2; exit 1; }

"$compiler" --quiet projects/cc/main.mlx -o "$work/mlx-cc" || fail "mlx-cc does not build"
cp -r projects/cc/include "$work/include"
cc="$work/mlx-cc"
echo "ok   mlx-cc builds ($(stat -c %s "$cc") bytes)"

tests=projects/cc/tests
(cd "$tests" && "$cc" -E preprocess.c) > "$work/preprocess.out" 2> "$work/preprocess.err" || fail "-E: $(head -3 "$work/preprocess.err")"
cmp -s "$tests/preprocess.expected" "$work/preprocess.out" || fail "-E differs: $(diff "$tests/preprocess.expected" "$work/preprocess.out" | head -10)"
echo "ok   -E: the C11 examples expand as the standard says"

# The test programs: NAME.c (and NAME2.c), their expected output.
programs=()
for source in "$tests"/*.c; do
    name=$(basename "$source" .c)
    [[ $name == preprocess || $name == standards || $name == *2 ]] && continue
    programs+=("$name")
done
mkdir -p "$work/run"
for name in "${programs[@]}"; do
    files=("$tests/$name.c")
    [[ -f "$tests/${name}2.c" ]] && files+=("$tests/${name}2.c")
    flags=()
    [[ -f "$tests/$name.flags" ]] && read -r -a flags < "$tests/$name.flags"
    "$cc" "${flags[@]}" -I "$tests" -o "$work/$name" "${files[@]}" 2> "$work/$name.cc" || fail "$name does not compile: $(head -5 "$work/$name.cc")"
    [[ ! -s "$work/$name.cc" ]] || fail "$name: the compiler said: $(head -5 "$work/$name.cc")"
    status=0
    (cd "$work/run" && timeout 20 "$work/$name" arg > "$work/$name.out" 2>&1) || status=$?
    echo "exit $status" >> "$work/$name.out"
    cmp -s "$tests/$name.expected" "$work/$name.out" || fail "$name prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.out" | head -12)"
done
echo "ok   ${#programs[@]} programs compile and run as expected: ${programs[*]}"

# The variants: what each predefines, as gcc does.
variants=0
while read -r option expected; do
    [[ $option == default ]] && option=""
    "$cc" $option -o "$work/standards" "$tests/standards.c" 2> "$work/standards.cc" || fail "$option: standards.c does not compile: $(head -3 "$work/standards.cc")"
    printed=$("$work/standards")
    [[ "$printed" == "$expected" ]] || fail "$option: standards.c printed '$printed', expected '$expected'"
    variants=$((variants + 1))
done <<'TABLE'
-std=c89 none strict no-linux-unix
-ansi none strict no-linux-unix
-std=iso9899:1990 none strict no-linux-unix
-std=gnu89 none gnu linux unix
-std=c99 199901 strict no-linux-unix
-std=gnu99 199901 gnu linux unix
-std=c11 201112 strict no-linux-unix
-std=gnu11 201112 gnu linux unix
-std=c17 201710 strict no-linux-unix
-std=c18 201710 strict no-linux-unix
-std=gnu17 201710 gnu linux unix
-std=c23 202311 strict no-linux-unix
-std=c2x 202311 strict no-linux-unix
-std=gnu23 202311 gnu linux unix
default 201710 gnu linux unix
TABLE
echo "ok   -std=: $variants variants from c89 to gnu23 predefine what gcc does (gnu17 by default)"

# The same programs on mlxlibc alone.
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/lib" > /dev/null
lib="$work/lib/libmlxc.so.1"
for name in "${programs[@]}"; do
    status=0
    (cd "$work/run" && env -i LD_LIBRARY_PATH="$work/lib" timeout 20 "$lib" "$work/$name" arg > "$work/$name.mlxlibc" 2>&1) || status=$?
    echo "exit $status" >> "$work/$name.mlxlibc"
    cmp -s "$tests/$name.expected" "$work/$name.mlxlibc" || fail "$name on mlxlibc prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.mlxlibc" | head -12)"
done
echo "ok   the ${#programs[@]} programs run alike on mlxlibc as loader and C library"

# A program that names mlxlibc as its interpreter starts on it by itself.
"$cc" --dynamic-linker "$lib" -o "$work/own" "$tests/libc.c" 2> "$work/own.cc" || fail "--dynamic-linker: $(head -3 "$work/own.cc")"
status=0
(cd "$work/run" && env -i LD_LIBRARY_PATH="$work/lib" timeout 20 "$work/own" arg > "$work/own.out" 2>&1) || status=$?
echo "exit $status" >> "$work/own.out"
cmp -s "$tests/libc.expected" "$work/own.out" || fail "the program with mlxlibc as interpreter prints otherwise: $(diff "$tests/libc.expected" "$work/own.out" | head -8)"
echo "ok   --dynamic-linker: a program naming libmlxc.so.1 as its interpreter runs on its own"
echo "check_cc.sh: all checks passed"
