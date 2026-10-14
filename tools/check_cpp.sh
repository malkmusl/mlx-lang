#!/usr/bin/env bash
# Checks mlx-cc's C++: builds the compiler, then compiles and runs every
# program in projects/cc/tests/cpp (NAME.cpp, with the options of
# NAME.flags when there is one) against the output in NAME.expected,
# which g++ -std=c++17's build of the same program printed when the
# test was written. Needs mlx-out/bin/compiler/mlx4 (tools/bootstrap.sh).
set -u
cd "$(dirname "$0")/.."

compiler=mlx-out/bin/compiler/mlx4
[[ -x "$compiler" ]] || { echo "check_cpp.sh: $compiler is missing (run tools/bootstrap.sh)"; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "FAIL $1"; exit 1; }

"$compiler" --quiet projects/cc/main.mlx -o "$work/mlx-cc" || fail "mlx-cc does not build"
cp -r projects/cc/include "$work/include"
cc="$work/mlx-cc"
export MLX_CC_INCLUDE="$work/include"
echo "ok   mlx-cc builds"

tests=projects/cc/tests/cpp
programs=()
mkdir -p "$work/run"
for source in "$tests"/*.cpp; do
    name=$(basename "$source" .cpp)
    programs+=("$name")
    flags=()
    [[ -f "$tests/$name.flags" ]] && read -r -a flags < "$tests/$name.flags"
    "$cc" "${flags[@]}" -o "$work/$name" "$source" 2> "$work/$name.cc" || fail "$name does not compile: $(head -5 "$work/$name.cc")"
    [[ ! -s "$work/$name.cc" ]] || fail "$name: the compiler said: $(head -5 "$work/$name.cc")"
    status=0
    (cd "$work/run" && timeout 20 "$work/$name" arg > "$work/$name.out" 2>&1) || status=$?
    echo "exit $status" >> "$work/$name.out"
    cmp -s "$tests/$name.expected" "$work/$name.out" || fail "$name prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.out" | head -12)"
done
echo "ok   ${#programs[@]} C++ programs compile and run as g++'s builds do: ${programs[*]}"

# Separate compilation: the mangled names link across objects.
"$cc" -c -o "$work/classes.o" "$tests/classes.cpp" 2> "$work/sep.err" || fail "-c of classes.cpp: $(head -3 "$work/sep.err")"
"$cc" -o "$work/classes_sep" "$work/classes.o" 2>> "$work/sep.err" || fail "linking classes.o: $(head -3 "$work/sep.err")"
status=0
(cd "$work/run" && timeout 20 "$work/classes_sep" arg > "$work/classes_sep.out" 2>&1) || status=$?
echo "exit $status" >> "$work/classes_sep.out"
cmp -s "$tests/classes.expected" "$work/classes_sep.out" || fail "classes from an object prints otherwise"
echo "ok   a C++ object links and runs"

echo "check_cpp.sh: all checks passed"
