# MLX Smart Threads: the language and the compiler

The language part of brixOS's own threading model. It covers the memory
classes in the type system, transfer of ownership, task blocks and
structured scopes, raw threads, the per-thread value arena as the compiler
sees it, and the plan.

The design was written against the tree as it is today (read only; no file
of the repository was changed). `file:line` references are to that tree.
"codemap" means `mlx-out/bin/mlx-codemap`.

Several facts were checked by compiling small probes with
`mlx-out/bin/compiler/mlx4` in the scratchpad (`scratchpad/smartlang/`):

| Probe | What it showed |
|---|---|
| `tl.mlx` | `threadlocal var` → `MLX-E9001` |
| `blk.mlx` | `const f = \|x\| { ... }` → `MLX-E2001`, so a block literal is free syntax |
| `spawn.mlx` | `@spawnThread` outside `unsafe` compiles today |
| `trail.mlx` | `const v = task.scope { return 1 }` parses today as two statements and returns 1 |
| `alias.mlx` | a `*` into a `@noncopy` value stays usable after `@move` |
| `arena.mlx` + objdump | the allocation is `lock xadd %rax,(%r14)` … `cmp %r15,%rdx; jbe; ud2` |

THREADS.md (same scratchpad folder) is the analyst's kernel, arena and
std.thread design. This document uses its facts:

- native calls 86–92 (`thread_create`, `thread_exit`, `memory_wait`,
  `memory_wake`, `thread_self`, `thread_yield`, `thread_info`), and the
  handle kind `thread` with the state `ended`;
- `@arena()` and `@arenaSwitch()`, with a Block in the thread arena's
  header;
- the import-closure constraint on `std.turns`, `std.crash` and `std.heap`;
- `MAX_REGIONS` raised to 1024.

THREADS.md's std.thread (a function plus a context word, by hand) becomes
the **raw** layer here. It is not the base programs are written against.

> **Critic: verdict.** The architecture holds up: classes on addresses,
> second-class blocks, closure-shaped structured scopes, one arena per
> thread with an unchanged allocation path, and `threadlocal` relative to
> r15. **The safety claim does not hold as written.** Seventeen
> counter-programs in §C.2 race, double-free or dangle in code the rules
> accept:
>
> - a moved capture in a `parallel_for` body is dropped once per item;
> - a block can be captured by a task;
> - `@shared` and `@immutable` addresses have no lifetime;
> - a shared `const` Mutex can be neither locked nor safely captured;
> - an aggregate with a `@shared` field copies non-atomically;
> - an atomic load duplicates an `@owned` pointer;
> - std's safe wrappers turn integers back into addresses;
> - a dynamic `task.spawn` escapes R4;
> - a guard's data escapes if R4 is limited to `@owned` types;
> - a thread outlives a by-reference capture.
>
> Several plan steps also cannot pass as specified (§C.4), and the
> byte-identity promise breaks at L4 (§C.6). §C.8 gives the amended rules.
> Inline notes marked **Critic:** point to §C.

---

## 0. The decisions on one page

1. **A class belongs to addresses and storage, not to bytes.** Data
   without addresses ("plain": integers, floats, enums, and arrays or
   structs of them) crosses threads by copy. Every address type
   (`*T`, `[*]T`, `[]T`, `?*T`, `*anyopaque`) is **LOCAL** unless it is
   qualified with one of three new type constructors, which follow the
   `@noncopy(T)` precedent:
   - `@immutable(P)`
   - `@shared(P)`
   - `@owned(P)`

   A type is **TRANSFERABLE** when it is non-copyable and all its
   addresses are owned, immutable or shared. The compiler computes a
   type's class structurally, beside `copyability`, and stores it in the
   type store.

2. **Seven rules make safe code race-free:**

   | Rule | Content |
   |---|---|
   | R1 | the classes |
   | R2 | nothing with a LOCAL address crosses a thread boundary |
   | R3 | transfer is `@move` |
   | R4 | borrows of owned memory are second-class |
   | R5 | task code does not touch unsynchronized globals |
   | R6 | immutability is transitive |
   | R7 | shared data is reached only through atomics or a lock's borrow |

   A raw boundary, R8, adds that raw threads, `@ptrFromInt`, `@ptrCast`
   and class casts need `unsafe`. Section 2 has the argument for why the
   rules are enough.

   **Critic:** they are not enough. §C.2 has the counter-programs and
   §C.8 the amendments A1–A16. The amendments give lifetimes to
   shared/immutable addresses, make `@shared` non-copyable with interior
   mutability, split once and many task blocks, and make blocks
   uncapturable.

3. **No general closures.** Blocks (`|x| { ... }`, `|| { ... }`, and a
   trailing `f(args) { ... }`) are allowed only where a parameter has a
   block type:
   - `@Block(fn...)`: sequential;
   - `@Task(fn...)`: may run on another thread;
   - `@Scope(fn...)`: sequential, and every task spawned in it is joined
     when it ends.

   Blocks are second-class: they can be called or passed on, never
   stored. A block is lowered to a generated function plus a context
   record: two words, laid out like a slice.

4. **Structured concurrency is std code**, `std.task`, built on raw
   threads. The compiler's part:
   - the capture rules;
   - result sendability;
   - handles that cannot leave their scope (R4);
   - names, so that crash reports, profiles and Observatory show
     `path:line:task`.

5. **Raw threads are `unsafe`.** This covers `@spawnThread`,
   `@arenaSwitch` and THREADS.md's std.thread, which is declared with a
   new `unsafe fn` modifier.

   `threadlocal` finally gets an ABI: Mlx-native and **arena-relative**,
   `[r15 + offset]`. It needs neither the FS register nor the C library.
   That fits THREADS.md's choice that std threads leave FS alone.

6. **One value arena per thread, with today's code.** The allocation path
   stays `lock xadd [r14]`. Only the register's value differs per thread
   (THREADS.md §2). The compiler adds one guarantee: under R2, no address
   into a task's arena outlives the task. So a worker may reset its arena
   after every task without any check.

   Programs without threads compile **byte-identically**. A check script
   proves it at every step.

7. **The checks are in two places:**
   - sema handles the classes, captures, borrow extents, shared access and
     conversions;
   - a new post-lowering pass, `ir/thread_check.mlx`, does the
     interprocedural part. It checks borrows passed to escaping parameters
     (reusing `ir/escape.mlx`'s facts) and globals reachable from tasks.

   New codes: `MLX-E6007` to `E6009`, `E7006` to `E7012`, `E4016`,
   `W7001` and `W7002`. `E7004` is reused for the raw boundary.

---

## 1. What the compiler and runtime have today (verified)

| Fact | Where |
|---|---|
| The value arena is 1 GiB + 8 bytes with the fill word at its base. On x86_64, **r14 = base and r15 = end**, set in `_start` (brixOS `memory_map` 0x4010, Linux mmap with MAP_NORESERVE). | `backend/x86_64/codegen/emitter.mlx:194-215`, called from `emitStartStub` `:108-157` |
| An allocation is 8 instructions: `mov rcx,n; mov rax,rcx; lock xadd [r14],rax; mov rdx,rax; add rdx,rcx; cmp rdx,r15; jbe +2; ud2`. | `codegen/memory.mlx:103-114`; `encoder.mlx:1181` (F0 0F C1); confirmed by objdump |
| r14 and r15 are never homes of Mlx locals (homes are rbx, r12 and r13; r14/r15 only in mlx-cc's code). | `codegen/state.mlx:33-37`, `homeRegister` `:1177-1183` |
| An export called from C reloads the main arena from DATA_ARENA (linked programs). A plugin maps an arena per call, or uses `--stack-arena`. | `codegen/labels.mlx:73-87` (`enterFromForeignCode` `:81`), `:101-138` (`makePluginArena` `:109`) |
| aarch64 bumps a global `__mlx_arena_base` in the helper `__mlx_arena_alloc` (LDAR, then an LDXR/STXR loop, mapped lazily and published with CAS). | `backend/aarch64/codegen/runtime.mlx:6-10, 109-170` |
| Aggregates are taken from the arena (`alloca` arg0 = bytes). `escape.mlx` moves those that cannot outlive their frame into the frame. It runs interprocedurally, callees first, and marks parameters `ESCAPES`/`CONTENTS`. `ret` marks ESCAPES; an unknown callee means "escapes". | `ir/lir.mlx:36-44`; `ir/escape.mlx:1-36, 56-65, 150-245, 829, 940-975`; run only for whole programs (`ir/optimize.mlx:116-117`) |
| Raw threads: `@spawnThread` is Linux clone, and the child **inherits r14/r15**, so it shares the arena. `@threadPointer` is `mov rax, fs:[0]`. | `memory.mlx:126-171`; `ir/lower.mlx:3446-3480`; `lir.mlx:85-95`; spec `atomics-tls.xml:19-26` |
| Users of raw threads: h264/h265 workers, the splash crew, mlxlibc pthreads, tests 298 and 315. None uses `unsafe` around the spawn. | `std/src/h264/decode.mlx:5081`, `std/src/h265/decode.mlx:4241`, `projects/desktop/splash/bands.mlx:84`, `compiler/libc/pthread.mlx:294` |
| Atomics: `@atomicLoad`, `@atomicStore`, `@atomicRmw`, `@cmpxchgWeak`, `@cmpxchgStrong`, `@fence`; orders `unordered` to `seq_cst` with C11's combination rules. | spec `atomics-tls.xml:3-17`; `sema/builtins/atomics.mlx`; test 295 |
| `threadlocal` is parsed, flagged (`FLAG_THREADLOCAL`), kept out of the globals, and rejected with E9001. | `parser/syntax.mlx:151`; `sema/symbols/collect.mlx:24`; `ir/lower.mlx:580-592`; `SPEC_CONFLICTS.md:59-68` |
| Unsafe is a token-level scan for `@ptrCast`/`@ptrFromInt` outside `unsafe {` (E7004), run before declaration analysis. | `sema/unsafe_boundary.mlx:15-49`; `driver/pipeline.mlx:227-232` |
| Ownership: `@noncopy` is a type constructor (copyability `copyable`/`explicit_nocopy`/`contains_nocopy`, propagated to aggregates). `@move` sets the symbol state to Moved. Use after a move is E6001, a copy of a non-copyable value E6002. Conditional moves keep a runtime drop flag. | `sema/types/kind.mlx:31`; `types/store.mlx:561`; `sema/declarations.mlx:540, 1323-1330, 1414-1419, 1999`; `symbols/symbol.mlx:5, 84`; `ir/lower.mlx:81-93, 1867-1948` |
| No closures and no anonymous functions. `fn(...) -> R` values are code addresses; the Allocator is a struct of function pointers. | `parser/expression.mlx:358-382`; `std/bootstrap/allocator.mlx:24-30` |
| Diagnostic codes are an enum. E6001–E6006 and E7001–E7005 are used; 4xxx goes to E4015. | `compiler/selfhost/diagnostic.mlx:4-25`; `spec/02-compiler/diagnostics/codes.xml` |
| `std.turns` finds the arena by probing and caches it process-wide in `located`. `std.crash` caches `arenaBase` once. Both are in the compiler's import closure, so they cannot use new builtins for two compiler generations (THREADS.md §2.7). | `std/src/turns.mlx:11-13, 36-75, 206-225`; `std/src/crash.mlx:114, 1303` |
| brixOS gives a process 256 memory regions (THREADS.md raises it to 1024). | `projects/brix/kernel/process.mlx:79` |
| Module-level `var`s: 837 lines in std/ (grep), 260 in Nebula. The analyst's classifier sorts 657 of std's into C 335 (never written after init), M 195, K 47 (caches), O 40 (once flags), A 25 (atomic), L 15 (locks). **Critic:** the counts 837/260 check out. `globals_classified.txt` is no longer in the scratchpad; SMART-RUNTIME.md Appendix A has a classification. "Never written *after init*" is not "never written", which is what §2's rule accepts (§C.2 CP15). | `scratchpad/brix/globals_classified.txt:1` |
| Nebula: 64×64 tiles of `u32`. A tile is shared while `refs > 1` and then never written (copy on write). The tile pool and lookup tables are lazily initialized `var` globals ("one thread paints"). | `projects/nebula/core/device.mlx:34-48`; `core/tilepool.mlx:1-40`; `core/color.mlx:563-569`; `filters/*.mlx` (`var kept`, `var *_ready`) |

---

## 2. The model, and why safe code cannot race

**Memory a thread can address in safe code.** A thread can reach:

- its own frames and arena;
- heap blocks it allocated;
- whatever is reachable from values it *received*: block captures, a join
  result, a channel message, a shared cell;
- globals.

Turning an integer back into an address needs `unsafe` (`@ptrFromInt`).
So does any `@ptrCast`, and so does calling foreign code. That is the
raw boundary, R8.

**Critic:** this holds for the builtin but not for std. Take
`pub fn fillWord(base: usize) -> *usize` (`std/src/turns.mlx`): it has a
safe signature with `@ptrFromInt` inside, so it revives any integer as a
writable address in safe code. std has 1207 `@ptrFromInt` and 2159
`unsafe {` (277 files); Nebula has 572 `unsafe {`. Every safe-signature
wrapper of that kind is a hole (§C.2 CP7). The guarantee holds only
relative to sound encapsulation, and the plan has no audit for it
(§C.8 A12).

**What crosses.** By R2 a received value holds no LOCAL address. Every
address it holds falls into one of three cases:

- **`@immutable`**: nobody writes that memory, because no writable path to
  it exists. It is read-only data (literals and module `const`s in the
  constant image), or memory whose single owner was consumed when it was
  frozen (R3/R4), with no surviving borrows (R4).
- **`@shared`**: it is reachable only through atomics or a lock's guard,
  whose borrows cannot outlive the guard (R7 + R4).
- **`@owned`**: it is unique. The sender gave it up with `@move` (R3).
  None of the sender's borrows survive the move (R4(d)). An address of
  the moved-from binding is itself a borrow, so it cannot be used after
  the move either (R4(e)).

**Globals.** Code reachable from a task does not read or write a module
`var` that some code writes, unless it is:

- `@shared`;
- `threadlocal` (of a sendable type);
- accessed inside `unsafe` (R5).

Never-written `var`s count as immutable.

**The parent during `parallel_for`.** The parent is blocked and takes
part only by running iterations. The iterations get pointers to distinct
elements of one slice of sendable elements, so they are disjoint by
construction.

Hence no two threads access one location without atomics, with one of
them writing. The same argument gives the arena guarantee: an address
into a task's arena is LOCAL, and no LOCAL address leaves the task. So
the worker can release the task's values when the task ends.

**Critic:** "an address into a task's arena is LOCAL" is false for
`&n` with `n: @shared(u64)` (or `@immutable`) held in a task's frame or
arena. Its type is `@shared(*u64)`, which R2 lets cross through a channel
or a shared store. It then dangles after the task ends and the reset
(§C.2 CP4). The three cases above have no lifetime component: a
`@shared` or `@immutable` address is sendable however short-lived its
storage is. §C.8 A1 makes such addresses borrows unless the storage is
static or owned by a refcounted owner.

**What is outside the guarantee, and marked:**

- `unsafe { }`: the existing lexical boundary, `spec/00-language/unsafe.xml`;
- raw thread builtins and `unsafe fn` calls (R8, E7004);
- foreign (`extern`) functions called from task code, which need `unsafe`
  or a `threadsafe` declaration (E7012);
- `volatile` (MMIO), which is not synchronization (`atomics-tls.xml:27`);
- the lifetime of memory after its owner frees it: as in single-threaded
  Mlx, use after free is undefined behavior (`memory-model.xml`). Threads
  add no new kind of it, because structured scopes bound every capture.

Debug runs can catch both kinds of mistake (§6.2):

- `MLX_THREAD_CHECK=1` turns races in unsafe or foreign code into faults
  that name the owning thread;
- poisoned resets turn wrong lifetimes into visible garbage.

---

## 3. Part 1: the four memory classes in the type system

### 3.1 How a class is written

```mlx
// LOCAL: the default. Nothing to write.
var buffer: [4096]u8 = undefined
const view: []u8 = buffer[0..]                 // a LOCAL address

// IMMUTABLE: an address to frozen memory.
const names: @immutable([]const u8) = "abc"    // literals and module consts are
const table: @immutable(*const [256]u8) = &LUT // immutable already (rodata / image)

// SHARED: storage several threads touch, through atomics or a lock only.
var hits: @shared(u64) = 0                     // &hits : @shared(*u64)
const jobs: std.sync.Mutex(std.ArrayList(Job)) = ...   // Mutex(T) is a @shared struct

// Critic: by this document's own rule this line is E7011. std.ArrayList
// has `allocator: Allocator`, whose `context: *anyopaque` is LOCAL
// (std/bootstrap/allocator.mlx:30-31). So ArrayList(Job) is LOCAL, and a
// @shared Mutex may not hold it. Also, `const jobs` cannot be locked:
// sema refuses a receiver-changing method on a const ("`bump` changes its
// receiver, which is a constant here", probed). See §C.2 CP3 and §C.8 A2.

// TRANSFERABLE: unique owners (non-copyable). Addresses inside are @owned.
pub fn Box(comptime T: type) -> type {
    return @noncopy(struct) {
        pointer: @owned(*T)
        allocator: std.mem.SharedAllocator
        fn deinit(self: *Self) -> void { ... }
    }
}
```

- **Type constructors.** These are new builtin type constructors, parsed
  like `@noncopy(T)`:
  - `@immutable(P)` for P a pointer, slice or many-pointer type (`const`
    is implied and normalized in);
  - `@owned(P)` for the same kinds of P;
  - `@shared(T)` for a place's type, or for a pointer type.

  They are part of the type's identity: new bits in `Type.flags`
  (`sema/types/kind.mlx:37-44` ends at `FLAG_LAYOUT_COMPLETE = 128`; add
  `FLAG_IMMUTABLE = 256`, `FLAG_SHARED = 512`, `FLAG_OWNED = 1024`).
  `@owned(P)` is non-copyable implicitly, because a copy would make two
  owners.
- **The computed class**, the class of the *type*, is derived and not part
  of its identity. `classOf(store, id)` returns one of `plain`, `local`,
  `immutable`, `shared` or `transferable`. The type store computes it on
  demand and memoizes it per type id, as it keeps layouts:
  - pointers decide by their qualifier;
  - arrays, tuples, optionals, error unions, unions and structs take the
    weakest member (`local` < `transferable` < `shared`/`immutable` <
    `plain`);
  - `fn` types are plain (code);
  - a non-copyable aggregate whose addresses are all
    owned/immutable/shared is `transferable`.

  Recursion stops at pointers, so recursive types terminate.
- **Inference.** Locals take their initializer's type, as today. Users
  never write *borrow* status: sema derives it (R4). Never-written globals
  count as immutable (whole-program, R5). Parameter non-escape comes from
  `escape.mlx`. Block capture modes are inferred (§5.3).
- **Allocators.** Today's `Allocator` (`std/bootstrap/allocator.mlx:30`)
  has a LOCAL `context: *anyopaque`, so a type that stores one is LOCAL.
  That is correct: an arena or a fixed buffer is not thread-safe. Owner
  types that should be transferable store `std.mem.SharedAllocator`
  instead: the same function pointers, with the context typed
  `@shared(*anyopaque)`. The process heap (`std.heap`, one lock) hands
  one out.

  **Critic:** so std.ArrayList (and every std container that keeps an
  `Allocator`) is LOCAL whatever its `pointer` is. L4's `@owned([*]T)`
  migration makes it no more transferable. L3's test expecting
  `ArrayList(u32)` and `ArrayList([]u8)` to differ fails: both are LOCAL.
  `std/bootstrap/allocator.mlx` is in the compiler's import closure, so
  its context cannot become `@shared` for two compiler generations. See
  §C.4.
- **Reflection.** `@threadClass(T)` returns an enum `.plain`, `.local`,
  `.immutable`, `.shared` or `.transferable`. `@isSendable(T)` is shorthand
  for "not `.local`". They sit beside `@isCopyable` in
  `sema/builtins/{kind,registry,analyze}.mlx`, so generic std code can say
  `comptime if !@isSendable(T) { @compileError(...) }`.

### 3.2 What each class permits

| Class (of the memory an address reaches) | Read | Write | Send to another thread | Share (several threads at once) |
|---|---|---|---|---|
| LOCAL (default) | yes | yes (unless `const`) | **no** (R2) | **no** |
| IMMUTABLE | yes | **no** (R6, E7009) | yes, by copying the view | yes |
| SHARED | through atomics or a lock's borrow | the same (R7, E7007) | yes, by copying the handle | yes |
| TRANSFERABLE (owned) | yes, by its current owner, through borrows | yes, the same | yes, by `@move` only (R3) | **no** |
| plain value (no address) | yes | yes | by copy | each thread has its own copy |

### 3.3 Conversions

| From → to | How | Checked |
|---|---|---|
| LOCAL → IMMUTABLE | (a) literals and addresses of module `const`s coerce implicitly; (b) `std.frozen.freeze(@move(x))` for a transferable x; (c) `std.frozen.copy(alloc, slice)` deep-copies plain data into a fresh read-only region | any other route: E7010 |
| IMMUTABLE → LOCAL read-only view | implicit: `@immutable([]const T)` → `[]const T`; the result is LOCAL, so it cannot cross back | — |
| IMMUTABLE → writable | never; copy it | E7009 / E7010 |
| LOCAL → TRANSFERABLE | allocate an owner and copy in (`Box.init(alloc, value)`), or assert uniqueness of raw memory: `unsafe { @ptrCast(@owned(*T), p) }` | E7010 outside unsafe (and E7004 for `@ptrCast`) |
| TRANSFERABLE → another thread | `@move` into a capture, a channel or `spawn` (R3) | E6001 on later use; E6008 while borrowed |
| TRANSFERABLE → IMMUTABLE | `std.frozen.freeze(@move(x))`: the owner is consumed and its memory is never writable again (debug: the pages are made read-only) | — |
| plain / TRANSFERABLE → SHARED | move into shared storage: `Mutex(T).init(@move(v))`, or `var g: @shared(u64) = 0` | the element type must have no LOCAL address (E7011) |
| SHARED → TRANSFERABLE | `mutex.into(@move(m))` consumes the mutex; only possible once it is no longer shared, because it is a moved owner | E6001/E6008 |
| SHARED field → value | `@atomicLoad(T, &s.f, order)`, or `const g = m.lock(); g.data().*.f` | E7007 for a direct access |

`store.isCoercible` (`sema/types/store.mlx:777-834`) gains exactly these
rules. `*T` → `*const T` stays as it is. An address whose qualifier would
be dropped or added outside these routes fails the coercion, and sema
reports E7010 (not E4001) when only the class differs, so the message can
name the route.

### 3.4 What is checked where

**Compile time, in sema**, inside the existing passes (`driver/pipeline.mlx:219-252`):

1. `parser`: class constructors in `parser/type.mlx` (next to
   `parseNoCopy` `:247`, dispatch `:344`).
2. `resolve`: `types/resolve.mlx:301` (the `nocopy_type` case) gets the
   class constructors; `declarations.mlx:632` `resolveType`.
3. `declarations` (`modules.analyzeDeclarations`, `pipeline.mlx:234`):
   `resolveAggregate` (`declarations.mlx:384-600`) checks, where it
   propagates copyability (`:540`), that a `@shared` or owner type holds
   no LOCAL address (**E7011**).
4. `function bodies` (`pipeline.mlx:244`; `sema/functions.mlx:64`,
   `control_flow.mlx:260` `analyzeStatement`, `declarations.mlx:1262`
   `expressionWithReturn`):
   - writes through `@immutable` (**E7009**; next to the mutability test
     `isMutable` `:150`);
   - direct field access or assignment of `@shared` places (**E7007**;
     the `field_expression` and assignment paths);
   - class coercions (**E7010**);
   - block literals and captures (§5, new `sema/blocks.mlx`);
   - boundary checks R2 on captures and block results (**E7006**);
   - borrow extents R4 (new `sema/borrows.mlx`; **E6007** for local
     stores and returns, **E6008** for a move while borrowed; hooked
     where `@move` marks the symbol, `:1323-1330`).
5. New, after lowering and before `optimize` (between `pipeline.mlx:274`,
   `lowerModules`, and `:296`, "optimize LIR"; a step "check thread
   safety"): `ir/thread_check.mlx`:
   - **E6007** for a borrow passed to a parameter that escapes (other
     than by return), using `escape.mlx`'s parameter facts;
   - **E7008** for a global reachable from a task;
   - **E7012** for a foreign call reachable from a task.

   These need the whole program, call graph and generic instances
   included, which only the LIR has. `lir.setSource` (`lir.mlx:423`)
   gives every instruction its module and offset, so the diagnostics
   point at source. The error codes are in phase `sema` in `codes.xml`.
6. The `unsafe` token scan (`sema/unsafe_boundary.mlx:15`) adds
   `@spawnThread` and `@arenaSwitch`. A call of an `unsafe fn` is checked
   in sema's call analysis (`declarations.mlx`, call path) with the
   symbol's new `FLAG_UNSAFE`. Both report **E7004**.

**Run time** (std.task, std.sync, the runtime, debug mode):

- join, cancellation and the scope protocol;
- drop flags for conditional moves (existing);
- the trap "spawn outside a scope";
- `MLX_THREAD_CHECK=1`:
  - frozen pages made read-only;
  - each thread's arena range registered, so std.crash names "an address
    in task X's arena";
  - the held-lock count checked at blocking points;
  - the lock-order graph;
  - poisoned per-task resets (std.turns' `poison`, `turns.mlx:213-221`).

**Out of scope, and marked:** see §2. The marker is always lexical:
`unsafe { }`, `unsafe fn`, `extern`, or a `threadsafe` extern attribute.

### 3.5 New diagnostic codes

| Code | Name | When |
|---|---|---|
| `MLX-E4016` | InvalidBlockUse | a block is stored, returned, compared or put in a field; a block literal is given where no block parameter is; `break`/`continue` would leave a block |
| `MLX-E6007` | BorrowEscapes | an address into owned memory (or into a block or scope parameter) is stored outside the block that took it, returned, or passed to a parameter that keeps it (the message names where it is stored) |
| `MLX-E6008` | MoveWhileBorrowed | `o` is moved, frozen or sent while a binding borrowing it is in scope, or in the same argument list |
| `MLX-E6009` | InvalidCapture | a task captures a `var` by reference, writes a captured name, or captures by reference a binding declared inside its scope |
| `MLX-E7006` | LocalAddressCrossesThread | a LOCAL address (or a borrow) in a task capture, a task result, a channel element or a shared store; the message gives the path, e.g. field `items: []u8` of `List` |
| `MLX-E7007` | UnsynchronizedSharedAccess | a `@shared` place read or written other than by an atomic or a guard. **Critic:** this includes copying an aggregate that contains one (`const snap = stats` reads `stats.hits` non-atomically, §C.2 CP5); simplest: a `@shared` place is non-copyable (A2) |
| `MLX-E7008` | TaskReachesUnsynchronizedGlobal | a task (`path:line`) reaches global `g` (written at `path:line`) along `a -> b -> c` |
| `MLX-E7009` | WriteThroughImmutable | a store through an `@immutable` address |
| `MLX-E7010` | InvalidClassConversion | LOCAL → immutable, owned or shared outside the routes of §3.3 |
| `MLX-E7011` | SharedHoldsLocalAddress | a `@shared` type, or an owner, holds a LOCAL address |
| `MLX-E7012` | ForeignCallInTask | a task reaches an `extern` function that is not `threadsafe`, outside `unsafe` |
| `MLX-W7001` | LargeTaskCapture | a capture copied into a task's context is over 4 KiB |
| `MLX-W7002` | TaskReachesGlobal | the staged form of E7008 (§8, step L7) |

Raw threads outside `unsafe` reuse **`MLX-E7004`**.

### 3.6 Spec and docs pages

**spec:**

- `spec/00-language/threads.xml` (new): `MlxMemoryClasses`, covering the
  classes, R1–R8, the conversions table and the boundaries.
- `ownership.xml`: owned pointers, borrows, and "a moved owner's borrows
  end".
- `types.xml`: class constructors and block types.
- `grammar.ebnf`: §5.1.
- `unsafe.xml` `<RequiredFor>`: raw thread builtins, `unsafe fn` calls,
  class casts.
- `atomics-tls.xml`:
  - the TLS ABI (§6.4), which closes `SPEC_CONFLICTS.md:59-68`;
  - `@arena` and `@arenaSwitch` (THREADS.md);
  - `@pause`;
  - atomics on `@shared` pointers.
- `memory-model.xml` `<UndefinedBehavior>`: "data race (impossible in code
  outside unsafe, raw threads and foreign calls)".
- `spec/02-compiler/diagnostics/codes.xml` plus the `Code` enum
  (`diagnostic.mlx:4-25`).
- `pipeline.xml`: the thread-check step.
- `lir.xml`: `tls_address`.
- `spec/04-stdlib/task.xml` (std.task, std.sync and std.frozen) replaces
  the unspecified `thread.xml` and closes `SPEC_CONFLICTS.md:70-77`.

**docs:**

- `docs/guide/13-threads.md` (new: the classes and tasks by example);
- `docs/guide/11-diagnostics.md` (rows);
- `docs/reference/memory-and-concurrency.md`: the threadlocal gap closes,
  and a table of classes;
- `docs/internals/compiler-pipeline.md`: the thread-check step.

---

## 4. Part 2: transfer of ownership without a large borrow checker

### 4.1 What Mlx already gives

- **Non-copyable values, and `@move` that marks the source Moved**
  (`symbol.mlx:84`). Every later use is E6001. A conditional move keeps a
  runtime drop flag (`lower.mlx:1925` `setBindingDropFlag`). This *is*
  transfer for the binding.
- **Value semantics.** Plain data copies, so sending it is copying it.
  Slices copy only `(pointer, length)` (`types.xml`), so the class of a
  slice is the class of its pointer.
- **Escape analysis** already knows, for every function parameter, whether
  what it points to leaves the call (`escape.mlx:940-975`). It is sound
  by construction: the frame placement it drives would crash otherwise.
- **What it does not give**: aliases made before a move stay usable
  (probe `alias.mlx`: `const p = &o.data; take(@move(o)); p.*[0] = 9`
  compiles and runs). Single-threaded this is a use-after-move of
  storage. Across threads it would be a race. That is the one gap the
  new rules close.

### 4.2 The smallest set of new rules

**R2 (crossing).** At each crossing point, the type must be sendable
(not LOCAL), or E7006. The crossing points are:

- a task capture;
- a task's result type;
- a channel's element type;
- a store into `@shared` storage;
- the `items` element type of `parallel_for`.

**R3 (transfer).** A transferable value crosses by `@move` (the existing
machinery). A non-copyable value used inside a task block is captured by
move (§5.3).

**R4 (borrows), only for memory reached through `@owned`, and for
block or scope parameters:**

- (a) **Roots.** These are borrows:
  - the value of an `@owned` field or binding, read as an address;
  - `&o` or `&o.field` of a binding whose type is transferable;
  - an address-typed block parameter;
  - the `*Scope` parameter of a scope block.

  Anything derived from a borrow is a borrow too:
  - field addresses, indexing, slicing, pointer arithmetic, casts;
  - addresses loaded through it;
  - the address-typed result of a call that got a borrow argument.
- (b) A borrow may be:
  - read and written through;
  - passed to a parameter that does not escape;
  - bound to a local declared in the same block as its root's use, or in
    an inner block. Such a local is a borrow binding.
- (c) A borrow may not be:
  - stored in any other place: an outer local, a field of a non-borrow
    place, a global, the heap;
  - returned;
  - captured by a task;
  - passed to a parameter that escapes other than by `ret` (E6007).
- (d) While a borrow binding of `o` is in scope (lexically), `o` may not
  be moved, frozen, sent or reassigned. The same holds within one
  argument list: `f(o.view(), @move(o))` is refused (E6008).
- (e) This follows from (a): the address of an owner binding is a borrow,
  so a pointer to the moved-from storage cannot outlive the move either.

**Critic:** (d) forbids move, freeze, send and reassign, but not
mutation. `const v = list.items(); _ = list.append(x); v[0] = 1` passes,
and `append` may free the block `v` points into. With one process heap
that block can be another thread's a moment later, so a single-threaded
use-after-free becomes a cross-thread write (§C.2 CP14). The lexical
cure (§C.8 A8 (d')): while a borrow binding of `o` is in scope, `o` is
used only through `*const` methods.

The rules are **lexical and intraprocedural**, plus one interprocedural
fact per parameter. There are no lifetime parameters, no annotations in
signatures and no flow-sensitive region inference. They also apply
**only to types that contain `@owned`**, which are new.

**Critic:** this contradicts R4(a), whose roots include "`&o` of a
binding whose type is transferable". A `Guard` (non-copyable, holding
only a `@shared` address) is transferable without any `@owned`.
- If R4 applies only to `@owned` types, `keep = g.data()` escapes the
  lock (§C.2 CP11), and the L8 fixture `e6007_guard_escapes.mlx` cannot
  fail as expected.
- If R4 applies to all transferable types, a Guard is also *sendable*,
  so a lock can be released on another thread.

Both need §C.8 A8/A9. Also, R4(c) forbids storing a borrow in a local
array, so `[64]Handle` cannot be filled (§C.2 CP10). So no existing
program changes meaning. Today's `@noncopy` containers (std.ArrayList,
`std/src/containers/array_list.mlx:19-47`, whose `items()` already says
"valid until the list grows") opt in by turning their `pointer: [*]T`
into `@owned([*]T)`. The compile errors that produces are the list of
places to look at.

**Interprocedural part.** Sema records, for each call node, which
arguments are borrows (a side table keyed by AST node). The lowering
copies the marks to the LIR call's argument slots (`lowerCallArguments`,
`lower.mlx:5721`). `thread_check.mlx` then asks `escape.mlx` for each
callee parameter's flags. This needs one new flag in `escape.mlx`,
`RETURNED`: set where `KIND_RETURN`/`KIND_RETURN_SLICE` mark
(`escape.mlx:829, 840`), so "escapes only through the result" is told
apart from "stored". It also needs an entry point
`escape.parameterFacts(ir)` that runs `prepare`/`solve` without `move`.

Indirect calls count as escaping unless the function type is a block
type, whose parameters are checked at the literal.

**Within a task, R4 is not even needed.** Every exit from a task is
guarded by R2 and R5, so a LOCAL pointer inside it has nowhere to go that
another thread can see. R4 is the price of three things:

- zero-copy transfer of heap data;
- freezing without a copy;
- locks that hand out direct access.

### 4.3 How a transferable value leaves the sender (moved-from state)

The source binding becomes `State.moved` (`symbol.mlx:5`). Its drop flag
is cleared, so its `deinit` does not run in the sender, and the
destination takes the obligation (`ownership.xml` `<AutomaticDeinitialization>`).
No borrow of it can be live (R4(d)), and no pointer to its storage
survives (R4(e)).

Defense in depth costs one store per owned word: with runtime safety on,
`@move` of a value whose type contains `@owned` fields also zeroes those
fields in the source storage. A stale read through unsafe code then sees
null and traps on use, instead of racing. This does not run user code,
so the spec's "@move invokes no user code" holds.

**Critic:** runtime safety is on by default
(`driver/options.mlx`: `.runtimeSafety = true`; `--safety=off` turns it
off). After L4 puts `@owned` into std.ArrayList, this zeroing changes
the code of every single-threaded program that moves a list. That
includes the desktop, the compositor, Nebula and Observatory, which are
on §7.3's byte-identity list, so `check_threads_identity.sh` fails at L4.
Emit it only in programs whose LIR has a thread entry (`clone_thread`,
`arena_switch`, a task block), or not at all (§C.6).

### 4.4 How the arena memory travels: it does not

The value arena is the bump region of one thread, with no per-value
ownership. Handing pages out of its middle would split it. So:

- **Arena addresses are LOCAL by definition and never cross (R2).** That
  is what makes the per-task reset sound (§2, §7).
- **Plain results travel by copy, with no copy at join.** A task block
  returning a struct, union or array uses the existing result-slot
  convention (`FLAG_RETURN_SLOT`, `lir.mlx:153-164`). The worker calls the
  block function with the *task record's* result storage as the slot.
  The task record is in the parent-owned scope region, so the result is
  built where the parent reads it.
- **Owned heap blocks travel by moving the pointer.** `std.heap` is one
  process heap with one lock (`std/src/heap.mlx:1-25`), so a block freed
  by the receiver goes back correctly.
- **Bulk data travels by moving a `std.Region`.** A region is a chunk list
  of pages from the page allocator: the explicit "arena pages handed
  over". Values inside it link to each other by **offsets** (`Region.Ref(T)`,
  plain integers), never by addresses. So a region is sendable by
  construction and relocatable. On brixOS a region can be a
  `memory_object` (native call 18) and be sent over a native channel
  with transfer rights, to another *process* too, at no copy.
  **Critic:** two conditions are missing.
  - "Sendable by construction" holds only if `Region.alloc(T)` requires
    `@threadClass(T) == .plain` (with `Ref` plain). Otherwise a region
    holds a LOCAL `[]u8` and crosses.
  - A mapped `memory_object` page stays mapped in the sender after the
    handle is sent (`brix.xml:103`: "they outlive the handle while
    mapped"), so both processes write it. Moving a region to another
    process must unmap the sender's view, at the cost of an unmap
    (§C.2 CP12).
- **The context of a task** (its captures) is copied into the scope's own
  region at spawn (§5.4). The spawner's arena is never read by the child.

### 4.5 Keeping pointers into a thread's LOCAL arena from crossing

- **By type.** Every unqualified address is LOCAL, and R2 checks every
  crossing point.
- **No laundering in safe code.**
  - `@intFromPtr` gives a plain integer, but making an address of it is
    `@ptrFromInt`, which needs `unsafe`.
  - Qualifying an address is a `@ptrCast`, which needs `unsafe`.
  - Constructing an `@owned` from a raw address is `unsafe`.
- **Globals.** R5 also flags task-side reads of globals of a LOCAL-class
  type, including `const` ones: a module `const p: *u32 = &v` is a LOCAL
  address every thread could load. It also flags task-side stores of a
  LOCAL address into a `threadlocal`, which survives the task on its
  worker and would dangle after the reset.
- **Run time** (`MLX_THREAD_CHECK=1`):
  - std.task registers each worker's arena range;
  - the join, channel and shared-store paths get a compiler-generated,
    type-directed check that no address word of the value lies in another
    thread's arena (the compiler knows which words are addresses);
  - std.crash names "address in task T's arena (worker W)" for a fault in
    another thread's range.

---

## 5. Part 3: task blocks

### 5.1 Syntax and grammar (parser, AST)

```mlx
const task = std.task

fn render(image_a: Image, image_b: Image) -> !Picture {
    return task.scope |s| {
        const a = s.spawn { process_image(image_a) }       // a, b: Handle(!Pixels)
        const b = s.spawn { process_image(image_b) }
        return combine(try a.join(), try b.join())
    }
}

fn blur(tiles: []TileJob, radius: u32) -> void {
    task.parallel_for(tiles, |job| { apply_filter(job, radius) })   // job: *TileJob
}
```

The owner's sketch, `task.scope { task.spawn { ... } }`, is accepted as
sugar. In a scope block written without parameters, an unqualified
`task.spawn`/`task.parallel_for` binds to the innermost scope (a rewrite
in sema, keyed by the `@Scope` parameter type).

**Critic:** these examples do not type-check under §5.2's "a block
literal is typed against its expected type":
- `task.scope |s| { ... }` leaves `scope`'s `comptime R` unbound (the
  runtime design writes `task.scope(E!Combined, |s| ...)`);
- `s.spawn { process_image(image_a) }` must yield `Handle(R)` with R known
  only from the block's body.

Mlx infers `anytype` from an argument's type (`comptime.xml:3`), and a
block literal has none until it is typed. Either R is written everywhere,
or sema gains a rule: "a block given to a parameter whose return type is
a generic parameter infers it from its `return`s". That rule is not in
§5.8's estimates.

**Critic:** the trailing form with parameters, `x.f(a) |s| { ... }`,
sits where `|` is the bitwise-or operator (`expression.mlx:507`; `||` is
logical-or, `:504`). Today `f(a) | s | {` fails only at `{`. So the
parser needs the lookahead `| IDENT {, IDENT} | {` before it treats `|`
as a block, and the head flag must also cover the `if x |v| {` and
`while it.next() |v| {` captures (`syntax.mlx:176`).

**Critic:** the sugar must stay lexical. An unqualified `task.spawn`
outside a scope literal must be **E4016 at compile time**, not §3.4's
run-time trap "spawn outside a scope" that resolves through the Block's
current scope. A dynamic `task.spawn` returns a `Handle` that is derived
from no borrow, so it escapes R4. Inside a task it would spawn into the
*parent's* scope, so joins no longer form a tree (§C.2 CP8).

Grammar additions (`spec/00-language/grammar.ebnf`):

```ebnf
primary        += block_literal ;
block_literal   = ( "|", block_param, { ",", block_param }, "|" | "||" ), block ;
block_param     = [ "*" ], IDENT, [ ":", type_expr ] ;
postfix_suffix += trailing_block ;          (* only where the parser allows it, see below *)
trailing_block  = [ "|", block_param, { ",", block_param }, "|" ], block ;
type_expr      += class_type | block_type ;
class_type      = ( "@immutable" | "@shared" | "@owned" ), "(", type_expr, ")" ;
block_type      = ( "@Block" | "@Task" | "@Scope" ), "(", fn_type, ")" ;
decl_modifier  += "unsafe" ;                (* unsafe fn: its callers need unsafe *)
```

**Why each form is free:**

- `|` and `||` cannot start a primary today (`parsePrimary`,
  `expression.mlx:358-382`; probe `blk.mlx` gives E2001).
- `expr {` on one line **does** parse today, as two statements (probe
  `trail.mlx`; `parseBlock` `syntax.mlx:344-374` needs no terminator
  between statements). A heuristic scan of all `.mlx` sources found no
  such line outside string literals.

  So the trailing block is accepted in `parsePostfix`
  (`expression.mlx:437-499`), after a call or a field, only when a parser
  flag allows it. `parseIf`, `parseWhile`, `parseFor` and `parseMatch`
  (`syntax.mlx:173, 192, 210, 240`) clear the flag while they parse
  their heads, so `if ready {`, `while x {`, `for t in list {` and
  `match k {` keep their meaning. Swift uses the same rule.

  A trailing block given to a function that takes none is **E4016**,
  where today it silently ran as a bare block.
- A `{` on the next line never attaches: newline filtering gives a
  STATEMENT_END first.

**AST** (`ast/node.mlx:3-63`), new tags:

- `block_literal`: lhs = the first parameter `capture` node (the existing
  `capture` tag, as `parseFor` makes them, `syntax.mlx:210-229`),
  rhs = the body block, extra = count | flags (trailing, has_params);
- `class_type`: lhs = the child type, main token = the builtin name;
- `block_type`: lhs = the fn type, main token = the builtin name.

**Size:**

| Component | Lines |
|---|---|
| parser (`parseBlockLiteral`, trailing block and flag; `unsafe fn`; class and block types next to `parseNoCopy`) | about 150 |
| AST | about 10 |
| lexer | none; every token exists |

### 5.2 Typing and the second-class rule

A block literal is typed **against its expected type**. A literal
anywhere else is E4016:

- the parameter's block type gives the parameter types (annotations are
  optional, as in `|tile: *Tile|`) and the return type R;
- the body is analyzed in a child scope with a function boundary marker;
- `return` in a block returns from the block;
- `try` propagates out of the block, so R must be an error union;
- `break`/`continue` cannot cross the boundary (E4016).

Three block kinds, by type:

| Type | Runs | Captures | May be passed to |
|---|---|---|---|
| `@Block(fn(A) -> R)` | on the caller's thread, during the call | by reference, any class; may write captured `var`s | calls; another `@Block` parameter |
| `@Scope(fn(*Scope) -> R)` | on the caller's thread; the callee joins every task spawned before it returns | as `@Block` | the same |
| `@Task(fn(A) -> R)` | on any thread, concurrently | §5.3 (R2, R3, E6009) | `std.task` primitives only (their parameters are `@Task`) |

**Inside a callee, a block parameter is second-class.** It can be called
(`body(x)`), passed to a parameter of the same kind, or (for `@Task`)
handed to the runtime's submission. That last path is std code written
with `unsafe`: it copies the context bytes (§5.4). Storing, returning,
comparing or putting a block in a field is **E4016**.

The type store gets one new `Kind.block`, with `lhs` = the fn type and
the flags giving the kind (`types/kind.mlx:1-29`). Its representation is
two words, like a slice.

**Critic:** `classOf` is not defined for `Kind.block`, and E4016's list
(stored, returned, compared, field) omits **captured**. If a block is
classed like a `fn` type (plain), §5.3 row 1 lets a `@Task` capture a
`@Block` parameter by reference. Two workers then run a block that writes
the caller's `var` by reference (§C.2 CP2). A block's class must be LOCAL
(its context is an address), and capturing a block in any block literal
must be E4016 (§C.8 A4). Also, a `@Task` that captured by move is a
*once* value: calling it twice, or handing it to two `spawn`s, runs
`appendAutoDrop` twice (§5.4.3). So the kind needs linear use
(§C.8 A5).

**Sema size:** `sema/blocks.mlx`, about 650 lines. It types literals,
discovers captures (names that resolve across the boundary marker, using
`symbols/scope.mlx`'s parent chain), assigns modes and checks the
second-class rule.

### 5.3 What a block may capture, and how

Mode of each captured name, decided by sema:

| Captured binding | `@Block` / `@Scope` | `@Task` |
|---|---|---|
| `const` of a **plain or immutable or shared** class, declared **outside the innermost enclosing `@Scope` literal** | by reference | **by reference** (no copy; it outlives the scope, and nobody can write it) |
| `const` of such a class, declared inside that scope (or with no enclosing `@Scope` in this function) | by reference | **by copy** into the context |
| `var` of a plain/sendable class | by reference (may write) | **by copy**, read-only inside (a write is E6009); large captures warn W7001 |
| non-copyable value | by reference (a borrow) | **by move**: the source becomes Moved at the literal (E6001 after) |
| LOCAL address or a borrow | by reference | **E7006** |
| `*Scope` parameter | by reference | E7006 (a task cannot spawn into its parent's scope, so joins form a tree) |

Two lifetime facts make the by-reference row sound:

- a binding declared lexically outside a `@Scope` literal lives in a frame
  that is active for the whole scope call (blocks are second-class, so
  lexical nesting is dynamic nesting);
- the scope joins every task before it returns.

A task block's **result type R must be sendable** (E7006 at the return
type).

**Critic:** the table's rows overlap and give no precedence.
- A `const m: Mutex(T)` is both "shared class" (row 1, by reference) and
  "non-copyable" (row 4, by move). Under row 4 a second `spawn` using `m`
  is E6001, so tasks cannot share a local mutex. Under row 1 the scope
  body may `@move(m)` while tasks lock it: a probe confirms that a const
  non-copyable binding can be moved today.
- Even by reference, `m.lock()` on a const receiver is refused today
  ("changes its receiver, which is a constant here"). The design needs
  interior mutability for `@shared` places and does not say so (§C.2 CP3).
- Row 1 keys by-reference capture on *lexical position* relative to any
  `@Scope` literal, not on whether the primitive receiving the block is
  joined by that scope. A safe thread (`std.thread.spawn` with a block,
  SMART-RUNTIME §3.1) or `task.service`, written inside a scope literal,
  would capture by reference and outlive the frame (§C.2 CP9).
- Row 4 (by move) is wrong for `parallel_for`. Its one block function
  runs once per item, concurrently, over one context, so a moved capture
  is dropped N times concurrently (§C.2 CP1).

§C.8 A2, A5 and A6 give a consistent table.

### 5.4 Lowering: a function plus a context record

The new file `ir/lowering/blocks.mlx`, about 600 lines, plus the dispatch
in `lower.mlx`, about 120.

1. **Context layout.** Sema synthesizes a struct type with a first word
   `size: usize`, then one field per capture:
   - by reference: `*T` (`*const T` for const);
   - by copy or by move: `T`.
2. **At the literal**, in the enclosing function:
   - allocate the context as a region of the frame (`alloca` with arg1 = 1,
     `lir.mlx:36-44`); it cannot outlive the call because blocks are
     second-class;
   - store the captures;
   - for each move, call `setBindingDropFlag(source, 0)` (`lower.mlx:1925`):
     the obligation moves into the context.

   The block value is `(func_sym of the block function, context address)`.
3. **The block function** `__block` is queued and lowered later, as
   generic instances are (`generateGenericInstances`, `lower.mlx:1585-1610`,
   which saves and restores the builder's module, scope and owner).
   - Its parameters are `(context: *Context, declared params...)`.
   - Captures are bound with `pushBinding` (`lower.mlx:1656`) to
     addresses computed from the context: by reference, the loaded
     pointer; by copy or move, the field's address.
   - Moved captures get `appendAutoDrop` (`lower.mlx:1867`), so the block
     function drops them at its end on every path.
   - Its symbol gets `lir.setSymbolSource` with the literal's position,
     and is named `path:line:block` / `path:line:task` / `path:line:scope`
     (`object/symbol_table.mlx` already names functions `path:line:name`).
     Crash reports, `mlx-profile` and `mlx-codemap frames` then show the
     task by place.
4. **A call of a block parameter** is an indirect `call` through word 0,
   with word 1 as a hidden first argument.

   At `-O2` the inliner (`ir/inline.mlx`) sees the direct `func_sym` once
   the higher-order callee is inlined. A sequential `@Block`, such as
   `list.forEach(|x| ...)`, then costs nothing.
5. **Tasks.** `s.spawn(body)` is std code. It copies `context.size` bytes
   (`body` word 1) into a **task record in the scope's region**, a chunk
   from the page allocator that the Scope owns. That bitwise copy is the
   one place a non-copyable capture is moved by the runtime. The frame
   copy is never touched again, and the compiler emits no drop for it.

   Two more functions are generated per task block:
   - `__drop` runs the moved captures' drops when a task is cancelled
     before it starts;
   - `__result_drop` drops an unjoined non-copyable result.

   The record keeps both pointers, so cleanup is guaranteed even for a
   task that never ran.

   Each worker calls `fn(record.context, ...)` with `record.result` as its
   result slot (§4.4).

### 5.5 Structured scopes: how `task.scope` ends

`pub fn scope(comptime R: type, body: @Scope(fn(*Scope) -> R)) -> R` in
`std.task` (the error set is part of R):

1. It makes the Scope:
   - the region;
   - the cancel flag (a word watched by `memory_wait`);
   - the task list.

   It puts the Scope into the thread's Block (THREADS.md §6.2: a Block in
   the arena header, found from `@arena().0` at no cost).
2. It runs `body(&scope)` on the calling thread.
3. **On normal return:** it joins every task not yet joined. If one of
   them failed, the scope's result is the first such error, and later
   errors are recorded for Observatory.
4. **When the body returns an error** (a `try` failed, or a joined task's
   error was propagated), it sets the cancel flag, then joins every task.
   The tasks stop at their next checkpoint. The body's error is returned.
5. **Fail fast:** a task that ends with an error sets the cancel flag, so
   its siblings stop early. `task.scopeAll` keeps them running and
   collects every error.
6. **Cleanup:**
   - every task block's defers and drops run on its own exit paths, as
     in any Mlx function;
   - unstarted tasks run `__drop`;
   - unjoined results run `__result_drop`;
   - the region is given back.
7. **Critic:** this protocol relies on the callee of a `@Scope` block
   joining, which the compiler cannot check. It is sound only if
   `Scope` has **no public constructor**, so that `task.scope` alone
   makes one, in its own frame, and hands out only `*Scope`. A
   user-built Scope can be moved into a global or heap list and never
   deinit'd. Mlx does not guarantee that a non-copyable value's `deinit`
   runs, which is Rust's scoped-thread lesson. Its tasks would then keep
   by-reference captures of a dead frame. State it (§C.8 A7).

   **Critic:** the "no join deadlock" argument needs one more premise:
   **join must help** (run queued tasks) or the pool must grow.
   Otherwise nested scopes deadlock a fixed pool, with every worker
   blocked in a join while the children sit queued. SMART-RUNTIME §2.5
   has helping, but L6 below says "a simple pool". Channels and locks
   are not covered by the tree argument (§C.2 CP13).

   **Handles cannot escape.** A `Handle(R)` is the result of
   `s.spawn(...)`, a call with the borrow `s`, so it is a borrow (R4(a)).
   - It cannot be returned from the body, stored outside it or captured
     by a task (E6007, E7006).
   - Joining after the scope ended is impossible, and so is a thread leak.
   - Only the spawning code joins, so joins form a tree: **no join cycle,
     hence no join deadlock**.
8. **Panics and traps do not unwind** (`ownership.xml`). A trap in a task
   ends the process with std.crash's report, naming the task's
   `path:line:task` and the worker. This is unchanged Mlx semantics, and
   is documented.

### 5.6 Join returns a value or an error

`Handle(R).join(self) -> R`, with R = `E!T` typically, so
`const v = try a.join()`. Error sets are plain, so `E!T` is sendable
exactly when T is. A task cancelled before it produced a result yields
`error.Cancelled`, which is part of every task result's error set: std
adds it with an error set merge in `spawn`'s signature. Mlx has no
`catch`. The parent uses `try`, or the error capture
(`if a.join() |v| { ... } else |err| { ... }`, the existing `if_stmt`
form).

### 5.7 Cancellation: cooperative, with check points

- `try task.checkpoint()` returns `error.Cancelled` once the scope is
  cancelled. It is one acquire load of the flag through the Block.
- Implicit check points:
  - between `parallel_for` chunks;
  - in every std blocking call that a task makes (join, channel
    receive/send, `Mutex.lock`, sleep). On brixOS they wait with the
    native `wait`, with **the scope's cancel word** as an extra item
    (`memory_wait` on Linux), so cancellation wakes them.
    **Critic:** no such wait item exists. Native `wait` takes handles
    (`brix.xml:77,87`), and THREADS.md's calls 86–92 add `memory_wait` as
    a call of its own. On Linux the call is futex, not `memory_wait`.
    SMART-RUNTIME §2.5 needs new calls for this (94 alert, 99
    `memory_handle`), so §9's "nothing else is needed from the kernel" is
    wrong. Without new calls the native route is a handle: a per-scope
    channel pair whose readable end is the extra wait item, and cancel
    writes one message;
  - before a queued task starts, when `__drop` runs instead.
- **Cleanup is the normal error path.** Defers and auto-drops run, as for
  any `try`. The compiler inserts nothing. An optional later mode,
  `--task-poll-loops`, would put a checkpoint at the loop back-edges of
  task block functions. It is not the default: it changes code.

### 5.8 Sizes (all of part 3)

| Component | New or changed lines (estimate) |
|---|---|
| parser and AST | 160 |
| `sema/blocks.mlx` (literals, captures, modes, second-class), with the hooks in `declarations.mlx` and `control_flow.mlx` | 750 |
| type store: `Kind.block`, `classOf`, coercions | 250 |
| `ir/lowering/blocks.mlx` and the `lower.mlx` dispatch | 720 |
| LIR | none: `func_sym`, `call`, `direct_call`, two-word values and `FLAG_RETURN_SLOT` all exist |
| `std/src/task.mlx` (scope, spawn, join, parallel_for, checkpoint, Channel; scheduler hooks) | about 1200 (the scheduler part is the scheduler's) |

---

## 6. Part 4: raw threads in the language

### 6.1 The raw boundary

The raw boundary is `unsafe` (R8):

- `@spawnThread` (Linux clone; the Linux-compat adapter, as today) and
  `@arenaSwitch` (THREADS.md) are added to the lexical scan in
  `unsafe_boundary.mlx`. Outside `unsafe` they are **E7004**.
- **`unsafe fn` (new decl modifier).** A function that hands out raw
  power (THREADS.md's `std.thread.spawn(entry, context)`, a raw mutex, or
  an "assume this memory is exclusive" constructor) is declared
  `unsafe fn`. Calling it needs `unsafe { }` (E7004). The implementation
  is the symbol flag `FLAG_UNSAFE` (`symbol.mlx:13-28`), set in
  `collect.mlx` from the modifier and checked in sema's call path. It is
  about 40 lines.

  Without it, safe code could start a raw thread whose body touches
  globals, and R5 would never see it.
- **Critic:** an `unsafe fn` also needs its own function-pointer type.
  Otherwise `const start: fn(Entry, usize) -> usize = std.thread.spawnRaw;
  _ = start(entry, 0)` calls it with no `unsafe` anywhere (§C.2 CP17).
  Coercing an `unsafe fn` to a plain `fn` type must be E7004. sema also
  has no notion of "inside unsafe" today: the boundary is a token scan,
  run before declaration analysis (`unsafe_boundary.mlx:15-49`,
  `pipeline.mlx:227-232`). So the call check and R5's unsafe exemption
  in `thread_check.mlx` both need the unsafe ranges carried into sema and
  the LIR.
- **Critic:** natively first: on `--target=x86_64-brixos`, `@spawnThread`
  still issues Linux's clone (syscall 56, `codegen/memory.mlx:140-171`)
  and so goes through the compatibility layer. Our own brixOS programs
  should get E7004/E9001 for it there, or have it lowered to
  `thread_create`, so they cannot slip onto the Linux layer.
- **Migration:** the raw call sites (six, not five; **Critic**) wrap
  their spawn in `unsafe`:
  - `std/src/h264/decode.mlx:5081`;
  - `std/src/h265/decode.mlx:4241`;
  - `projects/desktop/splash/bands.mlx:84`;
  - `compiler/libc/pthread.mlx:294`;
  - tests 298 and 315.

  Their entries already recover their context with `@ptrFromInt` in
  `unsafe`, so the boundary is visible at both ends.

**Raw spawn with a function pointer and a context.** THREADS.md's
`std.thread.spawn(entry: fn(usize) -> usize, context: usize)`, as an
`unsafe fn`:

- natively, `thread_create` (86) with a start record, the new thread
  inheriting r14/r15 and then `@arenaSwitch` to its own arena (THREADS.md
  §2.4);
- on Linux, `@spawnThread`.

The context is a word, so the entry turns it back into an address with
`@ptrFromInt` in `unsafe`. The raw contract (lifetime, races) is the
programmer's, and it is visible as `unsafe` at the spawn and in the
entry. mlxlibc's pthreads live here, natively on brixOS per THREADS.md
step 9.

Scheduling control for raw threads goes through native calls and is not
language:

- affinity, priority and processor choice are the scheduler part's (a
  `ThreadStart` record field or later calls);
- the language adds nothing for it.

### 6.2 Atomics and memory orderings

What exists and stays (spec `atomics-tls.xml:3-17`, all on x86_64 and
aarch64):

- six atomic builtins;
- C11's six orders and their combination rules;
- `@fence`.

Needed:

- **Atomics on `@shared(*T)` operands**, besides LOCAL `*T`. This is the
  sanctioned access path of R7, a change of the operand check in
  `sema/builtins/atomic_arguments.mlx`.
  **Critic:** two more rules.
  - The spec's AtomicType includes pointers and optional pointers
    (`atomics-tls.xml`). So `@atomicLoad` of a `@shared(?@owned(*T))` slot
    copies an owner: two tasks load it and both own it (§C.2 CP6). On a
    type with an `@owned` word, only `xchg` and `cmpxchg` (which move the
    old value out) may be allowed.
  - The atomics must be able to write a `@shared` place through a
    `*const` path (interior mutability). A const `Mutex` captured by
    reference can be locked only that way (§C.8 A2).
- **`@pause()`**, a spin-wait hint: x86 `pause`, aarch64 `yield`. The
  work-stealing loops and std.sync's spin-then-wait need it. It is a new
  LIR opcode with one instruction per backend.
- Optional, later: a 16-byte compare-exchange (`cmpxchg16b`, `CASP`) for
  ABA-tagged multi-producer queues. A Chase–Lev deque needs only 8-byte
  CAS plus a `seq_cst` fence, which exist.
- **The happens-before edges are in the runtime, not in the compiler.**
  Spawn publishes the context with release semantics, the worker takes
  it with acquire, and completion and join pair the same way. Channels
  and Mutex do too. This matters on aarch64. The compiler inserts no
  fences for classes.

### 6.3 Data races in raw code

Raw code keeps today's rule: a non-atomic conflicting access is
undefined behavior (`atomics-tls.xml:28`). Debug mode (`MLX_THREAD_CHECK=1`)
adds the arena-range attribution and the held-lock and lock-order
diagnostics. It is reported to Observatory.

### 6.4 TLS: `threadlocal`, Mlx-native and arena-relative

Today `threadlocal` is E9001 (`lower.mlx:580-592`; SPEC_CONFLICTS says no
ABI was specified). THREADS.md keeps it so and offers `Local` words in
its Block. This design gives it a normative, native ABI that needs no FS
and no C library:

- **Place.** A thread's thread-local area lies **directly above its arena's
  end**: `[r15, r15 + TLS_SIZE)`. r15 stays the allocation limit
  (`cmp rdx, r15; jbe`), so values never reach the area. The compiler
  assigns each `threadlocal var` an offset in the program's TLS image,
  which also gives the initial bytes. Initializers are comptime, as the
  spec already requires.
- **Access.** A new LIR `tls_address` (arg0 = the offset) is emitted as
  `lea rax, [r15 + off]`. A load or store right after it fuses into
  `[r15 + off]`, as `emitGep` already fuses
  (`codegen/memory.mlx:176-200`). One instruction, as fast as
  FS-relative TLS.
- **Setup.**
  - **Main thread:** in programs that declare a `threadlocal` (and only
    those), `_start` reserves 1 GiB + 8 + TLS_SIZE instead of
    1 GiB + 8. r15 is still base + 1 GiB + 8, and `_start` copies the
    image to [r15].
  - **Threads:** std.thread maps its arenas with the same extra space
    and copies the image from a compiler-defined data symbol
    (`__mlx_tls_image`, `__mlx_tls_size`, read through a new builtin
    `@tlsImage() -> []const u8`).
  - Everything else is unchanged.
- **Where it is not available** (E9001 stays):
  - `--plugin` and `--stack-arena` builds, whose exports run on per-call
    arenas;
  - `--shared` (two images' offsets would collide);
  - aarch64, until it has per-thread arenas (§7.4).
- **Critic: missing cases.**
  - **Freestanding** (the kernel): `emitFreestandingStart` takes r14/r15
    from the loader, and "the stack is its too" (`emitter.mlx:166-186`).
    So `[r15, r15 + TLS)` lies past the region the loader gave. The kernel,
    whose processors another engineer is starting now, is the first
    natural user of per-CPU `threadlocal`. Freestanding must be E9001
    until per-CPU arenas are defined.
  - **Raw threads that never switch arenas.** `@spawnThread` children
    share r14/r15 (§1), so they share one TLS area. That is h264/h265,
    splash and mlxlibc today, and L2's own fixture.
  - **Exports entered from C** reload DATA_ARENA, so their
    `threadlocal`s are the main thread's on every C thread.

  `threadlocal` is per *arena*, not per thread; the spec text must say
  so. The layout must also match SMART-RUNTIME §3.1, whose one mapping
  per worker ends with the arena (`[guard][stack][arena]`), which leaves
  no room above r15.
- **Why not FS:**
  - std threads leave FS alone (THREADS.md §6.2/§6.7: "no SETTLS ... std
    never reads it"), so FS-relative TLS would alias the main thread's
    block;
  - linked programs' FS belongs to the C library.

  The arena registers are already per thread and reserved. FS stays the
  **C-compat** thread pointer (`@threadPointer`, mlxlibc's TCB).
- **Semantics for tasks.** A `threadlocal` is per *worker thread*, not
  per task. In task-reachable code R5 accepts reads and writes of plain
  or sendable thread-locals, and refuses storing LOCAL addresses into
  them, because the per-task reset would leave them dangling (E7008).
  Task-local storage, if wanted, is a std.task API keyed by the task
  record.

---

## 7. Part 5: the per-thread value arena from the compiler's side

### 7.1 How emitted code reaches the arena today

On x86_64, through **registers**:

- r14 is the base (the fill word is at `[r14]`), and r15 is the end;
- both are set once in `_start` and reserved for the whole program
  (§1 table);
- an allocation is 8 instructions with one `lock xadd` (about 20 cycles
  uncontended).

Shared arenas, as with today's raw workers, pay a cross-core RFO on that
line on nearly every function (THREADS.md §2.1: roughly 100–300 cycles).

There is **no FS-relative access anywhere** on the allocation path. The
only memory-addressed copy is DATA_ARENA, for exports of linked programs
(`labels.mlx:81-87`).

On aarch64, through a **global** (`__mlx_arena_base`) and a helper call
with an LDXR/STXR loop.

### 7.2 Per-thread arenas

This follows THREADS.md §2, which the compiler part agrees with:

- **Each thread has its own arena of the same shape.** r14 and r15 hold
  it. The thread starts with its parent's registers (the native
  `thread_create` and Linux clone both copy them) and calls
  `@arenaSwitch(base, end)` first thing. The emitted allocation code is
  **unchanged**. Only the register's value differs.
- **Cost:**
  - per allocation: the same 8 instructions, now on a cache line only
    this core writes (the contention disappears);
  - per thread: one reservation (one region on brixOS) and one touched
    page;
  - `@arena()`, which gives thread identity through the Block in the
    arena header: one `mov`.
- **Dropping the `lock` prefix** (about 18 cycles back per allocation) is
  possible once an arena is private. An unlocked `xadd [r14]` is still
  atomic against a signal or an upcall on the same thread. It is **not**
  part of this work, because:
  - it changes single-threaded programs' code;
  - linked programs' exports may run on C threads that share the main
    arena.

  It is listed as an independent follow-up, behind a whole-program test:
  no `clone_thread` or `arena_switch` in the LIR, no export, no plugin.
- **What the compiler adds** (the safe model):
  - **Per-task reset.** std.task's worker runs each task between
    `turns.beginAt(&t, @arena().0, poison)` and `turns.turn(&t)`
    (`turns.mlx:172, 206`). THREADS.md's Pool does this per batch, under
    a documented rule ("results go into heap memory or the context,
    never in a task's values"). With R2 the rule is **enforced by the
    compiler**: a task's values are LOCAL, and nothing LOCAL leaves it.
  - **Results and contexts live outside the worker's arena** (§4.4).
  - **Foreign callbacks.** An export entered from C on a worker thread
    reloads the *main* arena (DATA_ARENA). That is atomic and correct,
    but the main thread's turns could release those values. Since a task
    calling C is outside the guarantee (E7012 unless `unsafe` or
    `threadsafe`), this stays a documented limit. A later fix: in
    programs that use `@arenaSwitch`, exports keep r14/r15 when the
    Block at `r14 + 8` carries the thread magic. That is one compare.
    **Critic:** that fix is unsound. On entry from C, r14 holds whatever
    the C caller kept in a callee-saved register, so `[r14 + 8]` can
    fault or match by chance. A sound lookup needs no register: for
    example, align worker stacks so that `rsp & ~(STACK_ALIGN - 1)` finds
    the Block, or make one native `thread_self` call.
  - **Critic: per allocation.** The fill word is at `base + 0` and the
    Block at `base + 8`, on the same cache line. The Block holds fields
    other threads touch: the joiner's exit word, the scope's cancel word
    (SMART-RUNTIME §3.1), and the current job, which thieves and the
    Observatory sampler read. Then every allocation's `lock xadd` misses
    again whenever another core touches those fields, which contradicts
    "a cache line only this core writes" above. Give the fill word its own
    line and put cross-thread fields on separate lines (§C.3).

### 7.3 What the start code and std.turns must do

- **`_start`:** byte-identical, unless the program declares a
  `threadlocal`. Then it reserves the TLS area and copies the image
  (§6.4).
- **Thread start:**
  - the first function, std.thread's `threadMain`, allocates nothing
    before `@arenaSwitch` (THREADS.md §2.4);
  - the compiler makes `@arenaSwitch` a barrier (no `alloca` moved
    across it) and never inlines a function that contains it
    (`ir/optimize.mlx`, `ir/inline.mlx`);
  - for TLS it copies the image above the new r15.
- **std.turns and std.crash** stay buildable by mlx0 and the previous
  canonical compiler (`turns.mlx:11-13`; `tools/check_bootstrap.sh`).
  They therefore learn about threads through **hooks** that std.thread
  sets (`turns.threadArena`, crash's `contextSlot`; THREADS.md §2.7,
  §2.9), not through the builtins.
  - `located` keeps caching only the main arena.
  - `releasedTurns` becomes an atomic add. **Critic:** that changes the
    code of every turns user, including the compositor, the dock, the
    launcher and the desktop on the identity list. Count per arena in the
    Block instead, or exempt std.turns from the identity check
    explicitly.
  - **Critic:** `turns.turn`/`beginAt` are safe pub functions that free
    arena memory (`beginAt` takes any `base`). Two cases break the
    guarantee:
    - inside a scope body, `turn` frees what tasks read by reference;
    - inside a task, `locate()` returns the *main* arena, so a library
      loop resets another thread's values.

    Make them `unsafe fn`, or check always (not only in debug, as
    SMART-RUNTIME does) that no scope is open and that `base` is the
    caller's own arena.
  - Two compiler generations later, the hooks can become direct
    `@arena()` calls.
- **Single-threaded programs keep exactly today's code and speed.** None
  of the new emission is reached unless the post-DCE LIR contains
  `tls_address`, `arena_switch` or `clone_thread`. A new check,
  `tools/check_threads_identity.sh`, compiles a fixed list with the
  previous and the new compiler and `cmp`s the binaries:
  - `mlx4` itself;
  - `tests/01`–`120` runtime programs;
  - the desktop, the compositor, Nebula and Observatory.

  It runs at every step of §8.

### 7.4 aarch64

Threads keep sharing the global, atomic arena (THREADS.md §6.8). Per-thread
arenas need the helper `__mlx_arena_alloc` to read its base through
`TPIDR_EL0` (`mrs x16, tpidr_el0; ldr x1, [x16, #ARENA]`). That is one
`mrs` more than the global LDAR, and is emitted only for threaded
programs; the others keep the global variant.

**Critic:** in linked aarch64 programs TPIDR_EL0 is the C library's
thread pointer. That is the very conflict §6.4 gives as the reason not to
use FS on x86_64. The symmetric native choice is a reserved register
(x28, as r14 is on x86_64), in threaded programs only.

`threadlocal` on aarch64 then uses the same arena-relative rule against
the thread's end word. Until then `@arenaSwitch` and `threadlocal` are
compile errors on aarch64.

---

## 8. Part 6: the plan

Every step builds and passes alone:

- the existing runtime tests;
- `tools/check_bootstrap.sh`;
- `tools/ensure_compiler.sh`, with a new feature probe per step,
  following `enforces_unsafe` at `ensure_compiler.sh:825-831`;
- `tools/check_threads_identity.sh` (from L1);
- `mlx-codemap layers`, `cycles` and `unresolved` on the touched files.

Before each change, `mlx-codemap callers`/`refs` lists the touched
functions (CLAUDE.md).

Fixtures go in `tests/threads/`, a new folder, so that their numbers do
not collide with parallel work in `tests/NNN_*`. Refused programs are
checked as `mlx4 --quiet FILE -o /dev/null` with a grep for the code and
message, as the probes in `ensure_compiler.sh` do.

The order follows the owner's: native threads (L0–L2), then the safe
arena and ownership semantics (L3–L4), then structured concurrency
(L5–L7), then the smart scheduler (not this part), then Linux
compatibility (mlxlibc, THREADS.md).

**L0 (THREADS.md step 1, prerequisite): `@arena` and `@arenaSwitch`.**

- Owner: the THREADS.md work. Listed for order only.

**L1: the raw boundary and the catalog.**

- Codes E4016, E6007–E6009, E7006–E7012, W7001–W7002 go into the enum,
  `codes.xml` and the docs table. No behavior changes yet.
- `unsafe fn`.
- `@spawnThread` and `@arenaSwitch` need `unsafe`; the five call sites
  are migrated.
- `check_threads_identity.sh`.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| refused | `threads/e7004_spawn_outside_unsafe.mlx` (probe `spawn.mlx`) | E7004 |
| refused | `threads/e7004_unsafe_fn_call.mlx` | E7004 |
| runs | 298 and 315 | unchanged |
| runs | `threads/unsafe_fn_runtime.mlx` | exits 13 |

**L2: `threadlocal`, arena-relative (x86_64, executables).**

- `lower.mlx:580-592` lowers threadlocal globals to `tls_address`.
- `codegen/memory.mlx` emits it.
- `_start` gets the conditional TLS area.
- `@tlsImage`.
- spec `atomics-tls.xml` TLS ABI; `SPEC_CONFLICTS.md` entry closed.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| runs | `186` (becomes a runtime test): two raw threads (`unsafe`) increment their own `counter`; the main thread's stays 0. **Critic:** this cannot pass with `@spawnThread` alone: children inherit r14/r15, so they share `[r15 + off]`. The fixture must map an arena per thread, call `@arenaSwitch` (L0) and copy `@tlsImage()` above the new r15 by hand, or wait for THREADS.md's std.thread. | exits 13 |
| refused | `threads/threadlocal_runtime_init.mlx` (initializer calls a function) | E5001 |
| refused | `threads/threadlocal_plugin.mlx` (`--plugin`) | E9001 |
| identity | programs without `threadlocal` | byte-identical |

**L3: the classes in the type system (compile time only; no threads
needed).**

- Parser and resolve for `@immutable`, `@shared` and `@owned`.
- `classOf`, `@threadClass`, `@isSendable`.
- The coercions; E7007, E7009, E7010, E7011.
- Atomics accept `@shared(*T)`.
- `@pause`.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| refused | `threads/e7009_write_immutable.mlx`: `fn poke(t: @immutable(*const [4]u8)) -> void { t.*[0] = 1 }` | E7009 |
| refused | `threads/e7010_immutable_to_mut.mlx`: `const v: []u8 = names` with `names: @immutable([]const u8)` | E7010 |
| refused | `threads/e7007_shared_field.mlx`: `var hits: @shared(u64) = 0; hits += 1` | E7007 |
| runs | the same with `@atomicRmw(u64, &hits, add, 1, monotonic)` | exits 13 |
| refused | `threads/e7010_local_to_immutable.mlx`: `var a: [4]u8 = ...; const v: @immutable([]const u8) = a[0..]` | E7010 |
| refused | `threads/e7011_shared_holds_local.mlx`: `const S = @shared(struct { items: []u8 })` | E7011 |
| runs | `threads/thread_class_runtime.mlx` | `@threadClass` of 12 types (plain, local, immutable, shared, transferable, nested, generic `ArrayList(u32)` vs `ArrayList([]u8)`) as expected; exits 13. **Critic:** both ArrayLists are `.local` (the `Allocator` context); compare `std.owned.List(u32)` and `std.owned.List([]u8)` instead (§C.4). |

**L4: borrows (R4) and owned types.**

- `sema/borrows.mlx`: E6007 for local stores and returns, E6008.
- The `escape.mlx` `RETURNED` flag and `parameterFacts`.
- `ir/thread_check.mlx`, borrow-argument part (E6007 for escaping
  parameters).
- `std.owned.Box`, and std.ArrayList's `pointer` becoming `@owned([*]T)`.
  Its errors are the migration list; fix each one or keep a LOCAL
  variant.
  **Critic: do not migrate std.ArrayList.** It has 362 `ArrayList(` uses
  and 739 `.items()` calls in std, projects and tools, all of which turn
  into borrows. It stays LOCAL anyway (its `Allocator`), and the §4.3
  zeroing breaks byte identity. Add a transferable `std.owned.List(T)`
  (`SharedAllocator`, `@owned` storage) and leave ArrayList as it is.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| refused | `threads/e6008_move_while_borrowed.mlx`: the `alias.mlx` probe with `Box`: `const p = b.view(); give(@move(b)); p[0] = 9` | E6008 |
| refused | `threads/e6007_borrow_to_outer.mlx`: `var keep: []u8 = undefined; { keep = list.items() }` | E6007 |
| refused | `threads/e6007_borrow_escapes_call.mlx`: `fn stash(s: []u8) { saved = s }` called with `list.items()` (the message names `saved`) | E6007 |
| refused | `threads/e6008_same_call.mlx`: `f(b.view(), @move(b))` | E6008 |
| runs | `threads/owned_box_runtime.mlx`: Box moved through three functions, borrowed in blocks, dropped once (a deinit counter) | exits 13 |
| runs | `threads/owned_move_clears_runtime.mlx`: with safety on, an unsafe read of a moved-from owned word is 0 | exits 13 |

**L5: sequential blocks.**

- Parser (literals and the trailing block with the head flag), AST,
  `Kind.block`, `sema/blocks.mlx` (`@Block` and `@Scope` kinds),
  `ir/lowering/blocks.mlx`, E4016.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| runs | `threads/block_foreach_runtime.mlx` (sum through a by-reference `var` capture) | exits 13 |
| runs | `threads/block_trailing_runtime.mlx` (`with(lock) { ... }`) | exits 13 |
| runs | `threads/block_heads_runtime.mlx`: `if ready() { }`, `while f() { }`, `for x in g() { }`, `match h() { }` keep their meaning | exits 13 |
| refused | `threads/e4016_store_block.mlx` (`const f = \|x\| { x }`, probe `blk.mlx`) | E4016 |
| refused | `threads/e4016_block_to_plain_fn.mlx` (probe `trail.mlx`: `task.scope { }` to a function that takes no block) | E4016 |
| refused | `threads/e4016_break_out_of_block.mlx` | E4016 |
| codegen | `-O2` inlines a direct `@Block` call | checked by an `ensure_compiler.sh` probe (no `call` in the disassembly of the loop) |

**L6: task blocks and std.task.**

- `@Task` captures (§5.3) and result sendability.
- std.task (scope, spawn, join, parallel_for, checkpoint, Channel) on
  THREADS.md's std.thread (native on brixOS after its step 4; Linux at
  once), with a simple pool. The smart scheduler replaces the pool
  later behind the same API.
  **Critic:** the pool must have **join helps** (SMART-RUNTIME §2.5) from
  its first version. A simple pool whose join blocks deadlocks as soon as
  nested scopes occupy every worker. Also the `task_scope_sum` fixture
  (64 tasks) cannot keep 64 Handles under R4(c) (§C.2 CP10) until §C.8 A8
  lets a local array hold borrows.
- Per-task arena reset.
- Generated `__drop` and `__result_drop`.
- Task symbol names.
- R5 as the **warning** W7002 (the `thread_check.mlx` globals part).

Tests (runtime on Linux; on brixOS with `-smp 4/16/64` via
`check_brix_threads.sh`):

| Kind | Fixture | Expected |
|---|---|---|
| runs | `threads/task_scope_sum.mlx`: 64 tasks sum disjoint ranges | total correct; exits 13 |
| runs | `threads/task_error_cancels.mlx`: one task fails; the siblings see `checkpoint()` fail; the scope returns that error; every task's deinit counter = 1 | exits 13 |
| runs | `threads/task_cancel_before_start.mlx`: moved captures dropped once by `__drop` | exits 13 |
| runs | `threads/task_no_leak.mlx`: after 1000 scopes the process's thread count and region count are back to the pool's | exits 13 |
| runs | `threads/task_arena_reset.mlx`: 10⁶ tasks allocating 1 KiB values each; every worker's `turns.used()` stays below 1 MiB | exits 13 |
| runs | `threads/parallel_for_tiles.mlx`: `[]Tile` of 4096 `u32` each, filtered in place, checked against a sequential run | exits 13 |
| refused | `threads/e7006_capture_slice.mlx`: `s.spawn { use(view) }` with `view: []u8` | E7006 (the message: "`view` is a LOCAL `[]u8`") |
| refused | `threads/e7006_result_local.mlx`: a task returning `[]u8` | E7006 |
| refused | `threads/e6009_write_capture.mlx`: `var n = 0; s.spawn { n += 1 }` | E6009 |
| refused | `threads/e6009_ref_inside_scope.mlx`: a big `const` declared in the body, then `s.spawn` with its address | E7006/E6009 |
| refused | `threads/e6001_use_after_capture_move.mlx`: `s.spawn { consume(box) }; box.view()` | E6001 |
| refused | `threads/e6007_handle_escapes.mlx`: `return s.spawn {...}` out of the scope body | E6007 |
| warns | `threads/w7002_task_global.mlx` | W7002 |
| warns | `threads/w7001_large_capture.mlx` (`var big: [8192]u8`) | W7001 |

**L7: R5 strict (E7008), and the migration of task-reachable globals.**

- `mlx-codemap threads PROGRAM` (new) lists what each task reaches:
  globals by class, with the paths.
- Migration patterns:
  - never written → `const` (detected automatically, no change needed);
  - a lazily built table (`var t` + `var t_ready`, as in
    `nebula/core/color.mlx:563-569`) → `std.sync.Lazy(T)` or a comptime
    table;
  - a lock word or atomic → `@shared`;
  - a cache → `@shared` behind a Mutex, or threadlocal;
  - the rest by hand.
- Foreign calls in task code: E7012, with `threadsafe` on audited
  externs (`extern("syscall")` counts as thread-safe).

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| refused | `threads/e7008_task_global.mlx` (task → helper → `counter += 1`; the message shows the chain) | E7008 |
| refused | `threads/e7008_const_local_ptr.mlx` (`const p: *u32 = &v`, read in a task) | E7008 |
| refused | `threads/e7008_fnptr.mlx` (a task calling through a `fn` value whose possible targets, by type, include a global writer) | E7008 |
| runs | `threads/e7008_never_written_ok.mlx` (a `var` table never written after init is accepted) | exits 13 |
| refused | `threads/e7012_foreign_call.mlx` | E7012 |

**L8: std.sync, freeze and regions.**

- `Mutex(T)` as `@shared`, with a guard borrow and
  `with(|data| ...)`.
- `Lazy(T)`, `Once`, `Channel(T)` (sendable T), `std.frozen.freeze`,
  `Frozen.forever`, and `Region` with `Ref(T)` offsets.

Tests:

| Kind | Fixture | Expected |
|---|---|---|
| runs | `threads/mutex_counter.mlx` | 16 tasks × 10⁵ increments under the guard = exact |
| refused | `threads/e6007_guard_escapes.mlx` (a guard borrow stored to an outer `var`) | E6007 |
| runs | `threads/frozen_share.mlx`: 64 tasks read one frozen table; debug mode makes its pages read-only; an `unsafe` write faults with the crash report naming "frozen" | exits 13 |
| runs | `threads/region_send.mlx`: a region of 10⁵ linked nodes is built in one task, moved through a Channel, and walked in another | exits 13 |
| refused | `threads/e7006_channel_local.mlx`: `Channel([]u8)` | E7006 |

**L9: Observatory.**

- Task, block and scope names in the symbol table (done in L6).
- A `.mlx_tasks` section: literal place, captures and their modes,
  result type, kind.
- std.task's per-worker event ring (spawn, start, steal, end, cancel,
  join, wait), read by Observatory; the task view.
- `mlx-codemap tasks PROGRAM`; `crashes` grouped by task.

Tests: `tools/check_crash_report.sh` gets a case where a trap inside a
task names `path:line:task` and the worker.

**L10: Nebula, the first user.**

- `core/device.mlx` gains `parallelTiles(area, body: @Task(fn(*TileJob) -> void))`.
  Inside, in `unsafe` and audited, it:
  1. makes every affected tile exclusive on the calling thread: copy on
     write through `tileWrite`, with `refs == 1` asserted;
  2. builds `[]TileJob{ key, pixels: @owned(*[4096]u32) }`, asserting
     uniqueness with `@ptrCast`;
  3. calls `task.parallel_for`.
- The filter bodies are safe code: captured filter parameters are plain,
  and the lookup tables move to `Lazy` or comptime (L7).
- tilepool stays main-thread only: R5 will say so if a filter body ever
  reaches it.
- Brush dabs and compositing follow the same pattern. Compositing reads
  the source layers as `@immutable` views made in the same audited
  function, while the main thread is blocked.
  **Critic:** a `@immutable` view is sendable and not a borrow, so a body
  may keep it in a `@shared` global or send it on a channel. It is then
  read after `parallelTiles` returns, while the main thread writes the
  layer again. The audit inside cannot see that. The views must be
  borrows that live for the call (§C.8 A1); likewise `TileJob.pixels`
  (`@owned` here means "unique", with no `deinit`: say so).

Tests: Nebula's own `projects/nebula/test.sh` with filters compared pixel
for pixel against a sequential run, and a scaling benchmark at -smp
1/4/16 in brixOS.

**L11: aarch64.**

- Per-thread arenas through TPIDR_EL0.
- `threadlocal`.

Tests: the L2, L6 and L8 runtime fixtures under the aarch64 emulator
(`tools/aarch64_linux_emulator.py`).

### 8.1 Risks

1. **Soundness holes in a "small" checker.** Each rule is local, so the
   risk is in the seams:
   - generic instances: classes are computed per instance, and errors
     name the instantiation chain;
   - function pointers in R5: a type-based approximation of the possible
     targets (all address-taken functions of that type), which can over-
     or, if a cast hides a type, under-approximate. `@ptrCast` of
     function pointers is unsafe anyway;
   - `escape.mlx`'s precision: sound but conservative, so false E6007s
     are possible. The escape hatch is `unsafe`.

   Mitigation: one refused fixture per rule *and* per seam, and an
   adversarial review of `sema/borrows.mlx` and `thread_check.mlx`
   before L6.
2. **Migration volume.** There are 837 module `var`s in std and 260 in
   Nebula. R5 is staged (W7002, then E7008) and pointed by
   `mlx-codemap threads`. Its automatic never-written rule removes about
   half (the classifier's 335 "C").
3. **The trailing-block syntax change.** `expr {` changes meaning: today
   it is two statements. The scan found no occurrence in the tree, and
   any future one becomes E4016, not a silent change. The `if`/`while`/
   `for`/`match` heads are covered by tests.
4. **Single-threaded identity.** This is enforced by
   `check_threads_identity.sh` at every step. Any accidental change, for
   example a new LIR opcode shifting numbering in a way that changes
   emission order, fails the check.
5. **Bootstrap closure.** `std.turns`, `std.crash` and `std.heap` must
   build with mlx0 and the previous canonical compiler. They use hooks,
   and they never use class constructors or blocks until two
   generations have them.
6. **brixOS limits.** Two regions per thread (arena, stack) against
   `MAX_REGIONS` (256 now, 1024 in THREADS.md step 3). The TLS area is
   part of the arena reservation, so it adds no region. Worker pools
   size themselves to the processor count, not beyond.
   **Critic:** the region per **scope** is missing from this count.
   §5.4.5 and §5.5 put each scope's records in "a chunk from the page
   allocator". The page allocator maps every block above the tiny limit
   on its own (`std/bootstrap/page_allocator.mlx:44-56`), and each mapping
   is a kernel region (`process.mlx:423`); Nebula's tile pool exists to
   dodge this very limit (`tilepool.mlx:1-8`). Recursive fork-join on
   many workers keeps hundreds of scopes live, past 1024. Every scope's
   end is then an unmap, and with threads an unmap is a TLB shootdown to
   every processor running the process (THREADS.md decision 3). Take
   scope records from a per-worker slab instead, never a region per
   scope (§C.3).
7. **Upcalls and foreign callbacks.** Upcalls run on the event thread
   (THREADS.md §4.1) and may touch globals concurrently with tasks. R5
   protects the tasks' side. Treating upcall handlers as task entries
   for R5 is a follow-up.

   C callbacks into exports on worker threads use the main arena
   (§7.2), a documented limit.
8. **Compile time.** `thread_check.mlx` reuses `escape.mlx`'s solve,
   which already runs for whole programs, plus one call-graph pass for
   R5. The expected cost is a few percent of the compiler's own build;
   it is measured with `mlx-profile` on `mlx4` itself before L6 lands.
9. **Ergonomics.** The common cases are:
   - `parallel_for` over plain data;
   - `scope` with by-copy and const-reference captures;
   - `Mutex.with`;
   - `Channel` of plain or owned values.

   None of them needs a class annotation. `@owned` and borrows show up
   only in container and runtime code and in their error messages, which
   must name the route (§3.3). If the borrow rules prove too strict in
   practice, the first relaxation to consider is flow-sensitive (d)
   ("not used after the move" instead of "not in scope"). The lexical
   rule is kept until measured.

---

## 9. Interfaces with the other parts

- **Kernel** (THREADS.md §3–4):
  - `thread_create` (86): start record, inherited general registers,
    FS base, exit word;
  - `thread_exit` (87);
  - `memory_wait`/`memory_wake` (88/89), for cancel words and std.sync;
  - the thread handle, waited for as `ended`;
  - `thread_info` (92);
  - per-thread FS base;
  - `MAX_REGIONS` 1024.

  Nothing else is needed from the kernel for the language part.
  **Critic:** two corrections.
  - The language part needs neither the per-thread FS base nor the exit
    word: §6.4 says so, and they serve C TLS and
    CLONE_CHILD_CLEARTID-style joins, which belong to the compatibility
    layer. A native join is a `wait` on the thread handle's `ended`.
  - Cancellation of blocked waits does need something more: §5.7's wait
    item. Either new calls (SMART-RUNTIME 94/99) or a per-scope channel
    handle.

  The futex-shaped `memory_wait` is fine as a fast path, but the native
  primary path for joins, channels and cancellation is handles and
  `wait`.
- **Smart scheduler:** std.task's internal pool interface.
  - Input: submit a task record (fn, context, result slot, drop
    functions, scope, priority/energy/affinity options as a plain
    `Options` struct).
  - Output: run it on a worker that has done `@arenaSwitch` and runs
    tasks between turns marks.

  Topology, load, NUMA (workers map their own arena, so first touch puts
  it on the worker's node), work stealing and energy are the
  scheduler's. The language's guarantees do not depend on its choices.
- **Linux compatibility:** clone, futex and pthreads stay an adapter:
  `@spawnThread` (unsafe), mlxlibc, and the kernel's Linux layer
  (THREADS.md §5). std.task's Linux backend is std.thread's Linux path.
  Safe Mlx code never sees clone or futex.
- **Observatory and codemap:**
  - `path:line:task` symbols;
  - the `.mlx_tasks` section;
  - per-worker event rings;
  - `mlx-codemap threads` (static: classes, task-reachable globals) and
    `mlx-codemap tasks` (run time);
  - `memory_view` per worker arena.
- **Nebula:** `device.parallelTiles` (L10) is the trusted-partition
  pattern every image program can copy: unsafe and audited inside, safe
  task bodies outside.

---

## C. Critic's review

This section was written by the adversarial reviewer. It is read only
against the tree at `cdca2dbdc`, with probes compiled from stdin with
`mlx-out/bin/compiler/mlx4` (no file written). Every finding has the
place in the code that shows it, or a counter-program in this design's
own syntax.

### C.1 Claims about the code: checked

**Hold:**
- the 8-instruction `lock xadd [r14]` allocation (`codegen/memory.mlx:103-114`);
- the 1 GiB + 8 arena and the brixOS `memory_map` 0x4010 (`emitter.mlx:194-215`);
- clone inheriting r14/r15 (`memory.mlx:139-171`);
- `alloca` arg1 = 1 frame regions and `FLAG_RETURN_SLOT` (`lir.mlx:39-44, 153-164`);
- escape's KIND_RETURN/KIND_RETURN_SLICE marks (`escape.mlx:829, 840`)
  and its call-argument facts (`:940-975`);
- the lowering helpers at the cited lines (`lower.mlx:1585, 1656, 1867,
  1925, 5721`);
- the token-level unsafe scan (`unsafe_boundary.mlx`);
- `Type.flags` being `usize` with room above 128;
- `threadlocal` at E9001;
- `path:line:name` symbols (`symbol_table.mlx:306`);
- 837 / 260 module `var`s;
- Nebula's tile and tile-pool comments;
- the `turns`/`crash` caches;
- `MAX_REGIONS` = 256.

**Do not hold, or are incomplete:**
1. "Seven rules make safe code race-free" (§0.2, §2): see C.2.
2. "Turning an integer back into an address needs `unsafe`" (§2): std
   exports safe-signature wrappers of `@ptrFromInt`
   (`turns.fillWord(base: usize) -> *usize`). std has 1207 `@ptrFromInt`
   and 2159 `unsafe {` blocks in 277 files.
3. "An address into a task's arena is LOCAL" (§2, §4.4): not for
   `&x` with `x: @shared`/`@immutable` (C.2 CP4).
4. R4 "applies only to types that contain `@owned`" (§4.2) contradicts
   R4(a)'s "`&o` of a transferable binding"; a Guard has no `@owned`.
5. "No existing program changes meaning" and "byte-identical" (§4.2,
   §7.3) are broken by three things:
   - L4's ArrayList migration (362 uses, 739 `.items()` calls);
   - §4.3's zeroing, since `.runtimeSafety = true` is the default in
     `driver/options.mlx`;
   - `releasedTurns` becoming atomic.
6. §3.1's `Mutex(std.ArrayList(Job))` is E7011 by §3.4's rule:
   ArrayList's `Allocator.context` is a LOCAL `*anyopaque`
   (`std/bootstrap/allocator.mlx:30-31`). L3's "`ArrayList(u32)` vs
   `ArrayList([]u8)`" expectation is wrong; both are LOCAL.
7. §5.7's "native `wait` with the cancel word as an extra item" does not
   exist (`brix.xml:77, 87`: wait takes handles), so §9's "nothing else
   is needed from the kernel" is false.
8. §7.2's exports fix ("Block at `r14 + 8` carries the magic") reads a
   C caller's callee-saved r14.
9. "`@Block` at -O2 costs nothing" holds only where the inliner sees the
   higher-order callee. At -O0/-O1 it is an indirect call per invocation.
10. "Found from `@arena().0` at no cost": the main thread's Block is a
    static (THREADS.md §6.2, `if base == mainArena`), so it takes a
    compare and a branch.
11. "The five raw call sites": there are six (h264, h265, splash,
    mlxlibc, tests 298 and 315).
12. `scratchpad/brix/globals_classified.txt` no longer exists.

### C.2 Counter-programs: what the rules let through

Each counter-program is accepted by R1–R8 as written, then races,
double-frees or dangles. The fix is the amendment named in C.8.

**CP1: a moved capture in a `parallel_for` body (A5).**
```mlx
var palette = try Box(Palette).init(heap, loadPalette())   // non-copyable owner
task.parallel_for(tiles, |t| { mapTile(t, palette.get()) })
// §5.3 row 4: captured by move. §5.4.3: the block function drops moved
// captures at its end. parallel_for runs that one function once per item,
// concurrently, over one context: N concurrent Box.deinit()s.
```
The same happens with `fn twice(b: @Task(fn() -> void)) -> void { b(); b() }`,
or with one `@Task` parameter handed to two `s.spawn`s.

**CP2: a block captured by a task (A4).**
```mlx
fn each(body: @Block(fn(u64) -> void)) -> void {
    task.scope(void, |s| {
        _ = s.spawn { body(1) }        // `body`: a const declared outside the scope literal
        _ = s.spawn { body(2) }
    })
}
pub fn main() -> u8 {
    var sum: u64 = 0
    each(|x| { sum += x })             // @Block: writes `sum` by reference
    return @intCast(u8, sum)
}
```
`classOf(Kind.block)` is undefined, and E4016 does not list "captured".
If a block is classed like a `fn` (plain), row 1 captures it by
reference, and two workers write `sum`.

**CP3: a const Mutex (A2, A6).**
```mlx
const m = std.sync.Mutex(u64).init(0)   // shared class AND non-copyable
task.scope(void, |s| {
    _ = s.spawn { var g = m.lock(); g.data().* += 1 }
    consume(@move(m))                    // probe: a const non-copyable binding can be moved
})
```
- Row 4 (by move) makes a second user E6001, so a local mutex can never
  be shared.
- Row 1 (by reference) lets the scope body move it under the tasks.
- `m.lock()` on a const is refused today ("`bump` changes its receiver,
  which is a constant here", probed). So locking needs interior
  mutability, which is undefined.
- The L8 fixture `mutex_counter.mlx` cannot be written with a local
  mutex.

**CP4: a `@shared` address outlives its storage (A1).**
```mlx
const ch = task.Channel(@shared(*u64)).init(1)
task.scope(void, |s| {
    _ = s.spawn {
        var n: @shared(u64) = 0
        try ch.send(&n)                     // not LOCAL: R2 lets it cross
    }                                       // frame gone, worker arena reset
    _ = s.spawn {
        const p = try ch.receive()
        @atomicStore(u64, p, 7, seq_cst)    // writes what that worker put there since
    }
})
```
This breaks race freedom and the "reset with no check" claim. The same
happens with an `@immutable` view of a `Frozen` that is later freed, or
of Nebula's layers (L10), stored in a `@shared` global.

**CP5: copying an aggregate with a `@shared` field (A2).**
```mlx
const Stats = struct { hits: @shared(u64), misses: @shared(u64) }
var stats: Stats = Stats.{ .hits = 0, .misses = 0 }   // shared class: R5 lets tasks reach it
// task A: _ = @atomicRmw(u64, &stats.hits, add, 1, monotonic)
// task B: const snap = stats                // a plain copy: non-atomic read of stats.hits
```

**CP6: an atomic load duplicates an owner (A3).**
```mlx
var slot: @shared(?@owned(*Big)) = null
// task A: @atomicStore(?@owned(*Big), &slot, @move(p), release)
// tasks B and C: const mine = @atomicLoad(?@owned(*Big), &slot, acquire)   // two owners
```

**CP7: std's safe wrapper revives an integer (A12).**
```mlx
var buffer: [64]u8 = undefined
const where = @intFromPtr(&buffer)                      // plain: crosses by copy
task.scope(void, |s| {
    _ = s.spawn { std.turns.fillWord(where).* = 0 }     // a safe signature
    buffer[0] = 1                                       // the parent writes at once
})
```

**CP8: a dynamic `task.spawn` (A7).**
```mlx
fn helper() -> task.Handle(u64) {
    return task.spawn { return 1 }   // not lexically in a scope literal: the Block's current scope
}                                    // the Handle is derived from no borrow, so it escapes
```
It can then be joined after the scope freed its record. Called from
inside a task, it spawns into the parent's scope, so joins stop forming a
tree. A user-built `Scope` (if `Scope` has any public constructor) that
is moved into a heap list is never joined.

**CP9: a thread outlives a by-reference capture (A6).**
```mlx
fn start() -> std.thread.Thread(u64) {
    const table = makeTable()                          // plain const, outside the scope literal
    return task.scope(std.thread.Thread(u64), |s| {
        return std.thread.spawn(u64, .{}, { return sum(table) })   // row 1: by reference
    })
}                                                      // the thread reads a dead frame
```
SMART-RUNTIME §3.1's block-taking `std.thread.spawn` and §2.5's
`task.service` exist. Row 1 keys on lexical position, not on which
primitive is joined by the enclosing scope.

**CP10: Handles in an array (A8).**
```mlx
task.scope(void, |s| {
    var handles: [64]task.Handle(u64) = undefined
    var i: usize = 0
    while i < 64 { handles[i] = s.spawn { return part(i) }; i += 1 }   // R4(c): E6007
})
```
This is not a race but an unbuildable everyday pattern: L6's
`task_scope_sum.mlx` needs it.

**CP11: a Guard's data escapes (A8, A9).**
```mlx
var keep: *u64 = undefined
{
    var g = counter.lock()      // Guard: a @shared address, no @owned
    keep = g.data()             // §4.2: R4 only for types with @owned
}                               // unlocked
keep.* += 1                     // unsynchronized write to the protected data
```
If R4 does cover transferable types, the Guard is sendable instead
(`Channel(Guard)` passes R2), and the unlock happens on another thread.

**CP12: a region moved to another process stays mapped in the sender.**
`memory_map_object` pages "outlive the handle while mapped"
(`brix.xml:103`). So after `channel_send` of the handle, both processes
write the same pages.

**CP13: a deadlock (not a race) in safe code.**
```mlx
const ch = task.Channel(u64).init(1)
task.scope(void, |s| {
    const h = s.spawn { try ch.send(1); try ch.send(2) }   // the second send blocks
    try h.join()                                            // nobody receives: hangs
    _ = try ch.receive()
})
```
The join tree prevents only join cycles. Locks and channels are not
prevented, although the owner asked for "prevented by API design" (C.7).

**CP14: iterator invalidation becomes a cross-thread write (A8 (d')).**
```mlx
const v = list.items()      // a borrow
_ = list.append(x)          // (d) forbids move, freeze, send, reassign; not this
v[0] = 1                    // the freed block may now be another thread's heap block
```

**CP15: an address-taken global counts as never written (A11).**
```mlx
var limit: u64 = 10
fn raise(p: *u64) -> void { p.* += 1 }
// main: raise(&limit)    while a task reads `limit`
```
"Never written" must count every `&g` that yields a mutable pointer as a
write.

**CP16: `turns.turn` inside a scope body.** A safe pub function frees
the arena values that tasks read by reference. SMART-RUNTIME traps on it
only in debug builds (A12).

**CP17: an `unsafe fn` through a function pointer (A10).**
```mlx
const start: fn(fn(usize) -> usize, usize) -> usize = std.thread.spawnRaw
_ = start(entry, 0)          // no unsafe anywhere
```

### C.3 Costs the design leaves out

- **Per scope: a kernel region, a map and an unmap.** Each unmap is a TLB
  shootdown IPI to every processor running the process (THREADS.md
  decision 3); §8.1 risk 6 shows the numbers. Fine-grained recursive
  fork-join (quadtrees of tiles, sorts) would be slower than sequential
  and would run out of `MAX_REGIONS`. Take Scope state from the parent's
  frame (the scope is lexical) and job records from per-worker slabs.
- **Per spawn:**
  - the context copy (W7001 warns only above 4 KiB);
  - a job record;
  - a deque push;
  - a `memory_wake` when a worker is parked;
  - at join, help or a `memory_wait`.

  Each kernel entry runs under **the one kernel lock**, so wakes and
  waits from 256 processors serialize. The design gives no figure and no
  granularity guidance for `spawn`. SMART-RUNTIME's adaptive grain
  covers `parallel_for` only. State it: user-space cost per spawn is tens
  to a few hundred ns; a kernel wake is µs and serialized.
- **Per allocation: false sharing.** The fill word (base + 0) shares a
  line with the Block (base + 8), whose cancel, exit and current-job
  fields other threads write or read. The line must be split (§7.2 note).
- **Per move, with safety on by default:** §4.3's zeroing, one store per
  owned word.
- **Per task, debug:** the poison loop is a byte store per byte the task
  allocated (`turns.mlx:213-221`).
- **Per helping join:** nested tasks deepen the worker's 1 MiB stack
  without bound, so deep helping chains overflow the guard page. Bound
  the help depth.
- **Compile time:** `thread_check.mlx` runs escape's solve on the
  *unoptimized* LIR, before `optimize`. escape.mlx was written and
  tested on optimized LIR (promoted locals, propagated copies), so its
  precision there is unknown, and possibly its soundness assumptions too.
  Run it for every program only when the LIR holds a task block, a
  `clone_thread` or an `arena_switch`; otherwise skip it.

### C.4 Steps that cannot build or pass alone

| Step | Problem | Fix |
|---|---|---|
| L1 | `unsafe fn` call checks need "inside unsafe" in sema; today it exists only in the token scan, which runs before declarations. | Carry the per-module unsafe token ranges into sema (and later the LIR, for L7). |
| L2 | Fixture 186 with raw threads shares TLS: `@spawnThread` children keep r14/r15. | Map an arena per thread, `@arenaSwitch`, and copy `@tlsImage()` in the fixture. |
| L3 | The `ArrayList` class expectations are wrong (C.1 item 6). | Use `std.owned.List`. |
| L4 | 362 + 739 sites; no transferability gained; identity broken. | Leave ArrayList alone; add `std.owned.List(T)`. |
| L5 | The examples need generic return inference from block bodies, which Mlx lacks. §5.8 does not budget it. | Add the inference rule, or write R explicitly. |
| L6 | A "simple pool" deadlocks on nested scopes; 64 Handles cannot be stored (CP10). | Helping join first; A8. |
| L7 | R5's `unsafe` exemption needs per-instruction unsafe marks in the LIR. | Carry them through lowering (`lir.setSource` already carries module and offset, so the unsafe ranges can be matched against it). std.heap, turns and crash cannot use `@shared` for two compiler generations, so they wrap their globals in `unsafe {}`. Plan that migration. |
| L10 | The `@immutable` views of layers that are written again later are unsound (CP4). | A1. |
| §6.4 / SMART-RUNTIME | The TLS above r15 does not fit SMART-RUNTIME §3.1's `[guard][stack][arena]` mapping. | Agree on one layout. |

### C.5 Where it slides back into Linux's model

1. §9 asks the kernel for a per-thread FS base and an exit word "for the
   language part". These are C TLS and `CLONE_CHILD_CLEARTID`; the
   language needs neither (§6.4). The native join is `wait` on the thread
   handle's `ended`.
2. The base blocking primitive is the futex (`memory_wait`, "the futex,
   named as a wait on memory", THREADS.md decision 4). Native first means
   handles and `wait`: the thread handle, channels, a wait item for a
   memory word. The bare word wait is a fast path, not the model.
3. `@spawnThread` stays Linux clone on brixOS targets
   (`codegen/memory.mlx:157`, syscall 56, with no target test in
   `emitCloneThread` `:140`). The language's only thread builtin
   reaches our own programs through the compatibility layer. Refuse it on
   `x86_64-brixos`, or lower it to `thread_create`.
4. aarch64 puts the arena behind TPIDR_EL0, the C library's register
   (§7.4 note).

### C.6 Single-threaded programs that would change or slow down

- §4.3's zeroing with safety on (the default): every move of an `@owned`
  value. With L4, every program that moves an ArrayList.
- `releasedTurns` made atomic: every std.turns loop (the compositor, the
  dock, the launcher).
- L4: R4 errors on hundreds of `.items()` stores, so code must copy or
  restructure. That is a source change in programs that never use
  threads.
- `expr {` on one line becomes E4016 (acknowledged in the design).
- `thread_check.mlx` compile time on every program unless gated (C.3).

`check_threads_identity.sh` as specified would fail at L4. With C.8's
gating, it can pass.

### C.7 What the owner's vision has that this design leaves out

- **SAFE THREADS as a model.** The design has tasks and raw threads. A
  long-lived isolated thread (an audio loop, a network reader) needs its
  own block kind, `@Thread`: captures by move or copy only, a sendable
  result, joined by its owner's `deinit`. Without it, CP9.
- **Deadlocks prevented by API design.** The language can prevent the
  common lock deadlocks statically and cheaply:
  - a Guard binding is lexical, so sema knows at each call whether one is
    live;
  - mark std's blocking functions (join, channel send/receive, lock,
    sleep) `blocking`, and refuse a call to one while a Guard binding is
    in scope (a new code);
  - carry the lock level in the type (`Mutex(T, level)`, SMART-RUNTIME
    §2.7) and check the nesting order within a function at compile time.

  Channel cycles (CP13) stay unprevented; say so.
- **Errors passed to the parent under control.** A checked-arithmetic or
  bounds trap in a task ends the process. That is consistent Mlx, but it
  contradicts the vision for safe threads, whose memory is isolated (R2).
  It is an open decision, to be put to the owner: a contained trap
  becomes `error.Trapped` at the join, with the locks it held poisoned.
- **Thread-affine types.** Guards, a window, a GPU context have no way to
  be declared non-sendable (A9).
- **Shared ownership beyond a scope.** `Shared(T)` and `Frozen(T)` with
  refcounts exist in SMART-RUNTIME §3.2. The language's classes give
  `@shared`/`@immutable` addresses no lifetime that ties them to these
  owners (A1).
- **The kernel itself.** It is a freestanding Mlx program that now runs
  on every processor. Its per-CPU data is the first natural
  `threadlocal`, and freestanding is silently outside §6.4.
- **"An arena per thread" for all four classes.** The design places
  SHARED, IMMUTABLE and TRANSFERABLE in heap blocks and regions, never in
  arenas (§4.4). That is a sound choice, but it departs from the owner's
  "built on the arena architecture"; present it as such.

### C.8 Amendments (the corrected rule set)

- **A1. Lifetimes for SHARED and IMMUTABLE addresses.** `&p` of a
  `@shared` or `@immutable` place whose storage is not static is a
  borrow (an R4 root). A sendable `@shared`/`@immutable` address is
  only one of:
  - an address of a module global or of constant data;
  - an address inside `Frozen.forever`;
  - one obtained from a refcounted owner handle (`Shared(T)`,
    `Frozen(T)`, transferable), whose `get()` is a borrow of the handle.

  Audited `unsafe` casts to `@immutable`/`@owned` (Nebula) produce
  borrows that live for the enclosing call.
- **A2. `@shared` places are non-copyable, with interior mutability.**
  Atomics, and std.sync via `unsafe`, may write a `@shared` place through
  a `*const` path. Copying an aggregate that contains one is E6002.
  `Mutex.lock(self: *const Self)`.
- **A3. Atomics on a type with an `@owned` word** allow only `xchg` and
  `cmpxchg`, which move the old value out. `@atomicLoad`/`@atomicStore`
  of such a type is E7007.
- **A4. Blocks are LOCAL and uncapturable.** Any capture of a block value
  is E4016. Passing a block parameter on directly is the only way to
  forward it.
- **A5. Two task kinds.**
  - `@Task`: once. A call or a submission consumes it, so a second use is
    E6001. If unconsumed, its `__drop` runs at the callee's end. Captures
    by move are allowed.
  - `@TaskEach` (`parallel_for` and its kin): called concurrently any
    number of times over one context. Captures are by const reference or
    by copy of plain/immutable/shared values. A non-copyable capture is
    E6009.
- **A6. By-reference capture only into a scope.** A `@Task`/`@TaskEach`
  literal given *directly* to `s.spawn` or `s.parallel_*`, where `s` is
  the parameter of an enclosing scope literal, may capture by reference.
  - Each such capture is a borrow of its binding until the end of that
    scope literal, so R4(d) forbids moving it in the scope body.
  - A shared-class non-copyable const (a Mutex) is captured by reference
    under this rule.
  - `std.thread.spawn`, `task.service` and every other primitive take
    `@Thread`: move or copy only.
- **A7. Only `task.scope` makes a `Scope`**, in its own frame, and
  exposes only `*Scope`. An unqualified `task.spawn` outside a scope
  literal is E4016, not a run-time trap.
- **A8. R4 roots and bindings.**
  - The roots are:
    - `&o` of any transferable binding (Guards included);
    - block and scope parameters;
    - results of std functions declared to return a borrow (a
      `@borrow(P)` result marker on `items()`, `get()`, `data()`,
      `view()`): one annotation in a handful of std signatures, not in
      user code.
  - A local aggregate declared in the same block may hold borrows; it
    becomes a borrow binding (fixes CP10).
  - **(d')**: while a borrow binding of `o` is in scope, `o` is used only
    through `*const Self` methods (fixes CP14).
- **A9. A non-sendable marker,** for example `@local(struct)`, for
  thread-affine types: Guard, windows, GPU contexts. `@isSendable` is
  false for them, and R4 still applies.
- **A10. `unsafe fn` has its own function-pointer type.** Coercing it to
  a plain `fn` type is E7004.
- **A11. R5's "written" includes every `&g` that yields a mutable
  pointer,** and every write in the init function. "Never written after
  init" is not accepted without `std.sync.Lazy`/`Once`.
- **A12. Audit in L1.**
  - Safe-signature std functions that turn a caller's integer into an
    address become `unsafe fn` (`turns.fillWord`, `turns.beginAt`, ...).
  - `turns.turn` checks, always, that no scope is open and that `base`
    is the caller's own arena.
  - A sema warning flags new safe functions whose `@ptrFromInt` operand
    derives from a parameter.
- **A13. A region moved to another process unmaps the sender's view.**
  `Region.alloc(T)` requires a plain `T`.
- **A14. `@spawnThread` is refused on brixOS targets.** Use
  std.thread.Raw on the native calls.
- **A15. The fill word gets its own cache line.** The Block starts at
  `base + 64`, and fields other threads write sit on separate lines.
- **A16. No region per scope.** The Scope lives in `task.scope`'s frame,
  and job records come from per-worker slabs.
- **Gating.** The §4.3 zeroing and the `thread_check` pass apply only to
  programs whose LIR holds a thread entry (a task block, `clone_thread`,
  `arena_switch`). `releasedTurns` is std source and cannot be gated that
  way, so count it per arena in the Block, or exempt std.turns users from
  the identity list by name. With that, §7.3's byte-identity promise can
  be kept.
