# mlxlibc (libmlxc)

The C library, written in Mlx on system calls alone: what C programs and
our own shared objects need from libc (`malloc`, `printf`, `fopen`,
`pthread_create`, `strtod`, `qsort`, ...), as one shared object that needs
no other. It is the library under a GNU-free system: glibc and musl are
what it stands in for.

```sh
tools/build_mlxlibc.sh             # into mlx-out/libc
tools/check_mlxlibc.sh             # against glibc: ctypes, a C program
gcc program.c -L mlx-out/libc -lmlxc -Wl,-rpath,$PWD/mlx-out/libc
```

| Library | What it is | Built from |
| --- | --- | --- |
| `libmlxc.so.1` (`libmlxc.so`) | 800 exported C names: memory, strings, ctype, stdlib, errno, environment, process, time, the system call wrappers, signals, stdio, the printf and scanf families, pthread, getopt, dirent, err/warn, syslog, locale stubs, dl stubs | `libc.mlx` |

It is a `--plugin` shared object on a stack arena with no C library under
it (`mlx4 --plugin --stack-arena --no-libc --soname=libmlxc.so.1`, see
[Shared objects](../../docs/reference/formats.md#shared-objects)): each
exported function takes its aggregates from its own stack, so a call costs
no system call, and the image has no `DT_NEEDED` entry at all. A C program
linked against it (`-lmlxc` before glibc) binds every libc name it uses
here; `tools/check_mlxlibc.sh` builds `tools/mlxlibc_check.c` that way and
against glibc alone and compares what the two runs print.

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
- **`stdio.mlx`**: `FILE` records on descriptors (80 bytes, buffered,
  line-buffered on a terminal, a recursive lock each), `stdin`/`stdout`/
  `stderr` as data symbols (the handles 1, 2, 3), `fopen` .. `fclose`,
  `fgets`, `getline`, `fread`/`fwrite`, `fseek`/`ftell`, the printf and
  scanf families with their `__isoc99_`, `__isoc23_` and `_chk` names,
  `perror`, `tmpfile`.
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
  blocks and stacks, `pthread_join`/`detach` (a registry of the live
  threads: a handle already joined answers ESRCH), mutexes (normal,
  recursive, error-checking), condition variables, rwlocks, spin locks,
  barriers, semaphores, keys with destructors, `pthread_once`, names.
- **`dl.mlx`**: `dlopen`/`dlsym`/`dlerror` as stubs until the loader.

## Checks

`tools/mlxlibc_check.py` calls the same functions through ctypes in
libmlxc and in glibc and compares the answers (15 800 comparisons: strings,
ctype, malloc, stdlib, unistd, signals, printf with every conversion and
flag, stdio on files, time, pthread). Threads mlxlibc starts have mlxlibc's
block behind the thread pointer, not glibc's, so Python never runs in them:
their bodies are the library's own exports, while Python's threads (glibc
threads, foreign to mlxlibc) take its mutexes and condition variables.
`tools/mlxlibc_check.c` is the C side: linked against libmlxc and against
glibc, the two programs print the same 121 lines.

## What a program linked against libmlxc gets, and does not yet

- glibc still starts the program (`_start`, `__libc_start_main`) and sets
  up the main thread's TLS; everything the program itself calls is
  mlxlibc's. A `main` that returns goes through glibc's `exit`, which knows
  nothing of mlxlibc's `atexit` handlers and stdout buffer: call `exit()`
  (mlxlibc's, which flushes and runs the handlers) or `fflush(stdout)`.
  The loader of our own (`PT_INTERP`, TLS, `libc.so.6` with glibc's
  version names) is the next step.
- `error()` is not exported: `error` is a keyword of Mlx.
- `setjmp`/`longjmp`, wide-character stdio and locales beyond "C" are
  absent; `dlopen` and `dlsym` answer with an error until the loader.
- `errno` of a thread mlxlibc did not start lives in a shadow block found
  by the thread pointer (up to the table's size), as the C `errno` of a
  glibc thread calling into mlxlibc.
