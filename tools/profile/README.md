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

At the end it prints the functions most of the time went to, then what the
program took as its main thread ended: processor time of every thread
(the samples see the main one) and memory at most and at the end:

```
mlx-profile: 512 samples (38 more while it waited)
  100% with calls, 0% itself: tools/codemap/main.mlx:1388:run
  57% with calls, 3% itself: tools/codemap/index.mlx:469:parseFile
  39% with calls, 0% itself: compiler/selfhost/parser/syntax.mlx:371:parseSource
mlx-profile: 2870 ms of processor time (330 ms in the kernel), every thread; memory 470 MiB at most, 311 MiB at the end
```

For memory kept by mistake, `MLX_LEAK_CHECK` (std.leaks, see
[the standard library](../../docs/reference/stdlib.md)) says which blocks a
program's loop kept and where they were allocated.

It writes the profile to DIRECTORY, by default
`$XDG_STATE_HOME/mlx/profiles` (`~/.local/state/mlx/profiles`), as
`PROGRAM-SECONDS.profile`:

```
profile <TAB> program <TAB> seconds since 1970 <TAB> samples <TAB> interval in µs <TAB> processor | gpu
self <TAB> total <TAB> function
```

The program's GPU is profiled beside it: `mlx-profile` sets
`MLX_GPU_PROFILE` for the program, and std.gpu (through
`std.gpu.diagnostics`) times each pass with timestamp queries and puts
the time on the functions that recorded the pass. That profile is written
next to the processor's as `PROGRAM-gpu-SECONDS.profile`, its header
ending in `gpu` and its times in µs. A program can be run with
`MLX_GPU_PROFILE=1` (or a directory) without `mlx-profile` too. Both are
written through `std.diagnostics`, as the crash reports are.

[MLX Observatory](../../projects/observatory/README.md) reads the newest
profile of each program. It shows them in two places: the *Hot* finding
lists them, and *Color by → Run time* colors the galaxy by them. The
codemap notices new profiles while it is open. `mlx-codemap hot` lists
them too, `mlx-codemap gpu` the GPU's.

`tools/check_profile.sh` profiles `tests/support/profile_busy.mlx` and
checks that the time goes to `spin`, and to `work` and `main` through
it. `tools/check_gpu_diagnostics.sh` profiles the GPU of
`tests/support/gpu_diagnostics_program.mlx` on lavapipe.
