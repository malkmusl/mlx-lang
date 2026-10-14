# mlx-cc

A C compiler written in Mlx, for MLX/Linux without GNU (see
[docs/ROADMAP.md](../../docs/ROADMAP.md), phase 1). It reads C from C89
to C23 (`-std=`, GNU variants included), preprocesses it, and writes an
x86_64 ELF program itself through the Mlx compiler's own encoder and
object writer
(`compiler/selfhost/backend/x86_64/encoder.mlx`,
`compiler/selfhost/object/elf64_dynamic.mlx`): there is no assembler and
no linker in between. The programs call the C library through
`libc.so.6`, which is mlxlibc (`projects/libc`) on MLX/Linux and glibc
elsewhere.

    mlx-out/bin/compiler/mlx4 projects/cc/main.mlx -o mlx-cc
    MLX_CC_INCLUDE=projects/cc/include ./mlx-cc -o hello hello.c
    ./hello                                   # on the system's loader
    mlx-out/libc/libmlxc.so.1 ./hello         # on mlxlibc alone
    ./mlx-cc --dynamic-linker /usr/lib/libmlxc.so.1 -o hello hello.c

    mlx-cc [-o OUT] [-std=VARIANT] [-E] [-I DIR] [-D NAME[=VALUE]] [-U NAME]
           [-lNAME] [--dynamic-linker PATH] FILE.c...

mlx-cc compiles whole programs: every `.c` file of the program is given
at once, each one a translation unit of its own (its own macros and
scope), and the result is the executable (`-c` is refused). `-E` writes
the preprocessed tokens instead. Options of other compilers that change
nothing here (`-O2`, `-g`, `-Wall`, `-fPIC`, `-pthread` ...) are
accepted silently. `-lm` adds `libm.so.6` (glibc's, or the one built
next to mlxlibc), `-lNAME` `libNAME.so`; `-lpthread`, `-ldl` and `-lrt`
are part of the C library.

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
`stddef.h`. Not there: `_BitInt`, `#embed`, `constexpr` objects of other
types than integers as constants. A program
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
| Constants and strings | `literal.mlx` |
| Types and syntax tree | `types.mlx`, `ast.mlx` |
| Parser | `cursor.mlx`, `state.mlx`, `scope.mlx`, `decl.mlx`, `initializer.mlx`, `expr.mlx`, `stmt.mlx`, `typing.mlx`, `eval.mlx` |
| Code | `x86.mlx` (instructions), `gen.mlx` (expressions, calls), `genstmt.mlx` (statements, functions) |
| Program | `program.mlx` (linking by name, data area, `_start`, ELF) |
| The C variant | `standard.mlx` (`-std=`: the macros, the keywords) |
| Driver | `main.mlx` |

The code generator is a stack machine on rax (xmm0 for floating values),
with every integer kept canonical for its type in the whole register.
Calls follow the System V AMD64 ABI, structs passed and returned by value
included, and variadic functions set up the ABI's register save area so a
`va_list` can go on to `vprintf`. Globals live in the writable data area
of the image; the addresses they hold are written at start-up by
`__mlxcc_init`, which `_start` calls before `main(argc, argv, envp)`.
A name with external linkage that no file defines is taken from the
libraries (functions and variables such as `stdout` alike, through the
GOT); static functions nothing refers to are left out.

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

## Gaps

- No separate compilation yet: no `.o`, no archives, no shared objects
  (roadmap phase 1 step 3).
- `long double` is `double` (no x87 code): functions of the C library
  that take or return `long double` (`strtold`, `printf("%Lf")`) do not
  match, and a program that computes in `long double` (SQLite's
  conversion of doubles to text, where its own check finds the type no
  wider than `double`) rounds otherwise in the last digits.
- No variable-length arrays, `_Atomic` operations, `_Thread_local`
  (declared, but a plain global), `_Complex`, inline `asm`, computed
  `goto`.
- `va_arg` of a struct of 16 bytes or less reads it from the stack, where
  only larger ones are.
- No optimization: the code is correct and slow.

## Check

`tools/check_cc.sh` builds mlx-cc, preprocesses the C11 standard's
macro examples (`tests/preprocess.c`), and compiles and runs every
program in `tests/` (with its second file `NAME2.c` when there is one,
and the options of `NAME.flags`: `c23.c` with `-std=c23`, `c89.c` with
`-std=c89`) against the output in `NAME.expected`, which gcc's build of
the same program printed when the test was written: first on the
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
