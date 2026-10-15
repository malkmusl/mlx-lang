# mlx-cc

A C compiler written in Mlx, for MLX/Linux without GNU (see
[docs/ROADMAP.md](../../docs/ROADMAP.md), phase 1). It reads C from C89
to C23 (`-std=`, GNU variants included), preprocesses it, and writes
x86_64 ELF itself through the Mlx compiler's own encoder, object writer
and linker (`compiler/selfhost/backend/x86_64/encoder.mlx`,
`compiler/selfhost/object/relocatable.mlx`, `link.mlx`,
`elf64_dynamic.mlx`): there is no assembler and no GNU linker in
between. The programs call the C library through `libc.so.6`, which is
mlxlibc (`projects/libc`) on MLX/Linux and glibc elsewhere.

    mlx-out/bin/compiler/mlx4 projects/cc/main.mlx -o mlx-cc
    MLX_CC_INCLUDE=projects/cc/include ./mlx-cc -o hello hello.c
    ./hello                                   # on the system's loader
    mlx-out/libc/libmlxc.so.1 ./hello         # on mlxlibc alone
    ./mlx-cc --dynamic-linker /usr/lib/libmlxc.so.1 -o hello hello.c
    ./mlx-cc -c lib1.c lib2.c                 # lib1.o, lib2.o
    ./mlx-ar rcs libmine.a lib1.o lib2.o
    ./mlx-cc -o hello hello.c -L. -lmine      # or hello.o libmine.a

    mlx-cc [-o OUT] [-std=VARIANT] [-E] [-c] [-I DIR] [-D NAME[=VALUE]] [-U NAME]
           [-L DIR] [-lNAME] [--dynamic-linker PATH] FILE.c... [FILE.o...] [LIB.a...]
    mlx-ar rcs LIB.a FILE.o...  |  mlx-ar t LIB.a  |  mlx-ar x LIB.a

The `.c` files given together are compiled as one, each a translation
unit of its own (its own macros and scope), their names linked by name,
into one relocatable object. With `-c` the object is written (`FILE.o`
in the working directory, or `-o`'s name for a single file); otherwise
it is linked with the `.o` and `.a` files given (an archive's members
as the program needs them) into the executable. The objects are
ordinary ELF relocatable files and the archives `ar`'s: `mlx-ar`
(`ar.mlx`) makes and lists them, GNU `ar` reads them, and objects an
ordinary C compiler made with `-fPIE` link too, in either direction
(`tools/check_link.sh`). `-lNAME` takes `libNAME.a` from a `-L`
directory when there is one, else the shared `libNAME.so`; `-lm` is
`libm.so.6` (glibc's, or the one built next to mlxlibc) and
`-lpthread`, `-ldl` and `-lrt` are part of the C library. `-E` writes
the preprocessed tokens instead. Options of other compilers that change
nothing here (`-O2`, `-g`, `-Wall`, `-fPIC`, `-pthread` ...) are
accepted silently.

## The C variants

`-std=` names the C variant (`standard.mlx`), as gcc spells them: `c89`
(`c90`, `iso9899:1990`, `-ansi`), `c99`, `c11`, `c17` (`c18`), `c23`
(`c2x`), and `gnu89` to `gnu23` with GNU's extensions; the default is
gnu17, gcc's. The variant decides:

- the predefined macros: `__STDC_VERSION__` (none for C90, `199901L`,
  `201112L`, `201710L`, `202311L`), `__STRICT_ANSI__` in the strict
  variants, `linux` and `unix` in the GNU ones;
- the keywords: `inline` and `restrict` from C99 (`inline` in GNU C90
  too), `typeof` and `asm` in GNU C (`__typeof__`, `__asm__` always),
  and C23's `bool`, `true`, `false`, `nullptr`, `static_assert`,
  `alignas`, `alignof`, `thread_local`, `typeof_unqual` and `constexpr`
  (the headers follow: `stdbool.h`, `stdalign.h` and `assert.h` define
  nothing that is a keyword);
- C23's digit separators (`1'000'000`), `true` and `false` in `#if`;
- `//` comments, which ISO C90 refuses (as gcc does).

C23 as programs use it is there whatever the variant allows otherwise:
`[[attributes]]` (passed over) on declarations, parameters, members,
enumerators, labels and statements (`[[fallthrough]];`), `auto` with the
type taken from the initializer, `constexpr` integers (bound like
enumerators, with their type), `enum E : unsigned char` with a fixed
underlying type, empty initializers `{}`, unnamed parameters in
definitions, labels before declarations and at the end of a block,
`#elifdef` and `#elifndef`, `nullptr_t` and `unreachable()` in
`stddef.h`, `_BitInt(N)` for N up to `BITINT_MAXWIDTH` (4096: up to 64
the smallest standard size that holds it, canonical to N bits in
registers; above 64 limbs of 64 bits in memory, aligned to 8 as the
x86_64 ABI lays them out, computed through a C runtime,
`include/__mlxcc_wide.c`, that is compiled into every program using
such a type: every operator, the conversions to and from the other
integers and the floating types, `wb` and `uwb` constants of any width,
wide parameters and results, in registers up to 128 bits and in memory
beyond, globals with constant or computed initializers; no promotion),
`#embed` with `limit`, `prefix`, `suffix` and `if_empty` and
`__has_embed`. Not there: `switch` on a `_BitInt` wider than 64 bits
(its low 64 bits decide), `constexpr` objects of other types than
integers as constants.

## long double

`long double` is the x87 extended format, 80 bits in 16 as on gcc: a
value lives on the x87 stack while an expression holds it (`fld` and
`fstp` to memory, `faddp` and the like between two, `fucomip` for the
comparisons, `fild` and `fisttp` for the conversions, with 2^63 and 2^64
added or taken for the unsigned ones), is passed in memory and returned
in `st0` as the ABI says (`va_arg` reads it from the stack, 16-byte
aligned), and a struct holding one goes in memory too. A constant's
digits are turned into the format by the compiler itself
(`extended.mlx`: 256-bit integer arithmetic, then rounding to 64 bits;
double constants come from the same conversion, which keeps DBL_MAX
finite), and a global's initializer that is not a constant alone (`1.0L
/ 3.0L`) is computed by `__mlxcc_init` when the program starts.
`float.h` has the real limits (`LDBL_MANT_DIG` 64, `LDBL_MAX`
1.19e4932L). `tests/ldouble.c` compares all of it with gcc, and SQLite's
conversion of doubles to text, which computes in `long double`, now
rounds as gcc's build does. A program
names the system's loader (`/lib64/ld-linux-x86-64.so.2`) as its
interpreter; `--dynamic-linker PATH` (also `-Wl,--dynamic-linker=PATH`)
names another, such as mlxlibc's `libmlxc.so.1`, so it starts on that
alone.

## Headers

`include/` holds mlx-cc's own C headers (`stdio.h`, `stdlib.h`,
`string.h`, `stdint.h`, `stdarg.h`, `unistd.h`, `fcntl.h`, `sys/stat.h`,
`math.h`, `pthread.h`, `dlfcn.h`, `signal.h`, `sys/resource.h` and the
rest of what common programs include: what zlib, Lua and SQLite ask
for), written for the x86_64 Linux ABI that glibc and mlxlibc share.
glibc has no `atexit` of its own, so `stdlib.h` makes it `__cxa_atexit`
under a static function. They are found in
`$MLX_CC_INCLUDE`, else in `include/` next to the `mlx-cc` program, else
in `/usr/local/lib/mlx-cc/include`.

## How it works

| Stage | Files |
| --- | --- |
| Memory, names, files, messages | `base.mlx` (one arena, interned names) |
| Preprocessor | `lexer.mlx`, `tokens.mlx`, `macros.mlx`, `ppexpr.mlx`, `paths.mlx`, `preprocess.mlx` |
| Constants and strings | `literal.mlx`, `extended.mlx` (long double constants) |
| Types and syntax tree | `types.mlx`, `ast.mlx` |
| Parser | `cursor.mlx`, `state.mlx`, `scope.mlx`, `decl.mlx`, `initializer.mlx`, `expr.mlx`, `stmt.mlx`, `typing.mlx`, `eval.mlx` |
| Code | `x86.mlx` (instructions), `gen.mlx` (expressions, calls), `genstmt.mlx` (statements, functions) |
| Object and program | `program.mlx` (linking by name, data area, the relocatable object; the program through `compiler/selfhost/object/link.mlx`), `ar.mlx` (mlx-ar) |
| The C variant | `standard.mlx` (`-std=`: the macros, the keywords) |
| Driver | `main.mlx` |

The code generator is a stack machine on rax (xmm0 for floating values),
with every integer kept canonical for its type in the whole register.
Calls follow the System V AMD64 ABI, structs passed and returned by value
included, and variadic functions set up the ABI's register save area so a
`va_list` can go on to `vprintf`. Globals live in the object's `.data`;
a name of another object is a relocation of the standard kinds
(`R_X86_64_PLT32` for a call, `PC32` for an address, `64` for one held
by data), and a unit's long double and wide integer initializers that
are not constants are computed by a function of its own listed in
`.init_array`. The linker (`compiler/selfhost/object/link.mlx`) resolves
the references between the objects, makes a stub and a GOT slot for
every name no object defines (the libraries' functions and variables
such as `stdout` alike), and makes `_start` and `__mlxcc_init`, which
writes the addresses held by data and runs the init functions before
`main(argc, argv, envp)`. Static functions nothing refers to are left
out.

## What is there

C11 as programs use it, and C23 as above: all of the preprocessor (`#`,
`##`, variadic macros, `#include_next`, `__has_include`, `#pragma
once`), every integer
and floating type, pointers and arrays, structs, unions, bit-fields,
anonymous members, flexible array members, enums, typedefs, every
statement and operator, GNU case ranges and statement expressions,
`typeof`, `__attribute__((packed, aligned))`, designated initializers
(GNU ranges too), compound literals, string literals of every kind (`L`,
`u`, `U`, `u8`, universal character names), `_Generic`,
`_Static_assert`, `_Alignas`, `_Alignof`, `__func__`, variadic functions,
static locals, tentative definitions, several files per program.

## C++

A `.cpp` (`.cc`, `.cxx`, `.C`, `-x c++`) file is read as C++ (`-std=c++11`
to `c++23` and `gnu++11` to `gnu++23`, gnu++17 by default, `__cplusplus`
set accordingly), with the C features of C23 under it and the system C
headers wrapped in `extern "C"` automatically. What is there
(`cpp.mlx`, `mangle.mlx`, and the C++ paths of `decl.mlx`, `expr.mlx`,
`stmt.mlx`): namespaces (nested, aliases, `using`, qualified names),
references, function overloading chosen by conversion rank, default
arguments, `bool`, `enum class`, `wchar_t` and the character types,
`extern "C"`, inline functions as weak symbols, `static_cast` and the
other casts, and classes: member functions with `this` (defined in the
class, read after it, or outside it as `T::f`), `const` members as
overloads of their own, constructors (initializer lists, member default
initializers, default arguments, converting constructors used
implicitly, the implicit default and copy constructors member by
member), destructors called when a scope ends (also on `return`,
`break` and `continue`), for temporaries at the end of their
expression, for parameters passed by value, by `delete` and
`delete[]`, static data members and static member functions, nested
classes, enums and typedefs of a class, friends (declared in the class,
defined there too), operator functions as members or free functions
(argument-dependent lookup for the latter: `operator<<` of a stream's
namespace), `new`, `new[]`, placement new, `T(args)` temporaries,
single inheritance with the base as the first member (pointers and
references to a derived class convert to its base), virtual functions
(pure ones too, `override`), virtual destructors, through a vtable
(`_ZTV...`, weak) whose pointer is the object's first word. A returned
local is moved out, not copied; a class returned by value comes back
through the caller's buffer; a global object is constructed when the
program starts (`__mlxcc_init_unit`). Templates (`template.mlx`): class
templates with type and integer parameters (`vector<int>`,
`Box<double, 2>`, nested `List<List<int> >`, the `>>` closing two),
their members defined inside or outside the class, function templates
deduced from the arguments or given them (`max<double>(a, b)`),
overloaded templates, templates in namespaces. A template is kept as
its tokens and read again per instance, with its parameters bound as
typedefs under the instance's prefix (`std::vector<i>`); an instance
made inside a function belongs to the file (the function's state and
scopes are put aside meanwhile); instances are weak, like inline
functions, and named by the arguments' codes (`_Z3maxIiEii`); the
member functions of an instance are read only when first called (so
`vector<T>::resize` needs T's default constructor only if used);
template parameters may have defaults. Also: conversion operators
(`operator bool()`, used in conditions, `!`, `&&`, casts and
arguments), the range-based `for` over arrays and anything with
`begin()` and `end()`, `auto` (with `&` and `*`) from the initializer,
and the left operand of `<<`, `>>` and assignments evaluated before the
right one, as C++17 says; the temporaries of a branch of `&&`, `||` or
`?:` live and die within it. A function and a function template of one
name are overloads of each other (`<string>`'s `operator==` next to
`<vector>`'s template), a function winning a tie. Explicit
specializations (`template <> struct X<bool> {...}`, `template <> R
f<int>(...)`, or `template <> R f(double)` with the arguments deduced)
are read as the instance they name. Lambdas (`lambda.mlx`): `[](int
x) { ... }` is an object of a closure class the compiler makes, whose
`operator()` is the body; captures by copy are members made from the
variables, by reference are reference members, `this` is captured as a
pointer (the enclosing class's members resolve through it), `[=]` and
`[&]` capture what the body uses; the result type is the `-> R` given
or the first return's; an empty class has size 1. Also: delegating
constructors (`Point() : Point(0, 0) {}`), static locals of a class
type made the first time their declaration runs (`static Point p(7)`),
`using T = type;`, explicit template arguments for some of a function
template's parameters with the rest deduced from the call
(`make_shared<Resource>(name)`), deduction through a class template's
instance (`f(const vector<T> &)` from a `vector<int>`), a cast to a
reference type (`const_cast<T &>(x)`) yielding the object. `const` is
part of a type (`const char *` and `char *` are two overloads, a const
object or reference calls the const member functions only, an
enumerator is of its enum, nothing converts to an enum by itself, a
const-qualified class is a twin of the class kept up to date with it),
a const integer global or local with an initializer is a constant
expression (`static const int N = 4; char buf[N]`). Member templates
(`template <class T> T *make(Pool<N> &)`, declared in the class and
defined inside or after it, called as `obj.make<Node>(pool)` with the
rest deduced, an integer parameter deduced through a class template's
instance), member functions of local classes (read apart from their
function), a nested class is a type of its class's scope, a null
pointer constant (`0`) fits any pointer parameter, `(void(0))` and the
other functional casts in parentheses. Partial specializations
(`struct Kind<T *>`, `Same<A, A>`, `Describe<Box<T, N>, N>`, `Box<T,
0>`), the most specific pattern chosen (a const, a pointer, a
reference, a class template's instance or a value around a parameter
each count), explicit specializations declared before they are
defined, alias templates (`template <class T> using Vec =
vector<T>;`), class templates declared only (an alias or a pointer to
an instance before the definition completes it), template parameters
without names or typed by an earlier one (`template <typename Key,
typename, Key last>`), inline namespaces, `defined` made by a macro
in `#if`. Variadic templates: parameter packs of function and class
templates (`template <class... Args> int count(Args... args)`,
`Tuple<Ts...>`), `sizeof...`, a pack expanded in a call's arguments
(`f(args...)`, `g(h(args))...`, `forward<Args>(args)...`), in an
initializer list (`{ args... }`, `{ 0, (f(args), 0)... }`) and in
template arguments (`Pair<Args...>`), a partial specialization taking
the rest of the arguments (`Tuple<T, Rest...>`), fold expressions
(`(args + ...)`, `(... && f(args))`, `(x + ... + args)`, empty ones
for `&&`, `||` and `,`). An expansion is read once per element of
the pack, the pack's names standing for that element each time; an
instance's pack parameter becomes a parameter per element (`args#0`,
`args#1`), known together as the pack. `constexpr` functions
(`interp.mlx`): a call with constant arguments in a constant
expression (`static_assert`, an array's size, a template argument) is
computed by walking the function's body (integers only: locals,
`if`, loops, `switch`, `break`, `continue`, compound assignments,
recursion, static member functions); `static constexpr` data members
and `constexpr` variables are constants; variable templates
(`template <class T> constexpr bool v = ...;`, with explicit
specializations); `if constexpr` reads the branch taken only;
`noexcept(e)` is true. Rvalue references: `T &&` is a type of its
own (mangled `O`), binding rvalues only (temporaries, call results,
values, objects named through `static_cast<T &&>` or `std::move`)
and before `const T &` does, so move constructors and move
assignments are chosen for them; a forwarding reference (`T &&` with
T a parameter) deduces a reference from an lvalue (references
collapse), so `std::forward<T>(x)` passes the category on; a
non-const reference does not bind a const object; a dependent member
type in a signature (`typename remove_reference<T>::type &&`) is
resolved in the instance; `decltype(e)`, `sizeof e` and `noexcept(e)`
evaluate nothing, so `declval<T>()` (declared only) works in them.
The type trait builtins (`__is_class`, `__is_base_of`,
`__is_constructible`, `__is_same`, `__underlying_type` and the rest)
are what `<type_traits>` is written with; a name that does not exist
for an instance's arguments (`enable_if<false, int>::type`, a member
a requires expression names) makes the candidate not viable, not an
error (SFINAE); concepts (`concept C = ...`, `requires` clauses after
the parameters or after the function's parameters, `requires`
expressions, constrained parameters `template <std::integral T>`) are
constraints evaluated per instance; a pattern over a pack
(`decay<Ts>::type...`, `(Is * 2)...`) expands in template arguments
too; a template given some arguments and deducing the rest
(`make_shared<T>(args...)`); member class templates (`template <class
F> struct Held` inside a class, as `std::function` keeps its callable);
a partial specialization over a function type (`function<R(Args...)>`).
The atomic builtins (`__atomic_load_n`, `__atomic_fetch_add`,
`__atomic_compare_exchange_n`, the `__sync_*` family, `atomics.mlx`)
are locked x86 instructions (`xadd`, `xchg`, `cmpxchg`, `mfence`) on
integers and pointers of 1 to 8 bytes, in C and C++ alike
(`tests/atomics.c`): `<atomic>` and `<mutex>` (over mlxlibc's pthreads)
are written with them. `tests/cpp/headers.cpp` runs `<concepts>`,
`<bit>`, `<array>`, `<span>`, `<tuple>`, `<optional>`, `<iterator>`,
`<functional>`, `<atomic>`, `<mutex>`, `<expected>` and `<memory>`
(`unique_ptr` with a deleter, variadic `make_unique` and
`make_shared`) as g++ -std=c++23 does.
`tests/cpp/interp.cpp`
is a small interpreter (lexer, recursive descent parser, virtual nodes,
maps of variables and functions) that runs as g++ builds it;
`tests/cpp/library.cpp` is what tinyxml2 asked of the compiler:
tinyxml2 (its library and `xmltest.cpp`, 5900 lines) builds with
mlx-cc and its 528 tests pass with the output of g++'s build. Names are mangled as the Itanium
ABI says (`_ZN8geometry4areaEii`, `_ZN1SC1Ei`, `_ZNK6Matrix2atEii`),
so objects of several files link by name. `tests/cpp/*.cpp` run as g++
runs them (`tools/check_cpp.sh`, on glibc and on mlxlibc).

C's `getline` and `std::getline` both apply where `using namespace std`
is in force: the functions a using-directive brings in join the
overloads of a name. Not there yet:
a pack of a base list (`struct X : Bases...`), a pack in a lambda's
captures,
generic lambdas (`auto` parameters), a lambda converting to a function
pointer, init-captures, class template argument deduction
(`std::pair p(1, 2)`, `std::lock_guard lock(m)`: the arguments must be
written), `<=>`, structured bindings, template template parameters,
exceptions (`try`, `throw` are refused),
multiple inheritance, a polymorphic class over a base without a vtable,
`constexpr` over classes, pointers or floating-point values,
`consteval`, destruction of globals at exit, a diagnosis
of writing through const (`const int x; x = 2` and `mutable` pass:
const chooses overloads and member functions only), a local class's member
functions naming the function's locals or the class itself,
user-defined `operator new` (the built-in `new` calls `_Znwm`, which
mlxlibcpp provides). The mangled names leave out the ABI's
substitutions (`S_`), so a name with a repeated parameter type
differs from g++'s; constructors have only their `C1` symbol; `long long` and
`signed char` are `long` and `char`, not types of their own. The C++
standard library is mlxlibcpp (`projects/libcpp`: `<string>`,
`<iostream>`, `<sstream>`, `<vector>`, `<map>`, `<set>`, `<memory>`,
`<utility>`, `<algorithm>`, `<numeric>`, `<new>`, `<type_traits>`,
`<concepts>`, `<initializer_list>`, `<limits>`, `<bit>`, `<array>`,
`<span>`, `<tuple>`, `<optional>`, `<expected>`, `<iterator>`,
`<functional>`, `<atomic>`, `<mutex>`, the `c*` headers, and `<compare>`
and `<ranges>` as stubs, under `include/c++`), linked
into every C++ program by itself as `libmlxcpp.a` from `$MLX_CC_LIB`,
`lib/` next to the compiler or `/usr/local/lib/mlx-cc/lib`
(`-nostdlib++` leaves it out), together with the math library.

## Gaps

- No shared objects yet (`-shared`, roadmap phase 1 step 3); objects
  built without `-fPIE` by other compilers (absolute 32-bit addresses,
  copy relocations) are refused with a message.
- On mlxlibc, `printf("%Lf")` prints a long double exactly within
  double's exponent range and as the nearest double (infinity, zero)
  beyond it; `strtold` reads the full 64-bit significand and the `sinl`
  family is x87 code of mlxlibc's own (`tests/ldouble_math.c` compares
  both C libraries with gcc's output).
- No variable-length arrays, `_Atomic` qualifier (the `__atomic_*` and
  `__sync_*` builtins are there), `_Thread_local` (declared, but a
  plain global), `_Complex`, inline `asm`, computed `goto`.
- `va_arg` of a struct of 16 bytes or less reads it from the stack, where
  only larger ones are.
- No optimization: the code is correct and slow.

## Check

`tools/check_cc.sh` builds mlx-cc, preprocesses the C11 standard's
macro examples (`tests/preprocess.c`), and compiles and runs every
program in `tests/` (with its second file `NAME2.c` when there is one,
and the options of `NAME.flags`: `c23.c`, `embed.c` and `bitint.c` with
`-std=c23`, `c89.c` with `-std=c89`; `ldouble.c` for the x87 long double)
against the output in `NAME.expected`, which gcc's build of the same
program printed when the test was written (clang's for `bitint.c`, gcc
13 having no `_BitInt`; `embed.c`'s follows from the standard): first on the
system's C library, then on mlxlibc as loader and C library with no GNU
code in the process. `tests/standards.c`, compiled with every variant,
prints what each predefines.

`tools/check_cc_projects.sh` builds real projects with it: zlib 1.3.1
(its `test/example.c` passes, on glibc and on mlxlibc alone), Lua 5.4.7
as POSIX systems build it, whose official test suite (21 files in user
mode: strings, math, coroutines, the garbage collector, errors, the
debug library ...) passes as it does for gcc's build, and SQLite 3.46.1
(the amalgamation and its shell, single-threaded and thread-safe with
the math functions), whose shell runs `tests/sqlite.sql` (tables,
joins, window functions, recursive CTEs, JSON, dates, an index, a
rolled-back transaction, `PRAGMA integrity_check`) and prints what
gcc's build printed (`tests/sqlite.expected`), on glibc and on mlxlibc
alone with the `libm.so.6` built next to it. The tarballs are
downloaded, or taken from `--sources DIR`, and checked against their
SHA-256 sums.
