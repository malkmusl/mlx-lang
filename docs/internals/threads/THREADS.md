# Threads on brixOS and in std: the design

Goal: a program uses every processor: several flows of execution in one
process (one address space, its handles), natively on brixOS first, through
the Linux layer for foreign programs, and `std.thread` for Mlx programs on
brixOS and Linux.

All file:line references are to the tree as it is at 40b80d5aa (read only;
nothing was changed). "codemap" means `mlx-out/bin/mlx-codemap`.

**Work in progress in the tree.** While this was written, the parallel
"every processor by autodetection" work changed the working tree
(uncommitted):

- `layout.Memory.MAX_CPUS = 256`, and the kernel arena is shared out by
  `arenaShare/arenasInRegion/arenaOf(cpu, others)`.
- The scheduler's process table is now `processes: [*]Process` with
  `capacity`, made in frames for `LEAST = 16 + PER_PROCESSOR = 2` per
  further processor (16 with one processor, 142 with 64).
- smp's `idle` is a `processors.Mask`, and per-processor blocks, TSSs and
  GDTs are made in frames.

The line numbers below for `scheduler.mlx`, `smp.mlx` and `layout.mlx` are
HEAD's and will move. The design follows that work's pattern:

- The thread table is made in frames and sized by the processors found.
- Shootdown targets are a `processors.Mask`.
- Limits scale with the processors running, not with MAX_CPUS.

---

## 0. The decisions in one page

1. **Value arena: one arena per thread, reached through r14/r15 as today.**
   The compiler already reaches the arena only through r14 (base = the fill
   word) and r15 (end), and already bumps the fill with `lock xadd`. So the
   allocation sequence does not change at all. A new thread gets its own
   1 GiB + 8 arena (the same shape as the main thread's), and its first
   function sets r14/r15 with a new builtin, `@arenaSwitch(base, end) ->
   (usize, usize)`. A second builtin, `@arena() -> (usize, usize)`, reads
   them. The main thread keeps its `_start` arena byte for byte. Cost per
   allocation: unchanged (the same 8 instructions). The only difference is
   that the cache line is no longer shared between cores. Per thread: one
   reservation and one touched page.
2. **The kernel has threads, but no kernel stacks.** brixOS restarts a
   blocking system call when its thread runs again (`wait.mlx:1-4`,
   `scheduler.mlx:390-396`), so a thread record holds only registers, SSE,
   FS base and wait state. The process record keeps the address space,
   handles, identity, events and regions. The scheduler runs threads. A
   thread runs on a processor while another thread of the same process runs
   on another one.
3. **TLB shootdown** is the one new SMP mechanism. A space can now be live
   on several processors at once. Each unmap, narrowing or copy-on-write
   break sends an IPI (new `Vector.FLUSH = 0xF4`) to the processors that
   have the space loaded, and frees frames only after they acknowledge. A
   process with one thread never sends one, so it pays nothing.
4. **Native interface, calls 86 to 92:** `thread_create`, `thread_exit`,
   `memory_wait`, `memory_wake` (the futex, named as a wait on memory),
   `thread_self`, `thread_yield`, `thread_info`. There is a new handle kind
   `thread` (12) whose state is `ended`, so joins use the one `wait` call.
5. **Linux layer:** clone/clone3 with CLONE_THREAD, and futex (WAIT, WAKE,
   REQUEUE, CMP_REQUEUE, WAKE_OP, WAIT_BITSET, WAKE_BITSET, the PRIVATE and
   CLOCK_REALTIME flags; the PI operations stay ENOSYS). It also gets
   set_tid_address, gettid, tkill/tgkill, exit as opposed to exit_group,
   sched_getaffinity/getcpu, and per-thread signal masks, pending sets and
   alternate stacks. Everything runs on the same kernel thread and futex
   code.
6. **std:** `std.thread` provides spawn (a function and a context word),
   join, detach, `processorCount`, `currentId`, locals and a `Pool`
   (parallel for over every processor). `std.sync` provides Mutex,
   Condition, Once, WaitGroup and Semaphore. The atomics are the language's
   own (`@atomicLoad` ... `@fence`). A seam module `std/bootstrap/os/wait*`
   gives std native `memory_wait`/`memory_wake` on brixOS and futex on
   Linux. The modules in the compiler's import closure (`std.turns`,
   `std.crash`, `std.heap`) become thread-aware through **hooks**
   (function-pointer vars that std.thread sets), not through the new
   builtins. They have to stay buildable by the previous canonical compiler
   and mlx0 (`tools/check_bootstrap.sh:1-17`).
7. **Order of the work:** compiler builtins, then the kernel refactor (one
   thread per process, no behaviour change), then TLB shootdown, then the
   native thread calls, then @spawnThread natively on brixOS, then the std
   seam, then std.thread/std.sync, then the Linux layer, then mlxlibc on
   brixOS, then the consumers (splash bands, h264/h265 workers, a scaling
   benchmark at -smp 4/16/64), then scalability follow-ups. Section 8 has
   the details.

---

## 1. What exists today (verified)

### 1.1 SMP (the facts given were right)

- `layout.Memory.MAX_CPUS = 8` (`projects/brix/layout.mlx:29`).
  `ARENA_PER_CPU = ARENA_SIZE / MAX_CPUS` (`:30`).
  `arenaOf(cpu) = ARENA + cpu * ARENA_PER_CPU` (`:33`). The kernel's
  aggregate arena is 32 MiB at 80 MiB (`:21-22`), so it is 4 MiB per
  processor.
- `PerCpu` block: TRAP_STACK, USER_RSP, ARENA_BASE, ARENA_END, INDEX
  (`layout.mlx:94-100`). It is the kernel's GS base. The trap and SYSCALL
  paths load the processor's arena into r14/r15 from it
  (`projects/brix/image/blob.mlx:167-168, 415-416`). User r14/r15 are
  pushed and popped with the frame (`blob.mlx:400-401, 423-424`), and are
  zeroed when a program starts (`:264-265`).
- smp.mlx: `MAX_LISTED = 256` (`:52`), `STACK_PAGES = 64` (`:54`),
  `MAX_CPUS` (`:56`), `PerProcessor(T)` (`:59-72`). The kernel lock
  (`Lock`, `:115-130`) is taken with `lock(id)` (`:213`) and let go with
  `unlock()` (`:284`). A waiting processor watches `kernel.work` for
  ENDING/HOLDING (`:229-233`). Per-processor setup is in `prepare(k)`
  (`:596-614`), with each processor's arena share at `:605-610`. INIT/SIPI
  happens in `start()` (`:617-737`). `resetArena`/`arenaMark`/
  `arenaRelease` are at `:740-756`, `idleProcessor` at `:769`, and `wake`
  (an IPI with Vector.WAKE) at `:781`. Processors beyond `limit` stay in
  the trampoline (`:705-731`).
- The vectors are LAPIC_TIMER 0xF0, WAKE 0xF1, STOP 0xF2 and HOLD 0xF3
  (`layout.mlx:247-254`). STOP and HOLD are handled before the kernel lock
  is taken (`trap.mlx:71-81`).
- The scheduler has `MAX = 16` processes (`scheduler.mlx:35`). It keeps
  `processes: [MAX]Process` and per-processor `current`/`startedAt`
  (`:39-43`). `running()` is at `:63`, `terminate` at `:161`, `stop` at
  `:221`, `send` at `:250` and `canGo` at `:352`. `suspend` (`:385`)
  restarts the call: `rip -= 2`, `rax = number`. `scan` (`:416`) runs
  round robin over processes, and `switchTo` (`:502`) contains the idle
  loop, which lets the kernel go (`:551-561`). There are 28 places that
  name MAX_CPUS or PerProcessor (`mlx-codemap refs Memory.MAX_CPUS`):
  memory.mlx:67, 360, 367, 393-394, 602, 678; scheduler.mlx:492;
  process.mlx:178; image/blob.mlx:567, 577; and test 476.

### 1.2 The process: one flow today

`Process` (`projects/brix/kernel/process.mlx:60-150`) mixes resources with
the state of one flow:

- Flow state (to move into the thread): `waitingFor` (`:101`), `waitItems`
  and `waitCount` (`:103-104`), `timedOut` (`:106`), `inCall` (`:109`),
  `context` (`:115`), `sse` (`:117`), `fsBase` (`:118`) and `cpuTime`
  (`:132`). `lastCpu` (`:122`) stays with the space.
- Resources (stay): `pid`, `parentPid`, `group`, `ending`, `change`,
  `events`, `signals` (the actions part), `directory`, `program`, `umask`,
  `user`/`group_id`, `space`, `entry`/`stack`, `breakStart/End`,
  `mmapNext`, `regions[MAX_REGIONS = 256]` (`:79`), `files` and `roles`.
- `State` is `{free, ready, blocked, zombie, stopped, running}` (`:44`).
- `fault()` (`:483-509`) returns **false** when the page is already there
  (`:488`). With two threads that fault on the same page, the second one
  would get SIGSEGV. This is a bug that threads expose; it is fixed in
  §3.8.

There is no thread: `thread_pointer_set` (native call 69) only writes
`p.fsBase` and the MSR (`native/calls.mlx:263-269`). The Linux layer
answers clone with CLONE_THREAD with ENOSYS (`linux/system.mlx:321-325`),
clone3 with ENOSYS (`:328`), gives GETTID the pid (`:291`), returns 1 from
SET_TID_ADDRESS (`:356`), returns 0 for SET_ROBUST_LIST and RSEQ (`:355`),
answers SCHED_GETAFFINITY as "CPU 0 only" (`:378-383`), treats EXIT and
EXIT_GROUP alike (`:203-207`), and sends futex (202) to `sayUnknown`
followed by ENOSYS (`:386-389`).

### 1.3 The value arena (the compiler)

- `_start` maps 1 GiB + 8 bytes (`capacity = 1073741832`). On brixOS it
  uses its own `memory_map` (rax = 0x4010); on Linux it uses mmap with
  MAP_NORESERVE. It then sets **r14 = base, r15 = base + capacity, and
  [base] = base + 8** (`compiler/selfhost/backend/x86_64/codegen/emitter.mlx:194-215`,
  called from `emitStartStub` at `:108-157`).
  `tools/ensure_compiler.sh:65-77` (`maps_brixos_arena_natively`) checks
  for `b810400000`, which is `mov eax, 0x4010`.
- An allocation (`codegen/memory.mlx:103-114`, `emitArenaAllocation`) is:

  ```
  mov  rcx, size8          ; aligned to 8
  mov  rax, rcx
  lock xadd [r14], rax     ; rax = the old fill = the value's address
  mov  rdx, rax
  add  rdx, rcx
  cmp  rdx, r15
  jbe  +2
  ud2                      ; the arena is full: SIGILL, std.crash says so
  ```

  **The fill word is reached through a register, never through a global
  symbol.** The one exception is a dynamically linked program: `_start`
  also stores r14/r15 into the data words DATA_ARENA/DATA_ARENA_END
  (`emitter.mlx:119-123`, `encoder.mlx:30-34`). Exported functions reload
  them when C code calls them on any thread (`labels.mlx:73-87`,
  `enterFromForeignCode`; spec `spec/01-abi/foreign-abi.xml:9`). That is
  why the bump is atomic. Plugins map an arena per export call
  (`labels.mlx:101-138`, `makePluginArena`), or reserve it on the stack
  with `--stack-arena`, which is what mlxlibc does.
- r14/r15 are never homes of Mlx locals. Only mlx-cc's C code uses them as
  homes (`codegen/state.mlx:33-37`, `homeRegister` at `:1177-1183`). So
  switching r14/r15 inside a Mlx function cannot clobber a cached copy:
  every allocation reads the registers again.
- aarch64 uses a helper, `__mlx_arena_alloc`, with a **global**
  `__mlx_arena_base` that is mapped lazily and bumped with ldxr/stxr
  (`backend/aarch64/codegen/runtime.mlx:6-10, 109-170`).
- The freestanding kernel takes its arena in rdi/rsi (`emitter.mlx:170-188`).
  The per-processor kernel arenas are the precedent for "one arena per
  flow by switching r14/r15".
- The thread builtins already exist (spec `spec/00-language/atomics-tls.xml`,
  `<Threads>`): `@threadPointer` (`memory.mlx:126-131`, `mov rax, fs:[0]`),
  `@frameAddress`, and `@spawnThread(flags, stack, parent_tid, child_tid,
  tls, entry, argument)` (`memory.mlx:140-171`). `@spawnThread` is a Linux
  clone (56). The child pops argument and entry, calls entry, and leaves
  with Linux exit (60). **The child inherits r14/r15, so it shares the
  parent's arena.** Lowering is in `ir/lower.mlx:3446-3480` and the opcode
  in `ir/lir.mlx:90-98`. Sema is in `sema/builtins/kind.mlx:62`,
  `registry.mlx:92,113`, `analyze.mlx:44`, `atomic_arguments.mlx:49` and
  `sema/declarations.mlx:1310`. The probe is `lowers_thread_builtins`
  (`ensure_compiler.sh:297-307`).
- `threadlocal` is parsed and rejected (`ir/lower.mlx:579`,
  `tests/186_threadlocal_abi_gap.mlx`).

### 1.4 What already runs threads (all on Linux, all sharing the main arena)

- `std/src/h264/decode.mlx:5065-5112` and `std/src/h265/decode.mlx:4241`:
  workers started with `@spawnThread`, Linux futexes, and
  rt_sigprocmask in the worker. The worker is an `export fn`, so in a
  plugin it gets an arena per call.
- `projects/desktop/splash/bands.mlx:66-116`: a crew of up to 7 threads
  that draws bands. On brixOS it is switched off ("No threads on brixOS:
  the bands are drawn by this one", `projects/desktop/splash/brixos.mlx:558-559`).
- mlxlibc's pthreads (`compiler/libc/pthread.mlx:1-7`). `pthread_create`
  is at `:256-299`, with flags VM|FS|FILES|SIGHAND|THREAD|SYSVSEM|SETTLS|
  PARENT_SETTID|CHILD_CLEARTID|CHILD_SETTID (`:290`). Joins use the TID
  word through futex (`:304-319`). The TCB is at the stack's top and is
  the thread pointer (`tcb.mlx:1-33`). On brixOS mlxlibc translates calls
  to native ones (`compiler/libc/sys_calls.brixos.mlx`), but threads
  answer ENOSYS (`:11-13, :769`) and the futex is a stub that never waits
  (`:805-810`). gettid gives the pid (`:742`), exit and exit_group both
  end the process (`:755-758`), and affinity says one CPU (`:797-803`).
- Tests: `tests/298_spawn_thread_runtime.mlx`, `315_tinypage_runtime.mlx`
  (4 threads), `180_linux_futex_runtime.mlx`, and `295_atomics_runtime.mlx`
  (glibc pthreads from a linked program). `tools/mlxlibc_check.c:284-345`
  covers the pthread section (4 threads, mutex, cond, once, key, barrier),
  run by `tools/check_mlxlibc.sh`.

---

## 2. The value arena: one per thread

### 2.1 What the shared arena would actually break

Two threads running Mlx code at once **do not corrupt the fill word
itself**: `lock xadd` makes concurrent bumps safe, and it was made atomic
for C threads that call exports. What breaks is everything around it:

1. **Contention.** Every struct, array or tuple value that is not a field
   is bumped from one cache line (`projects/desktop/compositor/arena.mlx:3-11`).
   An uncontended `lock xadd` is about 20 cycles. With N cores it becomes a
   cross-core RFO, roughly 100 to 300 cycles and more across sockets, on
   almost every function. At 64 cores the arena alone would serialize the
   program.
2. **std.turns frees other threads' live values.** `turns.turn()` stores
   the loop's mark back into the shared fill (`std/src/turns.mlx:206-225`).
   Everything another thread allocated since then is handed out again, and
   with `MLX_ARENA_POISON=1` it is overwritten. This is already a latent
   bug: the splash crew and the decoder workers share the arena of a main
   thread that may run a turns loop.
3. **Capacity and blame.** The 1 GiB is shared. A worker that never
   releases fills it for everyone, and the `ud2` happens in whichever
   thread comes next. std.crash's "arena full" path (`std/src/crash.mlx:304-311`)
   moves the shared fill under the other threads.

### 2.2 The decision

**Each thread has an arena of its own with exactly the main arena's
shape:** 1 GiB + 8 bytes reserved (MAP_NORESERVE on Linux; on brixOS
`memory_map`, where pages come when touched). The fill word is at its base.
The thread's r14 points at its base and r15 at its end. The compiler's
allocation code is not touched.

A spawned thread's arena carries a 4 KiB header:

```
base + 0        fill word (starts at base + 4096)
base + 8        std.thread's Block (magic, self, ids, exit word, result,
                locals, hooks' slots ...; at most 4088 bytes, see 6.2)
base + 4096     the thread's values
base + 1073741832  end (r15)
```

The main thread's arena is not changed in any way: there is no header,
the fill starts at base + 8 as `_start` sets it, and `_start` is
byte-identical. The main thread's Block is a static in std.thread.

### 2.3 Compiler changes (step 1)

Two builtins, documented in `spec/00-language/atomics-tls.xml` under
`<Threads>`:

- `@arena() -> (usize, usize)` returns the calling thread's value arena as
  (base, end) (x86_64: r14, r15).
- `@arenaSwitch(base: usize, end: usize) -> (usize, usize)` makes the
  arena at `base` (its fill word first) the one the calling thread's values
  go to from here on, and returns the previous one. It is valid until the
  function that switched returns into a frame that kept other registers:
  in an `export fn` the epilogue restores the caller's r14/r15
  (`labels.mlx:73-87`).

Codegen in `backend/x86_64/codegen/memory.mlx`, beside `emitThreadPointer`:

```
; @arena():          mov rax, r14 ; mov rdx, r15   -> the 2-word result
; @arenaSwitch(b,e): load b -> rcx, e -> rdx
;                    mov rax, r14 ; mov r8, r15    (the old pair, the result)
;                    mov r14, rcx ; mov r15, rdx
```

Files:

- `sema/builtins/kind.mlx`: add `arena` and `arenaSwitch`.
- `sema/builtins/registry.mlx`: names and arity (0 and 2).
- `sema/builtins/analyze.mlx`: the result is a tuple of two usize.
- `sema/builtins/atomic_arguments.mlx`: the arguments are integers, with
  the same message style as `@spawnThread`.
- `ir/lir.mlx`: opcodes `arena_get` and `arena_switch`.
- `ir/lower.mlx`: lowering next to `lowerSpawnThread`.
- `ir/optimize.mlx` and the inliner: `arena_switch` is a barrier. No
  `alloca` with an arena length is moved across it, and a function that
  contains it is not inlined into another.
- `backend/x86_64/codegen/memory.mlx` emits the instructions.
- On aarch64 `@arenaSwitch` is a compile error (a new diagnostic code in
  `spec/02-compiler/diagnostics.xml`: per-thread arenas need the x86_64
  backend). `@arena()` returns (`__mlx_arena_base`, base + capacity).
  On aarch64 threads keep sharing the atomic global arena (§6.8).
- `spec/02-compiler/x86_64.xml` gets the arena ABI rule. Today it is only
  in foreign-abi.xml:9. The rule: r14 = arena base (whose first word is
  the fill), r15 = its end, the bump is `lock xadd`, and r14/r15 are
  reserved.
- `tools/ensure_compiler.sh` gets the probe `switches_arenas`: a program
  that switches to a second mapped arena, allocates an address-taken
  local, and exits 13 when the local lies in the second arena. Register it
  in the feature list beside `lowers_thread_builtins` (`:1157`).
- Test: `tests/528_arena_switch_runtime.mlx`.

### 2.4 Thread start (runtime)

std.thread's entry is an Mlx function that does nothing before the switch:

```
// std/src/thread_os*.mlx: no address-taken local, no aggregate value
// before the switch; the user's entry is called through a pointer (never
// inlined into this frame)
fn threadMain(block: usize) -> usize {
    const b = blockAt(block)
    _ = @arenaSwitch(b.*.arenaBase, b.*.arenaEnd)
    hooks.enterThread(b)              // crash's per-thread stack (Linux), name
    const entry: Entry = b.*.entry
    b.*.result = entry(b.*.context)
    return b.*.result                 // Linux: @spawnThread's child exits with it
}                                     // brixOS: thread_exit(result), see 6.3
```

The kernel (natively) and Linux's clone both start the child with the
parent's general registers, so r14/r15 still point at the parent's arena
for the few instructions before the switch. That is safe because the bump
is atomic and threadMain allocates nothing there.

### 2.5 Cost

- **Per allocation: unchanged.** It is the same 8 instructions, and the
  `lock xadd` now hits a cache line that only this core writes (about 20
  cycles, which single-threaded programs already pay). Removing the `lock`
  for thread-private arenas (an option such as `--private-arenas`) would
  save about 18 cycles per allocation. It is left out because a linked
  program's exports may run on C threads that share the main arena.
- **Per thread:** one 1 GiB + 8 reservation (address space only; 64
  threads take 64 GiB of the 128 TiB user half), one touched page for the
  header and Block, plus its stack. On brixOS that is one region each for
  arena and stack (see 3.10).
- **Thread identity costs nothing:** `std.thread.current()` reads r14
  (`@arena()`), one register move. No FS is needed, so it also works in
  linked programs where the C library owns FS.

### 2.6 How the main thread stays exactly as it is

- `_start` is unchanged and so is the allocation sequence. No Mlx code runs
  differently unless it calls the new builtins.
- The closure modules change only through **hooks** that are null unless
  std.thread sets them. A program that does not import `std.thread` runs
  the old paths instruction for instruction (`turns.locate()` still walks,
  crash still uses `arenaBase`, and so on).
- On brixOS the kernel refactor (step 2) is behaviour-neutral: one thread
  per process, the same scheduling order and the same restart model.

### 2.7 std.turns

`std/src/turns.mlx` is in the compiler's import closure (`:11-13`), so it
cannot use `@arena()` until two canonical compiler generations have it
(`tools/check_bootstrap.sh:1-17`). The changes are:

- `pub var threadArena: ?fn() -> usize = null` (the same pattern as
  `page_allocator.mlx:22-23`). `locate()` (`:38-75`) asks it first. A
  non-zero answer is the calling thread's arena, and it is not cached in
  `located`, because `located` caches the main arena only. std.thread
  installs `fn() -> usize { return @arena().0 }` at its init.
- `releasedTurns += 1` (`:208`) becomes `@atomicRmw(usize, &releasedTurns,
  add, 1, monotonic)`. `noteReleased` (`:203`) gets the same change.
- `residency` (`:96`), mincore's scratch byte, is only used on the main
  thread's walk. Leave it.
- `ARENA_SIZE` (`:20`) remains right because thread arenas have the same
  size. `used()` counts the 4088 header bytes of a thread arena. That
  constant does not matter for marks.
- Rule (in the module's comment and std.thread's): a turn frees only the
  calling thread's values. A value handed to another thread must be heap
  memory, or must belong to a thread that does not release it while the
  other uses it.

### 2.8 std.heap and its allocators

- `std/bootstrap/general_purpose_allocator.mlx` already says "for every
  thread of a process at once" (`:1-4`). It has one futex lock (`:17`,
  `:96-115`), but it sleeps through `linux.thread.futexWait`. On brixOS
  that call goes to the Linux layer (ENOSYS today), so a contended lock
  spins. **Change:** use the OS seam (6.5): `wait.waitWord(&lock, 2,
  FOREVER)` and `wait.wakeWord(&lock, 1)`.
- `std/src/heap.mlx:69-87`: `make()` spins with `linux.thread.yield()`.
  Change it to `wait.yieldProcessor()`.
- `std/bootstrap/os/pages.brixos.mlx:36-41`: `sleepOn`, `wakeOne` and
  `yield` are no-ops ("a brixOS process runs one thread"). The tinypage
  allocator (`tinypage_allocator.mlx:122,148,155`), the debug allocator
  (`debug_allocator.mlx:150,157`) and diagnostics (`diagnostics.mlx:462,470`)
  sleep through them. **Change:** have them call the native `memory_wait`,
  `memory_wake` and `thread_yield`.
- Scalability, a later step (8.11): a per-thread cache of up to 32 blocks
  per class (magazines) for classes of 512 bytes or less, kept in the
  thread's Block. The hook `pub var threadCache: ?fn() -> usize = null` in
  the GPA tries the cache without the lock and refills or flushes 16
  blocks at a time under the lock. A thread's cache is flushed at its end
  by std.thread. Until then a single lock with spin-then-wait is correct,
  but serializes allocation-heavy workers.

### 2.9 std.crash

- `crash_os.brixos.mlx:38`: `threadId()` returns 1. Change it to
  `brix.threadSelf()`. The `reportingThread` logic (`crash.mlx:155-159,
  284-296`) then works on brixOS as it does on Linux: a second faulting
  thread waits while the first reports.
- `crash_os.brixos.mlx:47-48` (`idle`, "nobody else says a crash") stays
  correct.
- Which arena is full: today `arenaBase` is found once (`crash.mlx:114,
  1303`). In the handler, take the faulting thread's r14 from the context
  (gregs index 6, `REGISTER_NAMES` at `crash.mlx:95`) and test
  `turns.left(thatBase)`. The handler runs with the faulting thread's
  r14/r15: a signal handler and the upcall keep the interrupted
  registers (`events.mlx:234-245`). So its own values go to that arena.
  For the main arena, keep today's `arenaRoom` trick (`crash.mlx:115-121,
  1304-1309`). For a thread arena, std.thread does the same at spawn: the
  fill starts at `base + 4096 + ARENA_ROOM`, and the Block records
  `crashRoom = base + 4096`. On a crash with a full thread arena, the
  handler calls `turns.moveFill(base, crashRoom)` (the hook gives
  `crashRoom` for a base).
- Per-thread handler stack on Linux: `sigaltstack` is per thread, but
  `install()` (`crash.mlx:1297-1316`) sets it for the calling thread only.
  Add `pub fn installThread() -> bool`, which maps a 256 KiB stack and
  calls sigaltstack. std.thread's `threadMain` calls it through the hook.
  On brixOS the upcall stack is the process's one (`upcalls.mlx:72-84`),
  and the kernel serializes upcalls (3.7), so nothing more is needed.
- The context text (`crash.mlx:122-129`, `contextPointer/Length`) is the
  "while ..." of a report and is per thread in meaning. Give it the hook
  `pub var contextSlot: ?fn() -> usize = null`, which returns the address
  of two words in the thread's Block. Fall back to the globals.
- The report's first line says the thread: `thread 4711 (pool-3)` from the
  Block's id and name when the hook answers.

### 2.10 Other process-global state in std (the full list for threads)

| Module (file:line) | State | With threads |
|---|---|---|
| `std/src/turns.mlx:36,96,198` | `located`, `residency`, `releasedTurns` | 2.7 |
| `std/src/crash.mlx:114-178` | arenaBase/Room, context, report* | 2.9; report* is already guarded by `reportingThread` |
| `std/src/crash_os.brixos.mlx:30-34` | `handler`, `faultInfo`, `faultRegisters` | used only inside the serialized upcall: fine |
| `std/src/leaks.mlx:81-91,264-265` | debug allocator state, `arena/arenaMark`, `buffers`, `snapshotPath` | the debug allocator has its own lock (`debug_allocator.mlx:150-162`); the arena marks watch the main arena only (document); snapshots are main-thread only |
| `std/src/diagnostics.mlx:438,624-630` | GPU memory and frame counters | UI main thread only; the log lock already sleeps through `pages.sleepOn` |
| `std/src/events.mlx:25` | handlers table | set at init: fine |
| `std/bootstrap/os/upcalls.mlx:43-53` | handlers, handled, queued, installed, takenRecords | takenRecords is used only inside the upcall, which the kernel serializes (3.7): fine; `handle()`/`queue()` stay init-time calls |
| `std/bootstrap/os/host.brixos.mlx:15-19,245,468` | `remembered`, `cookies`, `primed`, `infoScratch`, `selfUser` | `prime()` races: make it an atomic once. `infoScratch` becomes a local. `remembered` writes are word stores of a cache (a stale handle is asked for again): benign. `cookies` is per descriptor, which two threads cannot share meaningfully anyway |
| `std/bootstrap/page_allocator.mlx:22-23` | watch hooks | set once |
| `std/bootstrap/tinypage_allocator.mlx:75` | `region` | CAS-made (`:102-122`) |
| `std/src/heap.mlx:51-57` | own, ownState, heap, words | CAS-made (`:78-86`) |
| `std/bootstrap/debug_allocator.mlx:444` | `pageWatcher` | set once |
| `std/src/sql/os.brixos.mlx:27-36` | spin `lock`, `threadId() = 1` | **broken with threads**: a recursive mutex would treat every thread as the owner. Move it to std.sync |
| `std/src/fontconfig/mem.mlx:529-556`, `std/src/shaping/memory.mlx:253-262` | own futex and spin locks, `gettid` | fontconfig is fine once Linux-layer gettid is per thread; on brixOS it should use the seam. shaping's spin lock is correct but burns CPU |
| `compiler/libc/sys_calls.brixos.mlx:805-810` | futex stub | step 9 |

`mlx-codemap find` and `refs` on `threadId` and `sleepOn` are the way to
re-check this list before step 6.

---

## 3. The kernel

### 3.1 Records

New file `projects/brix/kernel/thread.mlx`:

```
pub const ThreadState = enum(u8) { free, ready, running, blocked, stopped, ended }

// The table: room for THREADS_LEAST threads with one processor and
// THREADS_PER_PROCESSOR more for each further one that runs the kernel
// (768 with 64 processors, 2296 with 256), made in frames the first time
// (as the working tree's process table is), at most THREADS_MOST.
pub const THREADS_LEAST: usize = 256
pub const THREADS_PER_PROCESSOR: usize = 8
pub const THREADS_MOST: usize = 4096
// A process's threads at most (`too_many` past it): two for each
// processor running, at least 256 (perProcess(), computed once at start).
pub const PER_PROCESS_LEAST: usize = 256
// Threads kept for user 0's processes: another user's together may not
// take more than capacity - SYSTEM_RESERVE.
pub const SYSTEM_RESERVE: usize = 128

pub const Thread = struct {
    state: ThreadState
    // Its id: from the scheduler's pid counter (nextPid), so ids and pids
    // never meet; a process's first thread has the process's pid.
    id: usize
    process: u16                  // index in Scheduler.processes
    // Handles naming it (files.Kind.thread slots); detached: freed at its
    // end, else kept (ended) until the last handle closes.
    handles: u16
    detached: bool
    // The flow, as it was in Process (process.mlx:101-132)
    context: Frame
    sse: [frame_module.Sse.SIZE]u8
    fsBase: usize
    waitingFor: wait.Wait
    waitItems: [wait.HandleWait.MAX]wait.HandleWait
    waitCount: usize
    timedOut: bool
    inCall: bool
    // memory_wait: woken by memory_wake (the restarted call returns 0),
    // its key, its bucket chain, the Linux layer's bitset.
    woken: bool
    futexSpace: usize
    futexAddress: usize
    futexMask: u32
    futexNext: u16                // thread index + 1; 0: last
    // The word cleared and woken at its end (thread_create's exit word,
    // CLONE_CHILD_CLEARTID, set_tid_address); 0: none.
    exitWord: usize
    value: usize                  // thread_exit's value
    cpuTime: usize
    lastProcessor: usize
    // The Linux layer's per-thread view (signals.mlx's split, 5.6)
    signalMask: usize
    signalPending: usize
    senders: [Signal.COUNT]u32    // (pending thread-directed signals' senders)
    suspended: bool
    kept: usize
    altStackBase: usize
    altStackSize: usize
}
```

That is about 1.5 KiB. The records live in **frames**: one
`memory.allFrames().allocateContiguous(1)` per thread, made at
`createThread` and given back when it is freed. The scheduler keeps an
index table `threads: [*]usize` of `threadCapacity` words (the frame's
kernel address, 0 = free), made in frames at the first process, as the
working tree's process table is (`Scheduler.make`). Nothing is added to
the kernel's .bss, and there is no change to the image's room below
`Memory.ARENA`.

`Process` loses the flow fields and gains:

```
threadCount: u16       // threads not yet freed
runningCount: u16      // threads on a processor right now
mainThread: u16        // index + 1 of its first thread (0: ended)
upcallThread: u16      // index + 1 of the thread in the upcall (0: none)
exiting: bool          // it ends: no thread of it runs again (3.9)
```

`Process.state` keeps `free`, `zombie` and `stopped` for the process, plus
`ready`, meaning alive. `running` and `blocked` become thread states.
`process_list`'s state (`native/processes.mlx:292-310`) is derived:
running if a thread runs, else ready if one is ready, else blocked;
stopped and ended as now. Processor time is the sum of the threads'
`cpuTime`, the ended ones folded into `Process.cpuTime`.

### 3.2 The scheduler (`scheduler.mlx`)

- `current: PerProcessor(usize)` (`:42`) now holds a **thread** index.
- New `runningThread() -> *Thread`. `running()` (`:63`) returns
  `processOf(runningThread())`, so the hundred or so callers that take
  `p: *Process` keep working.
- The thread table gets a dense list of live indices (`live: [*]u16` of
  `threadCapacity`, `liveCount`). `scan` (`:416-440`), `runnable` (`:443`) and
  `nearestDeadline` (`:456`) walk it, not every slot of the table. A thread is
  never picked while its process is `stopped` or `exiting`.
- `canGo` (`:352-375`) takes the thread and gets two new kinds:
  `WaitKind.memory` (`t.woken`) and `WaitKind.upcall` (`p.upcallThread ==
  0`). Deliverability looks at the process's queue (native) or at
  `t.signalPending | p.signals.pending & ~t.signalMask` (Linux view).
- `suspend` (`:385-401`), `preempt` (`:403`), `stop` (`:221`) and `resume`
  (`:232`) work on the thread. Stop and resume act on **every** thread of
  the process (3.9).
- `switchTo` (`:502-561`): `Process.runningCount` is updated as the
  processor drops its thread and takes the next one. It loads
  `t.context`, `t.sse` and `t.fsBase` (`:519-530`). The space switch
  (`:526-528`) keeps `p.lastCpu` per process, but the reload rule is now
  "the space is not loaded here" (`active[id] != root`). That is safe
  because the shootdown (3.4) keeps every loaded copy coherent.
- `charge`/`cpuTimeOf` (`:479-500`) charge the thread.
- `create` (`:114`), `fork` (`:140`) and `createShell` (`:87`) create the
  process **and its first thread** (id = pid).
- `freeAllButRunning` (`:68`) frees the threads of the freed processes.
- Fairness is round robin over threads. A process with 64 threads gets 64
  shares, as on Linux without group scheduling. That is deliberate:
  "use every processor".

### 3.3 Traps (`trap.mlx`)

- `handle` (`:165-288`) works on `t = runningThread()` and `p = its
  process`. `after` (`:326-337`) maps outcomes as now, plus
  `OutcomeKind.thread_ended`, which switches away without ending the
  process.
- `deliver` (`:341-358`) checks, before events, whether the thread's
  process is exiting or stopped. If it is exiting, the thread ends here
  (`endThread`) and the processor switches. If it is stopped, the thread
  stops. A kicked processor (3.9) gets here.
- `deliverNative` (`:362-397`) takes only the events this thread may run
  (3.7). The upcall is entered only when `p.upcallThread` is 0 or this
  thread. Otherwise the thread parks with `WaitKind.upcall`.
- The page fault path (`:189-193`) is unchanged, but see 3.8 for the fix
  in `Process.fault`.
- New vector `FLUSH` handled before the lock, like HOLD (`:76-81`):
  `smp.flushHere(); smp.endOfInterrupt(); return`.

### 3.4 TLB shootdown (smp.mlx, memory.mlx, layout.mlx)

Until now a space was loaded on one processor at a time, so `Space.flush`
(`memory.mlx:600-603`), `release` (`:500-502`) and `copyOnWrite`
(`:653-670`) told only the current processor. With threads that is
**memory corruption**: another processor keeps a stale translation to a
frame that is already free or copied.

- `layout.Vector.FLUSH: usize = 0xF4`.
- `smp.mlx`: `flushAsked: PerProcessor(usize)` and `flushDone:
  PerProcessor(usize)` (generations).
  - `pub fn shootdown(root: usize) -> void` gathers the targets into a
    `processors.Mask` (the working tree's type, as `idle` is): every
    processor `x != current` with `memory.loaded(x) == root`. For each
    target it bumps `flushAsked[x]` and sends FLUSH. Then it waits until
    `flushDone[x] == flushAsked[x]` or `loaded(x) != root` for every
    target, at most 100 ms, and says on the log when a processor did not
    answer. With 256 processors a broadcast IPI (shorthand "all excluding
    self") is cheaper than 255 sends when the mask is large. That choice
    is the sender's: x2APIC and xAPIC both have the shorthand.
  - `pub fn flushHere()` reloads CR3 (`cpu.Cpu.usePageTables(current
    CR3)`) and sets `flushDone[self] = flushAsked[self]`.
  - The `lock()` spin loop (`:226-250`) calls `flushHere()` when its
    request is pending. A processor that waits for the kernel has
    interrupts off and would never take the IPI. This mirrors
    `Waits.HOLDING` (`:231-233`).
- `memory.mlx`:
  - `drop` (`:507-512`) returns the frame instead of freeing it.
  - `Space.release` (`:500`) becomes drop, then `invalidatePage`, then
    shootdown, then free.
  - `Process.giveBack` (`process.mlx:433-450`) collects the dropped frames
    (in the processor's kernel arena), then flushes once, shoots down once
    and frees them.
  - `copyOnWrite` (`:653-670`) shoots down the page **before**
    `frames.free(frame)`.
  - `Space.clone` (`:590-598`, fork narrowing pages to read-only)
    shoots down after its local flush.
  - `destroy` (`:675-714`) runs only when `runningCount == 0` (3.9), so no
    processor has the space loaded any more.
  - A processor with nothing to run loads the boot tables
    (`scheduler.mlx:547`), so it is never a shootdown target.
- **Cost for single-threaded processes: zero IPIs.** Only the processor
  running the process has `active == root`. The check is one loop over
  `found.running` words, done only when the process has more than one
  thread.
- A boot self-test, `smp.testShootdown()` (after `armHold`), sends FLUSH
  to every running processor and expects every acknowledgement.
  `check_brix_smp.sh` greps for "brixOS: TLB shootdown answered by N
  processors".

### 3.5 Creating, ending, joining

- **thread_create** (native 86) and Linux clone/clone3 call
  `Scheduler.createThread(p, frame, start) -> ?*Thread`:
  1. Check the limits: the process's `threadCount < perProcess()` (max(256, 2 × running)), a
     non-zero user's total `< threadCapacity - SYSTEM_RESERVE`, and a free
     slot. Otherwise `too_many`.
  2. Allocate the record frame and set `id = nextPid++`.
  3. Copy the caller's frame (all general registers: r14/r15 included,
     which is the contract @spawnThread relies on). Then set `rip =
     entry`, `rsp = stack - 8` with a zero word written at `stack - 8`
     (native), `rdi = argument`, `rsi = second` and `rax = 0` (in the
     native contract; for Linux clone the child keeps rip and rsp =
     newsp), reset SSE (native; clone copies it) and set `fsBase`.
  4. Write the id to the id word, if given (`p.prepare` first).
  5. Set `exitWord`, put the thread in the ready state, and `kickIdle()`.
- **Ending a thread**, `endThread(t, value)`, runs from thread_exit, Linux
  exit, or a whole-process end:
  1. Cancel any futex wait (`futex.cancel(t)`) and remove the thread from
     the upcall if it held it (that releases parked threads).
  2. If `exitWord != 0` and the word is still mapped, store 0 and run
     `futex.wake(key(p, exitWord), ALL)`.
  3. Set `t.value = value` and `t.state = ended`. Handles of kind thread
     now show `ENDED`. A detached thread with no handles is freed
     (record frame back, slot free).
  4. Decrement `p.threadCount`. If it was the **last** thread and the
     process is not exiting, the process ends as `process_exit(value)`
     would (`terminate`). This matches Linux's "last thread's exit
     status".
- **Joining** uses the handle of kind `thread` (new `files.Kind.thread`,
  `ObjectKind.THREAD = 12` in `native/objects.mlx:32-45`), whose state is
  `ENDED` once the thread ended (`stateOf`, `objects.mlx:98-158`). So a
  join is `wait([{h, ENDED}], deadline)` followed by `thread_info(h)` for
  the value, then `handle_close`. Closing the last handle of an ended
  thread frees it. Closing the handle of a live thread detaches it.
  Alternatively, user space can wait on the exit word with `memory_wait`,
  which is what mlxlibc and Linux do.

### 3.6 Waiting on memory: the futex (`projects/brix/kernel/futex.mlx`)

```
pub const BUCKETS: usize = 256
// A wait's key: (the address space's root, the user address) for a word
// in the caller's own memory; (1 | the frame, the offset) for a word of a
// page shared between processes (flag SHARED, a memory object's page:
// Page.SHARED). Private and shared waits on private memory meet (a
// superset of Linux's behaviour: futex callers expect spurious wakes).
pub const Key = struct { space: usize, address: usize }
var heads: [BUCKETS]u16 = …   // thread index + 1, FIFO
var tails: [BUCKETS]u16 = …
pub fn keyOf(p: *Process, address: usize, shared: bool) -> ?Key
pub fn enqueue(t: *Thread, key: Key, mask: u32) -> void
pub fn cancel(t: *Thread) -> void            // a wait ended otherwise
pub fn wake(key: Key, count: usize, mask: u32) -> usize
pub fn requeue(from: Key, to: Key, wakeCount: usize, moveCount: usize) -> usize   // Linux layer
```

`memory_wait(address, expected, deadline, flags)` runs under the kernel
lock, like every call:

1. `address % 4 != 0` gives `invalid`. `!p.prepare(address, 4)` gives
   `bad_address`.
2. If `t.woken`, clear it and `futex.cancel(t)`, then return 0. This is
   the restarted call after a wake.
3. If `t.timedOut` (set by `canGo` when the deadline passed), cancel and
   return `timed_out`.
4. If the u32 at the address is not `expected`, return `would_block`.
5. If `deadline == 0`, return `timed_out`.
6. Enqueue, then return outcome blocked with `Wait.on(WaitKind.memory, 0,
   deadline)`. The call is made again when the thread runs (restart
   model, `scheduler.mlx:390-396`), and step 2 or 3 answers it.

The compare in step 4 and the enqueue are atomic with respect to every
waker, because `memory_wake` also holds the kernel lock. A user-space
store with no wake can race, which is the futex contract: callers loop.

`memory_wake(address, count, flags)`: walk the key's bucket in FIFO
order. For up to `count` waiters with an equal key (and a mask overlap,
Linux layer), unlink them, set `woken = true`, make them ready (the
scheduler's `canGo` sees `woken`), and return how many were woken. Then
`kickIdle()` (already called on every way out, `trap.mlx:117`) brings an
idle processor.

- **Timeouts** use the existing generic deadline path (`canGo` sets
  `timedOut`, `smp.arm` and `cover` wake a halted processor in time,
  `smp.mlx:484-568`).
- **Interruption:** when an upcall or a Linux handler interrupts the
  waiting thread, the existing `inCall` path rewrites rax to
  `interrupted`/EINTR (`trap.mlx:369-373`, `signals.endCall`). Add
  `futex.cancel(t)` there.
- **Thread end and process end** call `futex.cancel` (3.5).
- **Unmapping a page** with waiters on it leaves them waiting, as on
  Linux. They are woken by their deadline or their process's end.
- **The kernel lock:** wait and wake serialize with every other kernel
  entry. That is correct and simple. It becomes the bottleneck only under
  heavy contention, because std's Mutex spins before it waits (6.4). A
  later step (8.11) can take futex calls off the big lock with per-bucket
  spin locks. They touch only the bucket and the thread's wait fields, but
  the scheduler state change still needs care.

### 3.7 Events, upcalls, signals: which thread

- **Process-directed events** (interrupt, quit, terminate, hangup, child,
  notice, user, broken, read_behind) stay in the process's queue
  (`events.mlx:86-125`). They are run by the **event thread**: the main
  thread while it lives, else the lowest-indexed live thread. Native
  programs claim and route as today. A claimed event interrupts the event
  thread's call (`interrupted`) as it interrupts the process today.
- **The fault event is thread-directed.** The record is queued with the
  internal field `thread = faulting index + 1` (not written out:
  `events_take` writes kind, detail, sender and reserved as now,
  `native/calls.mlx:342-366`), and only that thread runs the upcall for
  it.
- **The upcall is the process's one, serialized:** `p.upcallThread` is set
  on `Events.enter` (`events.mlx:234-245`) and cleared on `leave`
  (`:249-258`) and `upcall_return` (`native/calls.mlx:292-298`, which
  returns `invalid` unless the caller is `p.upcallThread`). A second
  thread that needs the upcall parks with `WaitKind.upcall` until it is
  free. One stack serves the whole process, and two crash reports never
  run on it at once. std.crash's `reportingThread` handles the rest
  (2.9).
- **The Linux view** (`linux/signals.mlx:153-200`) is split. Actions,
  `pending` (process-directed) and `senders` stay in `Process.signals`.
  `blocked`, the thread-directed pending set, `suspended/kept` and the
  altstack move to the Thread. Delivery
  (`deliverLinux`, `trap.mlx:400-434`) on the current thread takes
  `(t.signalPending | p.pending) & ~t.signalMask`. A process-directed
  signal is taken by the first thread on its way out that does not block
  it. If all threads block it, it stays pending. A thread that is blocked
  in a call and does not block the signal is made ready by `canGo` and
  interrupted, as now. Faults (`signals.force`) go to the faulting
  thread's pending set.
- **Kill and halt** are not claimable. The kill event ends the whole
  process (3.9). Halt and the default stop action stop every thread.
- **Linux job control in the shell is unchanged**: the group and session
  logic is process-level.

### 3.8 Faults with threads

`Process.fault` (`process.mlx:483-509`): change line 488 so that a page
that is present now and permits the access (present, and writable for a
write) answers **true**, and the thread retries. Two threads touching a
fresh page at the same moment are serialized by the lock. The second one
finds the page there and must not get SIGSEGV. A copy-on-write page is
handled by `copyOnWrite` first (`:485`), as now, with the shootdown of
3.4.

### 3.9 A process with threads ends, stops, branches, replaces

- **End** (`process_exit`, `exit_group`, kill, an event's default end, a
  fault without a handler) goes through `Scheduler.endProcess(p, ending)`.
  It replaces `terminate` (`scheduler.mlx:161-186`) as the entry point:
  1. Set `p.exiting = true` and `p.ending = ending`.
  2. For each thread of `p`: if it is ready, blocked or stopped, cancel
     any futex wait and end it with `endThread`. Exit words are not
     written; the space is going away. If it is running on **another**
     processor, send that processor Vector.WAKE (`smp.wake` without the
     idle condition, as a new `smp.kick(x)`). Its trap ends the thread in
     `deliver` (3.3). The calling thread ends on the way out.
  3. When the last thread of `p` leaves its processor (`runningCount`
     drops to 0 in `switchTo`), `finishProcess(p)` runs today's
     `terminate` body: files, nodes, memory (no processor has the space
     loaded now), zombie, and the parent's child event.
- **Stop** (suspend, read_behind, halt): set `p.state = stopped`. The
  threads that are not running are skipped by `scan`. Running ones are
  kicked and stop on their way out. **Resume**: `p.state = ready`, and the
  threads go on where they were.
- **process_branch / fork** (`scheduler.mlx:140-159`,
  `Process.forkFrom`, `process.mlx:233-272`): the child gets **one**
  thread, a copy of the caller's (POSIX). Locks held by other threads stay
  held in the child's copy. std's branch wrapper takes the heap lock
  around the call (an atfork for std.heap, 6.6).
- **process_replace / execve** (`native/calls.mlx:307-340`): the other
  threads end first (as in an end, but the process lives). The caller
  becomes the main thread with the process's pid as its id, and fsBase is
  0 as now.

### 3.10 Limits and memory

- The kernel table holds `threadCapacity = min(THREADS_MOST, 256 + 8 ×
  (running − 1))` threads: 256 with one processor, 280 with 4, 760 with 64
  and 2296 with 256.
- A process may have `max(256, 2 × running)` threads: 256 up to 128
  processors, 512 with 256.
- A non-zero user may hold `threadCapacity − SYSTEM_RESERVE` threads in
  all. Over a limit, thread_create returns `too_many` (Status 21) and
  clone returns EAGAIN.
- Memory per thread: a 4 KiB record frame, plus 2 words in the index and
  live tables.
- `Process.MAX_REGIONS` (`process.mlx:79`) goes from 256 to **1024**.
  std.thread uses two regions per thread (arena and stack). Regions are
  scanned linearly by `inRegion`/`regionEnd` (`:383-395, 506-515`).
  With 1024 that is a few microseconds per fault. A sorted array with a
  binary search is a follow-up (8.11). It adds 12 KiB to each `Process`
  record. In the working tree the process table is in frames, sized
  16 + 2 per further processor, so with 64 processors that is about
  1.7 MiB of frames, not .bss.
- The process count was `Scheduler.MAX = 16` at HEAD
  (`scheduler.mlx:35`). The working tree grows it with the processors.
  It is independent of threads.

### 3.11 Files the kernel work touches

- New files: `projects/brix/kernel/thread.mlx` and `projects/brix/kernel/futex.mlx`.
- `process.mlx`: fields, `init`, `forkFrom`, `fault` and `giveBack`.
- `scheduler.mlx`: almost all of it.
- `trap.mlx`: handle, after, deliver*, end and FLUSH.
- `smp.mlx`: shootdown, kick, and the ack in the lock loop.
- `memory.mlx`: drop, release, copyOnWrite, clone and `loaded(x)`.
- `layout.mlx`: Vector.FLUSH.
- `wait.mlx`: WaitKind.memory and WaitKind.upcall.
- `call.mlx`: OutcomeKind.thread_ended.
- `events.mlx`: the record's thread and upcallThread.
- `linux/signals.mlx`: the split.
- `native/calls.mlx`: the call table, the new calls and thread_pointer_set
  on the thread.
- `native/processes.mlx`: spawn/launch build the first thread (`:216-218`),
  and list, self and describe.
- `native/objects.mlx`: kind THREAD and its state.
- `files.mlx`: Kind.thread.
- `linux/system.mlx`: clone, futex and the others (§5).
- `native/system.mlx`: no change; it already reports `processors` at
  `:37`.

The field references to move were found with codemap (`mlx-codemap refs
Process.context` and so on). Counted by grep: waitingFor 9, waitItems 4,
waitCount 4, timedOut 14, inCall 12, `.context` 39 (some of them GPU
contexts, not this one), `.sse` 15, fsBase 9, lastCpu 4, cpuTime 7.

---

## 4. The native interface

### 4.1 Spec text (`spec/04-stdlib/os/brix.xml`)

In `<Principles>`, add:

```xml
<Rule>A process runs one or more threads: flows of execution in its one address space, with its handles, identity and events. A process's first thread has the process's pid as its id; the process ends with its last thread, or at once for all of them by process_exit.</Rule>
```

In `<Wait>`, a second way to wait, and why:

```xml
<Rule>A thread may also wait for a word of memory to change (memory_wait): the lock and condition words of a program's own threads, which have no kernel object behind them and whose uncontended path needs no call at all. Everything else is waited for as handles.</Rule>
```

In `<Handles>` `<Kinds>`, append `thread`. In `<States>`, the `ended` bit:
"a process ended; a group has no process left; a thread ended". In
`<Status>`, the meaning of `too_many` becomes "a table is full (handles
of a process, threads of a process or of a user)".

In `<Calls>`, after 85:

```xml
<Call n="86">thread_create(start*, size) -> thread h, or the thread's id with flag 1 (a new thread in the caller's process. The start record (size bytes, 64 at least): entry u64, stack u64 (its top, 16-byte aligned), argument u64, second u64, thread pointer u64 (its FS base; 0: none; all ones: the caller's), id word* u64 (a u32 the kernel writes the new thread's id to before it runs; 0: none), exit word* u64 (a u32 the kernel sets to 0 when the thread ends and wakes every memory_wait on; 0: none), flags u32 (1 detached: no handle comes back, the thread is freed when it ends), reserved u32 (0). The thread starts with the caller's general registers but for rip = entry, rsp = stack - 8 (a zero word there: entry runs as a function called with argument in rdi and second in rsi; it ends by thread_exit, a return faults), rax 0, its SSE state reset. The handle: kind thread, rights read, wait, duplicate; ended once the thread ended. invalid for an entry of 0, a stack not 16-byte aligned, size below 64, other flags; bad_address for a record, stack or word not the caller's memory; too_many past the threads a process (at least 256, two for each processor) or a user may have)</Call>
<Call n="87">thread_exit(value) (the calling thread ends: its exit word set to 0 and woken, its handles ended, thread_info's value `value`; the process's last thread ends the process as process_exit(value) would; does not return)</Call>
<Call n="88">memory_wait(address, expected, deadline, flags) -> 0 (the calling thread waits while the u32 at address (4-byte aligned) holds expected: would_block at once when it holds another; 0 when memory_wake woke it; timed_out at the deadline (monotonic nanoseconds; 0: do not block; all ones: none); interrupted when the upcall ran. flags 1 shared: the word is keyed by its page, which several processes map (a memory object's), not by the caller's address space. A wake may come without the word having changed, and a change without a wake: callers look again)</Call>
<Call n="89">memory_wake(address, count, flags) -> woken (up to count threads waiting on the word at address made to go on, those waiting longest first; all ones: every one; flags 1 shared as memory_wait's)</Call>
<Call n="90">thread_self() -> id (the calling thread's id; a process's first thread has its pid, the others ids from the same counter, never a living process's pid)</Call>
<Call n="91">thread_yield() (the processor to another ready thread, when there is one; the caller is ready again)</Call>
<Call n="92">thread_info(thread h, into*) -> state (a record of 32 bytes: id u32, state u32 (1 ready, 2 running, 3 blocked, 4 stopped, 5 ended), the value thread_exit gave u64 (0 until it ended), processor time u64 (nanoseconds), the processor it ran on last u32, reserved u32; wrong_kind for another kind of handle)</Call>
```

In `<Events>`:

```xml
<Rule>Events sent to a process are taken by its event thread (its first thread while it lives, else the one with the lowest id): the upcall runs there and a call it interrupts returns interrupted. A fault is the faulting thread's: the upcall runs on that thread. One upcall runs at a time in a process: another thread that needs it waits until upcall_return. A stop stops every thread; kill and the end of the process end them all.</Rule>
```

In `<Processes>`:

```xml
<Rule>process_branch copies the calling thread alone (its locks held by other threads stay held in the copy); process_replace ends the process's other threads first, the caller becoming its first thread.</Rule>
```

Section 5 of the spec gets a note that the Linux layer's clone with
CLONE_THREAD, futex, gettid, tgkill and exit as opposed to exit_group are
its view of these calls. The spec's own `<Personality>` (`:297`) already
states the general rule.

### 4.2 Kernel call table (`projects/brix/kernel/native/calls.mlx:38-126`)

```
pub const THREAD_CREATE: usize = 86
pub const THREAD_EXIT: usize = 87
pub const MEMORY_WAIT: usize = 88
pub const MEMORY_WAKE: usize = 89
pub const THREAD_SELF: usize = 90
pub const THREAD_YIELD: usize = 91
pub const THREAD_INFO: usize = 92
```

The handlers go in a new `native/threads.mlx`
(`create/exit/wait/wake/self/yield/info`), dispatched from `dispatch`
(`:156-305`). `THREAD_POINTER_SET` (`:263-269`) writes `t.fsBase`. On
`THREAD_EXIT` the outcome is `thread_ended` (or `exited` for the last
thread). `THREAD_YIELD` returns `OutcomeKind.yielded`.

### 4.3 std.os.brix bindings (`std/bootstrap/os/brix.mlx`)

```
pub extern("syscall") fn call0(number: usize) -> isize {}

pub const THREAD_CREATE: usize = NATIVE_BASE + 86
pub const THREAD_EXIT: usize = NATIVE_BASE + 87
pub const MEMORY_WAIT: usize = NATIVE_BASE + 88
pub const MEMORY_WAKE: usize = NATIVE_BASE + 89
pub const THREAD_SELF: usize = NATIVE_BASE + 90
pub const THREAD_YIELD: usize = NATIVE_BASE + 91
pub const THREAD_INFO: usize = NATIVE_BASE + 92
// Status: add too_many (21) beside the others (:101-126).
// Kind (:139-149): GPU 10, FENCE 11 (std/src/gpucard.mlx has them), THREAD 12.

pub const ThreadStart = struct {
    entry: usize
    stack: usize
    argument: usize
    second: usize
    threadPointer: usize
    idWord: usize
    exitWord: usize
    flags: u32
    reserved: u32
}
pub const ThreadInfo = struct { id: u32, state: u32, value: u64, time: u64, processor: u32, reserved: u32 }
pub const Threads = struct {
    pub const DETACHED: u32 = 1
    pub const INHERIT_POINTER: usize = 0xFFFFFFFFFFFFFFFF
    pub const READY: u32 = 1
    pub const RUNNING: u32 = 2
    pub const BLOCKED: u32 = 3
    pub const STOPPED: u32 = 4
    pub const ENDED: u32 = 5
}
pub const MemoryWait = struct { pub const SHARED: usize = 1 }

pub fn threadCreate(start: *const ThreadStart) -> isize { return call2(THREAD_CREATE, @intFromPtr(start), @sizeOf(ThreadStart)) }
pub fn threadExit(value: usize) -> void { _ = call1(THREAD_EXIT, value) }
pub fn memoryWait(word: *const u32, expected: u32, deadline: usize, flags: usize) -> isize { return call4(MEMORY_WAIT, @intFromPtr(word), @intCast(usize, expected), deadline, flags) }
pub fn memoryWake(word: *const u32, count: usize, flags: usize) -> isize { return call3(MEMORY_WAKE, @intFromPtr(word), count, flags) }
pub fn threadSelf() -> u32 { return @intCast(u32, call0(THREAD_SELF)) }
pub fn threadYield() -> void { _ = call0(THREAD_YIELD) }
pub fn threadInfo(handle: Handle, into: *ThreadInfo) -> isize { return call2(THREAD_INFO, @intCast(usize, handle), @intFromPtr(into)) }
```

`std/src/os.mlx:6` already exposes this module as `std.os.brix`.

---

## 5. The Linux layer (`projects/brix/kernel/linux/system.mlx`)

All of these sit on the kernel's thread and futex code. Nothing in the
core names a Linux number. The new `linux/threadcalls.mlx`, dispatched
like `FileCalls`/`SocketCalls` (`:228-229`), holds them.

### 5.1 clone (56), the thread form

What glibc's and musl's `pthread_create` pass:

- glibc (since 2.34 through clone3, before that clone):
  `CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SYSVSEM|CLONE_SIGHAND|CLONE_THREAD|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID`
  (0x3D0F00).
- musl: the same plus `CLONE_DETACHED` (0x400000, ignored by Linux).
- mlxlibc: the glibc set plus `CLONE_CHILD_SETTID` (`pthread.mlx:290`).

The rules:

- If `CLONE_THREAD` is set, `CLONE_SIGHAND` and `CLONE_VM` must also be
  set (EINVAL otherwise, as Linux does). `CLONE_FS` and `CLONE_FILES` are
  required: the process's directory and descriptors are the threads'. A
  thread without them is EINVAL ("not supported", and no program here
  uses it). `CLONE_SYSVSEM` and `CLONE_DETACHED` are accepted and ignored.
  `CLONE_VFORK`, `CLONE_PARENT`, `CLONE_NEWNS` and the other namespace
  flags with `CLONE_THREAD` give EINVAL. The exit signal (low byte) must
  be 0.
- The child is created by `createThread` with **the Linux registers**: a
  copy of the parent's frame, `rax = 0`, `rsp = newsp` (a2), the same
  rip, SSE copied, and `fsBase = tls` (a5) with `CLONE_SETTLS`, else the
  parent's.
- `CLONE_PARENT_SETTID`: write the tid at `parent_tid` (a3) before
  returning. `CLONE_CHILD_SETTID`: write it at `child_tid` (a4).
  `CLONE_CHILD_CLEARTID`: `t.exitWord = child_tid`.
- It returns the tid. EAGAIN over the limits. ENOMEM when no frame is
  left.
- The fork forms (`:321-326`) stay as they are.

### 5.2 clone3 (435)

`clone3(struct clone_args *, size)`: flags, pidfd, child_tid, parent_tid,
exit_signal, stack, stack_size, tls, set_tid, set_tid_size, cgroup (u64
each). With `CLONE_THREAD` the same path runs with `newsp = stack +
stack_size`. `set_tid` and `cgroup` must be 0 (EINVAL otherwise), and
size below 64 gives EINVAL. **Without CLONE_THREAD it stays ENOSYS**
(`:328`), because glibc's fork and posix_spawn fall back to clone and
vfork.

### 5.3 futex (202)

The arguments are `(uaddr, op, val, timeout|val2, uaddr2, val3)`.
`op & 128` (PRIVATE) is accepted (keys as in 3.6), and `op & 256`
(CLOCK_REALTIME) applies to the absolute forms. `cmd = op & 127`:

| cmd | Linux | brixOS |
|---|---|---|
| 0 WAIT | while `*uaddr == val`, relative timespec timeout (NULL: none) | `memory_wait` with deadline `now + timeout`, mask all ones; 0 / EAGAIN / ETIMEDOUT / EINTR |
| 1 WAKE | wake `val` | `futex.wake(key, val, ALL)` returns the count |
| 3 REQUEUE | wake `val`, move up to `val2` (in the timeout argument) to `uaddr2` | `futex.requeue`. Used by musl's condition variables (its `unlock_requeue`) |
| 4 CMP_REQUEUE | as 3, but EAGAIN unless `*uaddr == val3` | glibc before 2.25 used it for `pthread_cond_broadcast` |
| 5 WAKE_OP | atomically `op` on `*uaddr2`, wake `val` on uaddr, and if `cmp(old, cmparg)` wake `val2` on uaddr2 | the op is decoded `((val3 >> 28) & 7`: SET, ADD, OR, ANDN, XOR; bit 31 of the encoding means `oparg` is a shift), `cmp = (val3 >> 24) & 15` (EQ, NE, LT, LE, GT, GE), `oparg = (val3 >> 12) & 0xFFF`, `cmparg = val3 & 0xFFF`. It runs under the kernel lock, so it is atomic with respect to every other futex operation and to the threads' own `lock`-prefixed operations on that word. Old glibc's `pthread_cond_signal` uses it |
| 9 WAIT_BITSET | as WAIT, but with an **absolute** timeout (CLOCK_MONOTONIC, or CLOCK_REALTIME with flag 256: converted with `realtime - monotonic` at the call) and `val3` the mask (0 gives EINVAL) | glibc 2.25+ condition variables, `sem_clockwait`, and pthread_join with a timeout |
| 10 WAKE_BITSET | wake `val` waiters whose mask overlaps `val3` | the same |
| 6, 7, 8, 11, 12, 13 (the PI operations) | priority inheritance | **ENOSYS**. glibc then refuses PTHREAD_PRIO_INHERIT mutexes with ENOTSUP at `pthread_mutex_init`. A real PI implementation needs owner-tid tracking and priority boosting, which a round-robin scheduler does not have |

`futex_waitv` (449) stays ENOSYS (Wine and Proton use it). Timeouts that
are not readable give EFAULT. A negative or too-large tv_nsec gives
EINVAL.

### 5.4 The other calls

- **set_tid_address (218)**: `t.exitWord = a1`. It returns the tid, not 1
  as at `:356` today.
- **gettid (186)**: `t.id`. getpid (39) stays `p.pid` (`:291` split).
- **tkill (200) and tgkill (234)**: if the thread `tid` belongs to the
  caller's process (or the caller is user 0, or the same user), add the
  signal to that thread's pending set; else ESRCH. Signal 0 only checks.
  glibc depends on this for SIGCANCEL (32, pthread_cancel) and SIGSETXID
  (33, setuid in a threaded process, which signals **every** thread and
  waits for each to run its handler). Both must reach exactly the named
  thread. Credentials are per process on brixOS, so the first thread's
  setuid already changed them and the other threads' calls are no-ops.
- **exit (60)** is `endThread(t, a1 & 0xFF)`, or the process end for its
  last thread. **exit_group (231)** is `endProcess` (`:203-207` split).
- **sched_getaffinity (204)**: a mask with bits `0 .. running − 1`
  (`smp.processors().*.running`). `a2` must be at least `8 *
  ceil(running / 64)` (EINVAL otherwise, as Linux checks against
  `nr_cpu_ids`). It returns that byte count. Replaces `:378-383`.
- **sched_setaffinity (203)**: 0, accepted and ignored. A mask with no
  running processor gives EINVAL. brixOS does not pin threads yet; see
  §10.
- **getcpu (309)**: `cpu = smp.currentCpu()`, node 0.
- **sched_yield (24)**: `yielded` as now, per thread.
- **rseq (334)**: change it to **ENOSYS** (today it returns 0 at `:355`).
  Returning 0 tells glibc that rseq is registered, and the kernel never
  updates `cpu_id`. glibc treats ENOSYS as "no rseq".
- **set_robust_list (273)**: 0, accepted. The list is not walked when a
  thread dies holding a robust mutex; see §10.
- **rt_sigprocmask, rt_sigsuspend, sigaltstack, rt_sigtimedwait,
  rt_sigpending**: act on the current thread (signals split, 3.7).
- **madvise(MADV_DONTNEED)** on the dying thread's stack (glibc does this)
  stays 0.
- **The Linux layer's own futex wake on CLEARTID** is the `endThread`
  path in 3.5.

### 5.5 Which foreign programs here exercise it

- Mlx programs **built for Linux** and run in brixOS (the image can carry
  them): `tests/298_spawn_thread_runtime.mlx` (clone with SETTLS,
  PARENT_SETTID, CHILD_SETTID and CHILD_CLEARTID, arch_prctl, and a
  shared futex join), `tests/315_tinypage_runtime.mlx` (4 threads and the
  allocator's futex lock) and `tests/180_linux_futex_runtime.mlx`. The
  h264/h265 decoders and the splash crew when built for Linux.
- **mlxlibc built for Linux** (raw syscalls, `compiler/libc/sys_calls.mlx`)
  with `tools/mlxlibc_check.c`'s pthread section (`:284-345`: 4 threads,
  mutex, cond, once, keys, barrier), built by mlx-cc and run through the
  Linux layer. `compiler/libc/sqlite3/tests/threads.c` is the same kind of
  program. These give the layer's clone, futex WAIT/WAKE, CLEARTID and
  set_tid_address real use.
- **Not here:** glibc and musl themselves are not built ("no third-party C
  libraries in what is built"). Their flag sets above are the contract the
  layer answers. REQUEUE, CMP_REQUEUE and WAKE_OP get unit coverage from a
  small Mlx test program that issues them through `std.linux.thread.futex`
  (`std/bootstrap/os/linux/thread.mlx:49-51`).

### 5.6 Signals split (`linux/signals.mlx`)

`Signals` (`:153-200`) keeps `actions`, `pending` (process-directed) and
`senders` in the process. A `ThreadSignals` part holds `blocked`,
`pending`, `suspended`, `kept`, `stackBase`, `stackSize`, `faultAddress`
and `forced`, and is embedded in `Thread`. The functions that took
`p.*.signals` and the mask (`deliverable`, `take`, `place`, `enter`,
`endCall`, `suspend`, `force`, `faulted`) get the thread part as an
argument. `exec()` resets the thread part of the surviving thread.
`inherit` (fork) copies the calling thread's part into the child's first
thread.

---

## 6. std

### 6.1 Where things go

- `std/src/thread.mlx`, `std.thread`: the API.
  `std/src/thread_os.mlx` (Linux) and `std/src/thread_os.brixos.mlx`
  (native) are chosen by target like `crash_os*.mlx`
  (`compiler/selfhost/modules/loader.mlx:35`, `driver/pipeline.mlx:74`).
- `std/src/sync.mlx`, `std.sync`: Mutex, Condition, Once, WaitGroup,
  Semaphore.
- `std/bootstrap/os/wait.mlx` and `wait.brixos.mlx`: the OS seam. They are
  in the compiler's closure, so they use no new builtins.
- `std/src/std.mlx` gets `pub const thread = @import("./thread.mlx")` and
  `pub const sync = @import("./sync.mlx")`.
- `spec/04-stdlib/thread.xml` is rewritten to match (it shows
  `std.Thread.spawn(allocator, worker, .{arg})`, which needs closures and
  anytype tuples that Mlx does not have).

### 6.2 std.thread API

```
pub const Entry = fn(context: usize) -> usize

pub const Error = error {
    // No memory for the stack or the arena.
    OutOfMemory,
    // The process or its user has as many threads as it may (too_many /
    // EAGAIN).
    TooManyThreads,
    // The system has no threads (an old brixOS kernel: unsupported), or
    // the backend cannot give a thread an arena of its own (aarch64).
    Unsupported,
}

pub const Options = struct {
    stackSize: usize    // bytes, rounded to pages; 0: DEFAULT_STACK (1 MiB)
    name: []const u8    // up to 31 bytes, said in crash reports; "" none
    detached: bool
}
pub fn defaults() -> Options

pub const Thread = struct { block: usize }   // owns the thread until joined or detached

// Starts entry(context) on a new thread with an arena and a stack of its
// own. `context` is the caller's: heap memory, a global, or anything that
// outlives the thread; never a value the caller's turns release.
pub fn spawn(entry: Entry, context: usize) -> Error!Thread
pub fn spawnWith(options: Options, entry: Entry, context: usize) -> Error!Thread

// Waits for the thread's end; its entry's result. The thread's stack and
// arena are unmapped: nothing may point into its values afterwards.
pub fn join(thread: Thread) -> usize
// Whether it ended (join then does not wait).
pub fn done(thread: Thread) -> bool
// Its resources go when it ends; the Thread is not used again.
pub fn detach(thread: Thread) -> void

pub fn processorCount() -> usize      // brixOS: system_info's processors; Linux: sched_getaffinity's bits; cached
pub fn currentId() -> u32             // the kernel's id (thread_self / gettid), cached in the Block
pub fn current() -> ?*Block           // null on a foreign thread (a C library's)
pub fn yield() -> void
pub fn sleep(nanoseconds: u64) -> void

// Thread-local words (Mlx has no `threadlocal`): a key for the process,
// a word for each thread (0 at first).
pub const Local = struct { index: u32 }
pub fn newLocal() -> ?Local            // LOCALS = 64 at most
pub fn localWord(local: Local) -> *usize
```

The Block sits at arena base + 8 (the main thread's is a static). Its
fields are `magic: u64`, `self`, `arenaBase`, `arenaEnd`, `stackBase`,
`stackSize`, `id: u32` (written by the kernel: id word or
CLONE_CHILD_SETTID), `exitWord: u32` (1 while it runs, set to 0 by the
kernel), `handle: u32` (brixOS), `detached: u32`, `entry`, `context`,
`result`, `name: [32]u8`, `locals: [64]usize`, `heapCache`,
`crashContext: [2]usize` and `turnsReserve`. That is about 700 bytes,
well under 4088.

`current()` is:

```
const base = @arena().0
if base == mainArena { return &mainBlock }
const b = base + 8
if magic(b) == MAGIC && self(b) == b { return b }
return null
```

`mainArena` is recorded at `init()`, which runs at the first std.thread
call; the caller is then the main thread or a thread on its arena.

**Spawn, Linux** (`thread_os.mlx`):

1. Map the arena (1 GiB + 8, MAP_NORESERVE) and the stack (`stackSize` +
   one guard page, the guard `mprotect`ed to none).
2. Fill the Block, set fill = base + 4096, and set exitWord to 1.
3. Call `@spawnThread(THREAD_CLONE_FLAGS | CLONE_CHILD_SETTID |
   CLONE_CHILD_CLEARTID, stackTop, 0, &block.exitWord, 0, threadMain,
   block)`. No SETTLS is used: the child inherits FS, and std never reads
   it. A negative result is mapped to `Error`.

**Join, Linux**: `while @atomicLoad(&exitWord) != 0 { futexWait(&exitWord,
seen, …) }` (shared, as `tests/298:37-43` shows), then read `result`,
then munmap the stack and arena.

**Spawn, brixOS** (`thread_os.brixos.mlx`):

1. `brix.map` the arena and the stack: two regions. The kernel leaves a
   page of addresses between mappings (`mmapNext += size + Page.SIZE`,
   `process.mlx:571, 583, 603`), so a stack mapped after another region
   that overflows downwards faults in that gap.
2. `brix.threadCreate(&ThreadStart{ .entry = threadMain, .stack = top,
   .argument = block, .threadPointer = 0, .idWord = &id, .exitWord =
   &exitWord, .flags = detached ? 1 : 0 })`.
3. Keep the handle in the Block.

threadMain ends with `brix.threadExit(result)`.

**Join, brixOS**: `brix.wait([{handle, ENDED}], FOREVER)`, then
`threadInfo` (for the value, cross-checked with `result`), then
`brix.close(handle)` and `brix.unmap` of both regions.

Each new thread installs crash's per-thread stack (hook) on Linux. On
brixOS the process's upcall is enough (3.7).

### 6.3 Pool: spreading work over all processors

```
pub const Task = fn(context: usize, index: usize) -> void
pub const Pool = struct { … }
// `workers` threads (0: processorCount() - 1; the caller is the last
// worker while it runs a batch).
pub fn startPool(workers: usize) -> Error!*Pool
// task(context, i) for every i in 0..count, spread over the workers and
// the caller (an atomic counter hands out chunks of `grain` indices, as
// the splash bands are handed out: bands.mlx:119-133); returns when all
// are done.
pub fn run(pool: *Pool, count: usize, grain: usize, task: Task, context: usize) -> void
pub fn stopPool(pool: *Pool) -> void
```

The pool comes from heap memory. Its state words are `generation`,
`next` (u64: generation << 32 | index), `remaining` and `quit`. Workers
sleep with `memory_wait`/futex on `generation`, and the caller waits on
`remaining`. **Each worker runs its batches inside `std.turns` on its own
arena:** `turns.beginAt(&t, @arena().0, false)` before its loop, and
`turns.turn(&t)` after each batch. A task's values are therefore released
after every batch, and a worker's arena never fills, however long the
pool runs. The rule is that **results go into heap memory or the context,
never in a task's values.** The caller's own share runs on the caller's
arena and is not released (the caller decides).

### 6.4 std.sync

All the operations are built on the seam (6.5) and the language's atomics
(`spec/00-language/atomics-tls.xml`: `@atomicLoad`, `@atomicStore`,
`@atomicRmw`, `@cmpxchgWeak`, `@cmpxchgStrong`, `@fence`; tests 295, 437
and 182). They are already used in 25 std files; no new language feature
is needed.

```
pub const Mutex = struct { word: u32 }       // 0 free, 1 held, 2 held with waiters (Drepper)
pub fn mutex() -> Mutex
pub fn lock(m: *Mutex) -> void   // cmpxchg 0→1; else up to SPIN = 100 rounds of
                                  // (pause hint, load, cmpxchg); then
                                  // while xchg(2) != 0 { waitWord(&word, 2, FOREVER) }
pub fn tryLock(m: *Mutex) -> bool
pub fn unlock(m: *Mutex) -> void // if xchg(0) == 2 { wakeWord(&word, 1) }

pub const Condition = struct { sequence: u32, waiters: u32 }
pub fn wait(c: *Condition, m: *Mutex) -> void
     // seq = load(sequence); add(waiters, 1); unlock(m);
     // waitWord(&sequence, seq, FOREVER); sub(waiters, 1); lockContended(m)
pub fn timedWait(c: *Condition, m: *Mutex, deadline: u64) -> bool   // false: timed out
pub fn signal(c: *Condition) -> void      // add(sequence, 1); if waiters > 0 { wakeWord(&sequence, 1) }
pub fn broadcast(c: *Condition) -> void   // … wakeWord(&sequence, ALL)

pub const Once = struct { state: u32 }    // 0 new, 1 running, 2 done
pub fn once(o: *Once, function: fn(usize) -> void, context: usize) -> void

pub const WaitGroup = struct { count: u32 }   // add, done (wake at 0), wait
pub const Semaphore = struct { count: u32, waiters: u32 }
```

`lockContended` relocks with state 2 so that a woken waiter still wakes
the next one. That is the same as mlxlibc's `condWait`
(`pthread.mlx:698-718`), which is known to work.

The bounded spin is plain relaxed loads. Mlx has no builtin for x86's
PAUSE (aarch64's YIELD). A small `@spinHint()` builtin would be kinder to
a sibling hyperthread, and the kernel's lock spin uses PAUSE through
`cpu.Cpu.relax` (`smp.mlx:250`). That builtin is optional and not needed
for correctness.

### 6.5 The OS seam (`std/bootstrap/os/wait.mlx`, `wait.brixos.mlx`)

```
pub const FOREVER: u64 = 0xFFFFFFFFFFFFFFFF
pub const ALL: u32 = 0xFFFFFFFF
// 0 woken, -11 the word held another value, -110 the deadline passed,
// -4 interrupted (the caller looks at its word again in every case)
pub fn waitWord(word: *u32, expected: u32, deadline: u64) -> isize
pub fn wakeWord(word: *u32, count: u32) -> usize
pub fn yieldProcessor() -> void
pub fn threadId() -> u32
pub fn processorCount() -> usize
pub fn monotonic() -> u64
```

- On Linux it uses `futex(FUTEX_WAIT_BITSET | PRIVATE, expected,
  &absolute CLOCK_MONOTONIC, 0, ALL)` and `FUTEX_WAKE | PRIVATE`,
  sched_yield, gettid, `linux.system.availableProcessorCount()`
  (`std/bootstrap/os/linux/system.mlx:20-36`) and clock_gettime.
- On brixOS it uses `brix.memoryWait/memoryWake/threadYield/threadSelf`,
  `systemInfo().processors` and `brix.clock`. Native Status values are
  mapped to the -errno numbers above. On a kernel without the calls
  (`unsupported`), it falls back to today's behaviour: one thread, so
  there is nothing to wait for. That keeps new programs running on old
  images.
- `pages.mlx`/`pages.brixos.mlx`'s `sleepOn`, `wakeOne` and `yield`
  (`:43-47`, `:36-41`) delegate to it. The GPA (`:96-115`) and `heap.mlx`
  (`:83`) call it instead of `linux.thread.*`. `crash_os.brixos.mlx:38`
  and `sql/os.brixos.mlx:27-36` use `threadId()` and a real wait.

### 6.6 How the arena and the heap work with threads (summary for users)

- A thread's values are its own. They live until the thread ends (join
  unmaps them) or until its own turns release them. Pass data between
  threads in heap memory (`std.heap.allocator()`, which is thread-safe) or
  in globals.
- The heap is one lock (spin, then wait) until the per-thread caches land
  (8.11). `std.heap.allocator()` may be called from any thread.
- Branching a threaded process: std's `process.branch` wrapper (and
  mlxlibc's fork) take the heap lock and the tinypage lock around
  `process_branch`, and release them in both parent and child. Without
  that, a lock another thread held would stay held in the child.

### 6.7 Linked programs and foreign threads

In a dynamically linked program, exported functions called from C threads
reload the **main** arena (`labels.mlx:81-87`). That is safe because of
`lock xadd`, but those values share the main arena, and the main thread's
turns must not release them while C code uses them. This is the existing
rule, unchanged. On such threads `std.thread.current()` answers the main
Block. Use `currentId()` for the true id. std.thread's own spawn works in
linked programs too, because it never touches FS. C code called on a
std.thread thread finds FS = 0 (Linux, no SETTLS) or the parent's, so
**C libraries' thread-local state (errno, stack protector at fs:0x28) is
not set up on std threads.** Linked programs that call into C from worker
threads must use the C library's `pthread_create` and have the worker call
`@arenaSwitch` itself (std.thread offers `adoptForeign(block)` for that).
This is documented as a limit, not solved.

### 6.8 aarch64

Threads share the atomic global arena (`runtime.mlx:109-170`). On
aarch64, std.thread spawns them without an arena of its own,
`current()` uses the thread pointer (TPIDR_EL0, set with CLONE_SETTLS to
the Block), and turns must not run in a threaded aarch64 program
(`Error.Unsupported` from `Pool`). Per-thread arenas there need the helper
to read its base through TPIDR_EL0, which is a later compiler change.

---

## 7. Tests: what proves it

### 7.1 Compiler

- `tests/528_arena_switch_runtime.mlx`: map a 1 MiB + 8 arena (fill =
  base + 8), switch, take the address of a local struct, check that it
  lies in the new arena and that the fill moved by its size, then switch
  back. Check that `@arena()` equals the pair returned and that the main
  arena's fill is unchanged. It exits 13.
- Probe `switches_arenas` in `tools/ensure_compiler.sh`.
- `tests/529_spawn_thread_brixos.mlx` (step 5): `tests/298`'s logic with
  the native @spawnThread lowering. It runs in QEMU (7.3). Probe
  `spawns_brixos_threads_natively` (`mov eax, 0x4056` = `b856400000` in a
  `--target=x86_64-brixos` build).

### 7.2 std, on the host (Linux) and in brixOS

Each test is a `tests/5xx_*_runtime.mlx` that exits 13. The same sources
are built with `--target=x86_64-brixos` and run in QEMU by
`tools/check_brix_threads.sh` (7.3).

- `530_thread_spawn_join_runtime`: 8 threads return `3 * i`; join gives
  each; `done()` before and after; a detached thread's end is seen through
  a WaitGroup.
- `531_thread_arenas_runtime`: 16 threads each make 200 000 struct values
  (address-taken locals) at once. Each checks that every address lies in
  `[own base + 4096, own end)` and keeps a running checksum of the values'
  contents. A barrier holds them, then they check all the contents again.
  The main arena's fill must not change while they run (main makes no
  values during the wait), which proves they never touched it. With
  `MLX_ARENA_POISON=1`, each thread also runs a turns loop on its own
  arena (`turns.beginAt(@arena().0)`) and verifies its kept values.
- `532_thread_mutex_runtime`: 16 threads do 100 000 increments of a plain
  counter under `std.sync.Mutex`. The total must be exact, and a counter
  shows that the contended path (state 2, waits) was taken.
- `533_thread_condition_runtime`: a bounded queue with 4 producers, 4
  consumers and 200 000 items. The sum and count must match. `timedWait`
  must return false after its deadline, within 50 ms of it.
- `534_thread_once_locals_runtime`: a `Once` raced by 32 threads runs its
  function once. Each thread writes its own id to a `Local` and reads it
  back after a barrier. The main thread's Local is untouched.
- `535_thread_heap_runtime`: 16 threads each allocate and free 50 000
  blocks of random classes (16 bytes to 32 KiB plus large ones), writing a
  thread pattern and checking it before each free. `std.heap.liveBytes()`
  is back at the baseline after the joins, and no two live blocks overlap
  (a shared bitmap of 16-byte granules is checked under a Mutex).
- `536_thread_pool_runtime`: `Pool.run` over 100 000 indices marks each
  index exactly once (atomic increments, then a check for all ones).
  Running it 50 times must not grow any worker arena by more than one
  batch (`turns.used()` sampled by each worker).
- `537_thread_crash_runtime` (a script check in the style of
  `tools/check_crash_report.sh`): a worker fails a check; the report's
  first line names `thread <id> (<name>)`; the exit status is the crash's.
  On brixOS, the same through the upcall.
- The existing tests stay green: 298, 315, 180, 295, 296, and the whole
  runtime suite (single-threaded programs unchanged, §2.6).

### 7.3 Kernel in QEMU

`tools/check_brix_threads.sh` follows `tools/check_brix_smp.sh`: build
the kernel and image, then `qemu-system-x86_64 -smp 4 -cpu max`, plus a
run at `-smp 8`.

- `projects/brix/tools/threadcheck/main.mlx` uses raw `std.os.brix`
  calls only (no std.thread), so it fails on the kernel, not on std:
  - It creates 4 × N threads that each do `@atomicRmw add` one million
    times and end with `thread_exit(k)`. Joins by handle wait, then
    `thread_info` value equals k and state is ended.
  - Two threads play memory_wait/wake ping-pong 100 000 times.
  - `memory_wait` with a deadline 20 ms ahead returns timed_out within
    10 ms of the deadline.
  - `memory_wait` on a changed word returns would_block.
  - The exit word is 0 after the end, and a waiter on it was woken.
  - A detached thread needs no close.
  - At the limit, `too_many`.
  - `process_exit` from a worker while 8 others spin: the process ends,
    the shell prompt comes back, and `sysinfo` shows no leaked threads.
  - Ctrl-C ends a process whose threads all spin (the kick path).
  - A fault in a worker gives a report naming the worker; a second worker
    faulting at the same time is serialized.
  - **TLB shootdown stress**: thread A maps a page, writes a pattern,
    unmaps and maps again in a loop. Threads B and C on other processors
    read the same address and must see either a fault or a valid pattern,
    never another frame's contents. Every read faults into a recorded
    handler (upcall) or matches. Then fork (process_branch) from a
    threaded process while threads write (copy on write plus shootdown),
    with child and parent checksums.
  - Two threads fault the same fresh page at once 10 000 times (§3.8): no
    SIGSEGV.
- The scripts grep for lines such as `threads: 16 created, 16 joined`,
  `wake ping-pong: 100000`, `shootdown: ok` and `brixOS: TLB shootdown
  answered by 3 processors` (boot self-test), and fail on `brixOS: page
  fault` or `crashed` outside the expected reports.
- The std tests from 7.2, built for brixOS, run in the same image.

### 7.4 Scaling: does it use every processor?

`projects/brix/tools/threadbench/main.mlx` with N = `processorCount()`.
Each thread runs a fixed CPU-bound kernel (2^26 rounds of xorshift plus
multiply, no memory traffic and no system calls) once with 1 thread and
once with N. It prints `threadbench: N=16 t1=… tN=… speedup=…` and, from
`thread_info`, how many distinct processors the threads ran on.

`tools/check_brix_threads_scale.sh` runs it under
`qemu-system-x86_64 -machine q35 -accel tcg,thread=multi -smp 4`, then
`-smp 16` and `-smp 64`. The 16 and 64 runs need MAX_CPUS ≥ 64 from the
parallel autodetect work, and the script skips them until the kernel
reports that many processors.

- Always required: every thread joined with the right checksum, and the
  threads ran on `min(N, processors)` distinct processors.
- Required only when the host has at least N cores (`nproc`), since TCG
  needs a host thread per vCPU: `speedup ≥ 0.6 × N` (tN about 1/N of t1).
  Otherwise the speedup is printed, not judged.

The same benchmark built for Linux runs on the host (std.thread on
clone/futex) with the same rule.

### 7.5 The Linux layer in brixOS

`tools/check_brix_linux_threads.sh` puts host-target builds of tests
298, 315, 180 and 530-536 into the image and runs them in QEMU (-smp 4).
They exit 13. The kernel log must show their Linux calls (`linux call:
… 56`, `… 202`), which proves they took the compatibility path.

It also runs mlxlibc built for Linux with `tools/mlxlibc_check.c`
(compiled by mlx-cc as in `tools/check_mlxlibc.sh`). The pthread section
must print what it prints on the host. A small Mlx program issues
REQUEUE, CMP_REQUEUE, WAKE_OP, WAIT_BITSET with an absolute
CLOCK_REALTIME timeout, and tgkill to a named thread, and checks each
result.

### 7.6 mlxlibc natively (step 9)

`tools/check_brix_libc.sh` gets a pthread program
(`compiler/cc/tests/threads.c`, the pthread part of `mlxlibc_check.c`).
It must run with **no** `linux call:` line in the kernel log, as the
script already requires for the rest (`check_brix_libc.sh:1-15`).

---

## 8. The plan

Each step builds and passes the full check set on its own: the runtime
tests, `tools/check_bootstrap.sh`, the brixOS QEMU checks
(`check_brix.sh`, `check_brix_smp.sh`, `check_brix_crash.sh`,
`check_brix_libc.sh`, `check_brix_desktop.sh` where it runs), and
`mlx-codemap layers`, `cycles` and `unresolved` on the touched files.
Steps 1 and 2 can be done in parallel.

**Step 1: compiler: `@arena` and `@arenaSwitch`.**

- Files: `compiler/selfhost/sema/builtins/{kind,registry,analyze,atomic_arguments}.mlx`,
  `sema/declarations.mlx` (the argument diagnostic), `ir/lir.mlx`,
  `ir/lower.mlx`, `ir/optimize.mlx` and the inliner (barrier, no
  inlining), `backend/x86_64/codegen/memory.mlx`, the aarch64 backend
  (diagnostic), `spec/00-language/atomics-tls.xml`,
  `spec/02-compiler/x86_64.xml`, `spec/02-compiler/diagnostics.xml`,
  `tools/ensure_compiler.sh` (probe `switches_arenas`) and
  `tests/528_arena_switch_runtime.mlx`.
- Then `tools/ensure_compiler.sh` installs it as canonical.
- Risk: the optimizer moving an arena alloca across the switch, which the
  test and the barrier rule cover.

**Step 2: kernel: the thread record, one thread per process (a pure
refactor).**

- Files: `thread.mlx` (new), `process.mlx`, `scheduler.mlx`, `trap.mlx`,
  `native/calls.mlx`, `native/processes.mlx`, `native/objects.mlx`,
  `linux/system.mlx`, `linux/signals.mlx` (the split), `events.mlx`,
  `wait.mlx` and `call.mlx`.
- Use `mlx-codemap refs`/`callers` for every moved field before and after,
  then `mlx-codemap diff`.
- Passes when every existing brixOS check passes unchanged. Nothing new
  is visible.
- Risk: the size of the diff, around 120 references. Do it mechanically
  and run the QEMU checks after each file group.

**Step 3: kernel: shared spaces made safe.**

- TLB shootdown (Vector.FLUSH, `smp.shootdown`, the ack in `lock`, and in
  memory.mlx drop/release/giveBack/copyOnWrite/clone the order "free after
  the acknowledgement"), the `Process.fault` retry fix (3.8),
  `MAX_REGIONS` 1024, and the boot self-test.
- Files: `layout.mlx`, `smp.mlx`, `memory.mlx`, `process.mlx`, `trap.mlx`
  and `tools/check_brix_smp.sh` (grep the self-test).
- Risk: a deadlock between a shootdown and a processor that waits for the
  lock, which the ack in the spin loop prevents. Single-threaded processes
  must not send any IPI (counted by a debug counter, checked with
  `sysinfo` showing no FLUSH IPIs during the existing SMP check).

**Step 4: kernel: native threads (calls 86 to 92).**

- Files: `native/threads.mlx` (new), `futex.mlx` (new), `native/calls.mlx`
  (table and dispatch), `files.mlx` (Kind.thread), `native/objects.mlx`
  (kind and state), `scheduler.mlx` (createThread, endThread, endProcess,
  finishProcess, stop/resume all, kick), `trap.mlx` (thread_ended,
  exiting/stopped in deliver, the upcall serialization), `events.mlx`,
  `smp.mlx` (kick), `spec/04-stdlib/os/brix.xml` (4.1),
  `std/bootstrap/os/brix.mlx` (4.3),
  `projects/brix/tools/threadcheck/main.mlx` and
  `tools/check_brix_threads.sh`.
- The image build needs threadcheck listed like sysinfo in
  `check_brix_smp.sh:28`.
- Risk: process teardown while threads run elsewhere, which the
  `runningCount` gate covers, and lost wakeups in the restart model, which
  the `woken` flag covers.

**Step 5: compiler: `@spawnThread` natively on brixOS.**

- With `state.brixos`, `emitCloneThread` (`codegen/memory.mlx:140-171`)
  builds a ThreadStart on the parent's stack:
  - `CLONE_SETTLS` gives threadPointer = tls, else INHERIT_POINTER.
  - `CLONE_PARENT_SETTID`/`CLONE_CHILD_SETTID` give idWord = child_tid
    (else parent_tid).
  - `CLONE_CHILD_CLEARTID` gives exitWord = child_tid.
  - flags = DETACHED, because the id comes back as `@spawnThread`
    promises.
  - entry = a continuation label in the emitted sequence, rdi = argument,
    rsi = the entry function.
- It issues `0x4056`. The continuation does `and rsp, -16; call rsi; mov
  rdi, rax; mov eax, 0x4057; syscall`. The parent's result is the id or
  the negated errno (Status mapped to errno: `too_many` to EAGAIN,
  `no_memory` to ENOMEM, `invalid` to EINVAL).
- Files: `codegen/memory.mlx`, `spec/00-language/atomics-tls.xml` (the
  brixOS sentence), `tools/ensure_compiler.sh` (probe) and test 529.
- This makes mlxlibc's `pthread_create` native once mlxlibc's futex is
  (step 9).

**Step 6: std seam.**

- `std/bootstrap/os/wait.mlx`, `wait.brixos.mlx`, `pages.brixos.mlx`
  (delegate), `pages.mlx` (delegate), `general_purpose_allocator.mlx`,
  `std/src/heap.mlx`, `crash_os.brixos.mlx` (threadId),
  `std/src/sql/os.brixos.mlx` (lock and threadId) and `host.brixos.mlx`
  (prime once, infoScratch local).
- No behaviour change for single-threaded programs. The bootstrap check
  must stay green: no new syntax, only modules and calls.

**Step 7: std.thread and std.sync.**

- `std/src/thread.mlx`, `thread_os.mlx`, `thread_os.brixos.mlx`,
  `sync.mlx`, `std.mlx` (exports), the hooks in `turns.mlx`, `crash.mlx`
  (2.7, 2.9) and the GPA (the threadCache hook, null for now),
  `spec/04-stdlib/thread.xml`, tests 530-537, and the brixOS part of
  `tools/check_brix_threads.sh`.
- Requires step 1's compiler as canonical.

**Step 8: the Linux layer.**

- `linux/threadcalls.mlx` (new: clone thread form, clone3, futex,
  set_tid_address, gettid, tkill/tgkill, exit/exit_group split,
  affinity, getcpu, rseq ENOSYS), `linux/system.mlx` (dispatch, remove
  the old answers), `linux/signals.mlx` (per-thread delivery) and
  `tools/check_brix_linux_threads.sh` (7.5).

**Step 9: mlxlibc on brixOS.**

- `compiler/libc/sys_calls.brixos.mlx`:
  - 202: WAIT and WAIT_BITSET go to `memoryWait`, WAKE and WAKE_BITSET to
    `memoryWake`. REQUEUE and CMP_REQUEUE are wake-all on uaddr (correct,
    if slower). Others give ENOSYS.
  - 186 gives `threadSelf`. 60 gives `threadExit` and 231 `exit`. 218
    returns the tid.
  - 204 gives the processor count. 200/234 to the own process's thread
    is a native event to the process, or 0 for signal 0.
- Update the header comment (`:11-13`).
- Pthread creation is already native via step 5, because mlxlibc is built
  for brixos.
- `tools/check_brix_libc.sh` gets the pthread program (7.6).

**Step 10: consumers and scaling.**

- `projects/desktop/splash/brixos.mlx:558-559` starts the crew with
  `processorCount() - 1`, and `bands.mlx` moves its crew to std.thread
  (own arenas) with futexes through `std.sync`.
- The h264/h265 decode workers move to std.thread or call
  `@arenaSwitch` at worker entry (`decode.mlx:5065-5112`, h265 `:4241`).
- `projects/coreutils/nproc` and `pulsar` read `processorCount()`.
- `projects/brix/tools/threadbench` and `check_brix_threads_scale.sh`
  (7.4).

**Step 11 (optional, measured first with `mlx-out/bin/mlx-profile` and
`mlx-codemap hot`): scalability.**

- Per-thread heap caches (2.8).
- Futex calls taken off the big lock (per-bucket spin locks).
- A run queue instead of `scan` over all live threads.
- A sorted region array with binary search (`process.mlx:383-395`).
- `--private-arenas` without `lock` on the bump.

Dependency on the parallel work: the MAX_CPUS autodetection (in the
working tree: MAX_CPUS 256, tables in frames) is needed for the 16/64
processor runs of step 10. Step 2 must be made on top of it, because it
rewrites the same scheduler functions (`init`/`make`/`capacity`, `current`
and `startedAt`, and the new `busy` per-processor time). The thread
limits follow the processors running by their formulas (3.10).

---

## 9. Risks

1. **Silent memory corruption from stale TLBs** (step 3): every path that
   narrows or removes a user mapping must shoot down before freeing.
   `mlx-codemap callers Space.drop`, `Space.release`, `copyOnWrite` and
   `Space.map` list the places. The QEMU stress test (7.3) checks it.
2. **Deadlock between a shootdown and the big lock**: a processor spinning
   for the lock has interrupts off. It acknowledges in the spin loop
   (3.4), as HOLDING does.
3. **Values crossing threads.** A pointer into another thread's arena dies
   at that thread's turn or join. The rule is in std.thread's and
   std.turns' comments, and the poison mode test (531) catches it.
   Existing raw-`@spawnThread` users share the main arena until step 10;
   do not add turns loops to their mains before then.
4. **The bootstrap constraint**: closure modules cannot use the new
   builtins until the previous canonical compiler has them. The hook
   design avoids it. `tools/check_bootstrap.sh` guards it.
5. **The big kernel lock**: futex-heavy programs serialize in the kernel.
   Spinning before waiting (std.sync) and step 11 address it. CPU-bound
   threads take the lock only for their 100 Hz tick.
6. **Single-thread assumptions** scattered through std, mlxlibc and the
   kernel (2.10, `sys_calls.brixos.mlx:11-13`, `pages.brixos.mlx:36-41`,
   `sql/os.brixos.mlx:4-8`, `crash_os.brixos.mlx:38,47`). Grep for "one
   thread" and "threadId() -> u32 { return 1 }" again before step 6.
7. **Upcall serialization**: a thread parked behind a crashed upcall waits
   until the process ends. That is intended, but a program whose upcall
   never returns for a non-fault event blocks faulting threads.
   Documented in the spec rule.
8. **fork with threads**: locks held elsewhere. std's branch wrapper and
   mlxlibc's fork take the heap and tinypage locks.
9. **QEMU timing**: TCG speedup depends on the host's cores. The timing is
   judged only when nproc ≥ N; the structural checks (distinct processors,
   checksums) always are.
10. **Limits**: Process.MAX_REGIONS (1024 at two regions per thread, which
    allows about 480 threads in one process beside its other mappings)
    must stay above 2 × the per-process thread limit + 64. Raise it with
    that limit, or move std.thread to one region per thread (stack and
    arena together, with the stack at the low end so an overflow faults
    into the gap below the region).
11. **Collision with the parallel SMP work**: both rewrite
    `scheduler.mlx` and `smp.mlx`. Do step 2 only after that work is
    committed, and take its `capacity`/`busy`/`Mask` forms as the base.
12. **aarch64 and linked programs** get less: a shared arena, and no
    C-library TLS on std threads (6.7, 6.8).
13. **glibc's SIGSETXID and SIGCANCEL** need exact thread-directed
    delivery (5.4). A wrong thread hangs `setuid()` in threaded glibc
    programs.

---

## 10. Left open (decided "not now", with reasons)

- **Affinity and pinning**: `sched_setaffinity` is accepted and ignored.
  The scheduler picks the next ready thread round robin from the current
  index. Cache affinity (prefer `lastProcessor`) is a cheap later
  improvement in `scan`.
- **Robust futexes**: the robust list is not walked. A thread that dies
  holding a robust mutex leaves it held. No program here uses robust
  mutexes, and mlxlibc does not implement them.
- **Priority inheritance (futex PI)** is ENOSYS (5.3).
- **Native per-thread events**, such as a `thread_event_send`, are not
  needed: the fault is already thread-directed, and the rest is
  process-level by the native interface's design.
- **`threadlocal`** stays rejected. `std.thread.Local` covers Mlx
  programs, and C code has the C library's TLS (static TLS below the TCB,
  as mlxlibc already does in `pthread.mlx:262-282`).
