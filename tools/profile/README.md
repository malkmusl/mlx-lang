# mlx-profile

Where a program spends its time, without libc and without a kernel
profiler: `mlx-profile` starts the program under `ptrace` and stops it
every few milliseconds. At each stop it notes where the program is and
which functions called it, following the frame pointers that every Mlx
function keeps. The compiler's symbol table names each function
`path:line:name`; C libraries are named by their own symbols.

```
mlx-profile [-o DIRECTORY] [--every MILLISECONDS] [--wall] PROGRAM [ARGUMENT...]
```

- `--every`: how often to stop the program (default 5 ms).
- `--wall`: count the samples where the program waits in the kernel too.
  By default only samples where it runs count.
- The main thread is sampled; the program's other threads run on. The
  program is seized (`PTRACE_SEIZE`) and each stop is a
  `PTRACE_INTERRUPT` of the main thread alone: a stop signal would halt
  every thread, and a program waiting on a thread of its own (a Vulkan
  driver's) would stand still.

At the end it prints the functions most of the time went to:

```
mlx-profile: 512 samples (38 more while it waited)
  100% with calls, 0% itself: tools/codemap/main.mlx:1388:run
  57% with calls, 3% itself: tools/codemap/index.mlx:469:parseFile
  39% with calls, 0% itself: compiler/selfhost/parser/syntax.mlx:371:parseSource
```

It writes the profile to DIRECTORY, by default
`$XDG_STATE_HOME/mlx/profiles` (`~/.local/state/mlx/profiles`), as
`PROGRAM-SECONDS.profile`:

```
profile <TAB> program <TAB> seconds since 1970 <TAB> samples <TAB> interval in µs
self <TAB> total <TAB> function
```

[MLX Observatory](../../projects/observatory/README.md) reads the newest
profile of each program. It shows them in two places: the *Hot* finding
lists them, and *Color by → Run time* colors the galaxy by them. The
codemap notices new profiles while it is open. `mlx-codemap hot` lists
them too.

`tools/check_profile.sh` profiles `tests/support/profile_busy.mlx` and
checks that the time goes to `spin`, and to `work` and `main` through
it.
