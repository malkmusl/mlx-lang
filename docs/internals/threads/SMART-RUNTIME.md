# MLX Smart Threads: the runtime, std, the scheduler and Observatory

Design for brixOS's own threading model, written against the repository as
it is (HEAD 40b80d5aa plus the SMP work in progress in the working tree:
`projects/brix/kernel/smp.mlx`, `scheduler.mlx`, `layout.mlx`,
`processors.mlx`; their line numbers move while that work goes on, so
those are cited with the function name too). Every fact below was read
from the code; the kernel analysis in THREADS.md (same directory) appeared
while this was written, and 0.1 says what is taken from it and where this
design goes further. SMART-LANG.md (same directory, the language
designer's part) defines the classes, the rules R1-R8 and the task blocks;
2.1 says what this runtime takes from it.

> **Critic:** HEAD is now cdca2dbdc (the SMP work moved on; `switchTo` is
> at `scheduler.mlx:566`, `holdOthers` at `smp.mlx:1124`, `layout.mlx` is
> `projects/brix/layout.mlx`, not under `kernel/`). Notes marked
> "Critic:" below correct claims that do not hold, or mark gaps; section 8
> at the end has the verdict, the counter-programs and the order changes.

The binding frame: brixOS's native interface is the substrate. Linux's
clone, futex and pthreads are adapters on top (the kernel's Linux layer for
foreign binaries, std's Linux path for Mlx programs on a Linux host,
mlxlibc's pthreads for C/C++). Three execution models: **Safe Threads**,
**Smart Tasks**, **Raw Threads**. Order of the work: native kernel threads,
then safe arena and ownership semantics, then structured concurrency, then
the smart scheduler, then Linux compatibility.

---

## 0. What the code gives us to start from (verified)

| Fact | Where |
|---|---|
| A process is one execution context today: registers, SSE, FS base, wait record, `inCall`, `lastCpu`, `cpuTime` all live in `Process` | `projects/brix/kernel/process.mlx:62-160` (fields at 101, 103-106, 109, 115, 117-118, 122, 132) |
| **No kernel stack per process**: a call that must wait leaves the process at its SYSCALL and makes it again when it runs | `projects/brix/kernel/wait.mlx:1-4`; `scheduler.mlx:416` `suspend` sets `rip -= 2` |
| The scheduler scans processes round robin, polling every blocked one's `canGo` | `scheduler.mlx:383` `canGo`, `:447` `scan`, `:555` `switchTo` |
| The process table grows with the processors (16 + 2 per further CPU) and per-CPU busy time is kept | `scheduler.mlx:40-54` (SMP work) |
| One kernel lock (spin, measured: who held it, for how long) | `smp.mlx:247` `lock`, `:194` `Waits`, `:300` `noteWait` |
| Idle CPUs are a 256-bit `Mask`, woken by IPI `Vector.WAKE` (0xF1); `STOP` 0xF2, `HOLD` 0xF3 exist; `holdOthers`/`holdHere` is an IPI rendezvous that works without the kernel lock | `processors.mlx:133` `Mask`; `smp.mlx:126,965,974,1066,1115`; `layout.mlx:305-310` |
| Native calls are `NATIVE_BASE (0x4000) + n`, n = 0..85 taken (85 `gpu_fence`) | `kernel/native/calls.mlx:35-126`; `spec/04-stdlib/os/brix.xml:89-170` |
| `thread_pointer_set` (69) writes FS base of the *process* | `native/calls.mlx:265-269` |
| One upcall per process; its stack, `inUpcall`, fault address are per process | `kernel/events.mlx:85-112` |
| Handles are descriptor slots, 64 per process, slot in 8 bits of the handle | `files.mlx:192`; `native/objects.mlx` `descriptorOf` |
| 256 memory regions per process (Nebula's tilepool already designs around it) | `process.mlx:79`; `projects/nebula/core/tilepool.mlx:1-11` |
| The kernel's Linux layer: clone with `CLONE_THREAD` and clone3 are ENOSYS, `gettid == getpid`, `exit == exit_group`, `set_tid_address` returns 1, `set_robust_list`/`rseq` return 0, affinity = CPU 0, no futex at all | `kernel/linux/system.mlx:203,291,321-328,355-357,378-383` |
| ACPI tables are found generically; only MADT is read; CPUID is readable on the running CPU | `acpi.mlx:105` `table(from, signature, nth)`; `processors.mlx:92` `read`; `cpu.mlx:56-57` `identify` |
| **The value arena is addressed by r14 (base, fill word first) and r15 (end)**; every escaping value is a `lock xadd [r14]` then a bounds check that traps | `compiler/selfhost/backend/x86_64/codegen/memory.mlx:103-115`; `_start` maps 1 GiB + 8: `codegen/emitter.mlx:194-218` (brixOS: `memory_map` = 0x4010) |
| An export reloads r14/r15 from `_start`'s data words in a linked program; a plugin export maps an arena per call | `codegen/labels.mlx:81-87,109` |
| Atomics exist (`@atomicLoad/Store/Rmw`, `@cmpxchgWeak/Strong`, `@fence`), `@threadPointer` (fs:0), `@frameAddress`, `@spawnThread` (raw Linux clone, syscall 56, child exits with syscall 60) | `spec/00-language/atomics-tls.xml`; `codegen/memory.mlx:126,140-170`; `ir/lower.mlx:3446` |
| `threadlocal` parses but does not lower | `ir/lower.mlx:579,590` |
| `@noncopy`, `@move`, automatic `deinit`, moved-state tracking; panics do not unwind | `spec/00-language/ownership.xml:10-80` |
| std.turns resets the arena by its fill word; it locates the arena once and caches it in a global | `std/src/turns.mlx:20,36-71,206-225` |
| std's lock waits go through `pages.sleepOn/wakeOne/yield`, which are **empty on brixOS** ("a brixOS process runs one thread") | `std/bootstrap/os/pages.brixos.mlx:36-41`; Linux: `pages.mlx:46-48` |
| The heap's lock calls Linux's futex directly, on brixOS too | `std/bootstrap/general_purpose_allocator.mlx:100-115`; `std/src/heap.mlx:69-85` |
| mlxlibc on brixOS translates Linux numbers in-process; `clone(CLONE_THREAD)` ENOSYS, futex a no-op; `pthread_create` uses `@spawnThread` (a raw SYSCALL 56 that bypasses the translation) | `compiler/libc/sys_calls.brixos.mlx:11-13,742,755,769,797,805`; `compiler/libc/pthread.mlx:256-301` |
| Three hand-made worker crews exist on Linux only (h264, h265, the splash) | `std/src/h264/decode.mlx:5066-5093`; `std/src/h265/decode.mlx:4241`; `projects/desktop/splash/bands.mlx:72-98` |
| Observatory's live views are files a program writes under `$XDG_RUNTIME_DIR/mlx/<kind>/`, read by `tools/codemap/<kind>.mlx`, drawn by `projects/observatory/<kind>_view.mlx` | `tools/codemap/memory.mlx:262-281`; `projects/observatory/frames_view.mlx`, `memory_view.mlx`; `main.mlx:173-187` |

Three consequences shape everything below:

1. **A brixOS thread is cheap**: registers + SSE + a wait record. No kernel
   stack (the restart model), so a thread record is ~1.5 KiB and the kernel
   can hold thousands.
2. **A per-thread arena costs nothing in the compiler on brixOS**: the
   kernel sets r14/r15 of a new thread from its start record. The arena is
   the Mlx thread ABI.
3. **The arena bump is already atomic**, so sharing it is not a crash, but
   a `turns` reset on one thread frees what another thread still uses. Per
   thread arenas are required for correctness, not only for safety.

> **Critic (facts missing from the table, both verified):**
> - **There is no user-space clock on brixOS.** The monotonic clock is
>   `clock_read` (native 32), a system call (`std/bootstrap/os/brix.mlx:242`),
>   and every system call takes the one kernel lock (`trap.mlx` `brix_trap`
>   calls `smp.lock` before `handle`). Every timestamp, spin window and
>   grain measurement this design puts on a hot path (2.3, 2.4, 2.6, 2.7)
>   is a full kernel entry under the global lock. See 8.3.
> - **"Costs nothing" (2.) is not free and has an edge:** the `lock xadd`
>   stays on every allocation (~18 cycles a private arena would not need,
>   THREADS.md 2.5), and once *identity* is r14 (3.1: the thread block at
>   base + 8), every case where two threads run on one r14 shares the
>   thread block: `std.thread.Raw`'s `.shared` arena (4.1), aarch64's one
>   global arena (risk 12), and exports of a *linked* program called from
>   C on a worker, which reload the **main** arena
>   (`codegen/labels.mlx:81-87`). There the heap's per-thread cache, the
>   held-lock levels and the crash context become shared, unsynchronized
>   state. See 8.2 (5).
> - The per-thread record is nearer 2 KiB than 1.5 (`waitItems` 64 x 8,
>   FXSAVE 512, `Frame`, `Accounting` ~350, `ThreadSignals`): 8 MiB at
>   4,096 threads, still fine.

### 0.1 Relation to THREADS.md

THREADS.md plans the kernel threads, the arena builtins, the Linux layer and
std.thread/std.sync in eleven steps. This document **adopts its facts and
its kernel mechanics** and builds the Smart Threads runtime on them:

- Facts taken over (checked here again): `Process.fault` answers *false*
  for a page that is already there (`process.mlx:488`), so two threads
  faulting on one fresh page would kill the process; the places that must
  shoot TLBs down (`memory.mlx:500` `release`, `:507` `drop`, `:566`
  `clone`, `:600` `flush`, `:653` `copyOnWrite`, `:674` `destroy`;
  `process.mlx:433` `giveBack`); user r14/r15 travel in the trap frame and
  are zeroed at a program's start (`projects/brix/image/blob.mlx`); the
  splash crew is switched off on brixOS
  (`projects/desktop/splash/brixos.mlx:558-559`); aarch64 reaches its arena
  through a global, not a register
  (`compiler/selfhost/backend/aarch64/codegen/runtime.mlx:109-170`); the
  existing thread tests (`tests/298_spawn_thread_runtime.mlx`,
  `315_tinypage_runtime.mlx`, `180_linux_futex_runtime.mlx`,
  `295_atomics_runtime.mlx`, `tools/mlxlibc_check.c:284-345`); the debug
  allocator's and diagnostics' locks sleep through `pages.sleepOn`
  (`debug_allocator.mlx:150,157`, `diagnostics.mlx:462,470`).
- Mechanics adopted as they are: the thread record and its bounds
  (THREADS.md 3.1, 3.10), the restarted `memory_wait` (3.6), the TLB
  shootdown with `Vector.FLUSH` acknowledged from the lock loop (3.4), one
  upcall at a time per process with thread-directed faults (3.7), process
  end/stop/branch/replace (3.9), calls **86-92** with their numbers and
  records (4.1), `@arena()`/`@arenaSwitch()` (2.3), std's thread block in the
  thread arena's header found through r14, not FS (2.2, 6.2), hooks for the
  modules in the compiler's closure (2.6-2.9), the seam
  `std/bootstrap/os/wait*.mlx` (6.5).
- Where this design goes further, because the vision asks for it and
  THREADS.md leaves it open (its section 10): **priorities** and
  **priority inheritance** (wake order, the `owner` flag, hand-over),
  **affinity that is honoured**, **topology and NUMA**
  (`system_topology`), **cancellation's alert**, **per-thread accounting and
  a scheduling trace for Observatory**, `memory_move` as a native call
  (not only in the Linux layer), and on top of all of it **std.task** (Smart
  Tasks), the **typed Safe Thread layer** (Mutex(T)/Guard, Frozen, Shared)
  and the **Thread Inspector**. These are calls **93-99** and flags on
  86-92 that THREADS.md's callers do not have to pass.

---

## 1. The kernel's native thread model

### 1.1 A thread record beside the process record

New module `projects/brix/kernel/thread.mlx`. The `Process` keeps what a
program *has* (space, regions, descriptors, roles, identity, events' claims
and queue, the Linux layer's actions); the `Thread` gets what a program
*does* (registers, waits, where it runs).

Moves out of `Process` (`process.mlx`) into `Thread`:
`waitingFor` (101), `waitItems`/`waitCount` (103-104), `timedOut` (106),
`inCall` (109), `context` (115), `sse` (117), `fsBase` (118), `lastCpu`
(122), `cpuTime` (132). `Events` (`events.mlx:85-112`) stays the
process's, with one upcall at a time: `Process` gains `upcallThread` (the
thread in the upcall), and a thread that needs the upcall while another
runs it waits (THREADS.md 3.7). From the Linux view (`linux/signals.mlx`
`Signals`): the mask, the per-thread pending set and the alternate stack
(THREADS.md 5.6's `ThreadSignals`).

```mlx
pub const ThreadState = enum(u8) { free, ready, running, blocked, stopped, ended }

pub const Thread = struct {
    state: ThreadState
    // From the same counter as pids (Scheduler.nextPid): a process's first
    // thread has tid == pid, so the Linux layer's getpid/gettid agree there.
    tid: usize
    process: u32              // its process's index in Scheduler.processes
    generation: u32           // a thread handle names the record as it was
    // --- from Process (per-thread execution state)
    waitingFor: wait.Wait
    waitItems: [wait.HandleWait.MAX]wait.HandleWait
    waitCount: usize
    timedOut: bool
    inCall: bool
    context: Frame
    sse: [Sse.SIZE]u8
    fsBase: usize
    lastCpu: usize
    // --- new
    memory: MemoryWait        // 1.3: key, expected, flags, bucket links, woken
    alerted: bool             // thread_set ALERT: its wait ends `interrupted`
    exitWord: usize           // u32 set to 0 and woken at the end (0: none)
    value: usize              // thread_exit's value
    runningWord: usize        // count running (1.2): decremented when it blocks
    linux: ThreadSignals      // mask, pending, alternate stack, robust list
    priority: Priority        // class, level, base level, lent level (1.3)
    affinity: processors.Mask // hard: where it may run (256 bits)
    domain: u16               // soft: the memory domain it prefers (0xFFFF none)
    energy: u8                // 0 efficient, 1 balanced, 2 performance
    name: [16]u8
    acct: Accounting          // 1.5
    next: u32                 // ready queue / word bucket links
    prev: u32
}
```

The records and their index table are made in frames, as the process
table now is (`scheduler.mlx:85-92` `make`), with THREADS.md 3.10's bounds:
256 threads + 8 per further processor (at most 4,096), a process at most
max(256, 2 x processors), 128 kept for user 0's processes. `Scheduler.current` (`:49`) indexes
threads; `running()` (`:94`) becomes `&processes[threads[current].process]`
and a new `runningThread()` gives the thread. The wait record, the restart
at the SYSCALL and the `timedOut` hand-over (`native/calls.mlx:145-152`)
keep working unchanged, now per thread.

What touches the moved fields (codemap `refs`): `scheduler.mlx` (state 39
uses, context 5, waitingFor 8, sse 3, lastCpu 2, cpuTime 2), `trap.mlx`
(`inCall` 6: `deliver` 341, `deliverNative` 362, `deliverLinux` 400),
`native/calls.mlx` (`timedOut`, `fsBase`, `waitFor` 381), `native/objects.mlx`
(`anyReady`), `native/processes.mlx` (`branch` 240), `linux/system.mlx`
(`timedOut`, `ARCH_PRCTL` 357). Eight files: a contained refactor.

> **Critic:** more than eight. `events.mlx` (the upcall's context record
> copies the SSE area) and `linux/signals.mlx` (signal frames) read `sse`;
> `process.mlx` itself (`init`, `fault`, `branch`) uses most moved fields;
> and `Process.state` must split into a process state (alive, stopped,
> zombie) and the threads' (ready, running, blocked), which touches every
> `State.running/blocked` test in `scheduler.mlx` (`alive`, `scan`,
> `runnableBeyond`, `terminate`) and the Linux layer. Still contained, but
> K1 should list them. Also: `linux: ThreadSignals` in the *native*
> `Thread` record puts the adapter's shape into the base; keep it in a
> side table owned by `kernel/linux/`, indexed by the thread's slot.

Process life with threads:

- **process_exit (23)** ends every thread (C's `exit`, `_start`'s end:
  `codegen/emitter.mlx` emits `0x4017`). **thread_exit** ends one; the last
  thread's end ends the process with its code.
- **terminate** (`scheduler.mlx:192`) marks every thread `ended`; one that
  runs on another CPU gets `Vector.WAKE`, enters the kernel, sees its state
  and switches away. The space and the regions are freed by the **last CPU
  that leaves a thread of the process** (a count of its threads on CPU),
  the generalisation of the orphans in `freeAllButRunning` (`:99`).
- **process_branch** (`native/processes.mlx:240`) copies the calling thread
  only. **process_replace** ends the other threads first and waits until
  none is on a CPU (Linux's `de_thread`).
- **Faults** are the faulting thread's: it runs the process's upcall
  (one at a time, `upcallThread`); process events (interrupt, child,
  terminate, ...) go to the process's *event thread* (the first thread
  unless `thread_set(EVENT_THREAD)` names another). `trap.mlx:263` reads
  `p.events` for the fault and marks the record with the thread.
- **A fault on a page another thread just made present** must not end the
  process: `Process.fault` answers true when the page is there and allows
  the access (`process.mlx:488` answers false today; THREADS.md 3.8).

### 1.2 The native calls (86-92 as THREADS.md, 93-99 for Smart Threads)

Appended to `<Calls>` in `spec/04-stdlib/os/brix.xml` and to `Call` in
`native/calls.mlx:38-126`. The handle kind **thread** (12) joins
`<Kinds>`; its state has `ended` (0x8) when the thread ended. Calls 86-92
are THREADS.md 4.1's, word for word in what they already say; what this
design adds to them is marked *[+]* and is optional for a caller (a zero
field or flag gives THREADS.md's behaviour).

```xml
<Call n="86">thread_create(start*, size) -> thread h, or the thread's id with flag 1 (THREADS.md 4.1's record of 64 bytes: entry u64, stack u64, argument u64, second u64, thread pointer u64, id word* u64, exit word* u64, flags u32, reserved u32. [+] With size 96 the record goes on: arena u64 and arena end u64 (r14 and r15 at the start; 0: the caller's, as THREADS.md has it), priority u16 (class &lt;&lt; 8 | level; all ones: the caller's), domain u16 (all ones: none), energy u8, reserved [11]u8. [+] flags: suspended 2 (it waits for thread_set resume), count running 4 (it counts in its process's running word, 94 what 7))</Call>
<Call n="87">thread_exit(value) (THREADS.md 4.1)</Call>
<Call n="88">memory_wait(address, expected, deadline, flags) -> 0 (THREADS.md 4.1: would_block, timed_out, interrupted, flags shared 1. [+] owner 2: the word's low 30 bits are the tid of the thread that holds what the word guards; while the caller waits that thread runs with the caller's priority if it is higher (chains followed 8 deep); [+] wide 4: a u64; [+] interrupted also when thread_set alerts the thread (94 what 6))</Call>
<Call n="89">memory_wake(address, count, flags) -> woken (THREADS.md 4.1, but [+] the highest priority first, the longest waiting among equals (the same order as THREADS.md's while every thread has one priority); [+] hand over 2: the word set to the first woken thread's tid, with bit 31 when others still wait, before any of them runs)</Call>
<Call n="90">thread_self() -> id (THREADS.md 4.1)</Call>
<Call n="91">thread_yield(tid) (THREADS.md 4.1; [+] tid: that thread of the process first, a lock's holder; 0: any)</Call>
<Call n="92">thread_info(thread h, into*) -> state (THREADS.md 4.1's 32-byte record)</Call>
<Call n="93">memory_move(from, to, wake, move, expected, flags) -> woken | moved &lt;&lt; 32 (wake up to `wake` waiters of `from`, then move up to `move` of the rest onto `to`, where their wait goes on: a condition's waiters onto its lock's word instead of all at once; with flags check 4: would_block unless the word at `from` is `expected`; shared 1)</Call>
<Call n="94">thread_set(thread h or 0, what, value) (0: the caller; what: priority 1 (class &lt;&lt; 8 | level: real-time 0 (user 0 alone, no_right otherwise), interactive 1, normal 2, background 3, idle 4; level 0-15), affinity 2 (value: the address of 32 bytes, a bit for each processor; invalid when it names none that runs), name 3 (16 bytes), energy 4 (0 efficient, 1 balanced, 2 performance), domain 5, alert 6 (its current or next wait ends interrupted), running word 7 (the address of a u32 the kernel decrements when one of the threads counted in it blocks in a call and increments when it runs again), event thread 10 (process events go to it), suspend 11, resume 12)</Call>
<Call n="95">thread_get(thread h or 0, what, into*, capacity) -> bytes (as thread_set, read; and record 13 (thread_list's record), times 14 (a 16-byte pair for each processor it ran on: processor u32, reserved u32, ns u64; the most first), why 15 (where it ran last and why: processor u16, reason u8 (last processor 1, sibling 2, package 3, domain 4, affinity 5, preempting 6, queued 7), reserved, at ns u64))</Call>
<Call n="96">system_topology(into*, capacity) -> bytes (the processors, their cores and packages and the memory domains, as the firmware and the processors say: a 64-byte header (processors u32, packages u32, cores u32, domains u32, threads per core u32, flags u32 (1 SRAT read, 2 SLIT read, 4 cores of two kinds), the boot processor u32, reserved), a 32-byte record for each processor (index u16, domain u16, package u16, core u16, thread in core u8, kind u8 (0 unknown, 1 performance, 2 efficient), online u8, reserved u8, local APIC id u32, cache group u32 (processors sharing the last level cache), busy ns u64), a 32-byte record for each domain (index u16, processors u16, reserved u32, memory bytes u64, free bytes u64, reserved u64), then the domains' distances, a byte each (SLIT; 10 the domain itself); the bytes there are, the first `capacity` written)</Call>
<Call n="97">thread_list(records*, capacity, process h or 0) -> count (a 128-byte record for each thread of the process (0: the caller's; any process for user 0): tid u32, pid u32, state u8, class u8, level u8, lent level u8, processor now u16 (all ones: none), last processor u16, processor time u64, switches it made u32, preemptions u32, migrations u32, why u8, domain u8, energy u8, reserved u8, memory waits u32, other waits u32, memory wait ns u64, other wait ns u64, ns waiting for the kernel u64, started ns u64, name [16]u8, waiting for: kind u8, reserved [7]u8, its word's address or its handle u64, allowed processors u16, first allowed u16, reserved [12]u8)</Call>
<Call n="98">thread_trace(capacity) -> memory h (the kernel writes a 32-byte record for each switch of the caller's process's threads into a ring in this memory object (at most 1 MiB, the memory objects' bound): ns u64, processor u16, kind u8 (on, off, woken, moved, blocked, preempted), why u8, tid u32, other tid u32 (who woke it, who ran next), detail u64 (a word's address, a handle, a call's number); the first 64 bytes a header with the write index; a second call gives the same object)</Call>
<Call n="99">memory_handle(address, flags) -> h (later: a handle readable after a memory_wake reached the word, so the one wait can wait for words and handles together)</Call>
```

Notes on the choices:

- **No pthread shapes in the native calls.** The kernel sets rsp, rdi, rsi,
  FS base and, with the longer record, **r14 and r15**: on brixOS a thread
  can start on its own arena before any instruction of it runs (useful for
  mlxlibc's threads, 4.4); std's own threads switch with `@arenaSwitch`
  first thing (THREADS.md 2.4), which works on Linux too.
- **The exit word** gives a join without a handle (descriptors are 64 per
  process, `files.mlx:192`); flag 1 (detached) returns the id instead of a
  handle. std's pool workers are detached and joined by their exit words.
- **The tid comes from the pid counter**, so ids never collide and the Linux
  layer's `getpid()` of the main thread equals its `gettid()`.
- **Priority, affinity and topology are the kernel's**, not emulated in
  user space: Smart Tasks need the kernel to place worker threads (1.6),
  and Raw Threads (drivers, audio) need hard affinity and real-time class.

> **Critic:**
> - "No pthread shapes" overstates it: the id word and exit word are
>   `CLONE_PARENT_SETTID`/`CLONE_CHILD_CLEARTID`, and the `owner` word
>   (30-bit tid, bit 31 "others wait") is Linux's PI futex word
>   (FUTEX_TID_MASK, FUTEX_WAITERS). They are defensible as user-space
>   fast paths, but the reason the design reaches for words instead of
>   native handles is the **64-slot descriptor table** (`files.mlx:192`),
>   which this design keeps ("`files.Descriptors.MAX` stays", 1.6). With
>   64 slots a process cannot hold a handle per worker, nor a cancel
>   channel per scope. Native first means raising it in K2 (the slot has
>   8 bits: 256 without changing the handle format), so a thread *handle*
>   waited on with the native `wait`, and native cancel objects, are
>   affordable; the exit word stays as the no-call fast path.
> - **A detached thread cannot free its own stack.** `RawThread.detach`
>   (4.1) says "the thread frees its mapping itself at its end", but Mlx
>   code needs its stack to call `memory_unmap` and then `thread_exit`.
>   `thread_exit` needs an optional range the kernel unmaps *after* the
>   thread is off its stack: `thread_exit(value, unmap_base, unmap_length)`.
> - **tid space**: the `owner` flag keeps a tid in 30 bits, but tids come
>   from the pid counter, which only grows. Bound it (reuse ids, keep the
>   generation in the handle) or the PI word becomes ambiguous after 2^30
>   spawns.

### 1.3 Waiting on memory: `memory_wait`, `memory_wake`, `memory_move`

The substrate of every lock in std, mlxlibc and the Linux adapter. It is
brixOS's own, not a futex copy:

| | futex | brixOS words |
|---|---|---|
| deadline | relative (WAIT) or absolute with a clock flag (BITSET) | always absolute, the monotonic clock, as the native `wait` (`brix.xml` Wait) |
| wake order | unspecified | priority first, then the longest waiting |
| hand-over | no (or PI calls apart) | `memory_wake(.., hand over)` writes the next owner into the word |
| priority inheritance | separate LOCK_PI calls | flag `owner` on the ordinary wait |
| process-shared | by address, private flag | private by default; `shared` keys by the memory, like memory objects (`brix.xml` n=19) |
| interruption | signals, EINTR | the upcall (`interrupted`) and `thread_set(ALERT)`: cancellation's substrate |
| requeue/op variants | REQUEUE, CMP_REQUEUE, WAKE_OP, ... | `memory_move` with an optional check |

Kernel structures (in `thread.mlx`, all under the kernel lock at first):

- `MemoryWait` in the thread: key (the space's root for private, the frame's
  physical address for shared), address, expected, flags, `woken: bool`,
  bucket links (THREADS.md 3.6's `futex.mlx`, under the name of the native
  call).
- `buckets: [256]Bucket` hashed by key and address (THREADS.md 3.6); each a
  list ordered by priority, FIFO within a priority, so that with one
  priority the order is THREADS.md's FIFO.
- A new `WaitKind.memory` (`wait.mlx:5-38`). `canGo` (`scheduler.mlx:383`)
  never polls it: it is true only when `woken` or the deadline passed.
  `wakesAlone` (`:408`) counts it for its deadline only.
- **The restart model**: a woken thread makes `memory_wait` again at its
  SYSCALL (`wait.mlx:1-4`); the call sees `woken` (as it sees `timedOut`
  today, `native/calls.mlx:147-149`) and returns 0 without reading the
  word again. Without this, a lock's waiter woken by a release would read
  the word again, find it taken by a barging thread and sleep with no
  wake-up coming.

  > **Critic:** the reason given is wrong for std's lock: with Drepper's
  > 0/1/2 word a barger leaves 1 (the re-made call returns would_block) or
  > a waiter leaves 2, and the holder's release wakes again. The real
  > reason is a **consumed wake**: a woken thread that re-reads a word
  > equal to `expected` (ABA: a semaphore or latch count put back) sleeps
  > again and the wake meant for it is gone. Keep `woken`; also fix the
  > **precedence**: a thread both woken (above all with *hand over*, which
  > already wrote its tid into the word: it owns the lock) and then
  > alerted or upcalled before it runs must see 0, not `interrupted`, or a
  > handed-over lock is never released. And a `shared` word must not be
  > keyed by the frame's physical address (a copy-on-write break or a
  > `pages.release`, which unmaps and maps afresh on brixOS, changes the
  > frame under the waiters): key it by (memory object, offset), the way
  > the native memory objects (n=18/19) already name shared memory.
- **Priority inheritance** (`owner`): the waiter's effective level is lent
  to the owner tid read from the word; the owner's `lent` is recomputed when
  one of its waiters is woken or times out. Chains are followed to a depth
  of 8.
- **A wake readies** the thread and puts it on a CPU by the placement of
  1.6, without polling.

### 1.4 Topology: what the kernel reads, and adds

| Source | Today | Add |
|---|---|---|
| MADT: local APIC / x2APIC ids, enabled, online capable | `processors.mlx:92` `read` (SMP work) | the ACPI processor uid of each, to match SRAT |
| CPUID leaf 0xB / 0x1F (x2APIC id, SMT and core shift widths) | `cpu.mlx:56` `identify` on the running CPU only | each AP reads its own at its first entry (`smp.joined`, `smp.mlx:943`, called from `brix_ap`, `trap.mlx:133`) into a per-CPU topology record: package, core, thread in core |
| CPUID leaf 4 (the last level cache's sharing) | no | "cache group" = APIC id >> log2(sharing count) |
| CPUID leaf 0x1A (Intel hybrid core type) / AMD 0x8000001E | no | `kind`: performance / efficient |
| SRAT (`"SRAT"`): type 0 and 2 (processor -> proximity domain), type 1 (memory range -> domain) | `acpi.mlx:105` finds any table by signature; SRAT not read | domains, and the frames of each domain for the frame allocator |
| SLIT (`"SLIT"`): n x n distance bytes | not read | the distance matrix |

> **Critic:** CPUID leaf 4 is Intel's; AMD reports the last-level
> cache's sharing in 0x8000001D (same format, with TopologyExtensions),
> and 0x8000001E is AMD's core/node ids, not a core *kind* (AMD's
> heterogeneous cores are in 0x80000026). Read by vendor.

Without SRAT: one domain with every processor. QEMU makes both tables with
`-numa node,...` (7.2). `system_topology` (96) gives the result; `smp`'s
per-CPU busy time (`scheduler.mlx:54` `busy`) fills `busy ns`.

### 1.5 Per-thread accounting for Observatory

```mlx
pub const Accounting = struct {
    started: u64
    ended: u64
    cpuTime: u64                 // what Process.cpuTime is now (charge, scheduler.mlx:519)
    onCpu: [16]CpuTime           // processor u16 + ns u48: where it ran most
    elsewhere: u64               // the time on any other
    domainTime: [8]u64
    switched: u32                // it gave the processor up (a wait, a yield)
    preempted: u32
    migrations: u32
    waits: [7]WaitCount          // memory, handles, sleep, io (pipe/terminal/socket), child, signal, other
    kernelWait: u64              // its entries' waits for the kernel lock
    why: u8                      // the last placement's reason (1.6)
    whyAt: u64
}
```

`charge` (`scheduler.mlx:519`) adds the turn to `cpuTime`, `onCpu` and
`domainTime`. `smp.lock` (`smp.mlx:247`) measures each wait already
(`noteWait`, `:300`); it also gives `took` back so `brix_trap`
(`trap.mlx:68`, which takes the lock before it knows the thread) adds it to
the entering thread's `kernelWait`. Every `suspend` (`scheduler.mlx:416`)
starts a wait clock; the readying adds it to `waits[kind]`. Cost: a few
adds per switch, all under the lock that is held anyway.

### 1.6 How it meets the scheduler and the one kernel lock

**The lock stays.** Every thread, word and queue change happens under it,
as process changes do now. User space keeps the kernel off the fast paths:
an uncontended lock is a compare-exchange in user memory; tasks move
between workers through user-space deques; the kernel sees only parking
(`memory_wait`), unparking (`memory_wake`) and real waits. The cost of what is
left is measured per thread (`kernelWait`) and per holder (`smp.Waits`,
`system_info` n=52), so Observatory shows when the lock limits a program.

**Scheduler steps.**

1. K1: `scan` (`scheduler.mlx:447`) goes over threads instead of
   processes, round robin, the same policy. Pure refactor.
2. K4: ready queues per priority class (real-time, interactive, normal,
   background, idle) with a bitmap of non-empty classes; FIFO within a
   class; slices by class (4, 10, 20 ms); background aged so it is never
   starved; affinity filters what a CPU may take. Blocked threads in three
   lists: woken explicitly (words, children), deadline only (a min-heap
   replacing the O(n) `nearestDeadline`, `:493`) and polled (terminal,
   pipes, sockets, handles: `canGo`'s kinds as today).
3. Placement when a thread becomes ready: its last CPU if idle and allowed
   (`why = LAST_CPU`); else an idle allowed CPU in the same cache group,
   then package, then domain (`SIBLING`, `PACKAGE`, `DOMAIN`), with
   `energy = efficient` preferring a CPU whose package is awake and
   `performance` a whole idle core; else the CPU running the lowest
   priority thread below it, which gets **`Vector.RESCHEDULE` (0xF5)**
   (`PREEMPT`); else queued (`QUEUED`). `kickIdle` (`scheduler.mlx:509`)
   and `smp.wake` (`smp.mlx:974`) do the waking, one IPI per thread made
   ready (the SMP work's `woken` accounting).

   > **Critic:** K4 must also make `runnableBeyond` (`scheduler.mlx:476`)
   > and `kickIdle` (`:509`) O(1) (a count of ready records), not only
   > `nearestDeadline`: while any CPU idles, every way out of the kernel
   > runs `canGo` over the whole table under the one lock. With 4,096
   > thread records that, not user code, is the first scaling limit.
   > `regionEnd` (`process.mlx:508-517`) is a linear scan per page fault,
   > 1,024 long after K2, also under the lock.
4. Later and independent: word buckets and per-CPU run queues get spin
   locks of their own so that `memory_wait`, `memory_wake`, `thread_yield` and
   `thread_exit` do not take the big lock; then per-CPU queues with
   kernel-side stealing; then NUMA frame lists (first touch from the
   faulting CPU's domain in `Process.fault`, `process.mlx:483`).

**TLB shootdown.** Today a space runs on one CPU at a time; `lastCpu` plus
`memory.forgetActive` (`memory.mlx:366`) reload CR3 on the next CPU.
Threads of one process run on several CPUs at once, so a change that
removes or downgrades mappings must reach them before a frame is freed or
reused: `Space.release`/`drop` (`memory.mlx:500,507`, reached from
`munmap`, `process.mlx:622`, and `giveBack`, `:433`), `copyOnWrite`
(`memory.mlx:653`), `clone`'s narrowing for a branch (`:566`; `fork`,
`scheduler.mlx:171`, reloads only the current CPU), `flush` (`:600`);
`destroy` (`:674`) runs only once no thread of the process is on a CPU.
The mechanism is THREADS.md 3.4's:

- `Space` gets `loaded: processors.Mask` (set in `switchTo`, `:555`, when a
  CPU activates it) and a `generation`.
- After such a change: every other CPU in `loaded` that is in user mode with
  that space gets **`Vector.FLUSH` (0xF4)**, taken like `HOLD` before the
  kernel is asked for (`trap.mlx:77-80`, `smp.holdHere` `:1115`): reload
  CR3, acknowledge. The initiator waits for the acknowledgements (bounded,
  as `holdOthers`, `:1066`, does) while it holds the kernel lock; no
  handler needs the lock, so no deadlock.
- A CPU that spins in `smp.lock` with interrupts off cannot take the IPI.
  It is in the kernel and uses no user mapping: it sees the holder's work
  word say FLUSHING and acknowledges from the lock loop, exactly as it says
  HOLDING today (`smp.lock`'s loop, `heldWaiting`); `forgetActive` is set
  for it, so it reloads CR3 before it returns to user mode. A CPU in its
  idle halt has no user space loaded (the idle loop activates the boot
  tables, `scheduler.mlx:555` `switchTo`) and is skipped.

> **Critic: a shootdown may not be "bounded, as holdOthers does".**
> `holdOthers` waits at most 200 ms and then goes on with whoever did not
> answer (`smp.mlx:1141-1143`); that is acceptable for a display mode, but
> a shootdown that goes on and frees or reuses the frame leaves a CPU with
> a live translation to memory that now belongs to someone else: silent
> corruption, exactly risk 2. Either wait without bound (and report a CPU
> that does not answer) or, on timeout, keep the frames and the old page
> tables on a deferred list freed only when every CPU in `loaded` has
> acknowledged a later generation. **Cost:** the initiator waits for the
> IPI round trip *holding the one kernel lock*, so every unmap in a
> threaded process stalls every CPU's kernel entries. std unmaps often:
> `pages.release` is unmap + map on brixOS (`pages.brixos.mlx:27-30`),
> slabs go back, and 2.3 maps and unmaps a chunk per scope. Batch the
> frees (a per-space list flushed once per lock hold) and keep scope
> chunks in user space (2.3).

**Region and handle limits.** `MAX_REGIONS` (`process.mlx:79`) goes from
256 to 1,024 (THREADS.md 3.10); std's workers put guard, stack and arena
into one mapping (one region instead of two). Detached threads need no
handle (the exit word); `files.Descriptors.MAX` stays.

> **Critic:** at 256 processors 1,024 regions do not hold: up to 512
> workers (2 x processors, 2.4) + a background pool of 64 + Nebula's 240
> tile blocks (`core/tilepool.mlx:13-14`) + every heap slab and large
> block (each its own mapping) + scope chunks (2.3). Size `MAX_REGIONS`
> from the processor count, or make the region table growable, and cap
> the pool by the regions left. `files.Descriptors.MAX` should not stay
> (see the note in 1.2). Note too that one mapping per worker breaks
> `turns.locate`'s brixOS path, which takes the *mapping's* start from
> `memory_region` as the fill word (`turns.mlx:44-50`): with the guard
> and stack in front, only the hook (THREADS.md 2.7) finds it.

---

## 2. Smart Tasks: `std.task`

### 2.1 The contract with the language (SMART-LANG.md)

The language part gives the runtime exactly this, and the runtime relies on
nothing hidden:

- **Blocks** are second-class values of the types `@Block(fn...)`,
  `@Task(fn...)` (may run on another thread) and `@Scope(fn...)` (every
  task spawned in it is joined when it ends); a block value is two words,
  the generated function and its context record, whose first word is the
  context's size (SMART-LANG 5.4).
- **Classes** on addresses: `@immutable(P)`, `@shared(P)`, `@owned(P)`;
  everything else with an address is LOCAL; plain data crosses by copy
  (SMART-LANG 0.1, 3.x).
- **Rules R1-R8**, checked by sema and `ir/thread_check.mlx`: nothing with a
  LOCAL address crosses a thread (R2), transfer is `@move` (R3), borrows are
  second-class so a `Handle` cannot leave its scope (R4), task code does not
  touch unsynchronized globals (R5), immutability is transitive (R6),
  shared data only through atomics or a lock's borrow (R7), raw threads and
  casts are `unsafe` (R8).
- **Per task block** two more generated functions: `__drop` (the moved
  captures of a task that never ran) and `__result_drop` (an unjoined
  non-copyable result); the task record keeps both (SMART-LANG 5.4).
- **Names**: a task block's function is named `path:line:task`, so crash
  reports, profiles and the Thread Inspector name tasks by place.
- **The arena guarantee** that follows from R2: no address into a task's
  arena outlives the task, so a worker resets its arena after every task
  with no check (SMART-LANG 0.6).
- **Two builtins**, THREADS.md 2.3's: `@arena() -> (usize, usize)` (r14,
  r15) and `@arenaSwitch(base, end) -> (usize, usize)`, an optimizer
  barrier. A worker switches to its arena first thing; the runtime switches
  nothing per task (a task's values are reset by mark, 2.3). Until a
  canonical compiler has them, std's modules in the compiler's closure
  reach them through hooks (THREADS.md 2.6-2.9).

What the runtime adds on its side of the line: the lock levels and the
"no lock across a blocking call" check (2.7, run time, debug builds), and
the task-reachable-globals check as a tool until `ir/thread_check.mlx`
exists (5.).

### 2.2 The API

```mlx
const task = std.task

// Structured: leaving the scope finishes every task in it (the owner's
// sketch `task.scope { task.spawn { ... } }` is sugar for this).
const result = task.scope(E!Combined, |s| {
    const a = s.spawn { processImage(image_a) }       // Handle(E!Picture)
    const b = s.spawn { processImage(image_b) }
    return combine(try a.join(), try b.join())
})

// Data parallel: the runtime cuts the range, the developer does not.
task.parallel_for(image.tiles(), |tile| { applyFilter(tile) })
task.parallel_range(0, rows, |row| { blurRow(row) })
const total = task.parallel_reduce(u64, items, 0, countOne, add)

// Channels between tasks and threads (bounded, transferable items).
var lines = task.Channel(Line).init(64)
try lines.send(@move(line))
const next = try lines.receive()       // error.Closed, error.Cancelled

// Cooperative cancellation and options.
try task.checkpoint()                   // error.Cancelled once the scope is
const fast = task.options(.{ .priority = .interactive, .deadline = now + 16 * ms })
```

Library shapes (SMART-LANG 5.5-5.7): `scope(comptime R: type, body:
@Scope(fn(*Scope) -> R)) -> R`; `Scope.spawn(self: *Scope, body:
@Task(fn() -> R)) -> Handle(R)` (an unqualified `task.spawn` binds to the
innermost scope); `Handle(R).join(self) -> R`, with `error.Cancelled` merged
into a task's error set; `parallel_for(items: []T, body: @Task(fn(*T) ->
void))`; `scopeAll` keeps siblings running after an error and collects
every error. A Scope's default is fail fast.

### 2.3 Inside the runtime

Module layout: `std/src/task.mlx` (API) and `std/src/task/`: `pool.mlx`,
`deque.mlx` (Chase-Lev), `job.mlx`, `scope.mlx`, `parallel.mlx`,
`channel.mlx`, `cancel.mlx`, `policy.mlx` (priority, energy, NUMA),
`diagnose.mlx`, `report.mlx` (Observatory), on `std.sync` and `std.thread`.

**The runtime** is made once (`std.sync.Once`) at the first `std.task` call.
It reads the topology (`std.thread.topology()`: `system_topology` on brixOS;
`sched_getaffinity` and `/sys/devices/system/cpu` on Linux) and the
allowed processors. `workers = allowed - 1` (the thread that waits in a
join helps, so the whole machine is used), overridable with
`task.configure(.{ .workers = n })` or `MLX_TASK_WORKERS`. Workers are
**started on demand**: none at first, one more each time work is pushed
while every started worker is busy, up to the maximum (a program that runs
one `parallel_for` of ten items never makes 255 threads).

**A worker** is a Safe Thread (3.1) with: one mapping per worker (guard,
1 MiB stack, an arena of the main arena's shape, 1 GiB + 8 reserved, its
header holding the thread block: std.turns and std.crash assume that size,
`turns.mlx:20`; pages on first touch), a deque per priority class (interactive, normal; background tasks
go to the background pool), a park word, a slab of job records, its
`index` (`task.workerIndex()`: 0..workers-1, the outside threads after).

**A job** (task record):

```mlx
const Job = struct {
    state: u32                  // queued, running, waiting, done, failed, cancelled
    function: usize             // the task block's function (named path:line:task)
    drop: usize                 // __drop: the moved captures of a task that never ran
    resultDrop: usize           // __result_drop: an unjoined non-copyable result
    scope: *Scope
    parent: ?*Job
    result: [*]u8               // the slot in the joiner's memory
    error: u16
    priority: u8
    flags: u8
    site: usize                 // the spawn's return address (the spawning function)
    worker: u16                 // where it ran
    why: u8                     // LOCAL, STOLEN_SIBLING, STOLEN_PACKAGE, STOLEN_DOMAIN, STOLEN_REMOTE, INJECTED, HELPED
    arenaBytes: u32             // what it took of its worker's arena
    queued: u64
    started: u64
    ended: u64
    context: [CONTEXT_INLINE]u8 // the block's captures, copied (larger: on the heap)
}
```

Jobs of a `spawn` live in the scope's region (a chunk from the page
allocator the Scope owns, SMART-LANG 5.4): the context is copied there
bitwise, the one place the runtime moves a non-copyable capture. Jobs that `parallel_for` splits off come from the
splitting worker's slab and go back to it through an atomic free list (a
thief frees into the owner's list).

> **Critic (per-task costs this section does not count):**
> - **A scope's chunk from the page allocator** is, above the tiny-page
>   limit, a `memory_map` (a kernel entry under the one lock, and one of
>   the process's regions) and at the scope's end a `memory_unmap`, which
>   with threads is a TLB shootdown to every CPU running the process
>   (1.6). Per `task.scope`. Recursive divide and conquer opens thousands.
>   Take scope chunks from a per-worker cache that never goes back to the
>   kernel while the pool lives.
> - **`queued`/`started`/`ended`** are three clock reads per task; on
>   brixOS each is `clock_read`, a system call through the global lock
>   (0, Critic). Fill them only under `MLX_TASKS=1`, and only from a
>   user-readable clock (8.3: a kernel-published TSC page, step K2b).
> - **A stolen spawn/join round trip** on an otherwise parked pool is a
>   `memory_wake` (the push finds no searcher), an IPI to a halted CPU,
>   then a `memory_wait` by the joiner and a `memory_wake` by the
>   finisher: three kernel entries under the one lock, plus the three
>   clock reads. At 64 CPUs this, not the deques, bounds tasks per second;
>   the spin windows of 2.4 are what keep it rare, so they must be
>   counted in iterations, not microseconds, until the clock page exists.
> - **The injection queue under a `Lock`** is the path of every spawn from
>   a thread that is not a worker, and the most common spawner is the
>   main/UI thread. Give an outside thread that opens a scope a deque of
>   its own for the scope's life (it joins, so it is there to pop it).

**Arenas per task.** A worker notes its arena's fill before a task and puts
it back after (as `turns.turn` does, `turns.mlx:206-225`): what a task
made is gone when it ends; its result was copied into the joiner's slot
(R2, 2.1). Helping in a join runs other tasks deeper on the
same stack and arena, so marks nest like frames and a helper's reset never
frees the waiting task's values. `turns.turn` on a thread with an open
scope traps in a debug build. `MLX_TASK_POISON=1` overwrites what a task
leaves, as `turns`' poison does, so a value wrongly kept shows.

**Tasks are not coroutines.** A task runs to its end on the stack of the
worker that took it; a task that waits either helps (runs other tasks
nested deeper on the same stack) or, with nothing to help with, parks its
worker thread. No stack is switched and no frame is saved by the runtime,
which is what keeps the arena marks a stack, keeps std.crash's reports
plain stacks, and needs nothing from the compiler beyond blocks. A worker
parked inside a task is covered by a spare worker (2.4 *Load*).

> **Critic:** the price of "not coroutines" is that a task buried under a
> helper on the same stack cannot run until everything above it returns.
> For joins (a tree) that is only latency; for anything that waits on a
> *sibling* (channels, 2.6) it is a deadlock that no spare worker can
> break, because the task that would make progress is not runnable by
> anyone else. Two limits follow: (1) only joins may help; (2) a thread
> that is not a worker (the UI thread) helps only with tasks of the scope
> it joins, never with thefts, or one long stolen task drops its frames;
> workers stop helping at a nesting depth that fits the 1 MiB stack.

**Scheduling a task.** `spawn` pushes onto the current worker's deque
(LIFO for the owner, FIFO for thieves), or, from a thread that is not a
worker, onto the injection queue (a bounded MPMC queue under a
`std.sync.Lock`). A worker takes its own newest; when empty it steals, in
the order of distance (2.4), oldest first; then the injection queue; then
it spins (bounded, 2.4) and parks on its park word (`memory_wait`). A push
wakes one parked worker when no worker is searching (at most half the
workers search at once: no herd).

### 2.4 Priority, energy, topology, load

- **Priorities.** Interactive and normal deques per worker; a worker takes
  interactive first, and one normal task in every 32 picks so normal never
  starves. Background tasks run in a **background pool** (at most a quarter
  of the processors, kernel class background, energy efficient: efficient
  cores where the topology has them). The UI thread's tasks are interactive
  by default (std.ui marks its loop's thread).
- **Energy.** Three modes (`task.configure(.{ .energy = ... })`, default
  balanced): *performance* spins up to 50 µs before parking and wakes a
  worker for every push that finds none idle; *balanced* spins 20 µs and
  wakes another worker only when the backlog exceeds twice the active
  workers or a task waited over 1 ms; *efficient* spins 5 µs, packs work
  onto the workers of one package (and SMT siblings) and parks the far
  ones first. Workers pass the mode to the kernel (`thread_set(ENERGY)`),
  which places by it (1.6). A parked worker costs nothing: its CPU halts
  in the idle loop (`scheduler.mlx:555` `switchTo`'s idle branch).
- **Topology.** Workers get a soft preference for a domain
  (`thread_set(DOMAIN)`) spread evenly over the domains, not a hard pin
  (the kernel may still move them). Steal order: the SMT sibling's worker,
  the same cache group, the same package, the same domain, then other
  domains by SLIT distance; random within a level. Local thefts take one
  task, remote ones half the deque.
- **NUMA.** A worker's arena is first touched by that worker, so with the
  kernel's first-touch frames (1.6 step 4) its pages are on its domain.
  Tasks spawned by a task go to its worker's deque: the data its parent
  just wrote is near. `task.options(.{ .domain = d })` sends a scope's
  tasks to the workers of domain d (the injection queue of that domain).
- **Load.** On brixOS the workers are created with *count running* (flag 4 of `thread_create`, 86)
  on the pool's running word: the kernel decrements it when a worker blocks
  in a call (a disk read, a socket) and increments it when it runs again.
  When it falls below the target while work is queued, a parked worker is
  woken (or one more started, up to twice the processors): CPUs stay busy
  while tasks wait for I/O. Every 100 ms an idle worker compares the
  system's busy time (`system_topology` busy ns, `system_info` n=52) with
  its own; when other processes keep the machine busy the pool's target
  drops instead of oversubscribing. On Linux, std's own blocking calls mark
  the worker blocked in the runtime (the same counter, kept in user space).

  > **Critic: nobody acts on the running word.** The kernel only
  > decrements it; "a parked worker is woken" needs a running thread to
  > notice, and when every worker is blocked in I/O none runs. The kernel
  > must do the waking: `thread_set(what 7)` registers the word *with a
  > target and a spare word*, and a decrement that takes the count below
  > the target does a `memory_wake(spare, 1)` (one parked spare is kept
  > for this). Second gap: parking on the park word is also "blocking in
  > a call", so idle and I/O-blocked workers look the same; `memory_wait`
  > needs a flag "does not count" for park words. Third: the kernel writes
  > the word at block and at switch-in, so the switch-in write must happen
  > after the space is active (`switchTo`), and a word in unmapped memory
  > must be dropped from the thread, not fault the kernel.
- **Interactive tasks** (Critic): tasks run to completion, so an
  interactive task pushed while every worker runs a long normal task
  waits for the shortest of them, and the workers' kernel class is
  normal, so another process's normal threads compete with it. Keep
  interactive headroom (one more worker may be started beyond the target
  for an interactive push) and raise a worker's kernel class while it runs
  an interactive task (one `thread_set` per class change, not per task).

### 2.5 Structured concurrency

- **Scope** (`@noncopy`, `deinit` joins; SMART-LANG 5.5): records its tasks, the first
  error (an atomic compare-exchange), a count of further errors, a cancel
  word, a deadline. Leaving it, by return or by `try`, cancels when an error
  is set, then joins every task. Tasks cannot outlive it: no thread leaks,
  no task leaks.
- **Join helps.** `join` on a worker runs other tasks (the joined task's
  own descendants first, from its own deque, then thefts) until the task is
  done; only when nothing is runnable does it park on the job's state word.
  A worker never sits idle while runnable work exists, which removes the
  classic pool deadlock (all workers blocked in joins).
- **Cancellation** is cooperative with controlled cleanup: the scope's
  cancel word is set; tasks not started are dropped (their captures'
  `__drop` runs); running ones see it at `task.checkpoint()`, at every
  join, channel operation, lock wait and sleep (`error.Cancelled`), and
  `parallel_for` stops handing out chunks. SMART-LANG 5.7 has std's
  blocking calls wait with the scope's cancel word beside what they wait
  for: on brixOS that is the alert (94 what 6: the canceller alerts the
  workers whose tasks block, their wait ends `interrupted` and the wrapper
  returns `error.Cancelled`), later the cancel word as a `memory_handle`
  (99) item of the one `wait`; on Linux a futex wait on the cancel word. Cleanup is the
  error's path through `defer`/`errdefer`/`deinit`.

  > **Critic: the alert races, and the Linux path cannot be built.**
  > (a) The canceller reads "worker W runs a task of scope S" and alerts
  > W; meanwhile W finishes that task and takes one of scope S2, which
  > blocks on a `Mutex`: its wait ends `interrupted` and an *uncancelled*
  > task gets `error.Cancelled`. "Current or next wait" (sticky) makes it
  > likelier: the stale alert waits for whatever W blocks on next.
  > (b) A futex waits on one word; "a futex wait on the cancel word"
  > cannot also wait on the lock or channel word (that needs
  > `futex_waitv`, Linux 5.16+). (c) SMART-LANG 5.7 assumes the native
  > `wait` already takes the cancel word as an item, which this document
  > defers to 99 "later". **Fix, the same on both systems:** every std
  > blocking operation (lock, condition, channel, join) queues the
  > waiting thread on the object's user-space wait list and waits on the
  > thread's **own park word** (one word per thread, in its block: a
  > parking lot); cancelling sets the scope's flag and unparks the
  > threads on the scope's waiting list. Only waits in the kernel (I/O)
  > need the alert, and it carries a generation: `thread_set(ALERT, g)`
  > interrupts only while the thread's current task generation is `g`,
  > and the wrapper, on `interrupted`, checks its own scope's flag and
  > makes the call again when it is not set.
- **`task.service` and arenas** (Critic): a service is a task, so it
  holds a pool worker for its whole life (eight services on four CPUs
  starve every `parallel_for`), and a task's arena is reset only at its
  end, so a service loop that makes values each turn exhausts its 1 GiB
  and traps; `turns.turn` cannot help, since it "traps with an open
  scope" and the root scope is always open (this also makes *every*
  turns-based program that uses a service trap in debug builds). Give a
  service its own Safe Thread (own arena, `turns` on it), and exempt the
  root scope from the trap. And bound the end: a service that never
  reaches a check point keeps `main`'s root-scope join waiting forever;
  after a deadline std ends the process with a report.
- **Errors** go to the parent: a task's `E!T` comes back at `join`; an
  unjoined task's error is the scope's. The scope returns the first; the
  others are counted and reported (6.).
- **Traps are not errors.** A checked arithmetic or bounds trap ends the
  process (panics do not unwind, `ownership.xml`); std.crash's report says
  the task, its spawn site and its parents' spawn sites (the job chain),
  beside the thread's stack.
- **Background work** that must outlive a function goes to
  `task.service(...)`, attached to the program's root scope, which std's
  start cancels and joins after `main` returns (before `process_exit`).
  There is no detached task in the safe API.

### 2.6 `parallel_for`, `parallel_reduce`, channels

- **Lazy binary splitting**: a range job keeps the lower half and pushes the
  upper half while its range is above the grain *and* a thief is looking
  (its deque is empty or a searching worker exists). Little overhead when
  the machine is busy, full spread when it is not.
- **The grain** is adaptive: the first chunk of a call is timed; chunks aim
  at about 50 µs; `.grain = n` overrides. Under the grain the body runs
  inline on the caller (a 10-item loop costs no thread).
- **Items are disjoint**: a mutable slice's elements are handed out as
  unique `*T` by construction (SMART-LANG: the parent is blocked, the iterations get distinct elements); a `parallel_for` over a container
  takes the container's split view (Nebula's tiles, 7.4).
- `parallel_reduce(T, items, identity, map, combine)`: per-chunk partial
  results combined in the tree order of the splits; deterministic for
  associative operations, order-sensitive float sums documented.
- **Channel(T)**: bounded MPMC ring, items `@move`d in and out (R3);
  `send` on a full channel and `receive` on an empty one help like a join
  does before parking on the channel's word; `close`; `deinit` drops what
  is left (their `deinit` runs; the count is reported as lost ownership).

  > **Critic: helping on channel operations deadlocks.** Counter-program
  > (how the two channels are shared between the tasks does not matter):
  >
  > ```mlx
  > // -smp 1: workers = allowed - 1 = 0, so the scope's caller runs
  > // everything by helping. Any machine: whenever helping nests a
  > // consumer above its producer on one stack.
  > task.scope(error{Closed, Cancelled}!void, |s| {
  >     // a, b: Channel(u32) with capacity 1
  >     _ = s.spawn { while true { const x = try a.receive(); try b.send(x) } }   // stage
  >     _ = s.spawn { try a.send(0); try a.send(1); a.close() }                   // source
  >     _ = try b.receive()
  > })
  > ```
  >
  > The caller's `b.receive()` finds `b` empty and helps: it pops the
  > newest job, `source`, which sends 0 and finds `a` full on 1, so it
  > helps: it pops `stage`, which takes 0, sends it into `b`, then finds
  > `a` empty and helps: nothing is runnable, it parks. `source` could now
  > send and the caller could now receive, but both are *below* `stage`
  > on the one stack. A spare worker started for the parked thread finds
  > no job: all three are on that stack. Permanent hang; at `-smp 1`,
  > every time. Even without helping, tasks that block on siblings need a
  > thread each, and the pool is capped at 2 x processors, so a pipeline
  > with more blocked stages than threads hangs the same way.
  > **Fix:** channel operations never help; a `send`/`receive` that would
  > block is allowed in task code only as `trySend`/`tryReceive`
  > (non-blocking), and the blocking forms belong to Safe Threads and
  > services, which have a thread each. Channels as pipelines between
  > tasks need stackful tasks, which this design rejects; say so to the
  > owner rather than ship a hang. (Also: `workers = allowed - 1` is 0 on
  > one processor; the pool needs at least one worker for anything that
  > is not a pure join tree.)

### 2.7 Deadlocks: prevented by the API, diagnosed at run time

Prevented:

1. Joins form a tree (R4: a `Handle` cannot leave its scope, SMART-LANG
   5.5) and help (2.5): no join cycle, no pool starvation.
2. A `Mutex` is used through a `Guard` (3.2) with a **level**:
   `std.sync.Mutex(T, level)` may only be taken while every held lock has a
   lower level. Debug builds check it per thread (the thread block keeps
   the held levels, a few words); a wrong order is a report with both
   sites, not a hang months later.
3. No lock is held across a join, a channel operation or a sleep: the
   thread block's held count is checked there (debug builds trap with both
   sites).

Diagnosed (`MLX_TASK_CHECK=1`, off by default): every blocking operation
records waiter, object and owner in a wait-for graph; an idle worker looks
for cycles every 100 ms and writes a report (the tasks, their sites, the
objects, the owners) to stderr and to Observatory's file (6.2); also lock
holds over 10 ms, tasks queued over 100 ms, a kernel-blocking call inside an
interactive task, a scope still running at its deadline.

> **Critic:** "prevented" holds for joins only. Lock levels and the
> "no lock across a join" rule are checked in debug builds, so a release
> build is not prevented, only undiagnosed; channel waits were the hole
> above. The vision's "deadlocks prevented by API design" needs the level
> check where it can be static (a `Guard` of level n held while a
> `Mutex(_, m <= n).lock()` is called in the same function is a compile
> error, a few lines in sema once levels are comptime parameters), and
> the run-time check on in release builds for the rest (a few words per
> thread, one compare per lock).

---

## 3. Safe Threads in std

### 3.1 A thread with its own arena: `std.thread`

For long-lived isolated loops (an audio thread, a network reader, a
compositor's helper), and the base the task workers are made of.

```mlx
var reader = try std.thread.spawn(Stats, .{ .name = "reader", .priority = .interactive }, {
    var count: u64 = 0
    var line: Line = undefined
    while try input.receiveInto(&line) {      // input: a Channel(Line) moved in; false once closed
        count += handle(&line)
    }
    return Stats.{ .lines = count }
})
const stats = try reader.join()   // Thread(Stats) is @noncopy; deinit joins
```

- **One mapping per worker** where std makes the thread: `[guard
  page][stack][arena: a 4 KiB header holding the thread block, then the
  values]` (one region, 1.6); THREADS.md's two mappings for a plain
  `std.thread` are fine too. The thread block (below) is std's TLS.
- **Its own arena**: `@arenaSwitch` first thing in the thread's entry
  (THREADS.md 2.4), on brixOS and Linux alike; on brixOS the longer start
  record can set r14/r15 before the first instruction instead. The thread
  runs `std.turns` on its own arena if it has a loop (`turns.locate` asks
  the hook, THREADS.md 2.7).
- **Guaranteed cleanup**: `Thread(T)` is `@noncopy`; its `deinit` joins
  (the exit word: `memory_wait` on brixOS, futex on Linux) and unmaps the
  mapping. Detaching exists only in the raw layer.
- **Controlled exchange**: captures by move or copy only (no borrows: a
  thread is not a scope), results by value, channels for the rest.

> **Critic:** this only holds if `std.thread.spawn` takes a `@Task`
> block that sema checks (R2 on captures and result, R5 on globals).
> SMART-LANG 6.1 declares THREADS.md's `std.thread.spawn(entry, context)`
> an `unsafe fn`; here the same name is the *safe* typed spawn and the raw
> one is `spawnRaw`. Settle the names: the typed `spawn` takes
> `@Task(fn() -> T)`; `spawnRaw(Start)` (4.1) is `unsafe fn`, and so is
> any `Start.arena` other than `.own`.

**The thread block** (std's TLS until `threadlocal` lowers,
`ir/lower.mlx:579`) sits in the thread arena's header at base + 8 and is
found through r14 (`@arena()`), never through FS, so it works in linked
programs where the C library owns FS and beside mlxlibc's TCB
(`compiler/libc/tcb.mlx`); the main thread's is a static (THREADS.md 2.2,
6.2). Its fields are THREADS.md's (id, exit word, entry, context, result,
name, locals, heap cache, crash context, turns reserve) plus what the
runtime needs: the worker and current job, the innermost open scope, the
scope's cancel word, the held lock levels (debug).

> **Critic:** "found through r14" means r14 *is* the thread's identity,
> so (i) the main thread needs a compare (`r14 == main arena ?` static
> block : base + 8), and the closure modules (heap, turns, crash) can do
> it only through a hook, an indirect call on the heap's fast path that
> single-threaded programs then pay too (THREADS.md 2.8 keeps the
> thread cache behind such a hook); (ii) two threads on one r14 share one
> block: `Start.arena = .shared` (4.1) must go, aarch64 (risk 12) must not
> use the per-thread heap cache at all, and a C callback into an export of
> a linked program on a worker runs on the *main* thread's block (risk 13)
> and corrupts its heap cache. Risk 6 below says the block is reached by
> `@threadPointer`, which contradicts this paragraph and would fault: a
> static std program on brixOS never sets FS, so `fs:[0]` reads address
> 0. The hook is the only way in the closure.

### 3.2 The four memory classes in std

| Class | std types | Enforced by |
|---|---|---|
| LOCAL | the thread's / task's arena values and frame; per-thread heap cache | default (an unqualified address); R2 keeps it home |
| SHARED | `std.sync.Mutex(T, level)`, `RwLock(T)`, `Atomic(T)`; `Shared(T)` (heap, atomic refcount) | `@shared(P)`; R7: atomics or the lock's borrow (`Guard`) |
| TRANSFERABLE | `@noncopy` owned values on std.heap: `Box(T)`, `std.ArrayList(T)`, `Buffer`, channel items | `@owned(P)` inside a non-copyable type; R3: `@move` |
| IMMUTABLE | `Frozen(T)` (heap, atomic refcount, `get()` gives `@immutable(*const T)`); never-written `var`s and `const` data; frozen views | `@immutable(P)`; R6: transitive |

```mlx
var totals = std.sync.Mutex(Totals, 2).init(Totals.{})
{
    var guard = totals.lock()          // Guard(Totals): @noncopy, deinit unlocks
    guard.get().*.pixels += counted    // *Totals only through the guard
}                                      // unlocked here; the pointer cannot leave

const palette = std.sync.freeze(Palette, @move(loaded))   // Frozen(Palette)
try task.parallel_for(rows, |row| { mapRow(row, palette.get()) })
```

`Frozen(T)` accepts what R6 accepts: plain data, and addresses that are
themselves `@immutable`.

> **Critic:** the classes are all on std.heap here (Box, Shared,
> Frozen); none is "built on the arena architecture" as the vision asks.
> Everything that crosses a thread is copied out of an arena into the
> global heap (one lock, or the per-thread cache) and, for SHARED and
> IMMUTABLE, reference-counted with atomics. An arena-native form is
> missing: a TRANSFERABLE *arena chunk* (a task builds a large result in
> a chunk of its own, and the chunk's ownership moves to the joiner with
> no copy), and a *frozen arena* (built by one thread, then read by all,
> freed as a whole when the last reader's scope ends: no per-object
> refcounts). Both fit the classes (an `@owned` chunk; `@immutable` after
> the move), and both are what makes Nebula's results cheap. `std.sync.Lazy(T)` (SMART-LANG L7) is the
once-built table: built by the first reader under a `Once`, immutable
after.

`std.sync` itself (`std/src/sync.mlx`):

- `Word.wait(word, expected, deadline)`, `Word.wake(word, count)`,
  `Word.move(...)`: `memory_wait`/`memory_wake`/`memory_move` on brixOS, futex
  (`std/bootstrap/os/linux/thread.mlx:47-60`) on Linux, through THREADS.md
  6.5's seam (`std/bootstrap/os/wait.mlx`, `wait.brixos.mlx`).
- THREADS.md 6.4's untyped `Mutex`, `Condition`, `Once`, `WaitGroup`,
  `Semaphore` are the words underneath; the typed layer below is what safe
  code uses.
- `Lock` (0 free, 1 held, 2 held with waiters; spins up to ~100 relaxes, then
  waits), `Once`, `Mutex(T, level)`/`Guard`, `RwLock(T)`, `Condition`
  (a sequence word; broadcast moves the waiters onto the mutex word with
  `memory_move`), `Latch`, `Event`, `Atomic(T)`.
- `PiMutex` (owner tid in the word, `owner` flag, `hand over` on wake) for
  real-time and audio threads.

The low-level part (`Word`, `Lock`) sits in the seam, and
`pages.sleepOn`/`wakeOne`/`yield` (`pages.brixos.mlx:36-41`, empty today)
delegate to it, because the heap, the tiny pages, the debug allocator and
diagnostics' log use them and the compiler's own closure includes them
(`codemap.layers`: no new builtin there until a canonical compiler has
it).

---

## 4. Raw Threads, the Linux paths, mlxlibc

### 4.1 `std.thread.Raw` (brixOS native first)

```mlx
pub const Start = struct {
    entry: fn(argument: usize) -> void
    argument: usize
    stack: usize = 0               // 0: std maps the thread's mapping (3.1)
    stackSize: usize = 1048576
    tls: usize = 0                 // 0: std's thread block
    arena: Arena = .own            // .own, .shared (the caller's: only for code that never resets), .at(base, end)
    priority: ?Priority = null
    affinity: ?*const CpuSet = null
    domain: ?u16 = null
    name: []const u8 = ""
}
pub fn spawnRaw(start: Start) -> SpawnError!RawThread     // tid, exit word, mapping
pub fn RawThread.join(self: *Self) -> void
pub fn RawThread.joinUntil(self: *Self, deadline: u64) -> bool
pub fn RawThread.detach(self: *Self) -> void               // the thread frees its mapping itself at its end
pub fn current() -> Info                                   // thread_self
pub fn setAffinity(set: *const CpuSet) / setPriority / setName / setEnergy / yieldTo(tid)
pub fn topology() -> *const Topology                       // system_topology, read once
pub fn times(into: []CpuTime) -> usize                     // thread_get(TIMES)
```

This is THREADS.md 6.2's `std.thread` (`spawn(entry, context)`,
`spawnWith(options, ...)`, `join`, `done`, `detach`, `currentId`,
`processorCount`, `Local`) with the options extended by priority,
affinity, domain and energy, which go to `thread_set` (94) on brixOS and to
`sched_setaffinity`/`setpriority`/`prctl(PR_SET_NAME)` on Linux. brixOS:
`thread_create` (86); join = `memory_wait` on the exit word (or the one
`wait` on the handle). Linux: `@spawnThread` with
`CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID`, futex on the exit word. Both
sit behind `std/src/thread_os.brixos.mlx` / `thread_os.mlx`, the split std
uses elsewhere (`crash_os.brixos.mlx`, `sql/os.brixos.mlx`).

Shared memory between raw threads is plain pointers inside `unsafe`: outside
the guarantees, as the vision says.

### 4.2 std's Linux path (Mlx programs on a Linux host)

The same std API (`std.sync`, `std.thread`, `std.task`) on clone and futex,
which already have their raw ABI (`std/bootstrap/os/linux/thread.mlx`).
Topology from `sched_getaffinity` (`std/bootstrap/os/linux/system.mlx:20`)
and `/sys/devices/system/cpu/cpu*/topology/{physical_package_id,core_id}`,
`/sys/devices/system/node/node*/cpulist` (the fallback: one package, one
domain). Per-thread times from `/proc/self/task/TID/stat`. This path is the
*fallback* for Mlx programs; nothing in it is brixOS's base.

### 4.3 The Linux layer of brixOS as an adapter (`kernel/linux/`)

Every Linux thread call maps onto the native objects; nothing native learns
a Linux shape:

| Linux | Native mapping | Today |
|---|---|---|
| `clone(CLONE_VM\|CLONE_THREAD\|CLONE_SIGHAND, stack, ptid, ctid, tls)` | a thread of the process: the caller's frame copied, rax 0, rsp = stack; `CLONE_SETTLS` -> fsBase; `CLONE_PARENT_SETTID` -> tid at ptid; `CLONE_CHILD_SETTID` -> tid at ctid (in the child's memory = the same space); `CLONE_CHILD_CLEARTID` -> exit word = ctid (flag rules: THREADS.md 5.1) | ENOSYS (`system.mlx:321-326`) |
| `clone3(struct clone_args)` | the same after reading the struct | ENOSYS (`:328`) |
| `futex` WAIT / WAIT_BITSET | `memory_wait` (relative -> absolute deadline; `FUTEX_CLOCK_REALTIME` converted; bitset kept in the waiter, matched on wake) | not served (ENOSYS by `sayUnknown`) |
| `futex` WAKE / WAKE_BITSET | `memory_wake` | same |
| `futex` REQUEUE / CMP_REQUEUE | `memory_move` (with check) | same |
| `futex` WAKE_OP | the op on the second word under the kernel lock, then two wakes | same |
| `futex` LOCK_PI / UNLOCK_PI / TRYLOCK_PI | `memory_wait(owner)` / `memory_wake(hand over)` | same |
| `FUTEX_PRIVATE_FLAG` | private key (default); without it: `shared` | same |
| `gettid` | the thread's tid | = pid (`:291`) |
| `set_tid_address(addr)` | exit word = addr; returns tid | returns 1 (`:356`) |
| `exit` | `thread_exit` | ends the process (`:203`) |
| `exit_group` | `process_exit` | same |
| `tkill`, `tgkill` | the signal to that thread (per-thread pending) | to the process |
| `rt_sigprocmask`, `sigaltstack` | per thread (`LinuxThread`) | per process |
| `sched_getaffinity` / `sched_setaffinity` | the thread's affinity mask (Linux's cpusetsize honoured) | CPU 0 only (`:378-383`) |
| `sched_yield` | `thread_yield(0)` | yields the process |
| `sched_setscheduler`, `setpriority`, nice | priority: SCHED_OTHER normal, FIFO/RR the real-time class (user 0 only), nice -> level | absent |
| `set_robust_list` | stored per thread; at its end the list is walked (bounded, 2048), each held word gets FUTEX_OWNER_DIED and a wake | returns 0 and does nothing (`:355`) |
| `rseq` | ENOSYS (the kernel does not update an rseq area; returning 0 lets glibc believe a cpu_id that never changes) | returns 0 (`:355`) |
| `arch_prctl(ARCH_SET_FS)` | the thread's fsBase | the process's (`:357`) |

Signal delivery in the Linux view picks a thread that does not block the
signal (process-directed) or the named one (`tgkill`), as Linux does;
`deliverLinux` (`trap.mlx:400`) runs for the thread returning to user mode.
The details (clone's flag rules, clone3, WAKE_OP's decoding, the signal
split) are THREADS.md 5.1-5.6. Three rows go further than THREADS.md, which
leaves them open (its section 10): `sched_setaffinity` is honoured (not
ignored), the PI futex operations map onto the `owner` flag and hand-over
(not ENOSYS), and the robust list is walked at a thread's end; all three
come with K4, when the kernel has priorities and affinity.

### 4.4 mlxlibc's pthreads on the raw native calls

mlxlibc on brixOS answers Linux numbers in-process with native calls
(`sys_calls.brixos.mlx:1-13`); its threads must too, not go through the
kernel's Linux layer:

- `pthread_create` (`pthread.mlx:256`): on brixOS, `brix.threadCreate`
  (86) with entry `__mlxlibc_thread_start`, argument and thread pointer =
  the TCB, stack top = `block - tls_size` (as now, `:294`), id word and exit
  word = `block + tcb.TID` (= `CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID`),
  detached, and an **arena of its own** given in the longer record (mlxlibc
  is Mlx: its functions bump r14 from the first instruction): a reserve in
  the thread's mapping. On Linux `@spawnThread` stays. (THREADS.md's step 9
  does the same with the shared arena; the own arena is what keeps a C
  thread's values from being reset by the main thread's turns.)

  > **Critic: the premise does not hold.** `libmlxc.so.1` is a
  > `--plugin --stack-arena` shared object (`compiler/libc/README.md:23-25`):
  > every export, `pthread_create`, `malloc` and the thread's own entry
  > `__mlxlibc_thread_start` (`pthread.mlx:193`, an `export fn`) among
  > them, makes its arena from its own stack on entry and drops it on
  > return (`codegen/labels.mlx:109-125`). Nothing in mlxlibc runs on the
  > thread's r14, and a C program has no `turns`. The arena in the start
  > record protects nothing and costs a 1 GiB reservation and a region
  > per C thread: leave r14/r15 alone (the short record) for mlxlibc.
- `sys_calls.brixos.mlx`: futex (`:805`, a no-op today) -> `memory_wait` /
  `memory_wake` / `memory_move`; gettid (`:742`) -> `thread_self`; exit vs
  exit_group (`:755`) split; clone with CLONE_THREAD (`:769`) -> the native
  spawn for foreign code that calls it directly; `sched_getaffinity`
  (`:797`) -> `thread_get(AFFINITY)`; sched_yield -> `thread_yield`;
  `set_tid_address` -> the exit word.
- `waitForExit`/`pthread_join` (`:304`) keep their shape: they futex-wait on
  the TID word, which is now a native word wait.
- `pthread_setname_np` -> `thread_set(NAME)` (not `/proc/self/task`);
  `PTHREAD_PRIO_INHERIT` mutexes -> the `owner` flag;
  `CLOCK_THREAD_CPUTIME_ID` -> `thread_get(TIMES)`.

This makes WebKit's threads (the collector's markers, the compositor
thread: `projects/webkit/PORT.md`) possible on brixOS without the Linux
layer.

---

## 5. std's process-global state under threads

std has **657 module-level `var`s in 107 files** (`grep -rn "^var \|^pub
var " std/src std/bootstrap`). Classified mechanically (the rules are in
Appendix A, with the full list) and the core of it by hand:

| Class | Count | What it needs |
|---|---|---|
| C: never written after its initializer (tables) | 332 | `const`: IMMUTABLE, in the image; the static-tables work made this possible. A `codemap rewrite` kind can do it mechanically |
| O: a once flag and what it guards (`*Ready`, `loaded`, `installed`, ...) | 40 flags (+ their tables in M) | `std.sync.Once`; a plain bool race builds a table twice and publishes it before it is complete |
| L: a lock word of its own (spinning forever) | 15 | `std.sync.Lock` (parks instead of spinning) |
| A: published or counted with atomics already (CAS singletons, counters) | 25 | nothing, or a check |
| K: caches filled while running | 47 | under their lock (most have one) or per thread |
| M: other mutable state | 198 | by hand, below |

> **Critic: the classifier errs in the dangerous direction too, so "C"
> must not be rewritten to `const` mechanically.** It does not see writes
> through a module prefix or from outside std: `fontconfig/config.mlx`
> `currentLock` is "C" but is a `Mutex` entered by address
> (`&config.currentLock`, `fontconfig/load.mlx:118`); `truetype/render.mlx`
> `colorRender` is "C" but assigned in `truetype/face.mlx:264`;
> `crash.mlx` `faultWhileNaming` is "C" but written by
> `tests/support/crash_report.mlx:35`. It also mislabels the other way:
> `unicode/tables.mlx` `SCX_OFFSET` ("O") is a 204-entry table, and
> `shaping/set.mlx` `emptySet` ("O") is a CAS singleton (A). Classify each
> global with `mlx-codemap refs`, which resolves qualified names across
> the repository. (SMART-LANG 8.1 counts 837 vars and 335 "C"; the two
> documents should use one count.)
>
> **R5 and std's own globals.** Once R5 is strict (E7008), every task
> that allocates reaches `heap.heap`/`heap.words` (written in `make`),
> the tiny pages' `region`, turns and crash state. The closure modules may
> not use `@shared` until two compiler generations have it (SMART-LANG
> 8.1 risk 5), so their accesses must sit in `unsafe { }` (R5's sanctioned
> exception) as part of S0/S1; otherwise R5 refuses every task body that
> allocates.

**By hand, the runtime's own state** (these decide whether threads work at
all):

| Global | Where | Under threads |
|---|---|---|
| the heap: `own`, `ownState`, `ownWatch`, `heap`, `words` | `std/src/heap.mlx:51-57` | made by CAS already (`make`, `:69-85`); the loser's `linux.thread.yield()` becomes `pages.yield()` (native `thread_yield`) |
| the heap's lock | `general_purpose_allocator.mlx:100-115` | `linux.thread.futexWait/Wake` -> `pages.sleepOn/wakeOne` (native words on brixOS: today a busy spin there, since the kernel's Linux layer has no futex). Then a per-thread cache in the thread block: up to 32 blocks per class for classes up to 1 KiB, refilled and flushed in batches under the lock; a block freed by another thread goes into that thread's cache (the classes are global, so this is safe) |
| tiny pages: `region` and the header's lock word | `tinypage_allocator.mlx:75,137-160` | already through `pages.sleepOn` (empty on brixOS today: must become the native word) |
| `page_allocator.watchAlloc/watchFree` | `page_allocator.mlx:22-23` | set by std.leaks before any thread: documented; an atomic store/load |
| `debug_allocator.pageWatcher`, std.leaks' state | `debug_allocator.mlx:444`; `leaks.mlx:81-91,264-272` | the debug allocator's lock sleeps through `pages.sleepOn` (`debug_allocator.mlx:150,157`): right once the seam is native; the snapshot writer runs on one thread at a time (a `Once`-like word) |
| turns: `located`, `releasedTurns`, `residency` | `turns.mlx:36,96,198` | `locate` asks a hook first (the calling thread's arena, `@arena()`), `located` stays the main arena's; `releasedTurns` atomic; `residency` only on the main thread's walk (THREADS.md 2.7) |
| crash: `arenaBase`, `arenaRoom`, `contextPointer/Length/Buffer` | `crash.mlx:114-129` | per thread (the crashing thread's arena and context, from its thread block) |
| crash: the report's state (`entered`, `report*`, `withoutNames`) | `crash.mlx:154-178` | already gated by `reportingThread` (`:159`, CAS at `:285`); its id comes from `threadId()`, which is `1` on brixOS (`crash_os.brixos.mlx:38`) -> `thread_self` |
| crash: symbol caches `cached*`, `demangleTables` | `crash.mlx:960-977` | used under the report gate only: fine |
| crash_os.brixos: `handler`, `faultInfo`, `faultRegisters` | `crash_os.brixos.mlx:30-34` | used inside the serialized upcall only: fine |
| upcalls: `handlers`, `handled`, `queued`, `installed`, `takenRecords` | `std/bootstrap/os/upcalls.mlx:43-53` | handlers set before threads (documented); `takenRecords` is used inside the upcall only, which the kernel runs one at a time per process |
| host.brixos: `remembered`, `cookies`, `primed`, `infoScratch`, `selfUser` | `std/bootstrap/os/host.brixos.mlx:15-19,245,468` | the descriptor tables under a `Lock`; `primed` a `Once`; `infoScratch` a local |
| events: `handlers` | `std/src/events.mlx:25` | set before threads |
| diagnostics: `frames*`, `gpuMemory` | `std/src/diagnostics.mlx:438,624-632` | the frame timeline belongs to the thread that draws (documented); `framesState` a `Once` |
| sql: `lock`/`threadId` that spin and say 1 | `std/src/sql/os.brixos.mlx:27-38` | `std.sync.Lock` and `thread_self` |
| fontconfig, shaping, unicode, sql: private heaps and locks (`fontconfig/mem.mlx:529` futex direct; `shaping/memory.mlx:253` spin; `unicode/collation.mlx:516-523` spin) | | one `std.sync.Lock` for all of them |
| per-call scratch kept in globals (`unicode/breaks.mlx:86-87,471-473`, `unicode/format/number/skeleton.mlx:811`, `sql/query.mlx:460,733`) | | locals, or the thread block's slots: a data race today the moment two threads format numbers |
| ui: `scheme.current/known/fallback` | `std/src/ui/scheme.mlx:34-55` | the UI thread's (documented); read-only snapshot for tasks (`Frozen`) |
| wayland backend: `loops`, `knownEnvironment` | `std/src/wayland/transport/backend.brixos.mlx:557-558,727-728` | the event loop's thread |
| build: `arenaBase`, `arenaPoison`, `cache.memo` | `std/src/build.mlx:429-430`, `build/cache.mlx:332` | the runner's main thread |

Lock code exists in eight places today (the GPA, the tiny pages, the
debug allocator, diagnostics' log, fontconfig, sql, shaping, unicode), and
on brixOS every one of them spins: the kernel's Linux layer has no futex
and `pages.brixos.mlx`'s `sleepOn` returns at once. All go onto the seam
and `std.sync.Lock` in one step (S1 in 7.1).

The compiler's R5 (2.1) turns this table into errors at the spawn sites
that reach a global; until it exists, `mlx-codemap reach` from each spawn
site lists the globals a task can reach (a check in `tools/`).

---

## 6. Observatory: what is reported, how it gets there, the Thread Inspector

### 6.1 What the runtime and the kernel report

| Item | Source |
|---|---|
| tasks (tree by scope), state, priority, spawn site, worker, times queued/ran, arena bytes, error, cancel | std.task job records (2.3) |
| workers: thread, CPU now/last, domain, state (running a task, searching, parked, blocked in the kernel: which call), tasks run, steals by distance, time running/parked, arena fill/peak/resets | std.task pool + `thread_list` (97) |
| per-CPU load: busy/idle time, which thread ran when, kernel lock waits | `system_topology` busy ns (96), `thread_trace` (98), `smp.Waits` (`system_info` n=52) |
| lock waits and conflicts: per lock site, acquisitions, contended ones, wait time, longest, the holder's site | std.sync (debug and `MLX_TASKS=1`: counters per lock site) + kernel word wait accounting (`acct.waits[memory]`) |
| waiting tasks: what each waits for (join, channel, lock, word, kernel call) and on whom | the wait-for graph (2.7) |
| lost ownership: channel items dropped at `deinit`, values dropped by a cancel, a lock held by a task that failed, `Shared`/`Frozen` still referenced at exit | std.task/std.sync counters; std.leaks for the refcounts |
| which arena a task owns | job's worker + its arena bytes (mark to fill) |
| why a task ran on a CPU | the job's `why` (LOCAL, STOLEN_SIBLING ... REMOTE, INJECTED, HELPED) + the worker thread's kernel placement reason (`thread_get(WHY)`, trace `why`: LAST_CPU, SIBLING, PACKAGE, DOMAIN, AFFINITY, PREEMPT, QUEUED) |

### 6.2 How it gets there

Two paths, the first one the shape every Observatory view has today:

1. **Snapshots** (`MLX_TASKS=1`): std.task writes
   `$XDG_RUNTIME_DIR/mlx/tasks/PROGRAM-PID.tasks` about once a second,
   written beside its place and renamed into it, as std.leaks' memory
   snapshots and std.diagnostics' frames are (`tools/codemap/memory.mlx:262-281`
   finds the directory). Tab-separated lines, as `crashes.log` and the
   profiles are (`std/src/diagnostics.mlx:1-40`):

   ```
   threads  program  pid  seconds  cpus  workers  domains  energy
   cpu      index  domain  package  core  smt  busy-ns  idle-ns  kernel-wait-ns  tid-now
   worker   index  tid  cpu  domain  state  tasks  steals-sibling  steals-package  steals-domain  steals-remote  running-ns  parked-ns  arena-used  arena-peak  why-cpu
   task     id  parent  scope  state  priority  worker  path:line:function  queued-ns  ran-ns  arena-bytes  why  error
   wait     task  kind  object  path:line:function  since-ns  owner-task
   lock     object  path:line:function  acquired  contended  wait-ns  longest-ns  holder-site
   cycle    task  task  ...
   lost     kind  path:line:function  count
   ```

   On brixOS the writer adds the kernel's view of its own threads
   (`thread_list`, `thread_get(TIMES)`); on Linux `/proc/self/task/*/stat`.
2. **A live ring** for the timeline (later, O2): a memory object (at most
   1 MiB, `brix.xml` n=18) holding a ring per worker of 32-byte events
   (spawn, start, steal, park, unpark, block, join-help, cancel, error, end).
   Native first: the program makes a listener node
   `/temporary/mlx/tasks/PROGRAM-PID` (`listener_make`, n=51); Observatory
   opens it and receives the memory object's handle with the read right
   only over the channel (`channel_send` moves handles with their rights,
   `brix.xml` Handles); it maps it read-only (`memory_map_object` without
   the write flag) and reads behind the writers. The kernel's own ring
   (`thread_trace`, 98) comes the same way. On Linux the ring is a file in
   the same directory, mapped shared.

> **Critic:** on brixOS this puts the freedesktop convention
> (`$XDG_RUNTIME_DIR` files) first and the native path (listener node,
> channel, read-only memory object) "later" (O2), the reverse of native
> first. The existing memory and frames views do the same, but a new view
> need not: make the listener node and the memory object the brixOS form
> of O1, the files the Linux form. Two gaps either way: (a) the writer
> walks job records that workers change at that moment, so it needs a
> protocol (per-worker counters and a sequence word per record, read
> until stable), and it must not take the injection lock; (b) O1's
> "per-CPU lanes" and kernel-lock marks need `thread_trace` (98), which
> lands in O2, so O1 shows workers and tasks only.

### 6.3 The Thread Inspector

Built exactly as the existing views are:

- `tools/codemap/tasks.mlx`: reads the snapshots (as `frames.mlx`,
  `memory.mlx` do); `tools/codemap/cmd_tasks.mlx`: `mlx-codemap tasks
  [PROGRAM]` prints them (as `cmd_frames.mlx` does). The static side is
  SMART-LANG's `mlx-codemap threads` (classes, task-reachable globals); task
  names come from the `path:line:task` symbols and the `.mlx_tasks` section
  it defines.
- `projects/observatory/threads_view.mlx`: a new finding `FINDING_THREADS`
  (13) beside `FINDING_MEMORY` (`main.mlx:173-187`); `detail.mlx` draws its
  block where it draws memory's and frames' (`:421-452`); `input.mlx`
  refreshes it when the directory changes (`:785-807`); `filters.mlx`
  weighs functions by it (`:517`: *Color by -> task time*, *lock waits*);
  `actions.mlx` gets *Watch threads* (runs the chosen program with
  `MLX_TASKS=1`, `:529-542`).
- The panel joins the run-time and the static side: next to a spawn site
  it shows what the task may reach (SMART-LANG's `threads`) and what it did
  (`tasks`). Top to bottom: the program line (workers of CPUs, domains,
  energy, utilisation, tasks/s, steals/s); **per-CPU lanes** for the last
  seconds (which thread ran, idle gaps, kernel-lock waits marked); the
  **workers** table; the **task tree** by scope (spawn site, state, worker,
  times, arena bytes, why); **waits and locks** (hottest lock sites, longest
  waits, holders; a cycle in red with its tasks); **lost ownership**.
- On the map: spawn sites and lock sites are functions in the code graph
  (the job's `site` is a return address, resolved as the profiles' and
  crashes' are); selecting one draws lines to the task bodies it spawned
  and colours them by the time they took; contended locks glow.

---

## 7. Plan, tests, risks, Nebula

### 7.1 The steps (each builds and passes alone)

| Step | What | Passes when |
|---|---|---|
| K0 | (the SMP work in progress) every processor up to 256 runs the kernel | `tools/check_brix_smp.sh` |
| **C0** | compiler: `@arena`, `@arenaSwitch` (THREADS.md step 1) | its probe and runtime test |
| **K1** | kernel: the thread record, one thread per process, a pure refactor (THREADS.md step 2) | every `tools/check_brix*.sh` unchanged |
| **K2** | kernel: shared spaces made safe (shootdown, the `fault` fix) and calls 86-92 (THREADS.md steps 3-4); `@spawnThread` native on brixOS (step 5) | `tools/check_brix_threads.sh` part 1 (THREADS.md 7.3) |
| **S0** | std: the seam `wait*.mlx` with `pages.*`, the GPA and the heap on it (THREADS.md step 6) | host + brixOS; `tests/315_tinypage_runtime.mlx` |
| **S1** | `std.thread` and untyped `std.sync` (THREADS.md step 7), then the typed layer: `Mutex(T, level)`/`Guard`, `RwLock(T)`, `Frozen`, `Shared`, `Channel(T)`; the eight private lock copies onto it; the O-class globals onto `Once` | host + brixOS tests |
| **K3** | kernel: `memory_move` (93), priority order and hand-over on `memory_wake`, the `owner` flag (PI), alert (94 what 6), the running word | part 2: ping-pong across CPUs, PI bound, alert |
| **K4** | kernel: priority classes and ready queues, affinity honoured (`processors.Mask`), placement by topology, `Vector.RESCHEDULE`, `thread_set/get` (94-95), `system_topology` (96: per-AP CPUID, SRAT, SLIT), accounting and `thread_list` (97), the deadline heap | part 3 (`-numa`) |
| **C1** | compiler (SMART-LANG.md's plan): classes, task blocks, R1-R8, `ir/thread_check.mlx` | its tests; programs without threads byte-identical |
| **S3** | `std.task` core: pool sized by topology, on-demand workers, Chase-Lev deques, injection, park/unpark, scope/spawn/join-helps/cancel/errors, `parallel_for`/`Reduce`; the h264, h265 and splash crews ported (THREADS.md step 10's consumers, on std.task instead of `std.thread.Pool`) | host + brixOS; h264/h265 decode tests unchanged |
| **S4** | `std.task` smart: priorities and the background pool, energy modes, domain-aware stealing, count running (dynamic pool), load sensing, `MLX_TASK_CHECK` diagnostics | benchmarks and diagnostics tests |
| **O1** | snapshots (`MLX_TASKS=1`), `tools/codemap/tasks.mlx`, `mlx-codemap tasks`, `threads_view.mlx` (with SMART-LANG's static `mlx-codemap threads`) | codemap tests; Observatory screenshots script |
| **L1** | kernel Linux layer as adapter (THREADS.md step 8, with 4.3's three rows once K4 is in); mlxlibc pthreads native (THREADS.md step 9, own arenas: 4.4) | the existing thread tests (`tests/298`, `315`, `180`) built for x86_64-linux and run on brixOS; `tools/mlxlibc_check.c:284-345` on brixOS; the std.task host test built for Linux and run on brixOS |
| **N1** | Nebula: the prerequisites of 7.4, then `parallel_for` in filters, render, transform, large dabs | Nebula's tests unchanged + speedups |
| O2, K5 | the live ring and `thread_trace` (98); finer kernel locks (memory buckets and run queues off the big lock, THREADS.md step 11), per-CPU queues, NUMA frames, MWAIT idle | later, independently |

The owner's order is kept: C0-K2 and K3-K4 are the native kernel threads,
S0-S1 + C1 the safe arena and ownership semantics, S3 structured
concurrency, S4 the smart scheduler, L1 Linux compatibility. (std's own Linux path comes with
S1-S3 because the host tests run on Linux; it is not the brixOS Linux layer.)

> **Critic: S1 cannot build alone.** Its `Condition` broadcast uses
> `memory_move` (93) and `PiMutex` the `owner` flag and hand-over, which
> land in K3, after S1; its typed layer (`Guard` as a second-class
> borrow, `Frozen`'s `@immutable`, `Shared`'s `@shared`) needs C1, after
> S1; and `Channel(T)`'s helping needs std.task (S3). Risk 5 names an
> "S2" that the table does not have. The order that builds:
>
> | Step | What |
> |---|---|
> | K2b | a handle table above 64 (1.2) and a user-readable clock (a kernel-published TSC page, 8.3); `thread_exit` with an unmap range |
> | S1 | the seam, untyped `std.sync` (Lock, Once, Word, the parking lot of 2.5), `std.thread`'s raw layer; the eight lock copies and the O-class globals; `unsafe` around the closure modules' globals (5.) |
> | K3 | as above |
> | C1 | as above |
> | S2 | the typed layer (`Mutex(T, level)`/`Guard`, `RwLock`, `Frozen`, `Shared`), `PiMutex`, `Condition` on `memory_move`, the typed `std.thread.spawn` |
> | S3 | std.task, with `Channel(T)` (non-helping, 2.6) |
>
> K2b goes before S3: without it every task timestamp and every spin
> window is a kernel entry under the global lock.

### 7.2 Tests

The existing thread tests stay the base (`tests/298_spawn_thread_runtime.mlx`,
`315_tinypage_runtime.mlx`, `180_linux_futex_runtime.mlx`,
`295_atomics_runtime.mlx`, `tools/mlxlibc_check.c:284-345`, THREADS.md 7.x).
Host (Linux), new `tests/5xx_*` in the existing numbering (the SMP work and
THREADS.md already claim 528: take the next free numbers):
`sync_lock_stress` (N threads x M increments under `Mutex`, exact count),
`sync_once` (one build of a table under contention), `sync_condition`,
`thread_arena_isolation` (each thread fills and resets its own arena; the
others' values intact, poison on), `task_scope_join`, `task_parallel_for_sum`
(10^7 items, exact sum, 1..64 workers, oversubscribed),
`task_cancel_cleanup` (deinit counters balance after a cancel),
`task_error_first` (first error returned, rest counted),
`task_no_leak` (`MLX_LEAK_CHECK=1`, nothing held after scopes),
`task_deadlock_report` (a deliberate lock-order inversion with
`MLX_TASK_CHECK=1` -> a report, not a hang), `channel_mpmc`.

QEMU (`tools/check_brix_threads.sh`, built like `tools/check_brix_smp.sh`):

- `-smp 4`, `-smp 16`, `-smp 64` (TCG, `-accel tcg,thread=multi`, longer
  timeouts at 64) and a NUMA machine
  `-smp 16,sockets=2,cores=4,threads=2 -object memory-backend-ram,size=512M,id=m0 -object memory-backend-ram,size=512M,id=m1 -numa node,cpus=0-7,memdev=m0 -numa node,cpus=8-15,memdev=m1 -numa dist,src=0,dst=1,val=20`.
- a `threadtest` program: threads spawned until `too_many` (the table's
  bound and the per-process half both hold), then 1,000 spawn/join rounds;
  a word
  ping-pong between two threads pinned to different CPUs; priority order of
  wakes; priority inheritance (low holds, high waits, medium spins: high
  gets the lock within a bound); affinity respected (`thread_self` CPU);
  **TLB shootdown** (thread A unmaps a page that thread B on another CPU
  keeps reading: B must fault, never read the old page); a process killed
  while its threads run on other CPUs (nothing freed under them);
  `process_replace` from a threaded process.
- `sysinfo` extended: topology (packages, cores, domains, SLIT) must match
  the `-smp`/`-numa` options.
- the std host tests built for `x86_64-brixos`; Nebula's script benchmark
  (`nebula --script`, Gaussian blur of a 4096x4096 layer) at -smp 1/4/16
  reporting the speedup; the h264/h265 decode tests with workers.
- L1: the same task test built for x86_64-linux, run on brixOS through the
  Linux layer.

### 7.3 Risks

1. **The kernel lock under park/unpark storms.** Mitigated by spinning
   before parking, waking at most one searcher, measured per thread
   (`kernelWait`) and per holder (`smp.Waits`); the word calls are the first
   to leave the lock (K5).
2. **TLB shootdown errors corrupt memory silently.** It lands in K2 with
   the first second thread, with its own test.
3. **Values escaping a reset task arena** (use after reset) until the
   compiler's R2 check exists: S3 tasks return plain data only, poison in
   debug, results through slots.
4. **Address space and regions**: 1 GiB + 8 of arena and 1 MiB of stack
   reserved per thread (256 workers: 256 GiB of the 128 TiB user half);
   regions grow to 1,024 in K2; one mapping per worker.
5. **std's globals** (5.): a task that reaches an unconverted global races.
   `reach` check until the compiler's R5; the core list of 5. is done in
   S1/S2 before tasks exist.
6. **The bootstrap constraint**: std.heap, std.turns, std.crash and
   std/bootstrap are in the compiler's closure (`codemap.layers`): they may
   not use new builtins until a compiler with them is the canonical one.
   Hence the seam in `pages.*` and `@threadPointer` (old) for the thread
   block.
   **Critic:** not `@threadPointer`: 3.1 says the block is never found
   through FS, and in a static std program on brixOS FS is never set, so
   `fs:[0]` faults. The closure reaches the block through a hook
   (`pub var threadBlock: ?fn() -> usize`, THREADS.md 2.7-2.8).
7. **Two threads fault at once**: the kernel runs one upcall at a time
   per process (THREADS.md 3.7) and std.crash's `reportingThread` gate
   (`crash.mlx:159,285`) serialises the report; `threadId()` must stop
   answering 1 on brixOS (`crash_os.brixos.mlx:38`).
8. **Fork with threads**: `process_branch` copies the caller alone; locks
   other threads held stay held in the child. std's own programs start
   processes with `process_spawn`/`launch` (no fork): native first avoids
   the problem; documented for C code.
9. **Determinism**: per-pixel filters are identical in parallel; float
   reductions are not order-independent (documented, integer histograms
   are).
10. **Energy policy oscillation** (park/unpark thrash): hysteresis in the
    wake rule and the spin windows; measured by unparks per second in the
    snapshot.
11. **QEMU at -smp 64** under TCG is slow and timing-sensitive: tests
    check results, not times; benchmarks only at -smp 1/4/16 with KVM where
    the host has it.
12. **aarch64** reaches its arena through a global helper
    (`backend/aarch64/codegen/runtime.mlx:109-170`): per-thread arenas
    there need the helper to read its base through TPIDR_EL0; until then
    std.task on aarch64 runs with the shared atomic arena and no per-task
    reset (THREADS.md 6.8).
    **Critic:** that mode is broken, not merely slower: with no per-task
    reset a long-running program fills the one 1 GiB arena and traps,
    and the main thread's `turns` reset frees values the workers are
    using; all threads also share one thread block (3.1). Until aarch64
    has per-thread arenas, std.task there must run every task inline on
    the calling thread (no workers), which is correct, only serial.
13. **Linked programs**: exports called from C threads reload the *main*
    arena (`labels.mlx:81-87`); std.task never runs tasks on foreign
    threads, and a C thread that calls into Mlx helps nothing (THREADS.md
    6.7).
    **Critic:** the other direction is the dangerous one: a *worker* whose
    task calls C (in `unsafe` or a `threadsafe` extern) that calls back an
    export of the same linked program runs that callback on the main
    arena and, with identity = r14, on the main thread's block: its heap
    cache and crash context are then used by two threads at once. The
    export prologue of a linked program should keep r14/r15 when they
    already hold a registered thread arena (a magic word in the block),
    and reload the main arena only for threads std does not know.

### 7.4 Nebula as the first user

What runs in loops today, all on one thread ("one thread paints",
`core/tilepool.mlx:9-10`):

| Loop | Where | Becomes |
|---|---|---|
| filters over a layer (Filter menu, filter dialog, preview with a budget) | `ui/filterdialog.mlx:340` `filterBlocks` over 256 x 256 blocks (`BLOCK`, `:97`), each `filterBlock` (`:266`): read source with margin, `filters.apply`, mix, write the target's changed tiles; `applyNow` (`:394`) | `parallel_for` over the blocks; the preview's per-frame budget is spent by all workers |
| compositing the projection | `core/render.mlx:224` `update` over 256 x 256 blocks (`BLOCK`, `:44`), `children` mixes every layer into a block buffer, `writeRect` into the projection | `parallel_for` over the blocks, each writing tiles no other block writes: `update` starts its grid at the dirty area's corner (`render.mlx:231-237`), which is not on the tile grid, so the grid is first put on it (as `filterBlocks` does with `gridLine`); 256 is a multiple of the 64-pixel tile |
| transforms (free, perspective, warp) | `core/transform.mlx:1939` `resample`'s walk `blocksOf`/`next`/`generalBlock` | `parallel_for` over the blocks |
| brush dabs | `brush/pixel_engine.mlx:203` `direct` (and `wash`, `:158`) composite a dab tile by tile; `tip.forDab` (`brush/tip.mlx:1267`) makes the mask | a large dab (4 tiles and more: brushes from ~128 px) does its tiles with `parallel_for`; small dabs stay serial (under the grain); the next dab's mask is made while the current one composites (a two-stage pipeline keeps the order). Smudge and clone read what the dab before wrote: serial |
| also | thumbnails, the histogram docker (`parallel_reduce` of integer counts), PNG sequence export of an animation (a task per frame) | |

What they need first (all found in the code):

1. **The tile pool is global and unlocked**: `core/tilepool.mlx:33-39`
   (`blocks`, `count`, `live`, `current`), `take` (`:66`), `give` (`:99`).
   With `parallelTiles` (4.) it stays the calling thread's; R5 reports a
   task body that reaches it. Per-worker free lists only if a later loop
   must allocate tiles inside tasks.
2. **Tile reference counts are plain**: `Tile.refs` (`core/device.mlx:43-47`),
   `retain` (`:162`), `release` (`:166`). A parallel write breaks
   copy-on-write in shared tiles: either atomic counts, or (better)
   `parallelTiles` (4. below), in which workers never retain or release.
3. **Reading a device writes it**: `lookup` (`device.mlx:186-201`) updates
   the one-entry cache `lastKey/lastTile/lastKnown` (`:80-82`), and
   `readRect` takes `*Device` (`:492`). Workers reading one source race on
   the cache. A `Frozen` view of a device with a lookup that writes nothing
   (`readRectFrozen(view: *const DeviceView, ...)`); the compiler's
   `*const` then proves it.
4. **Writing a device inserts into its tile map and its mementos**:
   `place` (`:204`), `note` (`:213`), `tiles`/`touched`/`mementos`
   (`:75,86-87`) are hash maps and a list. This is SMART-LANG's L10
   `device.parallelTiles(area, body: @Task(fn(*TileJob) -> void))`, the
   trusted partition, done in three phases: on the calling thread every
   tile over the area is made the device's own (copy-on-write broken
   through `tileWrite`, memento noted, `refs == 1`), and the jobs are
   `TileJob{ key, pixels: @owned(*[4096]u32) }`; `parallel_for` writes
   pixels in place; back on the calling thread, tiles that came out
   uniform or unchanged are dropped (what `filterBlock`'s `differs` does
   today). A job may hold a block of tiles (4 x 4 for a 256-pixel block)
   where a margin is read, so the margin is read once a block, not once a
   tile. The tile pool stays the calling thread's (SMART-LANG L10): workers
   never take or give tiles.
5. **The scratch pool is one for all**: `scratch.common()`
   (`core/scratch.mlx:142-151`) used by `filterBlock`, `render.update`,
   `direct`, `resample`. `scratch.local()` = `PerWorker(Pool)`.
6. **Lazy tables with plain flags**: `color.mlx:563-569` (`tables_ready`,
   checked at `:596,602`), `transform.mlx:842-844` (`weightsReady`),
   `blend.mlx:422-423` (`pairTables`, `pairTableMade`), `palette.mlx:707-710`
   (`lab_ready`), `assistants.mlx:1026-1027` (`circle_ready`) ->
   `std.sync.Lazy`/`Once`, or comptime (SMART-LANG L7).
7. **Tables declared `var`**: `blend.mlx:1179,1332,1357,1373` (`MODES`,
   `SVG`, `OTHER`, `CATEGORIES`) -> `const`.

The filter loop then reads, with the language's blocks (the `|block|`
parameter is the owner's sketch syntax; the library takes a function and a
context record, 2.2):

```mlx
const copied = device_module.copy(node.*.device.?)
if copied == null { return noMemory(app) }
const source = device_module.freeze(copied.?)                   // Frozen(Device): reads write nothing
device_module.parallelTiles(target, area, BLOCK, |job| {        // claim, parallel_for, settle
    var pool = scratch.local()                                  // the worker's own scratch
    filterInto(job, source.view(), mask, params, margin, locked, pool)
})
```

> **Critic: `freeze(copied)` is not something R6 can accept.**
> `device.copy` shares every tile with the original (`refs += 1`,
> `core/device.mlx:272-290`), so the copy's tiles stay reachable and
> writable through `target`; claim and settle then change `refs` of
> those very tiles. "The compiler's `*const` then proves it" does not
> follow: a transitive `@immutable` view of shared COW tiles is a lie
> the checker must refuse. Two honest options: a trusted
> `freezeShared(device)` in `unsafe` inside `parallelTiles`, justified by
> the three-phase protocol (no `refs` change while workers run), or a deep
> copy of the source area (16 KiB a tile; a 4096 x 4096 layer is 64 MiB
> per filter run, every preview frame). State which; the first is the
> realistic one, and then Nebula's filters are *trusted*, not proven.

Expected effect: the filters, the projection and transforms are per-pixel
(or per-block with a margin) and independent across blocks, so they scale
with the cores until memory bandwidth limits them; the per-block results
are bit-identical to the serial ones, so Nebula's existing tests stay the
oracle.

---

## 8. Critic: verdict

**Verdict: not sound as written; sound in its skeleton and fixable.**
The kernel part (thread records beside processes, restart-model word
waits, shootdown, topology) and the order of the work hold up, and most
facts it cites check out against the code. What does not hold: one
deadlock the API itself creates (channels), one cancellation race, a
shootdown rule that is memory-unsafe, a pool mechanism with nobody to
run it, a false premise for mlxlibc, a classifier whose "make it const"
would break locks, a step order that does not build, and per-task costs
that all go through the one kernel lock because brixOS has no user-space
clock. Each is marked "Critic:" where it occurs; this section collects
them.

### 8.1 Claims checked

Held (spot-checked at cdca2dbdc): the arena bump `lock xadd [r14]` and
1 GiB + 8 mapping (`codegen/memory.mlx:103-115`, `emitter.mlx:194-218`);
the export reload in linked programs (`labels.mlx:81-87`); the `Process`
fields; `suspend`'s `rip -= 2`; `canGo` polling; `fault` answering false
for a present page (`process.mlx:488`); native calls 0-85 and
`thread_pointer_set` = 69 writing the process's FS; one upcall per
process; 64 descriptors; `pages.brixos.mlx:36-41` empty; the GPA and
`heap.make` calling Linux's futex/yield directly; the Linux layer's
stubs (`system.mlx:204,291,321-328,355-357,378`); mlxlibc's in-process
translation and futex no-op; `threadlocal` not lowered; 657 module vars
in 107 files; the Nebula globals and device paths; Observatory's
`FINDING_*` (13 findings, `FINDINGS = 13` must become 14).

Did not hold, or were missing: mlxlibc runs on per-call stack arenas,
not the thread's r14 (4.4); `holdOthers` gives up after 200 ms, so a
shootdown "bounded as holdOthers" is unsafe (1.6); the barging argument
for `woken` (1.3); `@threadPointer` for the block (risk 6); "eight files"
(1.1); the classifier's C/O labels (5.); no user-space clock (0); AMD's
CPUID leaves (1.4).

### 8.2 Rules that do not prevent what they claim

1. **Channels help like joins -> deadlock** (2.6, counter-program
   there). Joins form a tree; channels do not.
2. **Alert-based cancellation interrupts the wrong task** (2.5): the
   alert names a thread, the cancellation a scope; a sticky alert lands
   on the worker's next task. Generation-tagged alerts plus a per-thread
   park word (user-space parking lot) fix it on both systems.
3. **Hand-over plus interruption loses a lock** (1.3): woken must win.
4. **Bounded shootdown** (1.6): a CPU that has not acknowledged keeps a
   translation to a frame that is reused.
5. **Identity = r14 shares per-thread state** (3.1): `.shared` arenas,
   aarch64, and C callbacks into exports on workers put two threads on one
   thread block; the heap's per-thread cache is then an unsynchronized
   shared structure, i.e. heap corruption from code that is "safe".
6. **Lock levels are debug-only** (2.7): "prevented" is "diagnosed in
   debug builds".
7. **`Frozen(Device)` in Nebula** (7.4): not acceptable to R6 while tiles
   are COW-shared; it is a trusted partition, so say `unsafe`.

### 8.3 Costs the design leaves out

- **Per task:** three `clock_read` system calls (timestamps), each a
  kernel entry under the global lock; a stolen spawn/join adds up to three
  word calls and an IPI. **Fix:** a step K2b where the kernel publishes
  the TSC's scale and offset in a read-only page every process maps (a
  native memory object, n=18/19), std reads the TSC; until then
  timestamps only under `MLX_TASKS=1` and spin windows in iterations.
- **Per scope:** a `memory_map`, a region, a `memory_unmap` and (with
  threads) a shootdown under the lock (2.3). Use a per-worker chunk cache.
- **Per allocation:** unchanged for one thread (`lock xadd` stays); with
  the heap's thread cache behind a hook (the closure cannot use
  `@arena()`), an indirect call or a compare on every heap allocation,
  paid by single-threaded programs too.
- **Per context switch:** 512 bytes of FXSAVE each way plus the `Frame`,
  `wrmsr` of FS, the accounting adds; within a process CR3 stays, but
  `runnableBeyond`/`kickIdle`/`nearestDeadline` stay O(threads) per kernel
  exit until K4 replaces all three (1.6).
- **Per unmap in a threaded process:** an IPI round trip to every CPU
  that runs it, waited for under the one lock (1.6).
- **Per long-lived task:** the arena is reset only at a task's end, so a
  service loop fills its 1 GiB and traps (2.5).

### 8.4 Where it slides back into Linux's model

- Cancellation by a thread alert ("EINTR") first, the native multi-item
  wait "later" (2.5).
- Exit and id words and the PI word layout copied from clone/futex, with
  thread handles avoided because the 64-slot handle table is kept (1.2).
- `linux: ThreadSignals` inside the native `Thread` record (1.1).
- tid drawn from the pid counter so that `getpid() == gettid()` (1.2).
- Observatory's snapshots under `$XDG_RUNTIME_DIR` on brixOS, the native
  listener/channel only in O2 (6.2).

### 8.5 Single-threaded programs

Unchanged in code if C1 keeps its byte-identity check, and std's hooks
are null without `std.thread`. They do change in: the hook call on the
heap path once the thread cache exists (8.3); `Once` loads on the 40 lazy
tables (free on x86); uncontended lock CAS on the K-class caches that
gain locks (unicode, shaping, fontconfig); `Process.fault` answering true
for a present page (must keep refusing a write to a read-only page, or a
fault loops). A program that only calls `parallel_for` from its UI thread
changes in latency: the UI thread helps, and without the limit of 2.3
it can take a long stolen task.

### 8.6 What the owner's vision asks and this leaves out

- **Memory classes built on the arena architecture:** all three
  cross-thread classes are heap objects (3.2); no transferable arena
  chunk, no frozen arena.
- **Errors passed to the parent under control:** traps (checked
  arithmetic, bounds) still end the process (2.5). In Mlx, with no catch,
  those are the common run-time errors, so one overflowing tile ends
  Nebula. That needs an owner decision (e.g. a task mode whose trap ends
  the task's scope with `error.Trapped` when the task touched only its
  own arena and moved-in data, by cutting the worker's stack back to the
  task's entry from the upcall and resetting its arena: possible because
  R2 keeps a task's state in its arena; but no `deinit` runs, so the
  task's heap blocks leak and a trap while a `Guard` is held must still
  end the process), not silence.
- **Deadlocks prevented by API design:** not for channels, not in release
  builds (8.2).
- **CPU choice manually overridable** per task: only priority and domain
  per scope; no per-task processor kind or affinity.
- **Thread leaks: automatic join** holds for scopes; `task.service` and
  `RawThread.detach` need the exit deadline of 2.5 and the unmap-on-exit
  call of 1.2.
- **Monitoring native in Observatory:** yes, but through XDG files on
  brixOS first (6.2).

---

## Appendix A: every module-level `var` of std, classified

Made by a small classifier over `grep -rn "^var \|^pub var " std/src std/bootstrap` (657 globals, 107 files): a global is **C** when its initializer is a literal and no line of its file (any file, for a `pub var`) assigns it, indexes it on the left of `=`, or passes its address to a call other than `@intFromPtr`; **O** by its name (`*Ready`, `*Done`, `loaded`, `installed`, ...); **L** a `u32` named `*lock*`/`*guard*`; **A** when it appears in a `cmpxchg`/`atomicRmw`/`atomicStore`/`swapWord` line; **K** by its name (`cache`, `kept`). Classes: **C** never written after a literal initializer, in its file or (for a `pub var`) anywhere (make it `const`: IMMUTABLE); **O** a once flag (`std.sync.Once`; the tables it guards appear under M); **L** a lock word of its own (`std.sync.Lock`); **A** published or counted with atomics already; **K** a cache filled while running (under its lock, or per thread); **M** other mutable state (section 5 reviews the runtime's own by hand; in the codecs and text libraries M is mostly tables built lazily under an O flag, or per-call scratch). The heuristic errs towards M.

- `std/bootstrap/debug_allocator.mlx`: M pageWatcher
- `std/bootstrap/os/host.brixos.mlx`: M remembered cookies infoScratch selfUser; O primed
- `std/bootstrap/os/upcalls.mlx`: M handlers handled queued takenRecords; O installed
- `std/bootstrap/page_allocator.mlx`: M watchAlloc watchFree
- `std/bootstrap/tinypage_allocator.mlx`: M region
- `std/src/aac.mlx`: C SAMPLING_RATES
- `std/src/aac/tables.mlx`: C LENGTHS_1 CODES_1 VALUES_1 LENGTHS_2 CODES_2 VALUES_2 LENGTHS_3 CODES_3 VALUES_3 LENGTHS_4 CODES_4 VALUES_4 LENGTHS_5 CODES_5 VALUES_5 LENGTHS_6 CODES_6 VALUES_6 LENGTHS_7 CODES_7 VALUES_7 LENGTHS_8 CODES_8 VALUES_8 LENGTHS_9 CODES_9 VALUES_9 LENGTHS_10 CODES_10 VALUES_10 LENGTHS_11 CODES_11 VALUES_11 LENGTHS_SF CODES_SF VALUES_SF LONG_START LONG_BANDS LONG_OFFSETS SHORT_START SHORT_BANDS SHORT_OFFSETS TNS_MAX_BANDS_LONG TNS_MAX_BANDS_SHORT
- `std/src/build.mlx`: M arenaBase arenaPoison
- `std/src/build/cache.mlx`: M memo
- `std/src/crash.mlx`: M arenaBase arenaRoom contextPointer contextLength contextBuffer entered reportMessage reportMessageLength reportRecord reportRecordLength reportFrames reportFrameCount reportFirstFrame reportNext reportStatus reportRoom reportScratch reportMapsLength withoutNames mappingsOfProgram describeScratch demangleTables demangledText; A reportingThread; C faultWhileNaming; K cachedPath cachedPathLength cachedImage cachedSize cachedIndex cachedIndexBytes cachedCount cachedSymbols cachedSymbolsSize cachedStrings cachedStringsSize; O installed
- `std/src/crash_os.brixos.mlx`: M handler faultInfo faultRegisters
- `std/src/crc.mlx`: M crcTables powers cksumTable; O tablesReady cksumReady
- `std/src/crypto/mpi.mlx`: M smallPrimes smallPrimeCount
- `std/src/diagnostics.mlx`: M gpuMemory framesState framesMemory frameCount frameStarted frameFirst framesWritten framesStartup framesPath framesPathLength
- `std/src/events.mlx`: M handlers
- `std/src/flate/deflate.mlx`: C GOOD LAZY NICE CHAIN FUNC
- `std/src/flate/inflate.mlx`: C LBASE LEXT DBASE DEXT ORDER; M lenfix distfix; O fixedReady
- `std/src/flate/trees.mlx`: C EXTRA_LBITS EXTRA_DBITS EXTRA_BLBITS BL_ORDER; M staticLtree staticDtree distCode lengthCode baseLength baseDist; O staticReady
- `std/src/fontconfig/config.mlx`: M current prgname; C currentLock; O prgnameDone; L prgnameGuard
- `std/src/fontconfig/confparse.mlx`: M availableFiles
- `std/src/fontconfig/env.mlx`: M table count listed; O loaded; L guard
- `std/src/fontconfig/lang.mlx`: M names bits charsets ofBit countrySets countrySetCount defaults; O loaded; L guard defaultsGuard
- `std/src/fontconfig/mem.mlx`: M heap; O heapReady; A liveBlocks
- `std/src/fontconfig/objects.mlx`: M builtinNames others otherCount otherRoom nextId; L guard
- `std/src/fontconfig/scan.mlx`: M languageNames
- `std/src/fontconfig/strings.mlx`: A folds; M foldCount foldBytes; L foldGuard
- `std/src/h264/decode.mlx`: C ZIGZAG4 ZIGZAG8 NORM4 NORM8 CHROMA_QP BLK_RASTER skipDeblocking B_PRED0 B_PRED1
- `std/src/h264/tables.mlx`: C CABAC_INIT_I CABAC_INIT_PB RANGE_LPS NEXT_MPS NEXT_LPS COEFF_TOKEN_LENGTHS COEFF_TOKEN_CODES COEFF_TOKEN_DC_LENGTHS COEFF_TOKEN_DC_CODES TOTAL_ZEROS_LENGTHS TOTAL_ZEROS_CODES TOTAL_ZEROS_DC_LENGTHS TOTAL_ZEROS_DC_CODES RUN_BEFORE_LENGTHS RUN_BEFORE_CODES ALPHA BETA TC0 DEFAULT_4X4_INTRA DEFAULT_4X4_INTER DEFAULT_8X8_INTRA DEFAULT_8X8_INTER CBP_INTRA CBP_INTER SIG8X8 LAST8X8
- `std/src/h265.mlx`: C DEFAULT_INTRA DEFAULT_INTER
- `std/src/h265/decode.mlx`: M DIAG2 DIAG4 DIAG8 HORIZ4 VERT4 HORIZ2 VERT2; O scansReady; C LEVEL_SCALE QPC_TABLE CTX_MAP_4X4 DST INTRA_ANGLE INV_ANGLE COMBINED_L0 COMBINED_L1 SAO_DX SAO_DY
- `std/src/h265/tables.mlx`: C INIT_VALUES TRANSFORM EPEL QPEL TC BETA
- `std/src/heap.mlx`: M own ownState ownWatch heap words
- `std/src/leaks.mlx`: M state watching arena arenaMark releasedMark describer buffers snapshotPath snapshotLength lastSnapshot events mainFrame nameUsed gpuChanges
- `std/src/mp4.mlx`: C SAMPLING_RATES
- `std/src/shaping/arabic.mlx`: C STATES MODIFIER_COMBINING_MARKS
- `std/src/shaping/arabicpua.mlx`: C U8 U16
- `std/src/shaping/arabictable.mlx`: C JOINING JOINING_RANGES SHAPING LIGATURES LIGATURES_MARK LIGATURES_3
- `std/src/shaping/blob.mlx`: A emptyBlob
- `std/src/shaping/buffer.mlx`: A emptyBuffer
- `std/src/shaping/cfftable.mlx`: C STRING_OFFSETS STANDARD_ENCODING_TO_SID EXPERT_CHARSET_TO_SID EXPERT_SUBSET_CHARSET_TO_SID
- `std/src/shaping/colr.mlx`: M zeroLine
- `std/src/shaping/common.mlx`: A languages defaultLanguage; M localeFunction
- `std/src/shaping/draw.mlx`: A emptyFuncs
- `std/src/shaping/face.mlx`: A emptyFace
- `std/src/shaping/font.mlx`: A defaultFuncs nilFuncs nullFont adaptorFuncs
- `std/src/shaping/hebrew.mlx`: C DAGESH_FORMS
- `std/src/shaping/indictable.mlx`: C TABLE RANGES SINGLES
- `std/src/shaping/machines.mlx`: C INDIC_TRANS_KEYS INDIC_KEY_SPANS INDIC_INDEX_OFFSETS INDIC_INDICIES INDIC_TRANS_TARGS INDIC_TRANS_ACTIONS INDIC_TO_STATE_ACTIONS INDIC_FROM_STATE_ACTIONS INDIC_EOF_TRANS INDIC_PROGRAM_OFFSETS INDIC_PROGRAM KHMER_TRANS_KEYS KHMER_KEY_SPANS KHMER_INDEX_OFFSETS KHMER_INDICIES KHMER_TRANS_TARGS KHMER_TRANS_ACTIONS KHMER_TO_STATE_ACTIONS KHMER_FROM_STATE_ACTIONS KHMER_EOF_TRANS KHMER_PROGRAM_OFFSETS KHMER_PROGRAM MYANMAR_TRANS_KEYS MYANMAR_KEY_SPANS MYANMAR_INDEX_OFFSETS MYANMAR_INDICIES MYANMAR_TRANS_TARGS MYANMAR_TRANS_ACTIONS MYANMAR_TO_STATE_ACTIONS MYANMAR_FROM_STATE_ACTIONS MYANMAR_EOF_TRANS MYANMAR_PROGRAM_OFFSETS MYANMAR_PROGRAM USE_TRANS_KEYS USE_KEY_SPANS USE_INDEX_OFFSETS USE_INDICIES USE_TRANS_TARGS USE_TRANS_ACTIONS USE_TO_STATE_ACTIONS USE_FROM_STATE_ACTIONS USE_EOF_TRANS USE_PROGRAM_OFFSETS USE_PROGRAM
- `std/src/shaping/memory.mlx`: M allocator cCalloc cRealloc cFree; O allocatorReady
- `std/src/shaping/nametable.mlx`: C MS_CODES MS_OFFSETS MAC_CODES MAC_OFFSETS
- `std/src/shaping/otfont.mlx`: A otFuncs
- `std/src/shaping/outline.mlx`: A recordingFuncs
- `std/src/shaping/paint.mlx`: A emptyFuncs
- `std/src/shaping/paintextents.mlx`: A drawFuncs paintFuncs
- `std/src/shaping/post.mlx`: M macOffsets; O macReady
- `std/src/shaping/serialize.mlx`: M formatList; O formatListReady
- `std/src/shaping/set.mlx`: O emptySet; A emptyMap
- `std/src/shaping/shape.mlx`: A emptyPlan; M shaperNames
- `std/src/shaping/tags.mlx`: C NEW_SCRIPTS NEW_TAGS
- `std/src/shaping/tagtable.mlx`: C LANGUAGES2 LANGUAGES3
- `std/src/shaping/thai.mlx`: C SD_MAPPINGS SDL_MAPPINGS SL_MAPPINGS RD_MAPPINGS ABOVE_START ABOVE BELOW_START BELOW
- `std/src/shaping/unicode.mlx`: A emptyFuncs defaultFuncs icuFuncs; M scriptTable; O scriptTableReady
- `std/src/shaping/usetable.mlx`: C U8 U16
- `std/src/shaping/voweltable.mlx`: C RULES
- `std/src/sql/ddl.mlx`: M joined; O joinedReady
- `std/src/sql/engine.mlx`: M noValue; O noValueReady
- `std/src/sql/mem.mlx`: M heap; O heapReady; A liveBlocks
- `std/src/sql/os.brixos.mlx`: M inodes inodesWord
- `std/src/sql/os.mlx`: M inodes inodesWord
- `std/src/sql/query.mlx`: M joined decimalBuffer viewDepth; O joinedReady
- `std/src/sql/write.mlx`: M messageBuffer; O messageReady
- `std/src/timezone/tzdata.mlx`: C ID_STARTS ID_ZONES ID_CANONICAL ID_REGIONS ZONE_FIRSTS ZONE_INITIAL_RAWS ZONE_INITIAL_SAVES ZONE_FINAL_YEARS ZONE_FINAL_RAWS ZONE_FINAL_FIRSTS FINAL_MONTHS FINAL_DAY_KINDS FINAL_DAYS FINAL_WEEKDAYS FINAL_ATS FINAL_AT_KINDS FINAL_SAVES TRANSITION_TIMES TRANSITION_RAWS TRANSITION_SAVES
- `std/src/truetype/calc.mlx`: C arctanTable
- `std/src/truetype/cffload.mlx`: M standardStrings standardText; O standardStringsReady
- `std/src/truetype/psnames.mlx`: M aglNames aglValues aglText; O aglReady
- `std/src/truetype/render.mlx`: C colorRender
- `std/src/truetype/sfnt.mlx`: M macNames macText; O macNamesReady
- `std/src/turns.mlx`: O located; M residency releasedTurns
- `std/src/ui/scheme.mlx`: M current fallback hasFallback; O known
- `std/src/unicode/breaks.mlx`: M found statuses raw effective absorbed ucharsFuncs; O ucharsFuncsReady
- `std/src/unicode/calendar.mlx`: C MONTH_DAYS DAYS_BEFORE LEAP_DAYS_BEFORE DATE_LINES CALENDAR_LIMITS GREGORIAN_LIMITS CHINESE_LIMITS HEBREW_LIMITS ISLAMIC_LIMITS PERSIAN_LIMITS INDIAN_LIMITS CE_LIMITS; M eraStarts erasCount erasCurrent defaultZone defaultZoneLength hostBuffer; A erasRead; L erasLock
- `std/src/unicode/calendar/systems.mlx`: C DAYS_BEFORE HEBREW_MONTH_LENGTH HEBREW_MONTH_START HEBREW_LEAP_MONTH_START UMALQURA_MONTHLENGTH UMALQURA_YEAR_START_FIX PERSIAN_NUM_DAYS; K monthCacheKeys monthCacheValues monthCacheUsed yearCacheKeys yearCacheValues yearCacheUsed; L monthCacheLock yearCacheLock; O monthCacheReady yearCacheReady
- `std/src/unicode/charmaps/tables.mlx`: C UPPER
- `std/src/unicode/charsets.mlx`: C WINDOWS_1252_HIGH
- `std/src/unicode/collation.mlx`: L cacheLock; K cache; M availableList availableTotal emptyRules
- `std/src/unicode/collation/elements.mlx`: M hanPlaces tableContractions rootContractions usedPrimaries groupStarts groupCodes firstSecondary firstSecondaryTertiary lastSecondary lastSecondaryTertiary ducetVariableTop; L rootLock; O rootReady
- `std/src/unicode/collation/rules.mlx`: C LOCALE_KEYS_STARTS LOCALE_VALUES_STARTS RULES_KEYS_STARTS RULES_VALUES_STARTS HAN_RANGES
- `std/src/unicode/converters.mlx`: M defaultToU defaultFromU tableNames
- `std/src/unicode/enumeration.mlx`: K kept keptText keptCount
- `std/src/unicode/format/date/generator.mlx`: L cacheLock; K cachedCount cachedNext cachedIds cachedSkip cachedGenerators; M emptyUnits
- `std/src/unicode/format/date/patterns.mlx`: C LETTER_FIELDS FIELD_LEVELS
- `std/src/unicode/format/date/symbols.mlx`: C SIZES OFFSETS; L cacheLock; K cachedCount cachedNext cachedData cachedKind cachedNs cachedNsLength cachedSymbols
- `std/src/unicode/format/date/zonenames.mlx`: L cacheLock; K cachedId cachedLength cachedType
- `std/src/unicode/format/number/currency.mlx`: K keptKeys keptCopies keptLengths; O keptReady codesReady; M codeCopies
- `std/src/unicode/format/number/data.mlx`: M slotIds slotCopies slotLengths; O slotsReady
- `std/src/unicode/format/number/decimalformat.mlx`: L nameLock; K nameCacheNext nameCacheData nameCacheEntries nameCacheCounts nameCachePools nameCacheUsed; O nameCacheReady
- `std/src/unicode/format/number/skeleton.mlx`: M scratch
- `std/src/unicode/locale.mlx`: M isoLanguages isoCountries
- `std/src/unicode/locale/data.mlx`: C LIKELY_KEYS_STARTS LIKELY_VALUES_STARTS LANGUAGE_ALIAS_KEYS_STARTS LANGUAGE_ALIAS_VALUES_STARTS SCRIPT_ALIAS_KEYS_STARTS SCRIPT_ALIAS_VALUES_STARTS TERRITORY_ALIAS_KEYS_STARTS TERRITORY_ALIAS_VALUES_STARTS VARIANT_ALIAS_KEYS_STARTS VARIANT_ALIAS_VALUES_STARTS LANGUAGE_OVERLONG_KEYS_STARTS LANGUAGE_OVERLONG_VALUES_STARTS TERRITORY_OVERLONG_KEYS_STARTS TERRITORY_OVERLONG_VALUES_STARTS KEY_KEYS_STARTS KEY_VALUES_STARTS LEGACY_KEY_KEYS_STARTS LEGACY_KEY_VALUES_STARTS TYPE_KEYS_STARTS TYPE_VALUES_STARTS LEGACY_TYPE_KEYS_STARTS LEGACY_TYPE_VALUES_STARTS PREFERRED_TYPE_KEYS_STARTS PREFERRED_TYPE_VALUES_STARTS SPECIAL_TYPE_KEYS_STARTS SPECIAL_TYPE_VALUES_STARTS ALPHA3_KEYS_STARTS ALPHA3_VALUES_STARTS AVAILABLE_KEYS_STARTS AVAILABLE_VALUES_STARTS
- `std/src/unicode/locale/defaults.mlx`: M defaultText defaultLength; O defaultKnown
- `std/src/unicode/locale/resources.mlx`: M base localeCount keyCount localeTable keyTable entryTable stringOffsets stringBytes; O tried
- `std/src/unicode/normalize.mlx`: M instances
- `std/src/unicode/properties.mlx`: K nameCache nameCacheCount
- `std/src/unicode/tables.mlx`: C PROPERTY_INDEX PROPERTY_BLOCKS PROPERTY_CORE PROPERTY_EXTRA PROPERTY_BITS0 PROPERTY_BITS1 CASE_INDEX CASE_BLOCKS CASE_UPPER CASE_LOWER CASE_TITLE CASE_FOLD CASE_MIRROR CASE_BRACKET CASE_FLAGS DECOMPOSITION_CODES DECOMPOSITION_INFO DECOMPOSITION_DATA COMPOSITION_KEYS COMPOSITION_VALUES SPECIAL_CODES SPECIAL_LOWER SPECIAL_TITLE SPECIAL_UPPER SPECIAL_DATA FOLD_CODES FOLD_INFO FOLD_DATA SCX_FIRST SCX_LAST SCX_DATA NUMERIC_NUMERATORS NUMERIC_DENOMINATORS COLLATION_INDEX COLLATION_BLOCKS COLLATION_RECORDS COLLATION_CES CONTRACTION_FIRST CONTRACTION_INFO CONTRACTION_CES CONTRACTION_CODES IMPLICIT_FIRST IMPLICIT_LAST IMPLICIT_BASE IDNA_INDEX IDNA_BLOCKS IDNA_RECORDS IDNA_DATA BLOCK_FIRST BLOCK_LAST BLOCK_CODE; O SCX_OFFSET
- `std/src/vp9.mlx`: C LITERAL_TO_FILTER SEGMENT_FEATURE_BITS SEGMENT_FEATURE_SIGNED
- `std/src/vulkan/loader.mlx`: M builtIns builtInCount
- `std/src/wayland/transport/backend.brixos.mlx`: M loops knownEnvironment; O loopsReady knownEnvironmentSet
