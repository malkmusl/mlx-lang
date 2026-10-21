# mlxlibc (libmlxc)

The C library and dynamic loader, written in Mlx on system calls alone:
what C programs and our own shared objects need from libc (`malloc`,
`printf`, `fopen`, `pthread_create`, `strtod`, `qsort`, `dlopen`, ...),
as one shared object that needs no other and loads programs itself. It is
the library under a GNU-free system: glibc and musl are what it stands in
for, and a program built for glibc runs on it unchanged:

```sh
tools/build_mlxlibc.sh             # into mlx-out/libc
tools/check_mlxlibc.sh             # against glibc: ctypes, a C program, the loader
mlx-out/libc/libmlxc.so.1 /bin/echo hello     # a glibc-linked program, no glibc in the process
gcc program.c -Wl,--dynamic-linker=$PWD/mlx-out/libc/libmlxc.so.1  # a program that starts on it
gcc program.c -L mlx-out/libc -lmlxc -Wl,-rpath,$PWD/mlx-out/libc  # next to glibc, calls bound here
```

| Library | What it is | Built from |
| --- | --- | --- |
| `libmlxc.so.1` (`libmlxc.so`) | 847 exported C names: memory, strings, ctype, stdlib, errno, environment, process, time, the system call wrappers, signals, stdio (glibc's FILE layout, `popen`/`pclose`), the printf and scanf families, pthread with thread-local storage, getopt, dirent, err/warn/error, syslog, locale stubs, gettext, iconv, POSIX regular expressions, dlfcn; and the dynamic loader behind `__mlx_start` | `libc.mlx` |
| `libm.so.6` (`libm.so`) | the math functions of `<math.h>`: `std.math`'s sine, cosine (huge arguments reduced with the bits of 2/pi), e^x, logarithms and cube root, with the arc tangent, the inverse and hyperbolic functions, powers (whole exponents to an ulp in double-double arithmetic), `fmod` and `remainder` exact, the rounding functions, `frexp`/`ldexp`, `hypot`, `nextafter`, `erf`/`erfc` (a series in double-double, a continued fraction), `tgamma`/`lgamma` (Lanczos and a rising product), the classification, and the `f` variants; `tools/libm_check.py` compares it with glibc's libm call by call; the 41 `l` (long double) variants (`sinl` to `lgammal`) are x87 code of their own, `tools/libc/x87_long.s`, generated into `x87_long.mlx` by `tools/libc/gen_x87.py` and bound by the loader from a page of its own. Not yet: the Bessel functions, errno | `libm.mlx`, `x87_long.mlx` |

It is a `--plugin` shared object on a stack arena with no C library under
it, with an entry point and an initializer (`mlx4 --plugin --stack-arena
--no-libc --soname=libmlxc.so.1 --entry=__mlx_start --init=__mlx_init`,
see [Shared objects](../../docs/reference/formats.md#shared-objects)): each
exported function takes its aggregates from its own stack, so a call costs
no system call; the image has no `DT_NEEDED` entry at all and no
interpreter, so the kernel enters it directly. It runs programs three ways:

- **As their dynamic loader** (`PT_INTERP`, the way of a GNU-free
  system): `libmlxc.so.1 PROGRAM ARGS` runs any program, whatever its
  `PT_INTERP` names, and a program linked with `--dynamic-linker` set to
  it starts on it by itself. The names of glibc's pieces a program asks for
  (`libc.so.6`, `libpthread.so.0`, `libdl.so.2`, `librt.so.1`,
  `libutil.so.1`, `ld-linux-x86-64.so.2`) are this library; the system's
  `/bin/echo`, `/usr/bin/env`, `/bin/cat`, `/bin/true` and `/bin/sh`
  (dash) run on it, and `tools/check_mlxlibc.sh` runs them.
- **Next to glibc**: a program linked against `-lmlxc` before glibc, or
  Python's ctypes loading it, binds the calls it makes to this library
  while glibc's `ld.so` still starts the process. `dlopen` is not
  available that way (the other loader owns the process), and the
  standard stream variables stay the other library's.
- **From Mlx**: `std` programs are static and need no libc; our `--plugin`
  objects (libpulse, libpipewire) can be loaded by either loader.

## The C API shims beside it

The subdirectories of compiler/libc other than `api/` and `tests/` are the C
APIs of other libraries written in Mlx for the C and C++ code still
compiled here (WebKit through mlx-cc; third-party clients of MLX Audio and
MLX Capture), each a `--plugin` shared object: `glib`, `soup`, `icu`
(`./mlx build unicode`), `freetype`, `harfbuzz`, `zlib`, `png`, `jpeg`,
`webp`, `sqlite3`, `xml2`, `lcms2`, `gcrypt`, `tasn1`, `pulse` and
`pipewire`, each with its README. They are shims: the functionality is
std's (`std.unicode`, `std.http`, `std.flate`, `std.png`...) and a shim
converts C's types and conventions to std's calls; `api/` in each declares
the interface mlx-cheaders makes the headers of, `tests/` holds the C
programs `tools/check_LIB.sh` compares with the system's library.
[AUDIT.md](AUDIT.md) says, library by library, what logic each still holds
and where it goes.

## The modules

- **`sys.mlx`**: the system calls (`syscall0..6`, the `SYS_*` numbers, the
  errno values, `mmap`, futexes, a Drepper mutex), raw memory access by
  address (`get8`/`put8` .. `get64`/`put64`, `copy`, `fill`), C strings
  (`length`, `slice`, `literal`). Every C pointer is a `usize` here.
- **`tcb.mlx`**, **`errno.mlx`**: the thread block behind the thread
  pointer of threads this library started (its id, result, keys, errno),
  a shadow block per foreign thread (glibc's, found by its thread pointer),
  and `__errno_location` on it.
- **`heap.mlx`**: `malloc`, `free`, `calloc`, `realloc`, `posix_memalign`,
  `aligned_alloc`, `malloc_usable_size` on `std.general_purpose_allocator`
  with a 16-byte header per block.
- **`string.mlx`**, **`ctype.mlx`**: the `mem*`/`str*` set, `strerror`
  (glibc's texts), `strsignal`, the `__ctype_*_loc` tables.
- **`stdlib.mlx`**, **`bignum.mlx`**: `strtol`/`strtod` (exact, through
  1536-bit integers), `qsort`, `bsearch`, `rand` (glibc's TYPE_3
  sequence), `getenv`/`setenv`, `exit` and `atexit`, `system`.
- **`format.mlx`**: the printf engine: integers, strings, `%p`, `%n`,
  `%m`, and doubles formatted exactly (`%f`, `%e`, `%g`, `%a`) digit by
  digit, rounded as glibc rounds; a sink into a buffer or a stream; a
  `va_list` source. A variadic export builds its `va_list` from the
  registers of its call and `@frameAddress()` (`callSource`), so any number
  of arguments reaches it.
- **`stdio.mlx`**: `FILE` records on descriptors in glibc's `struct
  _IO_FILE` layout (216 bytes: `_flags`, the read and write pointers and
  `_fileno` where glibc's headers inline `getc_unlocked`, `putc_unlocked`,
  `feof_unlocked` and `ferror_unlocked`, and `putchar`/`getchar` when
  optimizing, calling `__overflow` and `__uflow` here only when a buffer
  runs out), buffered, line-buffered on a terminal, a recursive lock each;
  `stdin`/`stdout`/`stderr` as data symbols holding the three records'
  addresses; `fopen` .. `fclose`, `fgets`, `getline`, `fread`/`fwrite`,
  `fseek`/`ftell`, `ungetc` into the buffer, the printf and scanf families
  with their `__isoc99_`, `__isoc23_` and `_chk` names, `perror`,
  `tmpfile`, `popen`/`pclose` (`/bin/sh -c`, the child waited for by its
  stream), `%Lf` (a long double printed exactly within double's exponent
  range, as the nearest double beyond), glibc's `stdio_ext.h`
  (`__fpending`, `__freading`, ...).
- **`unistd.mlx`**, **`misc.mlx`**: the system call wrappers (`open`,
  `read`, `write`, `stat`, `mmap`, `ioctl`, `poll`, `epoll`, sockets,
  `fork`/`exec`/`wait`, ...), `getopt`/`getopt_long`, `dirent`, `uname`,
  `sysconf`, `err`/`warn`, `syslog`, `getpwnam`/`getgrnam` from the files,
  locale stubs, gettext (no message catalogs: a message is its own
  translation, as with GNU gettext when no catalog has it; the text
  domain and each domain's directory and codeset are kept and answered).
- **`iconv.mlx`**: `iconv_open`, `iconv`, `iconv_close` as glibc's: the
  Unicode forms (UTF-8; UTF-16 and UTF-32 with their byte order marks,
  read and written, and their LE and BE forms; UCS-2, UCS-4, `WCHAR_T`),
  ISO-8859-1 and US-ASCII by rule, and the single-byte charsets of
  `std.unicode.charmaps` (ISO-8859-2 to -16, the Windows code pages,
  KOI8-R and -U, IBM866, the Mac OS ones) by any of glibc's, ICU's or the
  Encoding Standard's names; `//TRANSLIT` (a few look-alikes, else `?`)
  and `//IGNORE`. Where glibc leaves a byte of a Windows code page
  undefined, it reads as the C1 control of that value (the Encoding
  Standard's tables, as browsers read them); UTF-8 past U+10FFFF is
  invalid; the East Asian multi-byte charsets are not here (`iconv_open`
  fails with EINVAL, as for any charset it does not know).
- **`regex.mlx`**: `regcomp`, `regexec`, `regerror`, `regfree` as
  glibc's (its `regex_t` and `regmatch_t` layout, its error codes and
  messages), basic and extended expressions with GNU's operators
  (`\| \+ \? \w \s \b \< \>`), bracket expressions with the C locale's
  classes, back-references, `REG_ICASE`, `REG_NEWLINE`, `REG_NOSUB`,
  `REG_NOTBOL`, `REG_NOTEOL`, `REG_STARTEND`: a tree compiled into a
  program for a backtracking machine that finds the leftmost match and
  of those the longest, as POSIX asks, remembering the states it has
  tried so a search costs the program's length times the text's (but
  with back-references); a character is a byte.
- **`time.mlx`**: `clock_gettime`, `gmtime`/`localtime` (`std.timezone`),
  `mktime`/`timegm`, `strftime`, `asctime`.
- **`signal.mlx`**: `sigaction` with a restorer of its own, `signal`,
  `raise`, `kill`, sigsets, `sigprocmask`.
- **`pthread.mlx`**: threads on `@spawnThread` (clone) with their own
  blocks and stacks and the loaded modules' static TLS below the block
  (next to glibc, the program's TLS adopted from `/proc/self/auxv`),
  `pthread_join`/`detach` (a registry of the live threads: a handle
  already joined answers ESRCH), mutexes (normal, recursive,
  error-checking), condition variables, rwlocks, spin locks, barriers,
  semaphores, keys with destructors, `pthread_once`, names;
  `pthread_getattr_np` of the main thread describes the kernel's stack
  as glibc does (the mapping holding the first stack pointer in
  `/proc/self/maps`, as deep as `RLIMIT_STACK` allows), since
  JavaScriptCore's stack limits are taken from it.
- **`loader.mlx`**: the dynamic loader: the list of loaded objects (the
  first five words of a record are glibc's `struct link_map`, and
  `_r_debug` through the program's `DT_DEBUG`, so gdb lists the libraries),
  mapping an ELF file's segments, the search for a library (the requester's
  runpath with `$ORIGIN`, `LD_LIBRARY_PATH`, the loader's own directory
  (where its `libm.so.6` is), the system directories, `/lib/mlx` first), symbol lookup through `DT_GNU_HASH` or `DT_HASH` in
  load order (symbol versions are not compared), relocations (`RELATIVE`,
  `RELR`, `64`, `GLOB_DAT`, `JUMP_SLOT`, `COPY`, `IRELATIVE`, `DTPMOD64`,
  `DTPOFF64`, `TPOFF64`), static TLS laid out below each thread's control
  block (the executable's block right under the thread pointer, as its
  code assumes; `__tls_get_addr`; a thread that started before a dlopen
  gets the new module's image on first use), RELRO, constructors and
  destructors, `setjmp`/`longjmp` and `strtold` (which returns on the x87
  stack the 64-bit significand `stdlib.mlx` reads with `extended.mlx`,
  decimal and hexadecimal, rounded as glibc rounds) as machine code in a
  page of their own, the long double functions of libm in another,
  and the names glibc gives twice (`__environ`, `__progname`, `error`).
- **`start.mlx`**: `__mlx_start`, the entry: the main thread's control
  block and thread pointer (the stack protector's guard at `fs:0x28`), the
  aux vector, this library relocated against itself, the standard streams,
  the program's libraries, then the program's own `_start` entered with
  the stack as the kernel left it; `__libc_start_main` (what crt1.o
  calls): the program's constructors, `main`, `exit`; `__mlx_init`, the
  `DT_INIT` for the other loader's processes.
- **`dl.mlx`**: `dlopen`, `dlsym` (`RTLD_DEFAULT`, `RTLD_NEXT`), `dlclose`
  (the mapping stays), `dlerror`, `dladdr`, `dlinfo`, `dl_iterate_phdr`
  on the loader's list.

## Headers

mlx-cc's headers (compiler/cc/include) are mlxlibc's for what they
declare. What mlxlibc added after them has its headers made of Mlx
modules, as the C API shims beside it have theirs: `api/` declares
the types and functions C sees, and `mlx-cheaders` writes the headers
into `mlx-out/include/mlxlibc` (`./mlx build libc-headers`; `-I` that
directory): `iconv.h` and `libintl.h`.

## Checks

`tools/check_regex.sh` runs `tests/regex.c` (extended and basic
expressions, brackets, case, back-references, GNU's operators, the
execution flags, the errors and their messages: 104 lines) linked by gcc
against libmlxc and against glibc, and built by mlx-cc (whose `<regex.h>`
has glibc's layout) under libmlxc.so.1 as its loader: all print the same.

`tools/check_iconv.sh` runs `tests/iconv.c` (the Unicode forms, byte
order marks, invalid and incomplete input, a full output, `//TRANSLIT`
and `//IGNORE`, the names, input in pieces, every byte of the single-byte
charsets there and back, and gettext's functions) linked against libmlxc
and against glibc, and built by mlx-cc on the generated headers: all print
the same, but for the bytes where the Encoding Standard's tables are not
glibc's (the test names them).


`tools/mlxlibc_check.py` calls the same functions through ctypes in
libmlxc and in glibc and compares the answers (15 800 comparisons: strings,
ctype, malloc, stdlib, unistd, signals, printf with every conversion and
flag, stdio on files, time, pthread). Threads mlxlibc starts have mlxlibc's
block behind the thread pointer, not glibc's, so Python never runs in them:
their bodies are the library's own exports, while Python's threads (glibc
threads, foreign to mlxlibc) take its mutexes and condition variables.
`tools/mlxlibc_check.c` is the C side: linked against libmlxc and against
glibc, the two programs print the same 121 lines; the glibc-linked one run
under `libmlxc.so.1` as loader (no glibc in the process) prints the same
130 lines as under glibc, the dlfcn section on an Mlx plugin and the
program's `__thread` variables included; linked with libmlxc as its
`PT_INTERP` it runs on its own; the system's echo, env, cat and sh run
under the loader; and a C program crashing under it (a store through
address 8, `abort()`) is reported with its functions and kept in
`crashes.log`.

## What a program on mlxlibc gets, and does not yet

- Under the loader the process has no glibc at all: `__libc_start_main`,
  `main`, `exit` with the destructors, `dlopen`, thread-local storage are
  all this library's. Symbol versions (`GLIBC_2.34` and the like) are not
  compared: a name is bound to the first object defining it in load order,
  as musl does; a program asking for something not here stops with
  "symbol lookup error: undefined symbol".
- The loader installs `std.crash`'s handler before the program's
  constructors run (`__libc_start_main` installs it when another loader
  started the process): a crash in the program, its libraries or this
  library says the signal, the function from the file's symbol table and
  the callers on stderr, exits with 128 plus the signal and is kept in
  `$XDG_STATE_HOME/mlx/crashes.log`, as the desktop programs' and the
  coreutils' crashes are. A handler the program sets with `sigaction`
  replaces it; a process glibc started (ctypes) gets none. Run as
  `libmlxc.so.1 PROGRAM`, the process is named after PROGRAM
  (`prctl(PR_SET_NAME)`), so the report and `ps` say its name.
- Next to glibc (a program linked with `-lmlxc`, or ctypes), glibc's
  `ld.so` starts the process and owns `dlopen`; a `main` that returns goes
  through glibc's `exit`, which knows nothing of mlxlibc's `atexit`
  handlers and stdout buffer: call `exit()` or `fflush(stdout)`. A C
  program linked that way binds `__libc_start_main` here, so its
  constructors and `main` run from here too.
- `error()` is exported as `__mlx_error` (`error` is a keyword of Mlx); the
  loader binds the name `error` to it, as it binds `__environ` to `environ`
  and `__progname` to `program_invocation_short_name`.
- Wide-character stdio and locales beyond "C" are absent
  (so /usr/bin/printf and /bin/ls do not run yet); `dlclose` keeps the
  mapping; a library with initial-exec TLS loaded by dlopen after threads
  started does not reach them.
- `errno` of a thread mlxlibc did not start lives in a shadow block found
  by the thread pointer (up to the table's size), as the C `errno` of a
  glibc thread calling into mlxlibc.
