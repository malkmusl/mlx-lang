#!/usr/bin/env bash
# Lattice L0 (projects/webkit/LATTICE.md): mlx-cc's C++ to Mlx translator.
# Builds mlx-cc from the sources, translates the pilot units of ANGLE's
# common/ with `--emit-mlx` and compares the output with the files kept in
# projects/lattice/angle/common (regenerate them with --update), compiles
# the translated modules with mlx4 and runs their tests: ANGLE's
# aligned_memory test as Mlx, and the mathutil differential test (the Mlx
# module and the C++ built by mlx-cc print the same lines for the same
# inputs). Needs mlx-out/bin/compiler/mlx4 and the WebKit checkout
# (projects/webkit/upstream).
#
#   tools/check_lattice_emit.sh [--update]
set -u
cd "$(dirname "$0")/.."

update=0
[[ ${1:-} == --update ]] && update=1
compiler=mlx-out/bin/compiler/mlx4
[[ -x "$compiler" ]] || { echo "check_lattice_emit.sh: $compiler is missing (run tools/bootstrap.sh)"; exit 1; }
angle=projects/webkit/upstream/Source/ThirdParty/ANGLE
[[ -d "$angle/src/common" ]] || { echo "check_lattice_emit.sh: $angle is missing (tools/fetch_webkit.sh)"; exit 1; }
out=projects/lattice/angle/common

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "FAIL $1"; exit 1; }

"$compiler" --quiet compiler/cc/main.mlx -o "$work/mlx-cc" > "$work/cc.log" 2>&1 || fail "mlx-cc does not build: $(tail -5 "$work/cc.log")"
export MLX_CC_INCLUDE="$PWD/compiler/cc/include"
export MLX_CC_LIB="$PWD/mlx-out/lib"
[[ -x mlx-out/bin/mlx-ld ]] || { echo "check_lattice_emit.sh: mlx-out/bin/mlx-ld is missing (./mlx build mlx-ld)"; exit 1; }
export MLX_LD="$PWD/mlx-out/bin/mlx-ld"
echo "ok   mlx-cc builds"
# The C++ references run on mlxlibc (loader, C library and libm from
# compiler/libc): its libm wraps the same std.math the Mlx side calls, so
# the two sides' sines, cosines and tangents agree bit for bit.
tools/build_mlxlibc.sh --compiler "$compiler" --out "$work/libc" > "$work/libc.log" 2>&1 || fail "mlxlibc does not build: $(tail -5 "$work/libc.log")"
loader="$work/libc/libmlxc.so.1"
echo "ok   mlxlibc builds (the references run on it)"

# ANGLE's flags for the Vulkan backend on Linux, as projects/webkit/build.mlx compiles it.
flags=(-DANGLE_ENABLE_ESSL -DANGLE_ENABLE_GLSL -DANGLE_ENABLE_VULKAN -DANGLE_PLATFORM_LINUX
    -DANGLE_ENABLE_SHARE_CONTEXT_LOCK=1 -DANGLE_ENABLE_CONTEXT_MUTEX=1 -DEGL_EGL_PROTOTYPES=0
    -DEGL_NO_PLATFORM_SPECIFIC_TYPES -DGL_GLES_PROTOTYPES=0 -DNDEBUG -DLIBANGLE_IMPLEMENTATION
    -I"$angle/WebKit" -I"$angle/include" -I"$angle/include/KHR" -I"$angle/src" -I"$angle/src/common/base"
    -I"$angle/src/common/third_party/xxhash" -I"$angle/third_party/zlib/google"
    -Imlx-out/include/mlx-zlib -Imlx-out/include/mlx-vulkan -Imlx-out/include/mlx-spirv
    -std=c++23 -include "$angle/ANGLEPrefix.h")

# The C++ library's string (mlxlibcpp, compiler/libcpp/string.cpp) as the
# std module the translations call (projects/lattice/runtime/libcpp.mlx).
runtime=projects/lattice/runtime
"$work/mlx-cc" -std=c++23 --emit-mlx-runtime=./c.mlx --emit-mlx="$work/libcpp.mlx" compiler/libcpp/string.cpp 2> "$work/libcpp.err" || fail "string.cpp does not translate: $(head -5 "$work/libcpp.err")"
if [[ $update -eq 1 ]]; then
    cp "$work/libcpp.mlx" "$runtime/libcpp.mlx"
elif ! cmp -s "$work/libcpp.mlx" "$runtime/libcpp.mlx"; then
    fail "$runtime/libcpp.mlx is not what the translator writes now (tools/check_lattice_emit.sh --update): $(diff "$runtime/libcpp.mlx" "$work/libcpp.mlx" | head -12)"
fi
echo "ok   mlxlibcpp's string translated ($(grep -c '^pub fn\|^fn' "$runtime/libcpp.mlx") functions, $(grep -c 'LATTICE:' "$runtime/libcpp.mlx") marks for the hand)"

units=(aligned_memory mathutil Float16ToFloat32 utilities matrix_utils MemoryBuffer platform_helpers string_utils)
unit_list=$(IFS=,; echo "${units[*]}")
for unit in "${units[@]}"; do
    "$work/mlx-cc" "${flags[@]}" --emit-mlx-units="$unit_list" --emit-mlx-std=../../runtime/libcpp.mlx --emit-mlx-std-file="$work/libcpp.mlx" --emit-mlx="$work/$unit.mlx" "$angle/src/common/$unit.cpp" 2> "$work/$unit.err" || fail "$unit.cpp does not translate: $(head -5 "$work/$unit.err")"
    if [[ $update -eq 1 ]]; then
        cp "$work/$unit.mlx" "$out/$unit.mlx"
    elif ! cmp -s "$work/$unit.mlx" "$out/$unit.mlx"; then
        fail "$out/$unit.mlx is not what the translator writes now (tools/check_lattice_emit.sh --update): $(diff "$out/$unit.mlx" "$work/$unit.mlx" | head -12)"
    fi
    marks=$(grep -c '^// LATTICE:\|// LATTICE: ' "$out/$unit.mlx" || true)
    externs=$(grep -c '^extern("c")' "$out/$unit.mlx" || true)
    echo "ok   $unit.cpp translated ($(grep -c '^pub fn\|^fn' "$out/$unit.mlx") functions, $marks marks for the hand, $externs C externs)"
done

# ANGLE's aligned_memory test (aligned_memory_unittest.cpp) as Mlx on the translated module.
"$compiler" --quiet "$out/tests/aligned_memory_test.mlx" -o "$work/aligned_test" 2> "$work/aligned.err" || fail "aligned_memory_test.mlx does not compile: $(head -5 "$work/aligned.err")"
"$work/aligned_test" > "$work/aligned.out" 2>&1 || fail "the aligned_memory test: $(cat "$work/aligned.out")"
echo "ok   $(cat "$work/aligned.out")"

# A differential test: the C++ reference (built by mlx-cc against the C++
# unit) and the Mlx driver (on the translated module) print the same.
differential() { # UNIT TEST WORDS [OTHER C++ UNITS THE REFERENCE LINKS...]
    local unit=$1 test=$2 words=$3
    shift 3
    local others=()
    local other
    for other in "$@"; do others+=("$angle/src/common/$other.cpp"); done
    "$work/mlx-cc" "${flags[@]}" -o "$work/${test}_ref" "$out/tests/${test}_ref.cpp" "$angle/src/common/$unit.cpp" "${others[@]}" 2> "$work/${test}_ref.err" || fail "$test: the C++ reference does not build: $(head -5 "$work/${test}_ref.err")"
    (env -i LD_LIBRARY_PATH="$work/libc" timeout 60 "$loader" "$work/${test}_ref" > "$work/${test}_ref.out" 2> "$work/${test}_ref.run") || fail "$test: the C++ reference does not run on mlxlibc: $(head -3 "$work/${test}_ref.run")"
    "$compiler" --quiet "$out/tests/${test}_test.mlx" -o "$work/${test}_test" 2> "$work/${test}.err" || fail "${test}_test.mlx does not compile: $(head -5 "$work/${test}.err")"
    "$work/${test}_test" > "$work/${test}_mlx.out" || fail "$test: the Mlx test does not run"
    cmp -s "$work/${test}_ref.out" "$work/${test}_mlx.out" || fail "$test: the Mlx and the C++ differ (C++ < > Mlx): $(diff "$work/${test}_ref.out" "$work/${test}_mlx.out" | head -12)"
    echo "ok   $unit: $words, $(wc -l < "$work/${test}_ref.out") lines the same in Mlx and C++"
}
differential mathutil mathutil "RGB9E5 packing and unpacking"
differential Float16ToFloat32 float16 "every half-precision pattern to a float"
differential utilities utilities "the GL type and target predicates, sizes, counts and orders over the enum range" string_utils
differential matrix_utils matrix "Mat4 made, rotated, translated, scaled, frustum, perspective, ortho, products with a matrix and a vector"
differential MemoryBuffer memorybuffer "a buffer resized, reserved, filled, cleared and destroyed"
differential platform_helpers platform "the platform predicates"
differential string_utils string_utils "splitting, trimming, prefixes, affixes, tokens, case, replacement and wildcards through the std module"

# The translator's own tests (projects/lattice/tests): small C++ files,
# each with what one feature of C++ needs of the translation, translated,
# kept, compiled and run against the C++.
feature() { # DIRECTORY NAME WORDS
    local directory=projects/lattice/tests/$1 name=$2 words=$3
    "$work/mlx-cc" -std=c++23 --emit-mlx-runtime=../../runtime/c.mlx --emit-mlx-std=../../runtime/libcpp.mlx --emit-mlx-std-file="$work/libcpp.mlx" --emit-mlx="$work/$name.mlx" "$directory/$name.cpp" 2> "$work/$name.err" || fail "$name.cpp does not translate: $(head -5 "$work/$name.err")"
    if [[ $update -eq 1 ]]; then
        cp "$work/$name.mlx" "$directory/$name.mlx"
    elif ! cmp -s "$work/$name.mlx" "$directory/$name.mlx"; then
        fail "$directory/$name.mlx is not what the translator writes now (tools/check_lattice_emit.sh --update): $(diff "$directory/$name.mlx" "$work/$name.mlx" | head -12)"
    fi
    "$work/mlx-cc" -std=c++23 -o "$work/${name}_ref" "$directory/${name}_ref.cpp" "$directory/$name.cpp" 2> "$work/${name}_ref.err" || fail "$name: the C++ reference does not build: $(head -5 "$work/${name}_ref.err")"
    (env -i LD_LIBRARY_PATH="$work/libc" timeout 60 "$loader" "$work/${name}_ref" > "$work/${name}_ref.out" 2> "$work/${name}_ref.run") || fail "$name: the C++ reference does not run on mlxlibc: $(head -3 "$work/${name}_ref.run")"
    "$compiler" --quiet "$directory/${name}_test.mlx" -o "$work/${name}_test" 2> "$work/${name}.err" || fail "${name}_test.mlx does not compile: $(head -5 "$work/${name}.err")"
    "$work/${name}_test" > "$work/${name}_mlx.out" || fail "$name: the Mlx test does not run"
    cmp -s "$work/${name}_ref.out" "$work/${name}_mlx.out" || fail "$name: the Mlx and the C++ differ (C++ < > Mlx): $(diff "$work/${name}_ref.out" "$work/${name}_mlx.out" | head -12)"
    echo "ok   $name: $words, the same in Mlx and C++ ($(tr '\n' ' ' < "$work/${name}_ref.out"))"
}
feature virtual shapes "virtual functions: overriding, a pure virtual, deletion through the base, a second base's adjustment of this"
feature bits bits "bit-fields in words of their own, an anonymous union, two bases with table pointers"
feature idioms idioms "pointers stepped through members, std::array members, same-named static locals, a class beside a function of its name, references from pointers, an object in storage, an atomic pointer"
feature values values "a class with a copy constructor taken by value, an empty base, a base assigned through the derived object, a reference from a virtual call"
feature overflow overflow "the overflow builtins over 32 and 64 bits, signed and unsigned, and with operands and a result of different types"
feature variadic variadic "functions with variable arguments: va_start, va_arg, va_copy, a va_list handed on, a method, and the printf family of the runtime"
feature system system "the system by number as foreign code calls it: a futex wait and wake, fcntl, and sscanf"
feature jumps jumps "goto: labels at a function's top (a parser's loop) and in a loop's body (a scanner's), a goto out of two loops; a switch whose cases run on and one with a break inside an if; continue and break across them"
feature hides hides "a member with the name of a base class's member, twice over: both fields of the record, each reached by its class"
feature members members "pointers to member functions in a table, called through this and through a reference, a virtual one, a pointer to a data member; a derived object where its base is taken"
feature cnames cnames "C's struct stat beside stat(), sigaction, std::copysign beside C's copysign: the names kept apart"
feature casts casts "a cast that keeps the element under an address taken again, an array's storage as bytes"

# A program of several units translated as one (compiler/cc/emitmlx.mlx,
# "One program of many units"): each unit lists what it defines, the lists
# make the manifest (a thing's home, its name), each unit is translated
# with it. The modules are kept beside their C++; the test module calls
# the main unit's `run`, as the C++ reference does.
program() { # DIRECTORY "UNITS" WORDS [c: the units are C]
    local name=$1 directory=projects/lattice/tests/$1 units=$2 words=$3 unit sources=()
    local ext=cpp standard=c++23
    if [[ ${4:-} == c ]]; then ext=c; standard=c11; fi
    : > "$work/$name.units"
    for unit in $units; do
        "$work/mlx-cc" -std=$standard --emit-mlx-runtime=../../runtime/c.mlx --emit-mlx-unit="$unit" --emit-mlx-index="$work/$name.$unit.idx" --emit-mlx=/dev/null "$directory/$unit.$ext" 2> "$work/$name.$unit.err" || fail "$name: $unit.$ext does not translate: $(head -5 "$work/$name.$unit.err")"
        printf '%s\t%s\n' "$unit" "$work/$name.$unit.idx" >> "$work/$name.units"
        sources+=("$directory/$unit.$ext")
    done
    "$work/mlx-cc" --lattice-units="$work/$name.units" --lattice-manifest="$work/$name.manifest" > "$work/$name.manifest.log" 2>&1 || fail "$name: the manifest was not made: $(head -3 "$work/$name.manifest.log")"
    for unit in $units; do
        "$work/mlx-cc" -std=$standard --emit-mlx-runtime=../../runtime/c.mlx --emit-mlx-unit="$unit" --emit-mlx-manifest="$work/$name.manifest" --emit-mlx="$work/$name.$unit.mlx" "$directory/$unit.$ext" 2> "$work/$name.$unit.err" || fail "$name: $unit.$ext does not translate with the manifest: $(head -5 "$work/$name.$unit.err")"
        if [[ $update -eq 1 ]]; then
            cp "$work/$name.$unit.mlx" "$directory/$unit.mlx"
        elif ! cmp -s "$work/$name.$unit.mlx" "$directory/$unit.mlx"; then
            fail "$directory/$unit.mlx is not what the translator writes now (tools/check_lattice_emit.sh --update): $(diff "$directory/$unit.mlx" "$work/$name.$unit.mlx" | head -12)"
        fi
    done
    "$work/mlx-cc" -std=$standard -o "$work/${name}_ref" "$directory/${name}_ref.$ext" "${sources[@]}" 2> "$work/${name}_ref.err" || fail "$name: the C++ reference does not build: $(head -5 "$work/${name}_ref.err")"
    (env -i LD_LIBRARY_PATH="$work/libc" timeout 60 "$loader" "$work/${name}_ref" > "$work/${name}_ref.out" 2> "$work/${name}_ref.run") || fail "$name: the C++ reference does not run on mlxlibc: $(head -3 "$work/${name}_ref.run")"
    "$compiler" --quiet "$directory/${name}_test.mlx" -o "$work/${name}_test" 2> "$work/${name}.err" || fail "${name}_test.mlx does not compile: $(head -5 "$work/${name}.err")"
    "$work/${name}_test" > "$work/${name}_mlx.out" || fail "$name: the Mlx test does not run"
    cmp -s "$work/${name}_ref.out" "$work/${name}_mlx.out" || fail "$name: the Mlx and the C++ differ (C++ < > Mlx): $(diff "$work/${name}_ref.out" "$work/${name}_mlx.out" | head -12)"
    echo "ok   $name: $words, the same in Mlx and C++ ($(tr '\n' ' ' < "$work/${name}_ref.out"))"
}

program library "library" "the library's classes as records: map and set, shared_ptr, mutex, optional, a vector from a braced list, sort, lambdas, function"
program program "main shapes" "two units as one program: a header's record, template instances and inline function in one home, a class made in one unit and deleted in the other"
program incomplete "user maker" "records only declared in one unit and defined in the other: references to them kept, returned and handed on"
program lists "lists" "an initializer_list of records, an array of records as a pointer, a vector of pairs and a map from braced lists, a table's rows, references to arrays of char"
program statics "statics" "objects made before main: constant ones as data (bit-fields in their words, a pointer into an array, a table of pointers to them, an empty record), one with a constructor through the unit's start, static locals behind their guards"
program casts2 "casts2" "const cast away, a function kept as a data pointer and called again, a std::array by reference, this handed to a template by reference"
program shapes "shapes" "operators outside a class and an assignment through a converting constructor, a parameter named as a class, the address of one of two strings, arrays or variables, one object's address indexed, an array of arrays as its bytes, const dropped below the first pointer, the library's math of an integer, a function pointer taking a function pointer, a temporary moved into an element, a base class's part returned by reference, a constant pointer by reference, a pointer made of an integer"
program cshapes "cshapes" "a C unit: comparisons used as numbers, a pointer stepped back more than once and read, const void pointers, a parameter named align" c
program twins "first second" "two units each with a record of its own under one name: two records, each in its unit"
echo "check_lattice_emit.sh: all checks passed"
