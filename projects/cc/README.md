# mlx-cc

A C compiler written in Mlx, for MLX/Linux without GNU (see
[docs/ROADMAP.md](../../docs/ROADMAP.md), phase 1). It reads C11,
preprocesses it, and writes an x86_64 ELF program itself through the
Mlx compiler's own encoder and object writer
(`compiler/selfhost/backend/x86_64/encoder.mlx`,
`compiler/selfhost/object/elf64_dynamic.mlx`): there is no assembler and
no linker in between. The programs call the C library through
`libc.so.6`, which is mlxlibc (`projects/libc`) on MLX/Linux and glibc
elsewhere.

    mlx-out/bin/compiler/mlx4 projects/cc/main.mlx -o mlx-cc
    MLX_CC_INCLUDE=projects/cc/include ./mlx-cc -o hello hello.c
    ./hello                                   # on the system's loader
    mlx-out/libc/libmlxc.so.1 ./hello         # on mlxlibc alone

    mlx-cc [-o OUT] [-E] [-I DIR] [-D NAME[=VALUE]] [-U NAME] [-lNAME] FILE.c...

mlx-cc compiles whole programs: every `.c` file of the program is given
at once, each one a translation unit of its own (its own macros and
scope), and the result is the executable (`-c` is refused). `-E` writes
the preprocessed tokens instead. Options of other compilers that change
nothing here (`-O2`, `-g`, `-Wall`, `-std=c11`, `-fPIC`, `-pthread` ...)
are accepted silently. `-lm` adds `libm.so.6`, `-lNAME` `libNAME.so`;
`-lpthread`, `-ldl` and `-lrt` are part of the C library.

## Headers

`include/` holds mlx-cc's own C headers (`stdio.h`, `stdlib.h`,
`string.h`, `stdint.h`, `stdarg.h`, `unistd.h`, `fcntl.h`, `sys/stat.h`,
`math.h` and the rest of what common programs include), written for the
x86_64 Linux ABI that glibc and mlxlibc share. They are found in
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

C11 as programs use it: all of the preprocessor (`#`, `##`, variadic
macros, `#include_next`, `__has_include`, `#pragma once`), every integer
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
  match.
- No variable-length arrays, `_Atomic` operations, `_Thread_local`
  (declared, but a plain global), `_Complex`, inline `asm`, computed
  `goto`.
- `va_arg` of a struct of 16 bytes or less reads it from the stack, where
  only larger ones are.
- The programs name `/lib64/ld-linux-x86-64.so.2` as their interpreter
  (the ELF writer's); on mlxlibc alone they are started through
  `libmlxc.so.1 PROGRAM` until mlx-cc can name mlxlibc itself.
- No optimization: the code is correct and slow.

## Check

`tools/check_cc.sh` builds mlx-cc, preprocesses the C11 standard's
macro examples (`tests/preprocess.c`), and compiles and runs every
program in `tests/` (with its second file `NAME2.c` when there is one)
against the output in `NAME.expected`, which gcc's build of the same
program printed when the test was written: first on the system's C
library, then on mlxlibc as loader and C library with no GNU code in the
process.

`tools/check_cc_projects.sh` builds real projects with it: zlib 1.3.1
(its `test/example.c` passes, on glibc and on mlxlibc alone) and Lua
5.4.7, whose official test suite (21 files in user mode: strings, math,
coroutines, the garbage collector, errors, the debug library ...) passes
as it does for gcc's build. The tarballs are downloaded, or taken from
`--sources DIR`, and checked against their SHA-256 sums.
