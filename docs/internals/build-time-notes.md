# Where the WebKit build's time goes (notes, 2026-10-21)

Measured with `mlx-profile` (tools/profile) on a small data set while a
full WebCore rebuild ran beside it. No code was changed for these notes.
The profiles are in the session's scratchpad (`prof/`).

## The data set

| Program | Input | Wall | User / sys | Memory |
|---|---|---|---|---|
| mlx-cc | WTF `wtf/text/WTFString.cpp` (JSCOnly flags) | 6.6 s | 6.0 s / 0.5 s | 1.17 GB arena (608 MiB resident at the end) |
| mlx-cc | WebCore `UnifiedSource-workers-2.cpp` (Mlx port flags) | 179 s | 167 s / 8.7 s | killed at the end by the 15 GB cgroup beside two other units; earlier measurements of such units: 2.4–3.9 GB resident, JSBindings-26 beyond the 8 GB arena |
| mlx4 | `compiler/cc/main.mlx` (mlx-cc itself, ~60k lines, 70 modules) | 1.5 s | 1.2 s / 0.1 s | 134 MiB |

The container has 4 cores and 15 GB. A WebCore unified source takes
2–4 GB in mlx-cc, so the build runs at `-j 2`: half the cores idle
through the whole build, and three units at once get one of them killed
(exit 137). That, not the per-step overhead, is the first thing to note:
**memory per unit decides the parallelism, and parallelism decides the
wall time.**

The Mlx compiler itself is not the problem: mlx4 compiles all of mlx-cc
in 1.5 s with its time spread over lowering (30%), the optimizer (27%),
analysis (15%) and code generation (12%); no function above 5% of it.
std.build's own overhead per step is milliseconds. Everything below is
about mlx-cc on C++.

## mlx-cc: half the time is one function

Self time (where the instruction pointer was), WebCore unit, 34224
samples at 5 ms:

| Self | Function |
|---|---|
| 47.6% | `compiler/cc/base.mlx:213:getWord` (one load: `@ptrFromInt(*usize, address).*`) |
| 5.2% | `base.mlx:261:putText` |
| 4.9% | `base.mlx:380:hashOn` |
| 4.5% | `base.mlx:196:get32` |
| 3.0% | `base.mlx:192:put8` |
| 2.4% (11.1% with calls) | `base.mlx:394:findJoined` |
| 2.0% | `paths.mlx:28:exists` |
| 1.9% | `base.mlx:217:putWord` |
| 1.5% (14.7%) | `scope.mlx:301:qualifiedKnown` |
| 0.9% (14.5%) | `scope.mlx:393:lookup` |
| 0.6% (15.6%) | `scope.mlx:319:headOf` |

On the small WTF file the same accessors take 36% (`getWord` 21%,
`get32` 6%, `put8` 4%, `hashOn` 5%); on the WebCore unit `getWord`
alone is 48%. The accessors are a few instructions each, so the time is
not the calls themselves but the loads they make: with a 1 GB arena the
loads miss the cache some of the time, with a 3–4 GB arena most of the
time. Two things stack up here:

1. **No inlining.** `getWord`, `get32`, `put8`, `putWord` are exactly
   the "one `return` inside `unsafe`" shape `ir/inline.mlx` inlines, but
   programs are built by `Build.program` without `-O2`, and the
   accessors are not marked `inline`, so every load of the compiler's
   data structures is a call (prologue, frame pointer, return). That is
   the part of the 48% that is pure overhead; `-O2` on mlx-cc or
   `inline` on base.mlx's accessors would show how much. (mlx-cc and
   mlx4 are the two programs where this matters most.)
2. **The working set.** The arena is append-only and never freed; the
   data structures are linked cells walked by `getWord` (scope chains,
   overload chains, joined names, template instance lists). The lookups
   dominate the totals: `qualifiedKnown`/`lookup`/`headOf` 15% each
   (the same walk seen from three entry points), `findJoined` 11%
   (joined-name interning), `resolveNrvo` 17%. Each step of such a walk
   is a dependent load into a cold cache line.

## Where the walks come from (totals, WebCore unit)

| Total | Function |
|---|---|
| 57% | `expr.mlx:1745:postfix` (expressions under parsing) |
| 47% | `decl.mlx:907:declspec` |
| 41% | `template.mlx:636:ensureDefinedNow` (template instances defined) |
| 38% | `decl.mlx:240:qualifiedTypedef` → `readQualifiedTypedef` (37.8%) |
| 36.5% | `decl.mlx:141:isTypename` |
| 35% | `decl.mlx:2100:classBody` |
| 26.7% | `template.mlx:7920:definePendingInstance` |
| 24.5% | `template.mlx:6354:instantiateClass` |

So the time is in **parsing the headers** (declspec/isTypename/
qualifiedTypedef: every identifier is looked up to tell a type from a
value, through qualified-name resolution each time) and in **template
instantiation** (ensureDefinedNow/instantiateClass: bodies re-read from
tokens), both bottoming out in the scope walks above. The lexing itself
is small (`tokenize` 1.3% of the WTF file).

## What the numbers say about the units

- The WTF file reads 2.88 M tokens (184 MB) and makes 738 k AST nodes
  (159 MB) and 176 k types (31 MB) for 1.17 GB of arena; "size 4096"
  allocations (header texts, token buffers, tables) are 390 MB of it.
  A WebCore unified source includes WebCorePrefix.h plus ~10 files'
  headers: 3–4 GB, and `names`/arena limits are hit by the largest.
- 167 s of user time per WebCore unit × ~1250 units / 2 jobs ≈ the
  observed 6–7 h full rebuild. With the memory halved the same build
  would run at `-j 4` on this machine in half the time; with the
  accessor overhead gone, less again.

## Candidate levers, in the order I would try them (not done)

1. **Build mlx-cc (and mlx4) with `-O2`**, or mark base.mlx's accessors
   `inline`: a one-line build change; measure on the two units above.
   Pure overhead, no semantic risk. (mlx4's inliner is already there.)
2. **Memory per unit**, which sets the parallelism: tokens spliced per
   header are kept for the whole unit (184 MB for one WTF file); the
   arena never frees per-function scratch (AST of a finished function,
   substitution scratch). Freeing or reusing per-function data, and
   sharing the prefix header's tokens between units, would cut the
   working set and the cache misses together.
3. **Name lookup**: `isTypename`/`qualifiedTypedef` resolve qualified
   names from scratch on every mention; a per-scope memo (the earlier
   "qualified type names remembered while the scope is unchanged" cache
   covers only some paths) and hashed scope lookup instead of chain
   walks would cut the 15%-each totals.
4. **Template instantiation**: bodies are re-tokenized from the pattern
   per instance; a parsed-once pattern AST would avoid the re-walk.
5. **Precompiled prefix**: the WPE build spends the first ~3 GB on
   WebCorePrefix.h per unit; snapshotting the state after the prefix
   (serialize arena + tables) and restoring it per unit would remove the
   largest constant cost per unit. Big change; last.

What was not measured: mlx-ar and the linker (seconds per archive, not
the problem), the derived-source generators (cached), and the
page-fault share precisely (sys time is 5% of the WebCore unit).
