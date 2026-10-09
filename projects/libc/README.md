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
| `libmlxc.so.1` (`libmlxc.so`) | 830 exported C names: memory, strings, ctype, stdlib, errno, environment, process, time, the system call wrappers, signals, stdio (glibc's FILE layout), the printf and scanf families, pthread with thread-local storage, getopt, dirent, err/warn/error, syslog, locale stubs, dlfcn; and the dynamic loader behind `__mlx_start` | `libc.mlx` |

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
  `tmpfile`, glibc's `stdio_ext.h` (`__fpending`, `__freading`, ...).
- **`unistd.mlx`**, **`misc.mlx`**: the system call wrappers (`open`,
  `read`, `write`, `stat`, `mmap`, `ioctl`, `poll`, `epoll`, sockets,
  `fork`/`exec`/`wait`, ...), `getopt`/`getopt_long`, `dirent`, `uname`,
  `sysconf`, `err`/`warn`, `syslog`, `getpwnam`/`getgrnam` from the files,
  locale and gettext stubs.
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
  semaphores, keys with destructors, `pthread_once`, names.
- **`loader.mlx`**: the dynamic loader: the list of loaded objects (the
  first five words of a record are glibc's `struct link_map`, and
  `_r_debug` through the program's `DT_DEBUG`, so gdb lists the libraries),
  mapping an ELF file's segments, the search for a library (the requester's
  runpath with `$ORIGIN`, `LD_LIBRARY_PATH`, the system directories,
  `/lib/mlx` first), symbol lookup through `DT_GNU_HASH` or `DT_HASH` in
  load order (symbol versions are not compared), relocations (`RELATIVE`,
  `RELR`, `64`, `GLOB_DAT`, `JUMP_SLOT`, `COPY`, `IRELATIVE`, `DTPMOD64`,
  `DTPOFF64`, `TPOFF64`), static TLS laid out below each thread's control
  block (the executable's block right under the thread pointer, as its
  code assumes; `__tls_get_addr`; a thread that started before a dlopen
  gets the new module's image on first use), RELRO, constructors and
  destructors, `setjmp`/`longjmp` as machine code in a page of their own,
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

## Checks

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
`PT_INTERP` it runs on its own; and the system's echo, env, cat and sh run
under the loader.

## What a program on mlxlibc gets, and does not yet

- Under the loader the process has no glibc at all: `__libc_start_main`,
  `main`, `exit` with the destructors, `dlopen`, thread-local storage are
  all this library's. Symbol versions (`GLIBC_2.34` and the like) are not
  compared: a name is bound to the first object defining it in load order,
  as musl does; a program asking for something not here stops with
  "symbol lookup error: undefined symbol".
- Next to glibc (a program linked with `-lmlxc`, or ctypes), glibc's
  `ld.so` starts the process and owns `dlopen`; a `main` that returns goes
  through glibc's `exit`, which knows nothing of mlxlibc's `atexit`
  handlers and stdout buffer: call `exit()` or `fflush(stdout)`. A C
  program linked that way binds `__libc_start_main` here, so its
  constructors and `main` run from here too.
- `error()` is exported as `__mlx_error` (`error` is a keyword of Mlx); the
  loader binds the name `error` to it, as it binds `__environ` to `environ`
  and `__progname` to `program_invocation_short_name`.
- Wide-character stdio, locales beyond "C", `libm` and `iconv` are absent
  (so /usr/bin/printf and /bin/ls do not run yet); `dlclose` keeps the
  mapping; a library with initial-exec TLS loaded by dlopen after threads
  started does not reach them.
- `errno` of a thread mlxlibc did not start lives in a shadow block found
  by the thread pointer (up to the table's size), as the C `errno` of a
  glibc thread calling into mlxlibc.
