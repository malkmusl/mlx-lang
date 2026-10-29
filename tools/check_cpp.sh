#!/usr/bin/env bash
# Checks mlx-cc's C++ and mlxlibcpp: builds the compiler and the C++
# library (tools/build_mlxlibcpp.sh), then compiles and runs every
# program in compiler/cc/tests/cpp (NAME.cpp, with the options of
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
cp -r compiler/cc/include "$work/include"
cc="$work/libcpp/mlx-cc"
export MLX_CC_INCLUDE="$work/include"
export MLX_CC_LIB="$work/libcpp"
echo "ok   mlx-cc and mlxlibcpp build ($("$work/libcpp/mlx-ar" t "$work/libcpp/libmlxcpp.a" | tr '\n' ' '))"

tests=compiler/cc/tests/cpp
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

# Two files of one -include prefix that opens an unnamed namespace (as
# ANGLE's does) each have a class of the same name in theirs: two classes
# (the files' unnamed namespaces are named after their paths), the
# prefix's names seen in both.
cat > "$work/anonprefix.h" << 'EOF'
namespace { inline int prefixed() { return 40; } }
struct Visitor { virtual ~Visitor() {} virtual int visit() = 0; };
EOF
for part in first second; do
    value=1
    [[ $part == second ]] && value=2
    cat > "$work/anon_$part.cpp" << EOF
namespace { struct Traverser : Visitor { int visit() override; }; int Traverser::visit() { return prefixed() + $value; } }
int $part() { Traverser t; Visitor &v = t; return v.visit(); }
EOF
    "$cc" -include "$work/anonprefix.h" -c -o "$work/anon_$part.o" "$work/anon_$part.cpp" 2> "$work/anon.err" || fail "anon_$part.cpp with a prefix: $(head -3 "$work/anon.err")"
done
printf '#include <stdio.h>\nint first();\nint second();\nint main() { printf("%%d %%d\\n", first(), second()); return 0; }\n' > "$work/anon_main.cpp"
"$cc" -o "$work/anon" "$work/anon_main.cpp" "$work/anon_first.o" "$work/anon_second.o" 2> "$work/anon.err" || fail "the files of one prefix's unnamed namespace do not link: $(head -3 "$work/anon.err")"
[[ "$("$work/anon")" == "41 42" ]] || fail "the files' unnamed namespaces: $("$work/anon")"
echo "ok   the unnamed namespaces of two files of one prefix are apart"

# A call whose result is a class declared only is an error (its size,
# and so how the result comes back, is not known), but not in decltype.
cat > "$work/incompleteresult.cpp" << 'EOF'
class Result;
Result handle(int);
struct Holder { decltype(handle(1)) *pointer; };
int use() { handle(2); return 0; }
EOF
if "$cc" -c -o "$work/incompleteresult.o" "$work/incompleteresult.cpp" 2> "$work/incompleteresult.err"; then
    fail "a call whose result type is incomplete compiled"
fi
grep -q "incompleteresult.cpp:4: error: calling a function whose result type is incomplete" "$work/incompleteresult.err" || fail "a call whose result type is incomplete: $(head -3 "$work/incompleteresult.err")"
echo "ok   a call whose result type is a class declared only is an error"

# Diagnostics (compiler/cc/tests/cpp/diagnostics): each NAME.cpp is
# refused with the message of NAME.error, its types spelled as C++ writes
# them.
diagnosed=()
for source in "$tests"/diagnostics/*.cpp; do
    name=$(basename "$source" .cpp)
    (cd "$tests/diagnostics" && "$cc" -std=c++20 -c -o /dev/null "$name.cpp") 2> "$work/$name.err" && fail "$name compiled"
    grep -qxF "$(cat "$tests/diagnostics/$name.error")" "$work/$name.err" || fail "$name says otherwise: $(head -3 "$work/$name.err")"
    diagnosed+=("$name")
done
echo "ok   ${#diagnosed[@]} programs refused with their messages: ${diagnosed[*]}"

# What an object holds: the inline, static and template functions and the
# vtables its code or data refers to, not those only named by functions
# never made (inlinereach.cpp). (Without inlining: with it, doubled is
# lowered into helper and rightly left out, as gcc -O2 leaves it out.)
"$cc" -fno-inline -c -o "$work/inlinereach.o" "$tests/inlinereach.cpp" 2> "$work/reach.err" || fail "-c of inlinereach.cpp: $(head -3 "$work/reach.err")"
symbols=$(nm "$work/inlinereach.o" | awk '{ print $NF }' | sort | tr '\n' ' ')
for name in _Z7tripledi _Z12unusedInlinei _ZTV6Unused _ZN6Unused5valueEv; do
    [[ " $symbols" != *" $name "* ]] || fail "inlinereach.o has $name, which nothing made refers to"
done
for name in _ZTV4Used _ZN4Used5valueEv _Z7doubledi _Z9quarteredi _Z6halvedIiEi; do
    [[ " $symbols" == *" $name "* ]] || fail "inlinereach.o lacks $name"
done
echo "ok   an object holds the inline functions and vtables its code and data refer to, no others"

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

# Lambdas of inline functions, templates, default member initializers and
# inline variables, which two files read in different orders: one class
# each for the program (a static local of its call operator one object,
# a template's instance for it one instance), named as g++ names it.
# (Without inlining, so that the objects hold the lambdas' call operators
# the symbols are read for below.)
"$cc" -fno-inline -c -o "$work/lambdaodr_a.o" "$tests/lambdaodr/a.cpp" 2> "$work/lambdaodr.err" || fail "-c of lambdaodr/a.cpp: $(head -3 "$work/lambdaodr.err")"
"$cc" -fno-inline -c -o "$work/lambdaodr_b.o" "$tests/lambdaodr/b.cpp" 2>> "$work/lambdaodr.err" || fail "-c of lambdaodr/b.cpp: $(head -3 "$work/lambdaodr.err")"
"$cc" -x c++ -o "$work/lambdaodr" "$work/lambdaodr_a.o" "$work/lambdaodr_b.o" 2>> "$work/lambdaodr.err" || fail "linking lambdaodr: $(head -3 "$work/lambdaodr.err")"
"$work/lambdaodr" > "$work/lambdaodr.out" || fail "lambdaodr exits with $?"
cmp -s "$tests/lambdaodr/expected" "$work/lambdaodr.out" || fail "lambdaodr prints otherwise (expected < > got): $(diff "$tests/lambdaodr/expected" "$work/lambdaodr.out" | head -6)"
# A static data member defined constexpr outside its class, in a header
# included by two files: inline, one object (constexprmemberunits).
"$cc" -c -o "$work/constexprmemberunits_a.o" "$tests/constexprmemberunits/a.cpp" 2> "$work/constexprmemberunits.err" || fail "-c of constexprmemberunits/a.cpp: $(head -3 "$work/constexprmemberunits.err")"
"$cc" -c -o "$work/constexprmemberunits_b.o" "$tests/constexprmemberunits/b.cpp" 2>> "$work/constexprmemberunits.err" || fail "-c of constexprmemberunits/b.cpp: $(head -3 "$work/constexprmemberunits.err")"
"$cc" -x c++ -o "$work/constexprmemberunits" "$work/constexprmemberunits_a.o" "$work/constexprmemberunits_b.o" 2>> "$work/constexprmemberunits.err" || fail "linking constexprmemberunits: $(head -3 "$work/constexprmemberunits.err")"
"$work/constexprmemberunits" > "$work/constexprmemberunits.out" || fail "constexprmemberunits exits with $?"
cmp -s "$tests/constexprmemberunits/expected" "$work/constexprmemberunits.out" || fail "constexprmemberunits prints otherwise (expected < > got): $(diff "$tests/constexprmemberunits/expected" "$work/constexprmemberunits.out" | head -6)"
symbols=$(nm "$work/lambdaodr_b.o" | awk '{ print $NF }' | sort | tr '\n' ' ')
for name in _ZZ7countervENKUlvE_clEv _ZZZ7countervENKUlvE_clEvE1n _ZZZZ13nestedCountervENKUlvE_clEvENKUlvE_clEvE1s _ZZZ14mutableCountervENUlvE_clEvE1n _ZNK6Widget4makeMUlvE_clEv _ZNK12sharedLambdaMUlvE_clEv _ZZZ21counterThroughPointervENKUlvE_clEvE1n; do
    [[ " $symbols" == *" $name "* ]] || fail "lambdaodr_b.o lacks $name"
done
echo "ok   the lambdas of inline functions and initializers are one class across files, named as g++ names them"

# Explicit instantiation definitions make what they name, weak, though
# nothing calls it (explicitinstance.cpp: a member function template's
# instances, a class instance's members, its member class's, its static
# data member; not its member templates nor what `extern template`
# names); another file calls them (explicitinstanceunits: b.cpp holds the
# definitions, a.cpp only declarations and `extern template`).
"$cc" -c -o "$work/explicitinstance.o" "$tests/explicitinstance.cpp" 2> "$work/explicitinstance.err" || fail "-c of explicitinstance.cpp: $(head -3 "$work/explicitinstance.err")"
nm "$work/explicitinstance.o" | awk '$2 == "W" || $2 == "V" { print $NF }' > "$work/explicitinstance.weak"
for pattern in '5visit.*2V1' '5visit.*2V2' 'C(<3>|ILi3EE)1fEv' 'C(<3>|ILi3EE)1gEv' 'C(<3>|ILi3EE)1sE' 'C(<3>|ILi3EE)5Inner1hEv' '5twiceI.*i' '5twiceI.*d' 'G(<0>|IL4Kind0EE)C1Ei' 'G(<1>|IL4Kind1EE)D1Ev' 'G(<1>|IL4Kind1EE)1fEv' 'G(<0>|IL4Kind0EE)1sEv'; do
    grep -Eq "^_Z.*$pattern" "$work/explicitinstance.weak" || fail "explicitinstance.o lacks a weak $pattern: $(tr '\n' ' ' < "$work/explicitinstance.weak")"
done
for pattern in 'same' 'C(<4>|ILi4EE)' '5twiceI.*l' '1fEi' 'G(<0>|IL4Kind0EE)D1Ev'; do
    ! grep -Eq "$pattern" "$work/explicitinstance.weak" || fail "explicitinstance.o has $pattern: $(tr '\n' ' ' < "$work/explicitinstance.weak")"
done
"$cc" -c -o "$work/explicitinstanceunits_a.o" "$tests/explicitinstanceunits/a.cpp" 2> "$work/explicitinstanceunits.err" || fail "-c of explicitinstanceunits/a.cpp: $(head -3 "$work/explicitinstanceunits.err")"
"$cc" -c -o "$work/explicitinstanceunits_b.o" "$tests/explicitinstanceunits/b.cpp" 2>> "$work/explicitinstanceunits.err" || fail "-c of explicitinstanceunits/b.cpp: $(head -3 "$work/explicitinstanceunits.err")"
"$cc" -x c++ -o "$work/explicitinstanceunits" "$work/explicitinstanceunits_a.o" "$work/explicitinstanceunits_b.o" 2>> "$work/explicitinstanceunits.err" || fail "linking explicitinstanceunits: $(head -3 "$work/explicitinstanceunits.err")"
"$work/explicitinstanceunits" > "$work/explicitinstanceunits.out" || fail "explicitinstanceunits exits with $?"
cmp -s "$tests/explicitinstanceunits/expected" "$work/explicitinstanceunits.out" || fail "explicitinstanceunits prints otherwise (expected < > got): $(diff "$tests/explicitinstanceunits/expected" "$work/explicitinstanceunits.out" | head -6)"
echo "ok   explicit instantiation definitions make the functions and members they name, for other files"

# A WTF::Visitor of 16 lambdas (visitormanylambdas.cpp): the call
# operators' overload set made of each base once, in a few thousand
# allocations, not 2^16 lookups' 1.5 million (css-19's 25 filled the
# arena).
made=$(MLX_CC_STATS=1 "$cc" -c -o "$work/visitormanylambdas.o" "$tests/visitormanylambdas.cpp" 2>&1 | awk '/^  size [0-9]+: / { n += $3 } END { print n + 0 }')
[[ "$made" -gt 0 && "$made" -lt 400000 ]] || fail "visitormanylambdas.cpp takes $made allocations"
echo "ok   a Visitor of many lambdas makes its overload set in $made allocations"

# Unnamed classes a typedef names, in a header two files include after
# different numbers of other unnamed classes: functions taking them have
# the same names in both files.
"$cc" -c -o "$work/typedefunits_a.o" "$tests/typedefunits/a.cpp" 2> "$work/typedefunits.err" || fail "-c of typedefunits/a.cpp: $(head -3 "$work/typedefunits.err")"
"$cc" -c -o "$work/typedefunits_b.o" "$tests/typedefunits/b.cpp" 2>> "$work/typedefunits.err" || fail "-c of typedefunits/b.cpp: $(head -3 "$work/typedefunits.err")"
"$cc" -x c++ -o "$work/typedefunits" "$work/typedefunits_a.o" "$work/typedefunits_b.o" 2>> "$work/typedefunits.err" || fail "linking typedefunits: $(head -3 "$work/typedefunits.err")"
"$work/typedefunits" > "$work/typedefunits.out" || fail "typedefunits exits with $?"
cmp -s "$tests/typedefunits/expected" "$work/typedefunits.out" || fail "typedefunits prints otherwise (expected < > got): $(diff "$tests/typedefunits/expected" "$work/typedefunits.out" | head -6)"
echo "ok   an unnamed class a typedef names has the typedef's name across files"
# Classes with virtual bases shared with code g++ compiled (without RTTI,
# exceptions and sized deallocation, which need g++'s library; vcall
# offsets before vbase offsets among them): each side makes objects of
# its classes and of the other's (through the other's complete-object
# constructors) and reads and writes the other's through their virtual
# bases, whose offsets are where the Itanium ABI puts them.
if command -v g++ > /dev/null; then
    link=$tests/vbaselink
    g++ -std=c++20 -fno-rtti -fno-exceptions -fno-sized-deallocation -fPIE -O1 -c -o "$work/vbaselink_gpp.o" "$link/gpp.cpp" 2> "$work/vbaselink.err" || fail "g++ does not compile vbaselink/gpp.cpp: $(head -3 "$work/vbaselink.err")"
    "$cc" -c -o "$work/vbaselink_mlx.o" "$link/mlx.cpp" 2>> "$work/vbaselink.err" || fail "-c of vbaselink/mlx.cpp: $(head -3 "$work/vbaselink.err")"
    "$cc" -x c++ -o "$work/vbaselink" "$work/vbaselink_mlx.o" "$work/vbaselink_gpp.o" 2>> "$work/vbaselink.err" || fail "linking vbaselink: $(head -3 "$work/vbaselink.err")"
    "$work/vbaselink" > "$work/vbaselink.out" 2>&1 || fail "vbaselink exits with $?: $(tail -3 "$work/vbaselink.out")"
    cmp -s "$link/expected" "$work/vbaselink.out" || fail "vbaselink prints otherwise (expected < > got): $(diff "$link/expected" "$work/vbaselink.out" | head -6)"
    echo "ok   classes with virtual bases work across objects of g++'s and ours"
    # Classes returned by value across objects of g++'s and ours: one not
    # trivially copyable in memory whatever its size, a small trivially
    # copyable one in registers (returnlink).
    link=$tests/returnlink
    g++ -std=c++20 -fno-rtti -fno-exceptions -fno-sized-deallocation -fPIE -O1 -c -o "$work/returnlink_gpp.o" "$link/gpp.cpp" 2> "$work/returnlink.err" || fail "g++ does not compile returnlink/gpp.cpp: $(head -3 "$work/returnlink.err")"
    "$cc" -c -o "$work/returnlink_mlx.o" "$link/mlx.cpp" 2>> "$work/returnlink.err" || fail "-c of returnlink/mlx.cpp: $(head -3 "$work/returnlink.err")"
    "$cc" -x c++ -o "$work/returnlink" "$work/returnlink_mlx.o" "$work/returnlink_gpp.o" 2>> "$work/returnlink.err" || fail "linking returnlink: $(head -3 "$work/returnlink.err")"
    "$work/returnlink" > "$work/returnlink.out" 2>&1 || fail "returnlink exits with $?: $(tail -3 "$work/returnlink.out")"
    cmp -s "$link/expected" "$work/returnlink.out" || fail "returnlink prints otherwise (expected < > got): $(diff "$link/expected" "$work/returnlink.out" | head -6)"
    echo "ok   classes returned by value across objects of g++'s and ours, as the ABI passes them"
fi

# Prefix snapshots (compiler/cc/snapshot.mlx): the files of each test of
# two files above, their common header given by -include (the #include of
# it taken out), compiled from a snapshot of the header and without one:
# the objects are the same, byte for byte, and the program runs as
# before. Then a snapshot whose header changed, whose include search
# would find another file, or whose options differ is not taken (the
# compiler says why and reads the prefix itself: the same object again);
# one whose header was only written again is.
mkdir -p "$work/snap"
for pair in shared:shared.h lambdaunits:units.h lambdaodr:lambdas.h constexprmemberunits:units.h explicitinstanceunits:instances.h typedefunits:units.h; do
    dir=${pair%%:*} header=${pair#*:}
    s="$work/snap/$dir"
    mkdir -p "$s"
    "$cc" --write-prefix-snapshot "$s/prefix.snap" -include "$tests/$dir/$header" -x c++ 2> "$s/write.err" || fail "$dir: the snapshot of $header is not written: $(head -3 "$s/write.err")"
    for unit in a b; do
        sed "s|^#include \"$header\"\$||" "$tests/$dir/$unit.cpp" > "$s/$unit.cpp"
        "$cc" -include "$tests/$dir/$header" -c -o "$s/$unit.plain.o" "$s/$unit.cpp" 2> "$s/$unit.err" || fail "$dir/$unit.cpp with -include: $(head -3 "$s/$unit.err")"
        "$cc" --prefix-snapshot "$s/prefix.snap" -include "$tests/$dir/$header" -c -o "$s/$unit.o" "$s/$unit.cpp" 2> "$s/$unit.err" || fail "$dir/$unit.cpp from the snapshot: $(head -3 "$s/$unit.err")"
        [[ ! -s "$s/$unit.err" ]] || fail "$dir/$unit.cpp from the snapshot: the compiler said: $(head -3 "$s/$unit.err")"
        cmp -s "$s/$unit.plain.o" "$s/$unit.o" || fail "$dir/$unit.cpp: the object made from the snapshot differs"
    done
    "$cc" -x c++ -o "$s/program" "$s/a.o" "$s/b.o" 2> "$s/link.err" || fail "linking $dir from the snapshot: $(head -3 "$s/link.err")"
    "$s/program" > "$s/out" 2>&1 || fail "$dir from the snapshot exits with $?"
    cmp -s "$tests/$dir/expected" "$s/out" || fail "$dir from the snapshot prints otherwise (expected < > got): $(diff "$tests/$dir/expected" "$s/out" | head -6)"
done
# Programs of one file cut before main: what precedes it (classes with
# virtual and deleting destructors, explicit instantiations, generic
# lambdas, member templates) as the prefix.
for name in deletingdestructor explicitinstance genericlambdatypes lambdamembertemplate; do
    s="$work/snap/$name"
    mkdir -p "$s"
    line=$(grep -n '^int main' "$tests/$name.cpp" | head -1 | cut -d: -f1)
    head -n $((line - 1)) "$tests/$name.cpp" > "$s/prefix.h"
    tail -n +"$line" "$tests/$name.cpp" > "$s/unit.cpp"
    "$cc" --write-prefix-snapshot "$s/prefix.snap" -include "$s/prefix.h" -x c++ 2> "$s/write.err" || fail "$name: the snapshot of its prefix is not written: $(head -3 "$s/write.err")"
    "$cc" -include "$s/prefix.h" -c -o "$s/plain.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "$name with -include: $(head -3 "$s/unit.err")"
    "$cc" --prefix-snapshot "$s/prefix.snap" -include "$s/prefix.h" -c -o "$s/unit.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "$name from the snapshot: $(head -3 "$s/unit.err")"
    [[ ! -s "$s/unit.err" ]] || fail "$name from the snapshot: the compiler said: $(head -3 "$s/unit.err")"
    cmp -s "$s/plain.o" "$s/unit.o" || fail "$name: the object made from the snapshot differs"
    "$cc" -x c++ -o "$s/program" "$s/unit.o" 2> "$s/link.err" || fail "linking $name from the snapshot: $(head -3 "$s/link.err")"
    run "$name" "$s/program" "$s/out"
    cmp -s "$tests/$name.expected" "$s/out" || fail "$name from the snapshot prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$s/out" | head -6)"
done
echo "ok   prefix snapshots: the objects of 6 tests of two files and of 4 cut before main are those made without one"
s="$work/snap/stale"
mkdir -p "$s/first" "$s/second"
printf '#pragma once\nstruct Dep { int v = 7; };\n' > "$s/second/dep.h"
printf '#pragma once\n#include <dep.h>\n#include "local.h"\ninline int prefixed() { return Dep().v + LOCAL; }\n' > "$s/prefix.h"
printf '#pragma once\n#define LOCAL 1\n' > "$s/local.h"
printf '#include "prefix.h"\nint use() { return prefixed(); }\n' > "$s/unit.cpp"
stale() { # WHAT: the snapshot not taken, the object as without it
    "$cc" -I "$s/first" -I "$s/second" -include "$s/prefix.h" -c -o "$s/plain.o" "$s/unit.cpp" 2> "$s/plain.err" || fail "stale snapshot ($1): $(head -3 "$s/plain.err")"
    "$cc" --prefix-snapshot "$s/prefix.snap" -I "$s/first" -I "$s/second" -include "$s/prefix.h" -c -o "$s/unit.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "stale snapshot ($1): $(head -3 "$s/unit.err")"
    grep -q "not used ($2" "$s/unit.err" || fail "a snapshot is taken though $1: $(head -3 "$s/unit.err")"
    cmp -s "$s/plain.o" "$s/unit.o" || fail "stale snapshot ($1): the object differs"
}
write() {
    "$cc" --write-prefix-snapshot "$s/prefix.snap" -I "$s/first" -I "$s/second" -include "$s/prefix.h" -x c++ 2> "$s/write.err" || fail "the snapshot of prefix.h: $(head -3 "$s/write.err")"
}
write
cp "$s/second/dep.h" "$s/dep.h.new" && mv "$s/dep.h.new" "$s/second/dep.h"
"$cc" --prefix-snapshot "$s/prefix.snap" -I "$s/first" -I "$s/second" -include "$s/prefix.h" -c -o "$s/unit.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "a snapshot whose header was written again: $(head -3 "$s/unit.err")"
[[ ! -s "$s/unit.err" ]] || fail "a snapshot whose header was only written again is not taken: $(head -3 "$s/unit.err")"
printf '#pragma once\nstruct Dep { int v = 8; };\n' > "$s/second/dep.h"
stale "a header changed" "$s/second/dep.h changed"
write
printf '#pragma once\nstruct Dep { int v = 9; };\n' > "$s/first/dep.h"
stale "the search finds another header" "$s/first/dep.h is new"
rm "$s/first/dep.h"
write
printf '#pragma once\n#define LOCAL 2\n' > "$s/second/local.h"
"$cc" --prefix-snapshot "$s/prefix.snap" -I "$s/first" -I "$s/second" -include "$s/prefix.h" -c -o "$s/unit.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "a snapshot with a header beside it in a later directory: $(head -3 "$s/unit.err")"
[[ ! -s "$s/unit.err" ]] || fail "a snapshot is not taken for a header the search never reaches: $(head -3 "$s/unit.err")"
"$cc" --prefix-snapshot "$s/prefix.snap" -DOTHER -I "$s/first" -I "$s/second" -include "$s/prefix.h" -c -o "$s/unit.o" "$s/unit.cpp" 2> "$s/unit.err" || fail "a snapshot of other options: $(head -3 "$s/unit.err")"
grep -q "not used (another command line" "$s/unit.err" || fail "a snapshot of other options is taken: $(head -3 "$s/unit.err")"
echo "ok   prefix snapshots: not taken when a header changed, the include search would find another or the options differ"

# The same programs on mlxlibc alone.
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/lib" > /dev/null
lib="$work/lib/libmlxc.so.1"
for name in "${programs[@]}"; do
    run "$name" "$work/$name" "$work/$name.mlxlibc" "$lib"
    cmp -s "$tests/$name.expected" "$work/$name.mlxlibc" || fail "$name on mlxlibc prints otherwise (expected < > got): $(diff "$tests/$name.expected" "$work/$name.mlxlibc" | head -12)"
done
echo "ok   the ${#programs[@]} programs run alike on mlxlibc as loader and C library"

echo "check_cpp.sh: all checks passed"
