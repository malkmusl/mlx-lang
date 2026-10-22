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

## Results (branch mlxcc-speed, 2026-10-21)

Stages 1 and 2 of the list above, measured on the same two units with
nothing else running (`getrusage` of the child: user time and the peak
resident set; MLX_CC_STATS=1 for the arena). Every variant below made
the same object files as the baseline, byte for byte (the gate at the
end).

| mlx-cc | WTF `WTFString.cpp` user / RSS | WebCore `UnifiedSource-workers-2.cpp` user / RSS |
|---|---|---|
| baseline (30ca2d6) | 4.9 s / 608 MiB (arena 1167 MB) | 156.6 s / 4687 MiB (arena 5242 MB) |
| accessors `inline`, built as before | 5.0 s / 608 MiB | – |
| accessors `inline`, `-O3` | 4.5 s / 608 MiB | 149.2 s / 4689 MiB |
| + NRVO choices per function | 3.3 s / 609 MiB | 110.5 s / 4703 MiB |
| + copies by the word, include lookups remembered, Node 208 and Type 144 bytes | 2.8 s / 592 MiB | 102.2 s / 4459 MiB |
| + class lines kept, pending instances counted, instances in a table | 2.8 s / 588 MiB (arena 1147 MB) | 52.7 s / 4150 MiB (arena 4676 MB) |

So 1.75× on the WTF file and 3× on the WebCore unit, with the objects
unchanged; memory down 11%. A full `./mlx build webkit` is not
remeasured here (6.4 h before; the units are 2–3× faster, the
parallelism the same).

### Stage 1: inlining, and what the profile had really measured

- `Build.program` steps are compiled by mlx4 at its **default level,
  which is already -O2** (driver/options.mlx:238): `-O2` on the build
  changes nothing (the binary is identical). `getWord`'s body,
  `@ptrFromInt(*usize, address).*`, costs 4 by ir/inline.mlx's measure
  (builtin call + two leaves + the deref), one above the -O2 limit of 3,
  which is why it was never inlined; the `inline` mark makes it
  eligible whatever the level, and the inliner honours it across
  modules (`calls inlined: 8234` for mlx-cc, 680 requests kept as calls:
  the `put*` accessors, whose body is a statement, not a `return`,
  which the inliner does not expand). mlx-cc, mlx-ar and mlx-cheaders
  are built at -O3 (build.mlx), which inlines twice as many calls
  (limit 12).
- The marks alone changed nothing (5.0 s against 4.9 s): the "48% in
  getWord" was the **cache misses of the loads**, which the profile
  books on the function that does the load; inlining moves them to
  the callers. -O3 gave 10% on the WTF file and 5% on the WebCore unit.
  mlx4 itself is left as it is (it compiles all of mlx-cc in 0.6 s).
- With the accessors inlined the profile shows the callers, and three
  of them were quadratic walks of lists of the whole unit:
  `cpp.resolveNrvo` (every function's `return local;` choices, walked
  for each function read: 20% of the WTF file's samples with calls),
  `template.pendingOuterOf` (20% of the WebCore unit's self time),
  `template.definePendingInstance`/`isPendingInstance` (16%) and
  `template.cached` (8%, each template's instances as a list keyed by
  name). Each keeps its data on the object now (Obj.nrvoChoices,
  Obj.pendingOuter, Obj.pendingCount) or in a table, with the same
  answers in the same order. `base.copy`, `putText` and `keep` were
  loops of `put8` calls (7% self): they copy by the word or through a
  pointer. `paths.find` tried the WebCore unit's 250 include
  directories with a system call or two each for every `#include`,
  guarded headers included: the directory search's answer is
  remembered per (name, start).
- Observatory (`mlx-codemap`, tools/codemap) on compiler/cc: `complex`
  names `expr.primary` (229 branches), `interp.value` (176),
  `decl.localDeclaration` (149), `template.chooseInstance` (144),
  `decl.readQualifiedTypedef` (130); `risky` the same with
  `cpp.memberFunction` and `decl.functionDefinition`; `cycles` lists
  the whole front end (initializer, typing, cpp, interp, eval, program,
  gen, genstmt, atomics, expr, decl, stmt, template, lambda, builtins)
  as one cycle; `copies` finds base.mlx's `same` and `bytes` copied in
  a dozen places of std and the generators; `unused` 25 declarations
  (scope.enterNamespace, tokens.say, types.isAggregate, ...). `hot`
  reads the profiles of ~/.local/state/mlx/profiles (all of them: keep
  the directory to the run being looked at); `callers getWord` must be
  qualified (`callers compiler/cc/base.mlx:getWord`: 1240 call sites,
  574 in template.mlx, 360 in cpp.mlx). `sizes mlx-cc` and `memory
  mlx-cc` find nothing: mlx-cc is not among the programs its index
  lists, and its arena is one `mmap` of its own (base.mlx), not
  std.page_allocator's blocks, so std.leaks' memory picture
  (MLX_LEAK_CHECK=1) holds none of it. MLX_CC_STATS is the instrument
  for mlx-cc's memory; it now also counts the tokens that are copies
  and the nodes made inside function bodies.

### Stage 2: where the arena's bytes go

The WebCore unit's 4.5 GB at the end, by MLX_CC_STATS (with a
temporary count per allocation site for the small cells, not kept):

| Bytes | What | Notes |
|---|---|---|
| 1289 MB | 6.19 M AST nodes (208 bytes) inside function bodies | 91% of all nodes; 9% more outside (initializers, default arguments, template arguments) |
| 599 MB | 4.16 M types (144 bytes) | 41% made while instantiating templates; most are `pointerTo`/`constOf`/`referenceTo` copies made per use, which cannot be shared: a type is mutated after it is made (`nullptrOf` sets `isNullptr` on a fresh pointer type) |
| 523 MB | 8.17 M tokens (64 bytes) | 2.73 M (33%) are copies made for template instantiation and macro expansion; the rest the headers' text, kept because every node, object and template pattern points at its token |
| 444 MB | 6690 allocations of 4 KB and more | the fixed tables (names 96 MB, the name-building stack 256 MB, scratch 16 MB), the files' text, the code and data buffers; largely reserved, not resident (RSS is 0.5 GB under the arena) |
| 386 MB | 24.1 M cells of 16 bytes | 22.2 M (355 MB) by `cpp.classAndBases`, a fresh list of a class and its bases for every member lookup; now kept per class until a base list changes (0.5 M cells on the WTF file instead of 0.8 M) |
| 367 MB | 1.83 M kept texts | the texts of tokens made for instances (`template.madeToken`: substituted names, instance keys) |
| 356 MB | 927 k objects (384 bytes) | functions, variables, parameters, temporaries |
| 144 MB | 2.99 M cells of 48 bytes | 1.82 M by `template.readArguments` (the template arguments read, with their tokens for partial specializations) |
| 127 MB | 2.27 M scope bindings (56 bytes) | block scopes' bindings are unlinked when the block ends but never reused |

The prefix alone (an empty unit with the same flags, WebCorePrefix.h
and MlxPrefix.h) costs **25.8 s and 1.7 GB** (6.0 M tokens, 1.8 M
nodes of which 1.6 M in the inline functions of headers, 1.1 M types);
the unit's own ten files and their headers add the other 2.8 GB.

What was done: the padding in Node (216 → 208: `passByStack` beside the
other bytes) and Type (176 → 144: the bytes before the words), the
class lines kept, and 32 KB tables instead of lists where the lists
were walked. What was not done, and why: **releasing a function's AST
after its code is made does not lower the peak**, because every
function's code is made at the end of the unit (`program.genFunctions`,
in declaration order, only for the functions referenced by then), so
the peak is the whole unit's AST; making each function's code when its
definition ends would let the nodes, tokens and bindings of its body be
reused (free lists per cell size; the arena is append-only), but it
changes the order of the functions in the object (an unreferenced
inline function of a header is left out today by a decision taken at
the end), so the gate would have to compare per function instead of
byte for byte. Tokens cannot be dropped either while nodes point at
them (diagnostics during code generation); keeping (file, line) on the
node instead would free the headers' 500 MB after parsing. Both are
stage-3 work on the compiler's structure, not memory management.
Beyond them, the 1.7 GB of the prefix is the same in every unit, and
only a snapshot of the state after it, mapped and shared between the
units (lever 5), brings a WebCore unit under 2 GB resident.

**Parallelism:** a WebCore unit is still 4.15 GB resident at its end
(the JSBindings ones more), so `webkit-wpe-webcore` stays at `-j 2` on
15 GB; every other target (WTF 0.6 GB a unit, JSC 1–2 GB) runs at
`-j 4` (projects/webkit/README.md).

### The gate

- tools/check_cpp.sh (377 C++ programs, on glibc and on mlxlibc),
  tools/check_cc.sh and tools/check_glib.sh (with the unicode headers
  built, so the WTF GLib sources compile too) pass.
- `jsc/UnifiedSource-runtime-1.cpp`, `-bytecode-1`, `-dfg-1`, `-b3-1`
  built by `./mlx build` with the new mlx-cc are byte-identical to the
  objects the baseline made (`cmp`); `wtf/text/WTFString.cpp`,
  `StringImpl.cpp`, `StringBuilder.cpp`, `HashTable.cpp` and the
  WebCore unit likewise, for every variant in the table.
- The WTF probes (w.cpp, y.cpp, z.cpp, sorting through WTF's Vector and
  String against libWTF.a) build to identical binaries and print the
  sorted 1 2 3.
- mlx4 at -O3 miscompiled nothing that these see; no mlx4 bug was
  found (the only -O3 effect is more inlining; the output of every
  variant is identical).

### What remains (stages 3–5)

1. The WebCore unit's time, with the profile of the last row (10880
   samples, `mlxcc-v5-O3-*.profile`): `pendingOuterOf`,
   `definePendingInstance` and `cached` gone, a quarter of the self
   time is **joined-name interning**: `hashOn` 12.6%, `findJoined`
   8.6%, `hashOf` 3.6% (with calls `findJoined` 23%), called from
   `scope.qualifiedKnown` (30% with calls, 5.7% itself) for every
   qualified name resolved by `declspec`/`isTypename`/
   `readQualifiedTypedef` (65–70% with calls: every identifier in the
   headers is looked up to tell a type from a value, by joining the
   scope's prefix and the name into a text and hashing it); then
   `putText` 7.8% (the joined names, instance keys and mangled names
   built byte by byte), the token tests of the parser (`tokens.is`,
   `cursor.isToken` 5%), `ast.make` 2.7% (first touch of the nodes'
   pages), `scope.lookup`/`headOf` (27–31% with calls: the scope
   chains). Class template instantiation is 42% with calls
   (`instantiateClass`), the class bodies 52%. Next: a (prefix, name)
   table in scope.qualifiedKnown instead of joining and interning a
   text per lookup, then a per-scope hash of the bindings instead of
   the chain walk, and instance keys hashed from their arguments
   instead of built as text.
2. Code made per function when its definition ends, with free lists
   for nodes, tokens and bindings (above): halves the peak of a unit
   and would let `-j 4` hold for WebCore on 15 GB.
3. The prefix snapshot (lever 5): 1.7 GB and 26 s (before this branch;
   now less) of every WebCore unit's 4.15 GB and 53 s are the same
   prefix. (Done: "Results: prefix snapshot" below.)
4. For a 40-minute full build on four cores (1250 units): the average
   unit must take ~8 s of processor time; the average was ~30 s (6.4 h
   at `-j 2`) and is ~12 s now if the units gained what the two
   measured did. The three above together are the rest of the path:
   the snapshot removes the prefix from every WebCore unit, the lookups
   and keys a third of what is left, and `-j 4` doubles the throughput.

## Results: frontend (branch mlxcc-speed-frontend, 2026-10-21)

Stages 3 and 4 of the list, worked on beside mlxcc-speed (above) from
the same baseline (30ca2d6) and merged into it at the end. Measured as
above (bash `time`'s user time of the one compile, nothing heavy
beside it), each row after the previous; every row made the same
objects as the baseline, byte for byte.

| mlx-cc (built as the baseline was: default level, no `inline`) | WTF `WTFString.cpp` user | WebCore `UnifiedSource-workers-2.cpp` user |
|---|---|---|
| baseline (30ca2d6) | 4.77 s | 150.9 s |
| 1. a (prefix, name) memo of `findJoined`'s answer, misses included; NRVO choices per function | 3.11 s | 113.6 s |
| 2. a template's instances by key, waiting bodies and outer parameters by object | 3.25 s | 57.6 s |
| 3. + 4. qualified names made from the pair, not from the text; the include search's answers remembered | 2.80 s | 50.8 s |
| 5. what a lookup found remembered by name; directives and inline namespaces by prefix | 2.65 s | 46.4 s |
| 6. a class's line of bases kept by class | 2.86 s | 45.3 s |
| 7. the mangled code of a class name kept by name | 2.63 s | – |
| **merged with mlxcc-speed (48a2f10: `inline` accessors, `-O3`, its own forms of 1b, 2, 4, 6)** | **2.21 s** (48a2f10 alone: 3.07 s) | **35.6 s** (48a2f10 alone: 50.4 s) |
| 8. texts written by the word (`putText`, 15% of the merged profile's self time) | 2.0 s | 32.7 s |

Against the first baseline, then: WTF's file 4.77 s → 2.0 s (2.4×),
the WebCore unit 150.9 s → 32.7 s (4.6×), the two branches together.

Rows 1b (NRVO), 2, 4 and 6 were done on both branches in the same
week; the merge keeps mlxcc-speed's forms (fields of Obj and Class, one
table of instances, its include-path cache) with one difference kept
from here: the bases' version moves on *after* a base is added, not
before `completeIfNeeded` of the base, which may ask for the class's
line meanwhile and would keep it without the base.

### What changed, algorithmically

All of it is the same answer found without the walk; nothing changes
what a name means (the gate below).

- **Qualified-name misses remembered** (base.findJoined, table.mlx for
  the tables). `scope.lookup` asks `prefix::name` for every prefix
  around and every namespace a using-directive names, and almost every
  such pair misses; each miss hashed the joined text and probed the
  names. base now keeps a (prefix, name) → id table with the misses in
  it. A remembered miss must be corrected when the name is made later
  by any route: `intern` splits a new name at its last "::" and, when
  both halves are names, writes the pair; misses are remembered only
  for a `name` without "::" of its own (an instance key with a
  qualified argument, `Vector<WTF::String>`, is asked whole each time),
  so that the last split is always the pair asked. `scope.qualify`,
  which *makes* the name, goes through `base.internJoined`: the hash
  continues from the prefix's stored hash, the text is written once,
  the pair is written into the table without a scan.
- **Lookups remembered by name** (scope.mlx). What `lookupOutward`
  found for a name (the walk over the prefixes and the directives) is
  kept in a per-name slot with a stamp of the context; the stamp moves
  on with the prefix stack, a using-directive, a block left that had
  directives, suspend/resume, and a parent set on an existing name.
  A binding made at file scope forgets its own name's slot (and, for a
  name with "::" in it, everything). Lambdas and local classes, which
  see the function's types through the suspended scopes
  (`exposeSuspended`), keep the walk. The local fast path (a binding
  at depth > 0) comes first, as before.
- **Directives and inline namespaces by prefix**: `findQualified`
  walked every namespace's using-directives and inline namespaces to
  find the prefix's own; both are chains per prefix now.
- **The mangled code of a class name by name** (mangle.classCode):
  every instance key spells its arguments' types, and `emitType`
  split the qualified name at each "::" and spelt the components each
  time; the code is a function of the name's text.
- Per-function NRVO lists, instances by key, pending bodies and outer
  parameters by object, the class line and the include search: as in
  mlxcc-speed's stage 1 (see above; both branches found the same
  walks from the same profile).

### What the counts say (MLX_CC_STATS plus a temporary counter per function, the WebCore unit, after row 5)

| Calls | Function | |
|---|---|---|
| 22.9 M | `cpp.classAndBases` | a fresh list each: 355 MB (row 6 keeps it) |
| 12.5 M | `scope.findQualified` | mostly from `findInherited` (1.7 M) over the bases |
| 11.4 M | `decl.isTypename` | 9.4 M `qualifiedTypedef`: 3.7 M memo hits, 5.7 M real readings |
| 4.9 M | `cpp.injectedClass` | asked at the end of nearly every unqualified name that is no type |
| 4.3 M | `base.intern` | 645 k new names; 1.62 M names in all |
| 4.2 M | `cpp.completeIfNeeded` | |
| 3.4 M | `scope.lookupOutward` | the walks left after the memo (before: every lookup) |
| 1.76 M | `cpp.convertible` | overload resolution: 708 k `chooseInstance`, 434 k `bestOverload` |
| 821 k | `template.instanceKey` | 728 k `classTypeHere`, 400 k `instantiateClassAt` of which 59 k new |
| 392 k | `template.instantiateFunction` | 95 k new, 73.5 k constraints evaluated |

### Stage 4, measured: where instantiation's time is

Instance bodies are not re-lexed: the pattern's tokens are kept and
`cursor.restore` walks them; what `ensureDefinedNow` (23% with calls
after row 6) and `instantiateClass` (77%, nested) spend is the parse of
the instance (`declspec`/`isTypename`/`readQualifiedTypedef`, 100%+
with calls, recursive), which is the semantic work, and whose lookup
side the rows above made cheap. What is left per mention of an
instance is `instanceKey` (15%: the key built as text from the
arguments' mangled types, then interned: 821 k texts of 50–150 bytes)
and `readArguments`/`parseArguments` (48% with calls, mostly the
nested instantiations they cause). Per new function instance,
`constraintHolds` is 31% with calls (requires-clauses evaluated with
their own instantiations), `chooseInstance` 45% (deduction and scoring
per candidate per call), `convertible` 37%. A parsed-once pattern AST
would replace the parse per instance with a substitution per instance;
it is a rewrite of the template reader (the parser reads tokens
everywhere), not done. Caching a constraint's or a conversion's answer
per (template, arguments) would be the next cut, but mlx-cc resolves
names at the point of use, so an answer can change as declarations
are read: not exact, not done.

### The gate (merged compiler against 48a2f10 built the same way)

- tools/check_cpp.sh (380 programs, with three new: `lookupremembered`
  — names declared after a lookup missed them under a prefix, through
  namespaces, classes, using-declarations and -directives, ADL, a
  directive in a block, lambdas and a local class, a namespace
  reopened; `nrvochoices` — returns of a local in place, per function
  around lambdas, local classes and instances; `instancesbykey` —
  instances named many times, members defined after them, bodies that
  wait), tools/check_cc.sh and tools/check_glib.sh pass (380 C++ programs on glibc and on mlxlibc).
- `jsc/UnifiedSource-runtime-1.cpp`, `-bytecode-1`, `-dfg-1`, `-b3-1`
  and `wtf/text/WTFString.cpp`, `text/StringImpl.cpp`, `HashTable.cpp`
  (the flags as projects/webkit/build.mlx composes them) are byte-identical to 48a2f10's objects, their dependency files too; the WebCore unit's object is identical for every row.
- The WTF probes w.cpp, y.cpp, z.cpp print what the baseline's builds
  print (sorted 1 2 3).

Observatory on the result: `unused compiler/cc` names nothing new
(table.mlx's per-template helpers, left over by the merge, were
removed on its word); `copies` found `hasScope` twice (now one, in
base); `callees scope.mlx:lookup` is the memo and `lookupOutward`;
`complex` is as in stage 1 (`readQualifiedTypedef` 130 branches is
untouched: the memo sits in front of it).

### What remains, and the prefix snapshot (stage 5)

The WebCore unit's profile after the merge, before row 8 (4349 samples; self time: `putText` 15%, `joinedEntry` 11.5% — the memo's probes, cache misses on a table of millions of pairs —, `tokens.is` 8%, `ast.make` 7.5%, `putWord` 7%, `cursor.isToken` 5%, `hashOf` 4.4%, `probeJoined` 4%, `completeNestedScope` 4% — the deferred nested classes, a list walked by name for every `N::` step) is now
spread: the accessors' loads (`get32`, `getWord`, `putText`: cache
misses on a 4 GB arena), `instanceKey`, the overload machinery, the
constraint evaluations. None is a walk any more. The large constant
left is the prefix: 25.8 s and 1.7 GB of every WebCore unit (stage 2
above). A design for the snapshot (done, with the changes "Results: prefix
snapshot" below says):

1. **Make the arena's address fixed** (base.init: `mmap` with a hint
   and `MAP_FIXED_NOREPLACE`, failing the run if taken), so that every
   address kept in it — tokens, nodes, types, bindings, the name table
   — is valid in a later process. mlx-cc's state is the arena plus the
   module-level `var`s of its ~30 modules (base's table addresses,
   scope's heads and stacks, cpp's `classByPrefix`, template's lists,
   preprocess's macros and once-marks, the counters), all words or
   small structs; the only addresses that point outside the arena are
   the program's own literals copied into it, which are at the same
   place in the same binary.
2. **Snapshot after the prefix**: a run with `-include` files and an
   empty main file (`--prefix-snapshot FILE`) stops when the main
   file's first token is reached and writes the arena's used part and
   every module's `var`s (a `save`/`restore` pair per module,
   mechanical; a check compiles a unit with and without the snapshot
   and compares the objects). The file is keyed by the compiler
   binary's hash, the flags (`-D`, `-I`, `-std`, `-include`) and the
   dependency list the prefix produced (paths, sizes, mtimes).
3. **Restore**: `mmap` the file at the fixed address, `MAP_PRIVATE`:
   the 1.7 GB is read from the page cache and shared between the
   parallel units until written (copy-on-write), so the unit's own
   resident set starts near 0 and the prefix's parse costs nothing.
   The dependency list restored keeps `-MD` right.
4. **What it buys**: ~26 s of every WebCore unit's time and 1.7 GB of
   its peak (so `-j 4` on 15 GB), one snapshot per flag set (WebCore,
   JSC and WTF differ in their defines; the build makes it as a step
   the units depend on, 30 s and 1.7 GB of disk each). With the units
   at ~20 s of processor time after it and `-j 4`, 1250 WebCore units
   are ~1.7 h; the 40-minute build needs stage 2's code-per-function
   (the peak) and the instance keys and overload work above as well.

## Results: memory (branch mlxcc-memory, 2026-10-21)

The goal was the four WebCore units over the 8 GiB arena
(`JSDOMWindow.cpp`, `UnifiedSource-css-19.cpp`, `-inspector-6.cpp`,
`-dom-7.cpp`) and a WebCore unit well under ~5 GB, for `-j 3`/`-j 4`.
Measured as above: the child's `getrusage` (user time, peak resident
set = VmHWM), MLX_CC_STATS for the arena, one compile at a time, with
the Mlx port's WebCore flags (scratchpad `mem/unit.sh`).

### Where the bytes went: not the finished functions, the unused ones

The plan was stage 2's: make each function's code when its body ends
and give its nodes back. MLX_CC_STATS now also counts the node bytes of
every function's body by what became of the function (made or not;
external, static/weak, constexpr, inline; read in place, a function
template's instance, a member whose body waited), and the largest
bodies. It changed the plan:

| Unit | Body nodes | In functions made | In functions never made |
|---|---|---|---|
| WTF `WTFString.cpp` | 124 MB | 8.5 MB (63 external, 771 inline/static) | 115 MB (7952 inline functions; 104 MB read in place: simde's `static inline`, WTF's inline free functions) |
| `JSDOMWindow.cpp` | 2098 MB | 454 MB | 1644 MB (79904 functions; 1383 MB of all are constexpr: the mpark variant's `visit_alt`/`step_at` instances, 1–2.7 MB each) |

So freeing a function's nodes after its code is made would have given
back a fifth of the body nodes at most: four fifths belong to inline
functions of headers that no code of the unit uses, read only because
C++ makes an inline function's code where it is used and mlx-cc read
every body where it stands. The fix is not to read them.

### What changed

1. **Inline bodies wait for a use** (cpp.lazyBodies, decl.waitsForUse,
   template.pendDefinition). An inline member defined in its class, an
   inline or static function at namespace scope and an inline member
   defined outside its class keep their body's tokens, as class
   template instances' members already did (cpp.pendBody), and the
   body is read when a call or an address needs it
   (template.ensureDefined: at the end of the unit with the instances,
   or at once when the call is computed or its result type deduced),
   under the prefix it stood in, with the definition's type (its
   default arguments and parameter names given to the function at
   once). The bodies nothing uses are never read: no nodes, objects,
   types, instances or literals for them. Not: virtual functions (the
   vtable names them), constructors and destructors with a VTT entry,
   a local class's members, C.
2. **constexpr bodies wait until computed**: a call names the function
   and its body waits like the others; the constant evaluator reads it
   when it computes a call (template.defineForEvaluation from
   interp.call, interp's nested calls, isConstantCall, eval.wholeCall).
3. Fixes the units needed, two of them old: a static data member of a
   class template instance's type with an initializer makes the
   instance whole (only an unread body made it so:
   SVGBoundingBoxComputation in inspector-6); std::optional's `operator=
   (U &&)` constrained as the standard's (a Markable<WallTime> goes
   through its conversion to optional: dom-7's Document::lastModified
   failed with the baseline too); memberOf links the alias chain a
   nested WTF::Visitor level made instead of copying it at every level
   (n × n aliases of 408 bytes per lookup before).

`MLX_CC_READ_ALL_BODIES=1` reads every body where it stands, as before;
with it WTFString.cpp's object is the baseline's byte for byte.

### The table

| Unit | Peak before | Peak after | User time before | User time after |
|---|---|---|---|---|
| WTF `WTFString.cpp` | 603 MiB (arena 1296 MB) | 397 MiB (1091 MB) | 2.1 s | 1.4 s |
| `UnifiedSource-workers-2.cpp` | 4348 MiB (arena 5057 MB) | 2472 MiB (3112 MB) | 32.9 s | 19.9 s |
| `JSDOMWindow.cpp` | 7461 MiB (arena 8194 MB of 8590) | 3456 MiB (4095 MB; change 1 alone) | 103.8 s | 29.8 s |
| `UnifiedSource-inspector-6.cpp` | 7272 MiB (arena 8040 MB) | 3577 MiB (4216 MB) | 56.6 s | 31.8 s |
| `UnifiedSource-dom-7.cpp` | failed at 46 s, 5953 MiB (the optional error above) | 3935 MiB (4565 MB) | – | 35.1 s |
| `UnifiedSource-css-19.cpp` | arena full in 16 s | arena full in ~30 s | – | – |

(The "after" rows of the units are commit 5d8ee1b's compiler, before
the merge of webkit-port-m2 at 51d882c; WTFString.cpp and workers-2
are the merged compiler's.)

So three of the four compile, at half the peak and a third to a half
of the time. A WebCore unit now peaks at 2.5–4 GB instead of 4.3–7.5
GB: WebCore can build at `-j 3` on 15 GB (three of the largest at once
are 12 GB) and mostly at `-j 4` (the average unit is ~2.5 GB; four of
the largest at once would not fit).

**css-19 is not a memory problem**: its arena fills with 13–16 million
allocations of 408 bytes in 16–30 s with the baseline and with this
branch alike, a runaway, not a unit too big. Sampled with gdb, they are
overload-set aliases (cpp.aliasChain) made by memberOf for the call
operator of a 25-lambda WTF::Visitor (StyleRule.cpp's
`StyleRuleBase::visitDerived(WTF::makeVisitor(...))`, 25 cases each
through `std::invoke`), 24 levels deep, from `callOperator` in the
instances' bodies. The n × n copying is gone (change 3) and a chain
kept per class (tried, not kept) did not stop it: the lookups that
allocate are not repeated ones from outside but something within the
instantiation; raising ARENA_SIZE would not help. Next step: count
`memberOf`/`callOperator` calls per instance of `visitDerived` to find
the loop.

Found (branch m2-fixes2): Visitor's `using Visitor<A>::operator();
using Visitor<B...>::operator();` name both bases by the template's
name, and each using-declaration recorded both bases, so every level
held each base twice and one lookup of operator() walked 2^n bases
(16 lambdas: 1.5 million aliases; 25: the arena). Each base is now
recorded once (cpp.usingKnown); css-19 compiles in 14 s.

### The gate

tools/check_cpp.sh (471 programs after the merge, two new:
`bodieswait` — bodies called before and after their definitions, by
address, through pointers, from constant expressions, with default
arguments the definition gives, members in and out of their class,
namespaces with using-directives, virtual members, recursion, `__func__`
and static locals, a static member only an unused body made whole;
`optionalfromconversion`) and tools/check_cc.sh pass. WTFString.cpp
makes the same 814 functions (815 before the merge) with the same
instructions (compared function by function, lambda numbers and
addresses aside: scratchpad `mem/objdiff.py`); they are in another
order, the data is smaller (the literals and static locals of the
unread bodies are gone), and `__mlxcc_init_unit`'s frame is smaller
(the temporaries of those bodies' initializers). Lambdas are numbered
in reading order, so a lambda in an inline function has another number
in its mangled name than before (as before, the number is the unit's,
not the program's).

## Results: prefix snapshot (branch mlxcc-prefix, 2026-10-22)

Stage 5 of the list above, done: `mlx-cc --write-prefix-snapshot SNAP
FLAGS -include P.h` writes the compiler's state after the `-include`
files; `mlx-cc --prefix-snapshot SNAP FLAGS -include P.h -c unit.cpp`
maps it back and reads only the unit's own file
(compiler/cc/snapshot.mlx, compiler/cc/README.md "Prefix snapshots").
Measured as the memory branch did (the child's `getrusage`: user and
system time, peak resident set; MLX_CC_STATS, which now also prints the
peak and the private and file-backed resident sets at the end), one
compile at a time, beside a full WebKit build at `-j 3` on the same
machine, with the Mlx port's flags and the generated headers of that
build (scratchpad `pfx/`: flags.py, one.sh, final.sh, mem2.sh).

### What a snapshot is

- **The arena** (every token, node, type, object, binding, macro,
  name, file text and table of the compilation): it is now mapped at a
  fixed address, 0x300000000000, with MAP_FIXED_NOREPLACE (anywhere,
  and no snapshots, when that is taken), so the addresses it holds are
  valid in another run of the same binary. The file holds its used
  pages, zero pages left as holes; a reader maps them MAP_PRIVATE and
  MAP_FIXED over its own arena, so they come from the page cache, shared
  by the units built at once, and are copied only where a unit writes.
- **Every module-level variable** of the compiler's 28 modules with
  state (about 470: the name table's and file table's addresses, scope
  heads and stacks, the macro table, pending and deferred bodies, the
  template instance tables, the lambda and instance counters and
  per-context lambda numbers, the include search's remembered answers,
  `#pragma once` marks, `-MD`'s list), by address and size: each module
  has a `keepState()` that tools/cc_snapshot_state.py writes from its
  declarations, and check_cc.sh fails when one is out of date, so a new
  variable is in the snapshot unless its line says `// (per run)`.
  The per-run ones: the arena's own bookkeeping, the object writer's
  encoder (its buffers are the process's heap), the link's inputs and
  libraries, -s and the map path, the MLX_CC_STATS counts, the
  constexpr trace switch.
- **Not the run's own memory.** Everything the command line makes (the
  file names, -D and -I texts, the link's inputs) is allocated in a side
  region after the arena (base.beginSide), and the include directories
  and macros are given to the compiler after the command line was read.
  So the arena when the prefix starts is the same in every run with the
  same options, whatever `-o`, `-MF` or the file are, which is what makes
  a snapshot valid for every unit of a target.

What changed in the compiler to make that true, in every run (with a
snapshot or without, so the objects are the same either way):

- **The unit is read in two parts**: the -include files are
  preprocessed and parsed, then the file is preprocessed and parsed
  (before, the whole unit was preprocessed, then parsed). A snapshot is
  the state between the two; the writer refuses when the prefix ends
  inside a namespace, `extern "C"` or a block.
- **The prefix does not depend on the unit**: the file's path is given
  to the compiler after the prefix (decl.startUnit), and while the
  prefix is read the unit's hash, which names its lambdas' closure
  classes and its unnamed namespace (`_GLOBAL__N_...`), is the hash of
  the prefix's identity (its options in order, the language). A prefix
  lambda's class is then the same class in every unit of the target,
  which is right: it is the same lambda of the same header. Lambdas
  numbered in their context (the Itanium `UlvE_` numbers) continue from
  the restored tables; the unnamed namespace is named when first opened.
  The unit's globals made by the prefix carry file 0 (a message about
  one names the unit). The prefix's warnings are kept and said again by
  a reader.
- **`-MD`'s text is made in the side memory, at once.** It was joined a
  name at a time, the whole text again for each of a WebCore unit's
  ~3000 headers: 2.3 GB of arena for JSDOMWindow.cpp (peak 5.8 → 3.5 GB)
  — and its size, which the object's name changes, moved everything
  after it in the arena. That mattered because **objects hold compiler
  addresses**: words of data the constant evaluator filled and a
  relocation then names (JSDOMWindowTable's `values`) keep the
  evaluator's own pointer (a tagged arena address). The linker
  overwrites them (RELA), so programs are right, but two runs whose
  arenas differ make objects that differ there (JSDOMWindow.cpp with
  `-o x.plain.o` and `-o x.snap.o` did, before the fix). Zeroing those
  words in program.mlx broke tests/gnuforms.c, so some path reads them:
  left as it is, to look at (it is why the arena layout must be
  identical, not just equivalent).

### How a reader checks it (else it reads the prefix and says why)

- **The key**, compared as text: a hash of the compiler's binary and its
  path (which finds the system headers), the working directory, the
  language, `MLX_CC_INCLUDE` and `MLX_CC_READ_ALL_BODIES`, and every
  option that changes the compilation in its order (-I, -isystem, -D, -U,
  -include, -std=, and any option mlx-cc does not know); not -o, -c, -E,
  -MD/-MMD/-MF/-MT/-MQ, -x (the file's language is in the key), -L, -l,
  the link's options or the files. The note names the first line that
  differs (`another command line or compiler: option -std=c++17`; the
  measurement scripts' first memory runs were refused for `in <another
  directory>`).
- **The variables' layout**: the same addresses and sizes.
- **Every file the prefix read**: read again, its size and a hash of its
  contents compared. A file written again with the same contents has
  another inode: its `#pragma once` mark follows it (a regenerated
  header that did not change does not cost the snapshot).
- **The include search's answers**: for each include directory (and
  each directory a quoted #include looked next to its file in), its
  modification time and identity. Where one changed, every name the
  search did not find there, before where it was found, is looked for
  again (a header new there would now be found first), the one found
  there must still be there, and a quoted include's miss next to its
  file must still miss. (Clang's PCH checks the files read, not the
  misses.)

tools/check_cpp.sh: the six tests of two files (lambda names in inline
functions and initializers, explicit instantiations in another file,
statics of inline functions, typedef names of unnamed classes, constexpr
members) compiled with their header as the prefix, from a snapshot and
without, objects compared and the program run; four programs cut before
`main` (deleting destructors, explicit instantiation definitions,
generic lambdas, member templates in the prefix) likewise; a changed
header, a header the search would now find first, and other options are
refused (and the object is the one made without), while a header written
again with the same contents and a new header in a directory the search
never reaches are not.

### The build

projects/webkit/build.mlx `usePrefixSnapshot(set, flags)`: for JSC's,
PAL's and WebCore's C++ flags, one step per (target, flag set) runs
`mlx-cc --write-prefix-snapshot OBJDIR/prefix.snapshot -MD -MF ...
FLAGS -x c++`; it names mlx-cc as its input (the snapshot is the
compiler's memory: any change of the binary makes it again) and the
prefix's headers by its depfile. Every unit compiled with those flags
(the same flags text) gets `--prefix-snapshot` and runs after the step
(`dependOn`), but the snapshot is not one of the unit's inputs: an
object made with it is the object made without it, so it is in no
object's key, and a unit whose snapshot is stale reads its prefix itself.
std.build gets `uncached(step)` (a command with a depfile stays out of
the result cache): the snapshots are 0.8–1.7 GB each (sparse: 0.1–1 GB
on disk) and would push the objects out of its 6 GB. WTF has no
`-include` in its flags (its prefix, config.h, is 0.26 s of a 1.6 s
unit) and Skia and bmalloc have no common prefix: unchanged. With
branch build-incremental's codegen epoch: this branch changes objects
(the prefix's lambda names, the arena's layout in the words above), so
the epoch must be bumped when the two meet; the snapshot step should
keep keying on mlx-cc's bytes, not the epoch.

### The numbers

The snapshots (written by the step above; their `-MD` file lists the
prefix's headers):

| Prefix | Read and written | Size (on disk) | Taken up (an empty unit) | Read (an empty unit, no snapshot) |
|---|---|---|---|---|
| WTF `-include config.h` | 0.3 s user, 0.4 s sys | 817 MB (96 MiB) | 0.01 s, 0.03 s | 0.16 s, 0.10 s |
| JSC `JavaScriptCorePrefix.h` | 3.4 s, 3.3 s | 1.40 GB (703 MiB) | 0.27 s user, 0.22 s sys | 4.1 s, 0.6 s |
| WebCore `WebCorePrefix.h` + `MlxPrefix.h` | 6.2 s, 5.1 s | 1.68 GB (981 MiB) | 0.33 s, 0.31 s | 5.8 s, 1.1 s |

(Taking one up is mostly the check: reading and hashing the prefix's
~1000 headers. Writing is the prefix read once more plus the pages
written.)

The units, user time / peak resident set, without and with the snapshot;
every object byte for byte the same, and the `-MD` files the same
(compiler 6ba601c; 8cf4046, which only names a snapshot's first
-include file once in its `-MD` file, rechecked on WTFString, JSC
runtime-1, WebCore css-1 and workers-2: the same):

| Unit | Without | With | |
|---|---|---|---|
| WTF `wtf/text/WTFString.cpp` (`-include config.h`) | 1.33 s / 399 MiB | 1.07 s / 391 MiB | |
| JSC, 11 units (runtime-1, -20, API-1, bytecode-1, dfg-1, b3-1, heap-1, jit-1, parser-1, wasm-1, yarr-1) | 4.21 s / 849 MiB (mean; 3.6–5.2 s) | 0.86 s / 579 MiB (0.23–1.98 s) | 4.9× |
| WebCore `UnifiedSource-workers-2.cpp` | 17.4 s / 2526 MiB | 12.6 s / 2208 MiB | |
| `css-1` | 9.5 s / 1539 MiB | 4.9 s / 1193 MiB | |
| `html-3` | 10.0 s / 1388 MiB | 3.2 s / 1061 MiB | |
| `rendering-5` | 12.7 s / 1709 MiB | 6.6 s / 1384 MiB | |
| `page-2` | 15.5 s / 2224 MiB | 11.3 s / 1907 MiB | |
| `loader-1` | 8.9 s / 1356 MiB | 2.7 s / 1024 MiB | |
| `platform-10` | 7.5 s / 1113 MiB | 0.9 s / 758 MiB | |
| `JSBindings-5` | 19.4 s / 2568 MiB | 13.2 s / 2393 MiB | |
| `svg-2` | 15.0 s / 2077 MiB | 9.6 s / 1754 MiB | |
| `dom-7` (the largest) | 33.9 s / 4014 MiB | 29.4 s / 3708 MiB | |
| `JSDOMWindow.cpp` | 27.9 s / 3508 MiB | 20.2 s / 3338 MiB | |
| WebCore, the 11: mean user + sys | 18.3 s | 11.7 s | 1.6× |

So a WebCore unit saves 4–8 s of user time (5.7 s on average, the
prefix being 5.8 s) and 0.9 s of system time, and a JSC unit 3.4 s of
its 4.8 s; what is left of a small unit (platform-10, most JSC units) is
its own code. The peak drops
by the part of the prefix the unit never touches; the private memory
(MLX_CC_STATS at the end) by more, since the pages a unit only reads stay
the page cache's, one copy for all the units built at once:

| Unit | Without: peak (private) | With: peak (private + snapshot pages) |
|---|---|---|
| WTFString.cpp | 399 MiB (398) | 390 MiB (361 + 29) |
| JSC runtime-1 | 1011 MiB (1009) | 748 MiB (520 + 228) |
| WebCore workers-2 | 2528 MiB (2527) | 2198 MiB (1852 + 346) |
| JSDOMWindow.cpp | 3512 MiB (3510) | 3336 MiB (2838 + 498) |
| dom-7 | 4020 MiB (4018) | 3707 MiB (3351 + 356) |

### What it gives the full build

The Mlx port's library objects: WebCore 584, JSC 207, PAL 5 (snapshot
steps), WTF 172, bmalloc 168, Skia 835 (no common prefix, unchanged).
From the sample's means (user + system per unit; the WebCore sample
holds the two largest units, so it is if anything heavy):

| | Units | Without | With |
|---|---|---|---|
| WebCore | 584 | 18.3 s each: 10 690 s | 11.7 s: 6 830 s (+ 11 s for the snapshot) |
| JavaScriptCore | 207 | 4.8 s: 1 000 s | 1.2 s: 240 s (+ 7 s) |
| together, CPU | | 3.25 h | 1.97 h (−39%) |
| at `-j 3` | | 65 min | 39 min |
| at `-j 4` | | 49 min | 30 min |

(WTF, bmalloc, Skia and the links come on top of both columns, as they
are.) Whether `-j 4` holds is the memory: with the snapshot a WebCore
unit's private memory is about 0.67 GB less than without it (workers-2
1.8 GB, dom-7 3.3 GB; the sample's peaks 0.76–3.7 GB, ~1.7 GB typical)
plus the snapshot's pages once for all of them (≤ 0.5 GB), so four units
fit in 15 GB unless three of the largest (dom-7, JSDOMWindow,
inspector-6, css-19) run together.

### Found on the way

- **mlx4's inliner captures names**: a function inlined into its caller
  reads the caller's local where its own module has a variable of the
  same name. `other.mlx`: `var dirs: usize = 7` and `pub fn get(index:
  usize) -> usize { return dirs + index }`; a caller with `const dirs =
  arguments.length` and `other.get(0)` gets 1, not 7, at the default
  level and at -O3 (scratchpad `inl/`). snapshot.mlx's first version
  crashed on it (`paths.directoryName` inlined into a function with a
  local `dirs`); its names are now apart. Any caller in mlx-cc with a
  local named like an inlined callee's module variable is affected:
  worth fixing in ir/inline.mlx before it bites silently.
- The `-MD` text and the compiler addresses in relocated data words
  (above).

## Allocators (notes, 2026-10-21; nothing changed yet)

docs/reference/stdlib.md says a program's blocks come from
`std.general_purpose_allocator` (size classes out of 256 KiB slabs,
free lists, one lock) over `std.page_allocator` (mmap), with
`std.tinypage_allocator` for blocks up to 2 KiB and
`std.debug_allocator`/`std.leaks` watching every block under
MLX_LEAK_CHECK. What the tree does, by `mlx-codemap refs
page_allocator.mlx:init` and `importers` at 48a2f10:

| Allocator | Who | Count |
|---|---|---|
| `std.page_allocator.init()` as the program's allocator | 230 files, 345 call sites | tests 85, coreutils 44 (ls 7, ownership 2, one each in 35 others), desktop 28 (capture 7; splash, notifications, lock, ipc, greeter, audio 2 each; topbar, terminal, settings, permissions, login, launcher, files, dock, compositor), tools 25 (codemap 7, network-test 4, 14 others), std 16 (src 9, registry 3, bootstrap 3, protocols 1), compiler 11 (selfhost 5, cc 4, libc 2), experiments 8, examples 6, other projects 8 |
| `std/bootstrap/page_allocator.mlx` imported directly (not as `std.page_allocator`) | 44 modules | 27 tests, selfhost 5, cc 4, std 8 (bootstrap 5, src 3) |
| `std.general_purpose_allocator.init()` | 5 call sites | compiler/libc/heap.mlx (mlxlibc's `malloc`) and 4 tests; imported only by std's two roots |
| `std.arena_allocator.init()` | 3 call sites | compiler/selfhost/driver/pipeline.mlx, tools/codemap/index.mlx (one arena of 3 GiB reserved), a test |
| `std.debug_allocator.init()` | 2 | std.leaks and its test; `page_allocator.watch` is called by the debug allocator alone |
| `std.tinypage_allocator` | 1 | page_allocator.mlx (every block up to 2 KiB) |

So the general-purpose heap serves C programs through mlxlibc and no
Mlx program: every program's heap is the page allocator, which puts
small blocks on tiny pages (64-byte runs) and gives whole pages to the
others. A program takes it where it needs memory (`const allocator =
std.page_allocator.init()`, 345 times) rather than passing one
allocator around; nothing is `deinit`ed. Two of the three programs
measured below are not watched at all: `mlx-codemap`'s commands
return before `crash.install()` (projects/observatory/main.mlx:1107,
the command path; the panel installs it), and mlx-cc installs nothing
(the diagnostics-everywhere branch adds these lines).

**Observatory's numbers** (MLX_LEAK_CHECK=1, then `mlx-codemap memory
PROGRAM`; the resident peak is VmHWM polled from /proc while the
program ran without the check; ls from the 2026-10-18 build,
mlx-observatory built from 48a2f10, mlx-cc from the 2026-10-21 build):

| Program, run | Blocks, bytes asked, in pages | Tiny pages | Waste per size class | Address space | Resident |
|---|---|---|---|---|---|
| `ls -lR projects std` (coreutils) | 118 blocks, 41 KiB asked, 51 KiB in pages (18% wasted); 4939 allocations, 4821 frees | 113 blocks on 5 pages, 37% full | below 256 B: 112 blocks, 1735 B in 7616 B (77% wasted); below 4 KiB: 4 blocks, 12 KiB in 16 KiB (25%); below 64 KiB: 2 blocks, 28 KiB (0%) | 13 mappings, 1 hole of 20 KiB, fragmentation 0.000 | 9616 KiB in the snapshot; 7.8 MiB peak; `values` (the compiler's value arena) 22 MiB |
| `mlx-codemap unused` (the whole tree, 4.0 s) | no snapshot: not watched (above); and its index is one `std.arena_allocator` over the page allocator reserving 3 GiB (tools/codemap/index.mlx `ARENA_BYTES`), so watched it would show one block | — | — | — | 550 MiB peak |
| `mlx-cc -c wtf/text/WTFString.cpp` (2.7 s) | no snapshot: not watched, and its arena is a raw `mmap` of 8 GiB MAP_NORESERVE (compiler/cc/base.mlx:41) bump-allocated in 16-byte steps, never freed; MLX_CC_STATS at the end: 1234 MiB arena bytes, of which 406 MB in 502 allocations of 4 KiB and more (the fixed tables, largely untouched), 185 MB 2.89 M tokens (64 B), 154 MB 738 k nodes (208 B), 35 MB objects, 26 MB cells of 48 B, 25 MB types | — | — | — | 587 MiB peak |
| `tests/support/leaks_program` (the fixture; the machinery works) | 5 blocks, 24 KiB asked, 24 KiB in pages (0% wasted) | — | below 64 KiB: 5 blocks (0%) | 9 mappings, 0 holes, fragmentation 0.000 | 560 KiB |

What the ls row says about the page allocator as a heap: the blocks
under 256 bytes are where the waste is (112 of 118 blocks, 77% of
their tiny pages unused, since a 20-byte path takes a 64-byte run),
the few larger blocks waste a quarter, and nothing is fragmented
because nothing is freed and reused. The general-purpose heap's
16-byte classes would put those 112 blocks in about 2.3 KiB of a slab
(about 25% wasted by the class rounding instead of 77%), and its free
lists would reuse what ls frees (4821 of 4939 blocks); resident memory
would not move, since a slab is touched a page at a time.

**The idiom proposed (not done; "not yet"):** one heap per program,
`std.heap`: `std.heap.allocator()` returns the Allocator interface of
one `general_purpose_allocator` over `std.page_allocator`, made on
first use so that a program keeps taking it where it needs memory (the
345 `std.page_allocator.init()` sites become `std.heap.allocator()`,
a mechanical `mlx-codemap rewrite`), `std.heap.deinit()` gives the
slabs back where a program ends on purpose, and under MLX_LEAK_CHECK
`std.leaks` wraps the heap in the debug allocator instead of watching
pages, so the leak check and Observatory see every block with its
place and not the heap's 256 KiB slabs (tiny pages then hold only the
heap's own small blocks, if any). What stays on raw pages: the
compiler's bootstrap (std/bootstrap's own files and the selfhost
driver, which the heap is built on), the loader, large mappings (files
read whole, framebuffers), and the arenas' backing, which should go
through `std.page_allocator`'s interface rather than their own `mmap`
so a snapshot lists them: for mlx-cc, `arena_allocator` over the page
allocator keeps the bump semantics and zeroed fresh pages if the
reservation stays one 8 GiB MAP_NORESERVE block (the page allocator
would have to pass MAP_NORESERVE above some size, or the arena grow in
chunks, which costs a bound check it already makes), and its fill
reported through std.diagnostics as std.gpu reports device memory, so
`memory mlx-cc` shows the arena's bytes by kind as MLX_CC_STATS does.
Two things to settle first: a block taken in one image and freed in
another (the compositor's modules, plugins; tiny pages solve it with
one region at a fixed address shared by every image, a per-image heap
does not), and the heap's one lock, which every block of every thread
would take, against today's one mmap per large block and the tiny
pages' own lock for the small ones (the tinypage test's four threads,
the GPA test's four).

## Proposal: the linker as a program of its own, mlx-ld (2026-10-22; not done)

Every WebKit object step names `mlx-out/bin/mlx-cc` as an input
(`Build.input(b, step, tc.cc)` in projects/webkit/build.mlx), and an
input's key is its contents: any change to the mlx-cc binary compiles
all of WebKit again (about 2000 objects, hours). The linker
(compiler/selfhost/object/link.mlx, archive.mlx, elf64_dynamic.mlx) is
compiled into mlx-cc (compiler/cc/program.mlx calls `link.writeProgram`
in the same process), so a change that only touches linking (the symbol
table of branch linker-symtab, a link map, a relocation kind) still
changes mlx-cc's bytes and throws away every object it never affected.

The split:

- **mlx-ld** (compiler/ld/main.mlx): link.mlx, archive.mlx,
  elf64_dynamic.mlx and the options that are the linker's: `-o`, the
  objects and archives, `-L`/`-l`/`-l:`, `--dynamic-linker`,
  `-Wl,-Map=`, `-s`/`--strip-all`. It reads relocatable objects only,
  as it does now.
- **mlx-cc** keeps the driver: `mlx-cc -o prog a.c b.o -lfoo` writes the
  object of the `.c` files as now (`program.buildObject`), to a file in
  `$TMPDIR` instead of handing it over in memory, and runs mlx-ld on it
  and the other inputs (posix_spawn, the arguments passed through), as
  `cc` runs `ld`. `-c` never involves mlx-ld. The C++ library it adds
  (`-lmlxcpp`, `addCppLibraries`) becomes an argument like any other.
  mlx-cc finds mlx-ld next to itself (as it finds `include/`), or by
  MLX_LD.
- The build: `cc()` in build.mlx declares a third program of the group,
  `mlx-ld` (compiler/ld/main.mlx), and `webkit.Toolchain` gets `.ld`.
  Object steps keep `Build.input(step, tc.cc)` and depend on mlx-cc
  only. Link steps (the archives' programs, webcore-test, webcore-dump,
  the tools') add `Build.input(step, tc.ld)` and depend on the mlx-ld
  step; better still, they call mlx-ld themselves with the objects (the
  driver adds nothing to a link of objects alone but `-lmlxcpp`).

What the cache keys would then depend on:

| Step | Key (besides the step's line and sources, headers by -MD) |
| --- | --- |
| an object (`mlx-cc -c`) | the bytes of mlx-cc: preprocessor, parser, code generator, object writer (relocatable.mlx) |
| an archive (`mlx-ar`) | the objects, mlx-ar |
| a program or shared library | the objects and archives, mlx-ld (and mlx-cc while the step goes through the driver) |

So a change to link.mlx, elf64_dynamic.mlx or archive.mlx relinks the
programs (seconds to a minute each for WebKit's) and compiles nothing.
A change to relocatable.mlx (the object format, which both read)
changes both binaries, and objects are rebuilt, which is right: their
bytes change. The Mlx compiler (mlx4) is untouched: it links its own
programs in-process and keys its steps on its own hash as now
(cache.mlx `compilerHash`).

A finer key for objects, independent of the split and complementary:
mlx-cc could print a code generation version (a hash of the modules
that decide an object's bytes, computed when mlx-cc is built), and
object steps would key on that instead of the binary, so that a
diagnostic's wording or a `--version` string changes nothing either.
That needs the build to read a tool's version rather than hash a file
(a `Build.inputVersion(step, tool, "--codegen-hash")`), which it cannot
yet.

Costs: one more process per link (nothing next to a WebKit link's
seconds); the in-memory object of `mlx-cc -o prog x.c` is written and
read back (the size of one unit's object); mlx-cc and mlx-ld must agree
on the object format, which they share by source (relocatable.mlx), and
`tools/check_link.sh`, `check_cc.sh` and `check_cpp.sh` cover both
paths as they do now.
