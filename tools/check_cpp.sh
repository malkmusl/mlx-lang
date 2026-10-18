#!/usr/bin/env bash
# Checks mlx-cc's C++ and mlxlibcpp: builds the compiler and the C++
# library (tools/build_mlxlibcpp.sh), then compiles and runs every
# program in projects/cc/tests/cpp (NAME.cpp, with the options of
# NAME.flags and the input of NAME.stdin when there are any) against the
# output in NAME.expected, which g++ -std=c++17's build of the same
# program printed when the test was written: first on the system's C
# library, then on mlxlibc as loader and C library. Needs
# mlx-out/bin/compiler/mlx4 (tools/bootstrap.sh).
set -u
cd "$(dirname "$0")/.."

compiler=mlx-out/bin/compiler/mlx4
[[ -x "$compiler" ]] || { echo "check_cpp.sh: $compiler is missing (run tools/bootstrap.sh)"; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "FAIL $1"; exit 1; }

tools/build_mlxlibcpp.sh --compiler "$compiler" --out "$work/libcpp" > "$work/libcpp.log" 2>&1 || fail "mlxlibcpp does not build: $(tail -5 "$work/libcpp.log")"
cp -r projects/cc/include "$work/include"
cc="$work/libcpp/mlx-cc"
export MLX_CC_INCLUDE="$work/include"
export MLX_CC_LIB="$work/libcpp"
echo "ok   mlx-cc and mlxlibcpp build ($("$work/libcpp/mlx-ar" t "$work/libcpp/libmlxcpp.a" | tr '\n' ' '))"

tests=projects/cc/tests/cpp
programs=()
mkdir -p "$work/run"
run() { # NAME EXECUTABLE OUTPUT [LOADER]
    local name=$1 exe=$2 out=$3 status=0
    local input=/dev/null
    [[ -f "$tests/$name.stdin" ]] && input="$PWD/$tests/$name.stdin"
    if [[ $# -gt 3 ]]; then
        (cd "$work/run" && env -i LD_LIBRARY_PATH="$work/lib" timeout 20 "$4" "$exe" arg < "$input" > "$out" 2>&1) || status=$?
    else
        (cd "$work/run" && timeout 20 "$exe" arg < "$input" > "$out" 2>&1) || status=$?
    fi
    echo "exit $status" >> "$out"
}
for source in "$tests"/*.cpp; do
    name=$(basename "$source" .cpp)
    programs+=("$name")
    flags=()
    [[ -f "$tests/$name.flags" ]] && read -r -a flags < "$tests/$name.flags"
    "$cc" "${flags[@]}" -o "$work/$name" "$source" 2> "$work/$name.cc" || fail "$name does not compile: $(head -5 "$work/$name.cc")"
    [[ ! -s "$work/$name.cc" ]] || fail "$name: the compiler said: $(head -5 "$work/$name.cc")"
    run "$name" "$work/$name" "$work/$name.out"
    cmp -s "$tests/$name.expected" "$work/$name.out" || fail "$name prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.out" | head -12)"
done
echo "ok   ${#programs[@]} C++ programs compile and run as g++'s builds do: ${programs[*]}"

# Separate compilation: the mangled names link across objects, and the
# library's members come in as needed (-x c++, or -lmlxcpp, says the
# objects are C++).
"$cc" -c -o "$work/stdlib.o" "$tests/stdlib.cpp" 2> "$work/sep.err" || fail "-c of stdlib.cpp: $(head -3 "$work/sep.err")"
"$cc" -x c++ -o "$work/stdlib_sep" "$work/stdlib.o" 2>> "$work/sep.err" || fail "linking stdlib.o: $(head -3 "$work/sep.err")"
run stdlib "$work/stdlib_sep" "$work/stdlib_sep.out"
cmp -s "$tests/stdlib.expected" "$work/stdlib_sep.out" || fail "stdlib from an object prints otherwise"
echo "ok   a C++ object links with the library and runs"

# Statics of inline functions and templates, and inline variables: one
# object for the program across separately compiled files.
"$cc" -c -o "$work/shared_a.o" "$tests/shared/a.cpp" 2> "$work/shared.err" || fail "-c of shared/a.cpp: $(head -3 "$work/shared.err")"
"$cc" -c -o "$work/shared_b.o" "$tests/shared/b.cpp" 2>> "$work/shared.err" || fail "-c of shared/b.cpp: $(head -3 "$work/shared.err")"
"$cc" -x c++ -o "$work/shared" "$work/shared_a.o" "$work/shared_b.o" 2>> "$work/shared.err" || fail "linking shared: $(head -3 "$work/shared.err")"
"$work/shared" > "$work/shared.out" || fail "shared exits with $?"
cmp -s "$tests/shared/expected" "$work/shared.out" || fail "shared prints otherwise (expected < > got): $(diff "$tests/shared/expected" "$work/shared.out" | head -6)"
echo "ok   the statics of inline functions are one object across files"

# Lambdas of two files numbered alike (each its file's first in a
# class), put into one template: each file's instance calls its own.
"$cc" -c -o "$work/lambdaunits_a.o" "$tests/lambdaunits/a.cpp" 2> "$work/lambdaunits.err" || fail "-c of lambdaunits/a.cpp: $(head -3 "$work/lambdaunits.err")"
"$cc" -c -o "$work/lambdaunits_b.o" "$tests/lambdaunits/b.cpp" 2>> "$work/lambdaunits.err" || fail "-c of lambdaunits/b.cpp: $(head -3 "$work/lambdaunits.err")"
"$cc" -x c++ -o "$work/lambdaunits" "$work/lambdaunits_a.o" "$work/lambdaunits_b.o" 2>> "$work/lambdaunits.err" || fail "linking lambdaunits: $(head -3 "$work/lambdaunits.err")"
"$work/lambdaunits" > "$work/lambdaunits.out" || fail "lambdaunits exits with $?"
cmp -s "$tests/lambdaunits/expected" "$work/lambdaunits.out" || fail "lambdaunits prints otherwise (expected < > got): $(diff "$tests/lambdaunits/expected" "$work/lambdaunits.out" | head -6)"
echo "ok   the lambdas of two files are classes of their own"

# The same programs on mlxlibc alone.
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/lib" > /dev/null
lib="$work/lib/libmlxc.so.1"
for name in "${programs[@]}"; do
    run "$name" "$work/$name" "$work/$name.mlxlibc" "$lib"
    cmp -s "$tests/$name.expected" "$work/$name.mlxlibc" || fail "$name on mlxlibc prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.mlxlibc" | head -12)"
done
echo "ok   the ${#programs[@]} programs run alike on mlxlibc as loader and C library"

echo "check_cpp.sh: all checks passed"
