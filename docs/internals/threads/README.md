# MLX Smart Threads: design drafts

Drafts, not decisions. They are being revised; nothing here is built yet.

- `THREADS.md`: the facts. What brixOS, the compiler and std have today
  (the value arena in r14/r15, kernel threads without kernel stacks, TLB
  shootdown, the native call numbers). Its Linux-shaped model is not the
  base.
- `SMART-LANG.md`: the language and compiler side. Memory classes
  `@immutable`, `@shared` and `@owned`; rules R1-R8; the block kinds
  `@Block`, `@Task` and `@Scope`. Its critics found it unsound: notes
  marked "Critic:", and the counter-programs in section C.2.
- `SMART-RUNTIME.md`: the runtime, std, scheduler, kernel threads and
  Observatory side (`std.task` with work stealing, `word_wait` and
  `word_wake`, the Thread Inspector). Its critics found it unsound too:
  notes marked "Critic:".
- `SMART-THREADS.md` (when present): one plan from the three, in the
  owner's order.

The order: native kernel threads, then safe arena and ownership semantics,
then structured concurrency, then the smart scheduler, then Linux
compatibility. Linux is only an adapter. The global kernel lock may stay
at first. Single-threaded programs must not change or get slower.

`notes/`:

- `sema-pending-elsewhere.patch`: an unverified change for the compiler
  limit near 3820 functions (projects/nebula/README.md, To do).
- `brix_nebula.sh`: the QEMU boot used for Nebula's brixOS screenshots.
