# mlx-cc

A C compiler written in Mlx, for MLX/Linux without GNU (see
[docs/ROADMAP.md](../../docs/ROADMAP.md), phase 1). It reads C from C89
to C23 (`-std=`, GNU variants included), preprocesses it, and writes
x86_64 ELF itself through the Mlx compiler's own encoder, object writer
and linker (`compiler/selfhost/backend/x86_64/encoder.mlx`,
`compiler/selfhost/object/relocatable.mlx`, `link.mlx`,
`elf64_dynamic.mlx`): there is no assembler and no GNU linker in
between. The linker is a program of its own, `mlx-ld`
(`compiler/ld/main.mlx`), which mlx-cc runs for every link as cc runs
ld (below). The programs call the C library through `libc.so.6`, which is
mlxlibc (`compiler/libc`) on MLX/Linux and glibc elsewhere.

    mlx-out/bin/compiler/mlx4 compiler/cc/main.mlx -o mlx-cc
    mlx-out/bin/compiler/mlx4 compiler/ld/main.mlx -o mlx-ld   # beside it
    MLX_CC_INCLUDE=compiler/cc/include ./mlx-cc -o hello hello.c
    ./hello                                   # on the system's loader
    mlx-out/libc/libmlxc.so.1 ./hello         # on mlxlibc alone
    ./mlx-cc --dynamic-linker /usr/lib/libmlxc.so.1 -o hello hello.c
    ./mlx-cc -c lib1.c lib2.c                 # lib1.o, lib2.o
    ./mlx-ar rcs libmine.a lib1.o lib2.o
    ./mlx-cc -o hello hello.c -L. -lmine      # or hello.o libmine.a

    mlx-cc [-o OUT] [-std=VARIANT] [-E] [-c] [-I DIR] [-D NAME[=VALUE]] [-U NAME]
           [-L DIR] [-lNAME] [--dynamic-linker PATH] [-MD|-MMD] [-MF DEP] [-MT TARGET]
           [-s] [-Wl,-Map=FILE] FILE.c... [FILE.o...] [LIB.a...]
    mlx-ar rcs LIB.a FILE.o...  |  mlx-ar t LIB.a  |  mlx-ar x LIB.a
    mlx-ld [-o OUT] [--dynamic-linker PATH] [-s] [-Map FILE] [-L DIR] [-lNAME] [-l:FILE]
           FILE.o... LIB.a... LIB.so...

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
`-lpthread`, `-ldl` and `-lrt` are part of the C library.

The link is mlx-ld's: mlx-cc compiles the `.c` files into the one
object, resolves `-L`/`-l` (and adds mlxlibcpp for C++), and then
runs `mlx-ld` in its own place (execve: the compiler's memory is gone
before the link starts), handing it the object as it is in memory
(`compiler/selfhost/object/unit.mlx`, in a memfd: the linker sees the
same sections, symbols and relocations in the same order, so the
program is byte for byte what the linker inside mlx-cc made) followed
by the `.o` and `.a` files in order, `-l:SONAME` for each library the
program needs, `--dynamic-linker`, `-s` and `-Map`. mlx-ld is `$MLX_LD`,
else `mlx-ld` beside mlx-cc (`mlx build cc` puts both in mlx-out/bin).
`mlx-cc -c` never runs it. Being two programs is what lets `mlx build`
keep them apart: an object step's result depends on mlx-cc alone, a link
step names mlx-ld as an input, so a change to the linker relinks the
programs and compiles no object again. `-E` writes
the preprocessed tokens instead. `-MD` (or `-MMD`) writes make's rule of
the source and every file it included and embedded, at `-MF`'s path
(else the output's with `.d`) naming `-MT`'s target (else the output):
what `mlx build` reads to rebuild an object when a header changed. Options of other compilers that change
nothing here (`-O2`, `-g`, `-Wall`, `-fPIC`, `-pthread` ...) are
accepted silently.

## The code index

`--index=DIR` (or `MLX_CC_INDEX=DIR` in the environment, which leaves
the command line, and so `mlx build`'s cache keys, as they are) writes
what the compiler resolved while compiling into DIR, for MLX Codemap
(`mlx-codemap`, tools/codemap/cpp.mlx): every declaration (functions,
variables, classes and their data members, macros; with the class it is
in, its linker name, where its definition and body are) and every use
(calls, reads, writes, types, base classes, overrides, macro
expansions, definitions apart from their declaration, `#include`s),
each use naming the declaration it resolved to. The index is a shard
per file read, named by its path and contents (`index.mlx` describes
the text): a header indexed by one unit is not written again by the
next, so indexing a whole build costs each header once. A template's
instances are read from its tokens, so their uses are the template's.
The objects are byte for byte the same with and without `--index`
(tools/check_codemap_cpp.sh compares them; all of compiler/cc/tests
compile alike both ways). The cost: a WebCore unit whose headers are
indexed already takes about 3% more processor time (the walk over its
bodies and its own file's records; what is in a header with a shard is
not even kept), the first unit to read 2,000 headers about 13% (it
writes their shards); mlx-cc's tests together 1%. `MLX_CC_INDEX_MAP=
OLD=NEW` writes paths under OLD as under NEW (a tree compiled in one
checkout, indexed for another). tools/codemap_index.sh and `mlx build
codemap-cpp-index` make the index of the repository's C and C++
(projects/observatory/README.md, "C and C++").

## The code generation epoch

`mlx build` keys mlx-cc's objects not by mlx-cc's bytes but by
`CODEGEN_EPOCH` (`epoch.mlx`; `mlx-cc --codegen-epoch` prints it,
`--codegen-epoch=FILE` writes the stamp the `mlx-cc-epoch` step of
build.mlx makes; docs/reference/build-system.md, "Programs keyed by an
epoch"). A change to mlx-cc that keeps it keeps every object of every
checkout's cache valid; only units that failed before compile again.

**Bump `CODEGEN_EPOCH` in the commit of any change that can change an
object's bytes (or its `-MD` file) for a source that compiled before**:
code generation, the ABI and layouts, vtables, mangling, the symbols
and what an object holds or leaves out and their order, constant
evaluation and the choices it feeds (sizeof, constexpr results,
overloads, templates), the predefined macros, the object format, the
line-table symbols. **No bump** for a change that only makes failing
code compile, makes compiling faster or leaner with the same objects,
or touches diagnostics, warnings, the crash report's wiring, the
driver's options, `-E`, `MLX_CC_STATS` or the linker (mlx-ld is keyed by
its bytes). When in doubt, bump: a bump costs a rebuild, a missing one
stale objects in every cache.

A forgotten bump is caught by `mlx build --verify-epoch N` (N cached
objects compiled again and compared; `tools/check_codegen_epoch.sh`
runs it on 20 of WTF's units). A check that fails when codegen files
change without a bump is not there on purpose: which files decide the
bytes is most of compiler/cc (the parser's overload and template
choices, interp.mlx's constant evaluation, the headers' semantics), and
most changes to them (the speed and memory work, the features that made
WebKit compile) changed no object; such a check would cry wolf on nearly
every commit, and an alarm that is always on gets bumped through. The
comparison of real objects does not.

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
| Object and program | `program.mlx` (linking by name, data area, the relocatable object; the program through mlx-ld), `compiler/ld/main.mlx` (mlx-ld: `compiler/selfhost/object/link.mlx` as a program, the link map), `ar.mlx` (mlx-ar) |
| The C variant | `standard.mlx` (`-std=`: the macros, the keywords) |
| Prefix snapshots | `snapshot.mlx` (below) |
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
`.init_array`. Thread-local variables (`_Thread_local`, `thread_local`,
`__thread`) live in `.tdata`, reached from the thread pointer
(`mov %fs:0` and `R_X86_64_TPOFF32`); the linker lays them out as the
program's TLS image (`PT_TLS`), which the dynamic loader copies for
each thread (`tests/threadlocal.c`). The linker (`compiler/selfhost/object/link.mlx`) resolves
the references between the objects, makes a stub and a GOT slot for
every name no object defines (the libraries' functions and variables
such as `stdout` alike), and makes `_start` and `__mlxcc_init`, which
writes the addresses held by data and runs the init functions before
`main(argc, argv, envp)`. Static functions nothing refers to are left
out.

A program carries its symbol table (`.symtab`, `.strtab`): every
function and variable of the objects linked, local and global, with
the objects' types, bindings and sizes (a function without a size gets
the distance to the next one), section symbols for `.text` and `.data`,
and the linker's own `_start`, `__mlxcc_init` and `NAME@plt` stubs, so
`nm`, `readelf`, `perf` and gdb name the functions, and the crash
report (std.crash) names a crash's frames with them (demangled for
C++). Each function's object marks where it was defined (a local
`@at PATH:LINE` symbol), and the linker makes the line table
`.mlx_lines` of those marks: the crash report says the file and line of
each frame's function. (A line table of every statement's line is not
written yet; the marks are where it would come from.) `-s` (or
`--strip-all`, `-Wl,-s`) leaves the tables out. `-Wl,-Map=FILE` writes
the functions by their offsets in the code.

### Prefix snapshots

    mlx-cc --write-prefix-snapshot P.snap FLAGS -include P.h [-MD -MF P.snap.d]
    mlx-cc --prefix-snapshot P.snap FLAGS -include P.h -c unit.cpp -o unit.o

A unit's `-include` files (WebKit's `WebCorePrefix.h` and what it
includes: thousands of headers, the same in every unit of a target) are
read before the file, preprocessed and parsed, and the file is read
after them, always in these two parts. `--write-prefix-snapshot` writes
the compiler's whole state at that point: the arena's used pages (it
lies at a fixed address, `base.ARENA_BASE`, so every address in it is
valid in another run of the same binary; zero pages are holes in the
file) and every module-level variable of the compiler's modules (each
module's `keepState()`, which `tools/cc_snapshot_state.py` writes from
the declarations; a variable marked `// (per run)` is left out, as the
run's own memory is: the command line's texts, the link's inputs and the
object writer's buffers are in a side region after the arena). With
`--prefix-snapshot` a run maps the pages back where they were
(`MAP_PRIVATE`: the page cache's copy, shared by every unit built at the
same time until a unit writes to a page) and reads only its file. The
object is the one the run would have made without the snapshot, byte for
byte (tools/check_cpp.sh compares them), and so is the `-MD` file.

A snapshot is taken only when it is the state the run would have
reached; otherwise the run reads the prefix and says why: the key (a
hash of the compiler's binary and its path, the working directory, the
language, `MLX_CC_INCLUDE` and `MLX_CC_READ_ALL_BODIES`, and the options
that change the compilation in their order: all but `-o`, `-c`, `-MD`
and its kind, the link's and the files), the variables' layout, every
file the prefix read (its size and a hash of its contents; one written
again with the same contents keeps its `#pragma once` mark), and, for
every directory searched whose time changed since, that the search
would still find what it found (no name it missed there is there now).
Nothing of the prefix depends on the file that reads it: the file's
path is given to the compiler after the prefix, and the prefix's
lambdas and unnamed namespace are named after the prefix's identity (its
options) in every run, with a snapshot or without. The prefix's
warnings are said again by a run that takes the snapshot.

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
once whatever the threads (a guard libmlxcpp's `__mlxcc_guard_acquire`
takes), and one object for the program when the function is inline or
a template's (weak `_ZZ<function>E<name>` symbols, the code reaching
them and inline variables by name; `tests/cpp/shared`), a class that
cannot move by its bytes (a copy or move constructor, a destructor of
its own) passed by value by its address, as the Itanium ABI passes it,
and a local returned made right in the caller's buffer when every
return returns it (else moved out by its move constructor;
`tests/cpp/wtfforms64.cpp`),
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
a partial specialization over a function type (`function<R(Args...)>`);
template template parameters (`template <template <class...> class L,
class... Ts> struct Count<L<Ts...>>`, `Apply<Box, int>`, deduced from
instances in function templates and partial specializations,
`tests/cpp/ttparams.cpp`); function templates of one name whose
parameters differ in kind (`f<size_t N>` and `f<typename T>`); `using
enum E;`. Files are compiled with `__GNUC__` defined (13.2: WebKit,
bmalloc and libpas insist on a GCC-compatible compiler), with GNU C's
labels as values (`&&label` a `void *`, `goto *p`, static tables of
them relocated against the code: Lua's interpreter dispatches so,
`tests/labelvalues.c`), with `__attribute` as
`__attribute__`, and with GCC's builtins: the bit builtins
(`__builtin_clz`, `ctz`, `popcount`, `bswap`, `ffs`, `builtins.mlx`,
as bsr, bsf, popcnt and bswap instructions, constant-folded in
constexpr code), `asm` statements (`asm volatile("" ::: "memory")` as
a compiler fence; any other text assembled by `asm.mlx`, below), and the
builtins that are
plain C as predefined macros (`__builtin_memcpy`, `__builtin_expect`,
`__builtin_isnan`, `__builtin_inf`, `__builtin_constant_p` as 0, ...;
`tests/builtins.c` against gcc; `__builtin_frame_address(n)` and
`__builtin_return_address(n)` walk the rbp chain every function keeps). `<emmintrin.h>` (and `xmmintrin.h`,
`smmintrin.h`, `immintrin.h`, `x86intrin.h`) gives the few SSE
intrinsics programs use for scalar conversions (`_mm_cvttsd_si64`,
`_mm_set_sd`, the 128-bit loads and stores, `_mm_pause`) as plain
functions over a 16-byte struct. C++20's three-way comparison `<=>`
(`tests/cpp/compare.cpp`) gives a class's operator function or, between
numbers and pointers, a `std::strong_ordering` (`partial_ordering` for
floating point) from `<compare>`; `a < b` of a class with only
`operator<=>` is rewritten to `(a <=> b) < 0`. `if (init; cond)` and
`switch (init; cond)` take an init statement; `T{a, b}` of an
aggregate sets its members in order (also as a local or a global, and
`int x{5}`, `long z{}` for scalars); `long long`, `signed char`,
`wchar_t`, `char16_t` and `char32_t` are types of their own (overloads,
specializations, mangled `x`, `a`, `w`, `Ds`, `Di`); a class template
named in a typedef or an alias (`using X = T<A>;`) is not instantiated
until something needs it whole (an object, a member, its size, pointer
arithmetic, a qualified name), as C++ says; `__PRETTY_FUNCTION__`,
`__VA_OPT__` and GNU's `, ## __VA_ARGS__` are there. User-defined
literals (`3_s`, `1.5_s`, `"text"_s`: a call of `operator""_s` with the
integer as unsigned long long, the floating value as long double, a
string with its length; `tests/cpp/literals.cpp`); `a != b` and
`b == a` rewritten from `operator==` (C++20); `__int128` and its
typedefs as `_BitInt(128)` aligned to 16 (GCC's); default template arguments of function
templates (`class = enable_if_t<...>`) taken in deduction, as SFINAE;
member operator templates (`template <class U> T &operator=(U &&)`);
friend functions read in their class (its own name is its instance
there); `if (T *p = f())` and `while (T *p = f())`; `std::variant`
in `<variant>` (`tests/cpp/variant.cpp`). Overloads gather from every
place a name may come from: a member function template and plain
member functions of one name are chosen among together, a qualified
call (`std::to_chars(...)`) weighs the namespace's templates too, and
an operator of a nested class (`thread::id`) is found in the
namespace around its class; of two templates fitting as well, the
more specialized one wins (`duration(const duration<R, P> &)` over
`duration(const R &)`). An object of a class is copied by a copy
constructor or member by member, never by a constructor template or
through its own conversion operator; a conversion operator may return
a reference (`operator const path &()`) and converts a class to
another class; a function binds to a reference to a function; a
range-based `for` takes `begin(x)` and `end(x)` of the class's
namespace too and keeps class iterators as objects (copied and
destroyed as such); `::name` reaches the global past a local of the
name, and a function hides a struct of its name (`::stat(...)`);
`char8_t` is a type of its own (mangled `Du`), and `u8""`, `u""`,
`U""` and `L""` are arrays of `char8_t` (C++20), `char16_t`,
`char32_t` and `wchar_t`, as their character constants are
(`tests/cpp/overloads.cpp`). The C headers under `include/sys` are
`extern "C"` as those above them are. A class's member aliases (`using
T = type;` in the class) are typedefs of its scope; an inline
namespace's names are its parent's from its opening brace (`lib::f`
inside `lib`'s inline namespace); a member `operator()` template makes
an object callable (a generic callable); a function template's
candidate instances are read whole only once chosen, so a
`static_assert` in an overload not taken stays silent; an alias
template whose arguments make it name nothing (`enable_if_t<false>`)
fails the candidate without being remembered, and a dependent value
argument is told apart by its tokens (`enable_if<!C>` is not
`enable_if<C>`); an explicit reference argument (`f<int &>(x)`) fits
a `T &` parameter; a constructor template's brace initializers
(`: value{ true }`) are passed over with it; a trailing return type
names the expanded parameter pack (`-> decltype(f(as...))`)
(`tests/cpp/members.cpp`). Pointers to members (`int P::*`, `int
(P::*)(int) const`, `&P::x`, `&P::f`, `obj.*pm`, `p->*pm`, calls
through them, `R T::*` deduced in templates; a data member's pointer
is its offset plus one, a member function's its address (functions
start at 16), a virtual one's its vtable entry's offset plus one, odd,
called through the object's vtable as Itanium has it;
`tests/cpp/memberptrs.cpp`, `tests/cpp/wtfforms48.cpp`). The
detection idiom (`struct has_size<T, void_t<decltype(declval<T>()
.size())>>`): a partial specialization's arguments that deduction
passes over are read again with the parameters deduced and must name
what was given (`tests/cpp/detection.cpp`); in a pattern a dependent
`decltype(...)` and a trailing return type are known per instance
only, so nothing is instantiated for them early. `auto` parameters
make abbreviated function templates (`int twice(auto x)`, `const
std::invocable<> auto &f`, `auto... xs`, in classes too: the
parameters are invented, the constraint dropped) and generic lambdas
(their `operator()` a member template, capturing what their body names
of the function's locals); `T x{a, b}` and `T x = {a, b}` call an
`initializer_list` constructor (`std::vector` has one, and one from an
iterator range); member alias templates, deleted function templates
(left out of overloads), `[[nodiscard]]` after a template header,
brace-initialized variable templates and `T::member` in a template's
signature are read (`tests/cpp/generic.cpp`). A lambda may have
template parameters of its own (`[]<size_t... N>(index_sequence<N...>)
{...}`); the rewrites behind `auto` parameters work on copies of the
tokens, so a template or concept holding them is read again as it was
written. A plain unqualified call that names nothing visible looks in
the namespaces of its arguments' classes and enums (`is_lt(ordering)`
is std's), and an operator in both operands' namespaces; an `auto`
global's initializer is read once (a lambda there is one closure);
`<ranges>` has `views::transform` and `views::reverse`, called or
piped (`tests/cpp/views.cpp`). Structured bindings (`auto [y, m, d] =
f();`, `auto &[k, v] : map`, `const auto &[a, b]`) bind each name as a
reference to a part of a hidden variable: an aggregate's data member,
`std::get<i>` of a `std::tuple` or `std::array`, an array's element
(copied element by element when bound by value). `return {a, b};`,
`x = {a, b}` and nested braces in an initializer list (`{ {1, 2}, {3,
4} }`) make the type wanted from the braces, and an array member takes
its elements without braces of its own (brace elision, SIMDe's vector
unions; `tests/cpp/bindings.cpp`). The forms WTF's headers use
(`tests/cpp/wtfforms.cpp`): `class X final`, `class [[nodiscard]] X`,
`consteval` (compiled as constexpr) and `if consteval` (the run-time
branch), `constinit`, a constructor delegating to a defaulted one,
`X<U>::template f<T>()` and `x.template f<T>()`, friend classes named
with a qualifier or a template header, a parameter pack used in a
lambda (each element captured as a member of its own), constrained
`Concept auto` parameters (the concept kept as the invented
parameter's constraint), an explicit template header with `auto`
parameters too, a function template declared before its definition
(its instances wait for the body), an explicit specialization whose
parameter only the result names, `f<int>(...)` where functions named
f were declared after the template, a global whose value is no
constant (`int g = f();`, computed when the program starts), function
pointers as template arguments (`template <void (*f)(T *)>`, the
function bound by the parameter's name), class template argument
deduction through deduction guides (`Locker(Lock &) -> Locker<Lock>;`
becomes a hidden function whose result the declaration's initializer
picks), defaulted comparisons (`bool operator==(const X &) const =
default;`, a friend `operator<=>` that brings `==` with it: member by
member, bases first, `std::partial_ordering` over floating members),
nested namespace definitions (`namespace A::B { }`), placement new with
several arguments (the class's operator new, else the global one; a
class's operator new and delete are static), a constrained partial
specialization naming itself in its body, and a class assigned from
another type through any of its constructors (`span<T>` to `span<const
T>`). `<string_view>` (with `std::char_traits` and `""sv`; `std::string`
converts to a view and is made from one), and `<cmath>` with float,
long double and integer overloads and the classification functions as
calls (`std::isnan`; in C++ `math.h` declares them as functions, not
macros; `tests/cpp/cmath.cpp`). `extern "C++" { }` gives C++ linkage
back inside a C header, and a C function of another signature is
overloaded rather than redeclared. `__attribute__((weak))` and C++17
`inline` variables are weak symbols. `_Pragma("...")` is read and left
without effect, as an unknown `#pragma` is; `__builtin_sqrt`,
`__builtin_roundevenf` and the other math builtins call libm (roundeven
as rint); GCC's `__ATOMIC_*` orders are predefined; `vector_size`
(GCC's vector types) is refused rather than ignored. A braced list is
an argument too (`f({a, b})`: overloads take it as a conversion to a
class, or to a scalar when it has one element at most; a template
deduces nothing from it; the parameter's type is made from it once the
function is chosen); `T{a, {b}}` makes a class as `T x{a, {b}}` does;
class template argument deduction works in expressions (`std::span {
first, last }`) and the standard headers declare their guides (`span`,
`pair`, `array`, `tuple`, `optional`); `decltype(auto)` is deduced as
`auto`; an argument for a parameter whose type was given explicitly
converts to it (`f<unsigned char>('A')`); `is_integral` and
`make_signed`/`make_unsigned` know every integer and character type
and enums; a static member template keeps `static` when defined
outside its class (next to an overload of its name); what braces leave
out of a C++ aggregate is zero (`T x { }`, `int a[4]{9}`), and a
default member initializer may be any braced list
(`tests/cpp/deduction.cpp`).
A class may have several bases (`struct S : A, B`): each is a member of
its own, an empty one takes no room (the empty base optimization), a
pointer or reference converts to any base and back (`static_cast`,
null kept null); the first base may be polymorphic, a later one may not
(refused with a message). A base list expands packs (`struct X :
leaf<Is, Ts>...`), `using Base::Base;` inherits the constructors, and an
object of a class with conversion operators to pointers to functions is
called through the one the arguments fit best (a surrogate call:
mpark's `variant`, WTF's `Variant`, chooses its alternative so).
Class template argument deduction takes the implicit guides of the
constructors too (`R r{s}`, `Pair p(1, 2.5)`, with the defaults of the
parameters not deduced). `decltype` gives an lvalue `T&`, an xvalue
`T&&`, a name its declared type, `decltype(auto)` keeps a returned
reference, and `decltype(x)::member` names a member. SFINAE covers
`void_t<decltype(...)>` in partial specializations (of class and
variable templates), `typename U::rep` with `U` not a class, `sizeof` of
an incomplete type, a call of an object that is not callable;
`is_invocable` and `invoke_result` are SFINAE-friendly, and a braced
element that narrows (`{double}` for `int`, a pointer for `bool`) does
not convert. A lambda without captures is constexpr; a member
template's partial specialization, an alias with a pack of its class, a
member template declared in its class and defined outside with an
`auto` parameter, and an explicit specialization `template <> R
C::f<X>()` are read; a class template whose argument is a class not
complete yet is made when something needs it whole, and a function's
body is not made for a call in an unevaluated operand
(`tests/cpp/deduce2.cpp`, `sfinae.cpp`, `packs2.cpp`, `classes2.cpp`).
An error inside a template instance names the instances being read and
where each was needed. WTF's `Assertions.cpp` compiles with what came
next (`tests/cpp/wtfforms2.cpp`, `wtfforms3.cpp`): GCC's
`__builtin_add_overflow`, `sub` and `mul` (computed exactly in a
130-bit integer); a member's default argument read when a call needs
it (it may name members declared later); a class nested in a class or
a class template defined outside it (`template <..> class Outer::Inner
{...}`); a constexpr function reading a string literal, taking a
constant by `const T &` or returning a reference (`std::max(a, b)` in
a constant); `{vs...}` in braces; base classes' enumerators and member
types; `x.f<N>()`; `static_assert` as a statement (`else
static_assert(...);`); `[[maybe_unused]]` on a parameter; `using
N::Class;` naming the class; `T(x).f();` and `M<T>(new (p) T);` as
expressions; the copy deduction candidate (`RefPtr r = makeRef();`
deduces `RefPtr`'s own arguments); a non-const `T &` binding no
temporary; forwarding references in packs (`Args &&...` keeps lvalues as
`T &`; `is_rvalue_reference<T &>` is false); overloaded member templates
of a class template each defined outside; and a constrained function
template declared only (`template <signed_integral S> S f(S, S);`)
kept apart from another constraint's definition. Then
(`tests/cpp/wtfforms4.cpp`): `typename X::template Alias<...>`; an
explicit specialization declared before a use (`template <> struct
DefaultHash<P>;`) waits for its definition; a class made while a
template's signature is read has its value arguments known; `Alias::E`
of a scoped enum's alias; braced default arguments (`Spec = { }`);
`unique_ptr<T[]>` and `make_unique<T[]>(n)`; a lambda expression is an
rvalue; overloaded member templates told apart by their parameters'
kinds and shapes; `const T *` deduced from a pointer to non-const and
`R (*)(Args...)` from a function's name (also as a constexpr argument);
a base's member types in qualified names (`V::ValueTraits::TraitType`);
a non-type template parameter of a dependent type (`typename
underlying_type<E>::type c = ...`); a free template's defaults kept
from its declaration; `x.f<T>(...)` beside a plain `f`; and at most
one user-defined conversion in an implicit conversion (a copy
constructor takes no value that would need its own class made).
`tests/cpp/wtfforms5.cpp` adds: a lambda without captures converting
to a pointer to a function (a static invoker read from its tokens); a
constructor template in an implicit conversion (not an `explicit`
one); a member template's own parameters found before its class's
members (`template <size_t length> C(const char (&)[length])` beside a
`length()`); `T (&&)[N]` and `initializer_list<T>` deduced from braces
(`toArray<uint16_t>({...})`, `std::max({3, 8, 2})`); `std::to_array`,
`lower_bound`, `binary_search`, `is_sorted`, `min_element`, `any_of`,
`copy_n`, `rotate`, `clamp` and the like, and `std::ranges::sort`,
`find`, `count`, `all_of`, ...; `static T x { ... }` of an aggregate;
`base::member` of a base by its typedef; const and non-const member
template overloads (mangled `_ZNK`, chosen by the object's constness);
`&N` of a constexpr variable; a class template's instance named before
its only partial specialization was defined. Polymorphic bases after
the first (`tests/cpp/polybases.cpp`): the vtable is ours, its first
word the offset from the subobject to the whole object, then per slot
the function and the adjustment of `this` it takes; a class has a
secondary vtable for each polymorphic subobject not at offset 0, whose
slots hold the class's final overriders, so a call through any base
reaches the override and `delete` through any base frees the whole
object; a polymorphic base goes first in the layout, and a class with
virtual functions over bases without any has its own vtable pointer
first. A virtual function of a base after the first called on the
derived object goes through that base's vtable pointer, a destructor
sets the vtable pointers back to its class's tables before its body (so
that what it calls is its class's, as the derived part is gone), and a
virtual destructor defaulted (`virtual ~A() = default;`), or the one a
class has implicitly when a base's is virtual, is made (when a vtable
holding it is used, or it is called: at the end of the unit, a member's
class complete by then), so `delete` through a base unmakes the whole
object (`tests/cpp/virtualdtors.cpp`, `tests/cpp/secondarycalls.cpp`).

Virtual bases (`cpp.mlx`'s section of that name) are laid out as the
Itanium ABI says, as g++ lays them out: the primary base (the first
non-virtual polymorphic base, else a nearly empty virtual base no other
base claims) or the class's own vtable pointer at 0, the other
non-virtual bases at their size without their virtual bases (a non-POD
base's tail padding reused), the members, then each virtual base once
(`tests/cpp/vbaselayout.cpp` prints the sizes and offsets g++'s build
does). A virtual base is reached through the offset its vtable holds
before the offset to the top, where the ABI puts the vbase offsets
(after the vcall offsets): a conversion to it, its members, its
functions (a null pointer stays null; the conversion back is refused,
as C++ has it). Every subobject with a vtable pointer has a table of
ours (the ABI's vcall and vbase offsets, offset to the top and a null
typeinfo before it); every base subobject with virtual bases has
construction vtables (its own functions, its virtual bases where the
whole object has them) and a VTT listing them, then its bases' VTTs. A
constructor of such a class reads its VTT from the object's first word,
which its caller sets: 0 for a complete object, whose virtual bases it
makes first (from the most derived class's initializers, an indirect
virtual base's too; the intermediate classes' are passed over), else
the VTT of the base subobject it makes; its symbol is the base-object
one (`C2`), and `C1` is an entry zeroing the word that complete objects
are made through (g++'s classes too). A destructor takes the VTT after
`this` (`D2`; `D1` its entry, which a virtual destructor's slot holds)
and unmakes the virtual bases, last, for a complete object
(`tests/cpp/vbasector.cpp` traces the constructions and destructions,
virtual calls in constructors and destructors among them). Final
overriders along virtual paths are taken per table (each slot's
adjustment of `this` is the table's own: no thunks, no vcall offsets
read; `tests/cpp/vbasecalls.cpp`). The implicit copy constructor and
assignment go member by member, the virtual base copied by the complete
object once, the vtable pointers left as they are
(`tests/cpp/vbasecopy.cpp`, `tests/cpp/vbaseforms.cpp`). Objects g++'s
code makes and ours share their layout, the vbase offsets and the
complete-object constructors and destructors (`tests/cpp/vbaselink`:
each side makes the other's objects and reads them through their
virtual bases); their vtables' slots, construction vtables and VTTs are
not g++'s, so a virtual call or a base subobject's construction across
the two does not work, and vtables nothing refers to are left out of an
object (so that g++'s of the name is the one linked). Constructors and destructors defaulted outside the class, and a
class template's friend functions made when used, followed; then
init-captures (`[f = std::move(f), &r = x, n{3}]`, a member made from
its initializer, `tests/cpp/lambdas2.cpp`), the implicit move
constructor moving the members of an rvalue, a base's scoped enum
named in a constructor's initializer (`Type::Serial`), C++20's
destroying `operator delete` (and a class's own `operator delete`
used by `delete`), `p->T::~T()`. 92 of WTF's 174 `.cpp`
files compile.
The rest of WTF followed, all 174 of its `.cpp` files compiling
(`tests/cpp/wtfforms*.cpp` hold the forms each step asked for, run
against g++'s results): `std::nullptr_t` a type of its own; lambdas in
lambdas capturing the outer one's captures; operator functions on
enums; `auto &` results; overload sets of lambdas (`using
A::operator()` across bases); implicit and defaulted assignment member
by member (moving from rvalues); class template argument deduction for
globals; designated initializers; partial specializations constrained
by `requires` and concepts with nested requirements; friend function
templates defined in their class; instances nested in their own
template named lazily (FastBitVector's `operator~`); `tuple_cat`,
`not_fn`, `<unordered_map>`, `<unordered_set>`, `midpoint`; bmalloc's
headers (libpas: empty `asm` with operands as an optimization barrier,
16-byte atomics through `cmpxchg16b`, and a second WebKit patch for
its x86 double-free check, `compiler/cc/webkit/`). The last one was a
conversion operator named in an expression (`p.operator int*()`)
whose result type a later declaration's `operator[]` took as its own.
Then all 168 of bmalloc's sources, 157 of them libpas's C
(`tests/gnuforms.c`, its expected output clang's, which libpas is
usually built with): compound literals initializing static objects
(`pas_lock x = PAS_LOCK_INITIALIZER;`) and their members read in
constant expressions (a heap configuration macro naming another's
fields, `(CONFIG).config_ptr` in a table); the typed overflow builtins
(`__builtin_usubll_overflow`); an instruction given inputs it does not
name (`int3` with the crash reason in `r11`); Valgrind's client request
(`rolq` by 3, 13, 61 and 51, then `xchgq %rbx,%rbx`: the default
answer when not run under Valgrind); default arguments given by a
declaration before the definition; `<linux/futex.h>`, and
`<string.h>` bringing `<strings.h>` as glibc's does.
Inline assembly is assembled by mlx-cc itself (`asm.mlx`, an assembler
of AT&T x86-64 text): the operands are given registers by their
constraints (`a`, `b`, `c`, `d`, `S`, `D`, or any free one of rax to
r10 for `r`, `q`, `g`, `rm`), memory operands (`m`) are addressed
through a register, constants (`i`, `n`) are put in as `$value`, and
`%0`, `%[name]`, the size modifiers (`%q0`, `%k[x]`, `%w0`, `%b0`,
`%h0`, `%c0`), `%%` and `%=` are replaced before the text is
assembled; the inputs are loaded from memory before the instructions
and the outputs stored after (the callee-saved registers the text
clobbers kept around it). The assembler knows the integer, string, bit,
SSE and fence instructions with every addressing form, local labels
(`1:` with `1b` and `1f`, and named ones) for the jumps inside the
text, and the data directives (`tests/inlineasm.c` against gcc's build;
`tests/asmencoding.c` prints each instruction's bytes, which are GNU
as's). JavaScriptCore's copy loops for the garbage collector
(`movups` through `xmm0`-`xmm3` with local labels), `rep stosq`, `divq`
and the register saves of its stack scan are written so.
JavaScriptCore came next (its build without JIT): its first unified
source compiles, and the rest is worked through from a pass over all
its 204 compile units, each form it asked for kept in
`tests/cpp/wtfforms16.cpp` to `wtfforms20.cpp`: copy elision (an
object initialized from a `T(args)` temporary of its class is made from
the arguments), functions as `template <auto f>` arguments
(`decltype(f)` their pointer type), members defined outside a partial
specialization, `template <> template <bool B> R C<X>::f` for one
instance, overloaded function templates told apart by a
specialization's parameters, class-type non-type parameters declared,
`new (arena) T` through the class's `operator new`, `if (constexpr T x
= e)`, empty braces to an `initializer_list` constructor, the
constant evaluator's opaque global addresses (`a.size()` of a
constexpr `std::array` in `static_assert`) and floating comparisons,
and inline assembly (above).
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
a pack in a lambda's explicit captures, a generic
lambda capturing `this` implicitly, a lambda with captures in a
constant expression, constant
evaluation of `<=>`, structured bindings in an
`if` or `switch` condition, `dynamic_cast` (a `static_cast` here,
refused from a virtual base) and `typeid`, a data
member of the class named in a declaration inside it (`auto get() ->
decltype(x)`; in a member template's signature it is seen), overloads
of a name joined across bases by `using B::f...;`,
exceptions (`try`, `throw` are refused),
`constexpr` over classes, pointers or floating-point values,
destruction of globals at exit, a diagnosis
of writing through const (`const int x; x = 2` and `mutable` pass:
const chooses overloads and member functions only), a local class's member
functions naming the function's locals or the class itself,
a replaced or class-specific `operator new` for `new T` without
placement arguments (it calls `_Znwm`, which mlxlibcpp provides). The mangled names leave out the ABI's
substitutions (`S_`), so a name with a repeated parameter type
differs from g++'s; constructors have only their `C1` symbol (those of a
class with virtual bases `C2` and the entry `C1`, its destructor `D2`
and `D1`). Programs are linked to
glibc without symbol versions, so a symbol glibc keeps in two
versions binds the older one (`realpath` refusing a null buffer).
The C++
standard library is mlxlibcpp (`compiler/libcpp`: `<string>`,
`<iostream>`, `<sstream>`, `<vector>`, `<map>`, `<set>`, `<memory>`,
`<utility>`, `<algorithm>`, `<numeric>`, `<new>`, `<type_traits>`,
`<concepts>`, `<initializer_list>`, `<limits>`, `<bit>`, `<array>`,
`<span>`, `<tuple>`, `<optional>`, `<expected>`, `<iterator>`,
`<functional>`, `<atomic>`, `<mutex>`, `<exception>`, `<iosfwd>`,
`<numbers>`, `<variant>`, `<charconv>`, `<system_error>`, `<ratio>`,
`<chrono>`, `<thread>`, `<condition_variable>`, `<iomanip>`,
`<version>`, `<filesystem>` (over POSIX calls, with `std::u8string`
in `<string>`; `tests/cpp/filesystem.cpp`, `tests/cpp/threads.cpp`),
`<random>` (the standard's engines, and distributions and `shuffle`
drawing as libstdc++'s do: `tests/cpp/random.cpp`),
the `c*` headers, `<ranges>` with the range concepts and
the whole-range algorithms, and `<compare>` as a stub, under
`include/c++`), linked
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
  `__sync_*` builtins are there), `_Complex`, GCC vector types. A
  thread-local variable is the program's own (local exec): one in a
  shared library, or a C++ `thread_local` object constructed per
  thread, is not supported (the main thread's is constructed at start). Inline
  `asm` has no floating-point register operands (`x`, `t`), no jumps
  out of its text (`asm goto`, a call of a symbol) and no other
  sections; the x87 and AVX instructions are not assembled.
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

## C headers made of Mlx: mlx-cheaders

A library written in Mlx for C and C++ programs (compiler/capi/icu) declares
its interface once, in Mlx modules, and `mlx-cheaders OUT MODULE.mlx...`
(compiler/cc/cheaders.mlx) writes the headers C includes. It reads the
modules with the Mlx compiler's own parser. Enums, structures (all
declared first, so that one may point to another), function types (a
value of one is a pointer in C), aliases, constants and `extern("c")`
functions become their C forms. A `pub fn` named in capitals becomes a
macro: of an expression when its body is one `return`, else of
statements, with its `*T` parameters the lvalues it writes and its
locals named apart. Other `pub fn`s become static inline functions. The
Mlx of the macros is real: it compiles and runs as Mlx, and
compiler/capi/icu/tests/api.mlx tests it so.
