# Allocators and containers: the inventory, the baseline, the idiom

The state of the tree at ac3d8b2 (2026-10-21) before the generic
containers (`std.HashMap`, `std.AutoHashMap`, `std.StringHashMap`,
`std.AutoHashSet`, `std.ArrayList`), `std.hash` and `std.heap` came, and
the order in which the tree is converted to them. The allocator notes in
[build-time-notes.md](build-time-notes.md) ("Allocators") counted who
uses which allocator and proposed `std.heap`; this document starts from
those numbers and does not repeat them. Everything below was found with
Observatory (`mlx-codemap refs`, `find`, `copies`, `memory`, `hot`) and
checked by reading the places it named.

## What is written by hand today

### Hash tables and interning tables

Open-addressed tables with linear probing (`slot = (slot + 1) & (N - 1)`
or `% N`), each with its own hash function (FNV-1a, Fibonacci hashing,
`x *% 2654435761`), its own notion of an empty slot (0, `NONE`, a state
byte), and mostly a fixed number of slots chosen for the largest input
seen:

| Where | Keys, values | Replace with |
|---|---|---|
| `compiler/cc/base.mlx:374` (names, 8 M fixed slots, `intern`, `internJoined` at :454, `internBuilt` at :336) | identifier bytes, pairs of ids → name id | `std.StringHashMap(u32)` and `std.AutoHashMap(u64, u32)` over the compiler's arena |
| `compiler/cc/table.mlx:1` (`Table`, doubled when half full) | word → word | `std.AutoHashMap(usize, usize)`; it is the one generic table in the tree |
| `compiler/cc/scope.mlx:266` (`QUALIFIED_SLOTS`) | (prefix, name) → qualified id | `std.AutoHashMap(u64, u32)` |
| `compiler/cc/paths.mlx:109` (`FOUND_SLOTS`) | include lookup key → file found | `std.AutoHashMap(usize, Found)` |
| `compiler/cc/template.mlx:5317` (`instanceSlotCount`) | (template, key) → instance | `std.AutoHashMap(struct { t: usize, key: u32 }, usize)` |
| `tools/codemap/index.mlx:380` (`TABLE_SLOTS`, 4 M fixed) and `:419` (`FILE_SLOTS`) | (container, name) → symbol; path → file | `std.HashMap(u32, u32, NameContext)` (the key's bytes live in the index), `std.StringHashMap(u32)` |
| `tools/codemap/analysis.mlx:54`, `tools/codemap/diff.mlx:56`, `projects/observatory/graph.mlx:101` (`keys: [*]u64`) | u64 → u32 | `std.AutoHashMap(u64, u32)` |
| `tools/profile/main.mlx:163` (`FUNCTION_SLOTS`), `:433` (`ADDRESS_SLOTS`) | function name → index; return address → function | `std.StringHashMap(u32)`, `std.AutoHashMap(usize, u32)` |
| `std/src/xml/text.mlx:414` (libxml2's dictionary) | bytes → interned string | `std.StringHashMap(u32)` |
| `std/src/shaping/set.mlx:513` (HarfBuzz's `hb_map`, with tombstones) | u32 → u32 | `std.AutoHashMap(u32, u32)` |
| `std/src/unicode/collation/elements.mlx:323` | code point → tailoring element | `std.AutoHashMap(u32, u32)` |
| `std/src/unicode/format/number/currency.mlx:176` (`KEPT`), `data.mlx:275` (4096 slots) | text address, id → kept copy | `std.AutoHashMap(usize, Copy)`, `std.AutoHashMap(u32, Copy)` |
| `std/src/truetype.mlx:546` (`ATLAS_SLOTS`) | glyph → atlas slot | `std.AutoHashMap(u32, AtlasGlyph)` |
| `std/src/build/cache.mlx:355` (`MEMO_SLOTS`) | path → file hash memo | `std.StringHashMap(Memo)` |
| `std/src/gpu/diagnostics.mlx:367` (`NAME_SLOTS`) | return address → name index | `std.AutoHashMap(usize, u32)` |
| `projects/shell/state.mlx:502` (`VARIABLE_SLOTS`) | variable name → index | `std.StringHashMap(u32)` |
| `projects/desktop/splash/paint.mlx:460` | light → tinted image | `std.AutoHashMap(u32, usize)` |

Not converted: `std/src/leaks.mlx:348` and `:416` (the leak checker's
places; it runs inside the allocators' hooks and must not allocate
through them), `compiler/libc/glib/hash.mlx` and `errors.mlx:30`
(GHashTable and GQuark are the C API, with C's semantics and C's
malloc), `compiler/selfhost/*` (four `hashName` copies; the bootstrap
compiler must still build them). `x11.mlx:269 internAtom` is the X
protocol's, not a table. Ring buffers that advance the same way
(`terminal.mlx:447`, `png/read.mlx:475`, `compositor/xwm.mlx:289`, the
format caches under `std/src/unicode/format`) are not tables.

### Dynamic arrays

A `[*]T` with a length and a `capacity: usize` next to it, grown by hand
(`grow`, `ensureRoom`, `push`, `append`, each file its own): 61 files
outside `std/bootstrap` and `projects/webkit` declare such a struct
(`grep -rl '^\s*capacity\s*:\s*usize'`), 34 of them in `std/src`, 8 in
`tools/codemap`, 8 in `compiler/libc`, 10 in `projects`, 1 in
`compiler/cc` (whose `gen.mlx`, `extended.mlx`, `cpp.mlx` and
`preprocess.mlx` grow lists in the arena with helpers of their own). The
copies finding groups the growth helpers: `append` is written in
`tools/codemap/{profiles,trend,memory,history,crashes,binaries}.mlx` and
`tools/profile/main.mlx` alike, `appendU8/U32/Slice` five times under
`experiments/android-aarch64-bluescreen`, `pushPointer` in
`compiler/libc/glib/{signals,objects}.mlx`, `grow` in `std/src/sql/mem.mlx`
and `std/src/fontconfig/mem.mlx`. Every one of them is a
`std.ArrayList(T)`; the byte buffers are `std.ArrayList(u8)`.

Four libraries ported from C keep "memory by address" helpers of their
own (`std/src/sql/mem.mlx`, `std/src/fontconfig/mem.mlx`,
`std/src/shaping/memory.mlx`, `std/src/unicode/memory.mlx`, and
`compiler/libc/glib/memory.mlx`): a length before each block, so that C's
pointer-only `free` works. The first three take an `Allocator` once
(`use`, the page allocator until then); their default becomes
`std.heap.allocator()`. The unicode and glib ones are C's `calloc` and
`realloc`, since C frees what they hand out; they stay.

### Arenas and bump allocators over raw pages

| Where | What | Becomes |
|---|---|---|
| `compiler/cc/base.mlx:17` | 8 GiB `mmap` MAP_NORESERVE, bumped in 16-byte steps, never freed, plus a scratch stack (`:94`); MLX_CC_STATS counts its fill by kind | `std.arena_allocator` over `std.page_allocator` (which passes MAP_NORESERVE for reservations that large), so `mlx-codemap memory mlx-cc` sees it |
| `compiler/cc/cheaders.mlx:90` | 4 GiB reserved the same way | the same |
| `tools/codemap/index.mlx:708` | `std.arena_allocator` of 3 GiB over the page allocator | stays; its one block is seen once a snapshot is written (below) |
| `std/src/crypt/yescrypt.mlx:419` | `mmap` MAP_NORESERVE for the algorithm's working memory | `std.page_allocator` (a backing site) |
| `projects/desktop/compositor/arena.mlx` | the compiler's value arena, released per turn (`std.turns`) | stays: not an allocator of the program's |
| `std/src/leaks.mlx`, `std/src/diagnostics.mlx`, `std/src/crash.mlx`, `std/bootstrap/debug_allocator.mlx` | mappings for their own tables | stay: they run inside the allocators' hooks and the crash handler |
| `std/src/truetype/face.mlx:69`, `std/src/shaping/blob.mlx:166`, `std/src/crash.mlx:562`, `tools/codemap/binaries.mlx:192`, `tools/profile/main.mlx:345` | files mapped | stay: mmap-shaped |

## `std.page_allocator.init()` outside the bootstrap

`mlx-codemap refs page_allocator.mlx:init` lists 345 call sites in 231
files. 224 of them, in 141 files, are outside `tests/` and
`std/bootstrap/`:

| Area | Sites | Files | Allocator to use |
|---|---|---|---|
| `projects/coreutils` | 72 | 42 | one `std.heap.allocator()` per program (`common.mlx` has 3 of its own; `ls` 7 over 7 files, `tr` 5, `env` 4) |
| `projects/desktop` | 54 | 28 | one heap per program (`settings/main.mlx` has 11, `login` 4, `capture` 9 over 6 files); the compositor's modules share the program's heap (below) |
| `tools/codemap` | 23 | 7 | the heap; `index.mlx` keeps its arena for the index (one phase: the whole run) |
| other `tools` | 21 | 18 | the heap |
| `std/src` | 11 | 9 | `std.leaks` stays on pages (it watches them); `truetype.mlx`, `shaping/memory.mlx`, `fontconfig/mem.mlx`, `build/{store,graph,cache}.mlx`, `ui/host.{linux,android}.mlx` take the heap as their default |
| `examples`, `experiments` | 10, 8 | 6, 8 | the heap |
| `compiler/cc` | 5 | 4 | the heap for `program.mlx`; the arena (above) for everything in a unit |
| `compiler/selfhost` | 5 | 5 | stays: the bootstrap compiler builds it (`driver/pipeline.mlx` already has an arena per pipeline) |
| `compiler/libc` | 2 | 2 | stays: mlxlibc's `malloc` is the heap already (`heap.mlx`) |
| `std/registry`, `std/protocols` | 4 | 4 | the heap (generators, run once) |
| `projects/{observatory,network,android,shell,init,console}` | 9 | 8 | the heap |

Raw pages remain where a block is a mapping: frame buffers, files read
whole, the reservations of arenas, and the leak checker's own tables.
Those sites are the allow-list of the codemap check (below).

## The baseline

Measured on this tree with Observatory, before anything was converted
(`MLX_LEAK_CHECK=1` and `XDG_RUNTIME_DIR` set, then `mlx-codemap memory
PROGRAM`; the resident peak is VmHWM polled from /proc while the program
ran without the check):

| Program, run | `memory` | Time, resident |
|---|---|---|
| `ls -lR projects std` (coreutils) | 318 blocks, 246 KiB asked, 267 KiB in pages (7% wasted); tiny pages: 309 blocks on 5 pages, 99% full; below 256 B: 309 blocks, 3520 B in 19 KiB (82% wasted); 13 mappings, 2 holes, fragmentation 0.480; the places: 279 blocks of `std/bootstrap/path.mlx:12 allocate`, three 64 to 96 KiB vectors | 4.7 MiB resident |
| `mlx-codemap unused` (the whole tree) | no snapshot: the index is one 3 GiB arena taken at the start, and a snapshot is written at an allocation at least a second after the last one (`std.leaks`), of which there are none | 3.2 s, 550 MiB peak |
| `mlx-cc -c wtf/text/WTFString.cpp` | no snapshot: the arena is a raw `mmap` (build-time-notes.md has its MLX_CC_STATS: 1234 MiB arena, 406 MB in 502 blocks of 4 KiB and more, 185 MB of tokens, 154 MB of nodes) | 2.7 s, 587 MiB peak |
| `mlx-sh -c SCRIPT` (the shell, headless) | no snapshot: the program ends within the second | — |

`mlx-codemap hot` is the mlx-cc profile of the notes: `declspec`, the
expression chain `logicalAnd` ... `postfix`, `qualifiedTypedef` and
`isTypename` carry 22 of 39 thousand samples. Three of the four programs
show Observatory nothing of their memory: that, as much as the waste, is
what the conversion is for.

**The heap's lock, measured.** The notes asked whether one lock on the
heap would cost more than today's tiny-page lock and one `mmap` per large
block. Four threads, each 200 000 allocations and frees of random sizes
through one allocator (`bench_lock.mlx` in the scratchpad; the same
sequence for both):

| Sizes | `std.page_allocator` | one `general_purpose_allocator` heap |
|---|---|---|
| 1 to 2000 bytes | 2053 ms | 288 ms |
| 1 to 60 000 bytes | 10 636 ms | 5238 ms |

The tiny pages take one lock too (the region's) and scan bitmaps under
it, and a page block is a system call each way; the heap's fast path is
a compare-exchange and a free-list store. The heap wins by seven times
for small blocks and by two for mixed ones (its blocks over 32 KiB are
still mappings). Settled: one heap per program.

**Blocks across images.** The compositor loads `libmlx-shell.so` and
`libmlx-render.so` and replaces them while it runs
(`projects/desktop/compositor/modules.mlx`); a window list the shell
module allocates is freed by its next version or by the compositor. The
tiny pages solve this with one region at a fixed address that every image
of the process shares. `std.heap` does the same: the heap's state lives
in the tiny-page region's header page (`std.tinypage_allocator.shared()`),
so every image's `std.heap.allocator()` is the one heap. Where the region
cannot be had (a kernel without MAP_FIXED_NOREPLACE), each image has a
heap of its own; a block freed by another image then goes on that image's
free list of the same size class, which is harmless (the block is reused
by that image) and no worse than today.

## The idiom

- A program has one heap, `std.heap.allocator()`, made on first use; it
  is passed to what needs memory (`init(allocator)`), never fetched
  again inside a library. `std.page_allocator.init()` is for backing
  allocators and mappings only, and `mlx-codemap workarounds` says where
  it is used otherwise.
- A phase that allocates much and frees nothing (a compiler's unit, an
  index, a request) takes a `std.arena_allocator` over the heap or, for
  a reservation of gigabytes, over the page allocator; `deinit` gives it
  all back at once.
- A table is a `std.HashMap(K, V, Context)` (the caller's `hash` and
  `eql`), a `std.AutoHashMap(K, V)` (derived from the key type by
  `std.hash.autoHash` and `autoEql`), a `std.StringHashMap(V)` or a
  `std.AutoHashSet(K)`; a growing array is a `std.ArrayList(T)`. Each is
  `@noncopy(struct)` with `fn deinit(self: *Self) -> void`, so a local
  one is freed where its scope ends and a stored one when its owner is.
- A hash function is `std.hash.wyhash` (or `autoHash`), not FNV written
  out; table sizes are powers of two grown by doubling, not a constant
  for the largest input seen.
- Under `MLX_LEAK_CHECK=1`, `std.leaks` wraps the heap in the debug
  allocator, so every block the program holds is seen with its place and
  its size class (not the heap's 256 KiB slabs); the page allocator stays
  watched for the backing sites.

### The check

`mlx-codemap workarounds` (and Observatory's findings, `query
workaround:pages|tables|arrays`, `history`) counts the idiom's
violations, so that the conversion is measured and does not come back
([tools/codemap/workarounds.mlx](../../tools/codemap/workarounds.mlx)):
a `page_allocator.init()` outside `BACKING_SITES` (the bootstrap,
`std.heap` and `std.leaks`, `compiler/selfhost`, `compiler/libc`, the
compilers' arenas in `compiler/cc/base.mlx` and `cheaders.mlx`,
`tools/codemap/index.mlx`, the tests), an open-addressing probe in a
function that speaks of a hash, a slot or a probe, and a struct with a
`[*]T` field and a `capacity: usize` (both outside `CONTAINER_SITES`).
On this tree before the conversion: 214 pages, 37 tables, 50 arrays.
`mlx-codemap rewrite pages [PATH] --apply` makes
`std.page_allocator.init()` into `std.heap.allocator()`.

### The order

1. `std`: `std.hash`, the containers, `std.heap`; then the tables and
   arrays of `std/src` (the C-shaped libraries take the heap as their
   default).
2. `tools`: codemap (its index's tables over the arena it keeps) and
   profile, then the test helpers.
3. `projects`: coreutils, the desktop and its services, the shell, the
   generators.
4. `compiler/cc`: the arena over `std.page_allocator` first (byte-identical
   objects, measured with MLX_CC_STATS and the WTFString.cpp timing of
   the notes), then its tables over the arena.

`compiler/selfhost` and `std/bootstrap` stay on the bootstrap containers
(`std.array_list`, `std.hash_map`, `std.vector`): mlx0 must still compile
them. `projects/webkit` is not touched.
