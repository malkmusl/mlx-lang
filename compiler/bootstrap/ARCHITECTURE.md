# zin0 module layout

Each directory owns one compiler responsibility and exposes a small `root.zig`
facade. New functionality belongs in a focused file below the owning directory;
the large compatibility units are reduced incrementally and must not gain new
unrelated responsibilities.

```text
source/          files, locations and structured diagnostics
syntax/          tokens, lexer, AST and parser
modules/         import classification, package lookup and module graph
semantic/        types, scopes, ownership, comptime, reflection and sema
ir/              target-independent LIR and lowering
backend/x86_64/  ABI, register allocation, encoding and code generation
object/          ELF objects, linking and executable layout
platform/linux/  raw syscalls and host abstractions used by zin0
driver/          command dispatch and compilation pipeline
```

Dependencies point down the pipeline. Syntax never imports semantic code;
semantic code may import syntax and modules; IR may import semantic code; the
backend may import IR; object emission may import the backend encoder. Platform
code is independent of the language front end.

## Focused compiler modules

The pipeline entry files are deliberately small facades. They own shared state,
dispatch by AST/LIR tag, and delegate feature-specific work to modules in the
adjacent directory. A new language feature should extend the focused module
that owns it instead of growing the facade again.

### Parser

`syntax/parser.zig` owns token navigation, diagnostics, AST storage and the
top-level dispatcher. Its parser modules are grouped by grammar responsibility:

```text
syntax/parser/
  declarations.zig          const, var and function declarations
  statements.zig            statement dispatch and block parsing
  control_flow.zig           if, while, for, break and continue
  expressions/
    root.zig                 expression precedence entry point
    literals.zig             scalar and aggregate literals
    primary.zig              identifiers, grouping, calls and postfix forms
  types/
    root.zig                 type-expression dispatcher
    type_expression.zig      pointers, slices, arrays and error unions
    aggregate.zig            struct, enum, union and error-set declarations
```

### Semantic analysis

`semantic/sema.zig` owns the semantic state maps and the central node
dispatcher. Analysis is split into modules that can be tested and evolved
without loading the full semantic facade:

```text
semantic/
  declarations.zig          bindings, function prototypes and bodies
  functions.zig             function lookup, declaration and parameter binding
  builtins/analyze.zig       builtin validation and result typing
  types/resolve.zig          annotation and builtin-name type resolution
  expressions/names.zig     identifiers, fields and module namespaces
  expressions/call.zig      ordinary and generic function calls
  control_flow/analysis.zig control-flow facts used by return checking
  control_flow/analyze.zig  blocks, unsafe blocks and loops
  control_flow/return.zig   return-value validation
  ownership/state.zig       move-state source tracking
```

### LIR lowering

`ir/lower.zig` owns LIR storage, common emission helpers and tag dispatch.
Feature lowering lives below `ir/lowering/`; in particular, bindings, builtins,
functions, calls, returns and loops no longer share one monolithic switch body.
The existing aggregate, cleanup, conditional, lvalue, match, operator, postfix
and prefix modules remain the owners of their respective operations. Tuple
literals are materialized by `lowering/aggregate.zig`; function calls keep the
value as an aggregate address until the target ABI classifies the return.

### x86_64 backend

`backend/x86_64/codegen.zig` owns allocator/frame state and coordinates the two
output modes. Instruction emission is split by output and encoding concern:

```text
backend/x86_64/codegen/
  text.zig    NASM text instruction emission
  binary.zig  raw x86_64 machine-code instruction emission
  memory.zig  shared pointer, byte, SSE and payload-copy encoders
```

`zincc` returns integer-compatible tuples up to 16 bytes in `rax`/`rdx`.
Larger tuples use caller-owned storage through the hidden first argument, so no
pointer into a completed callee frame can escape.

`backend/x86_64/instructions/` remains the location for instruction-family
implementations shared by both output modes.

## Module boundary rules

- Facades own mutable compiler state; focused modules receive the facade as a
  generic context and operate only on the state required for their feature.
- Syntax modules must not depend on semantic, IR or backend modules.
- Semantic modules may recurse through `Sema.analyzeNode`, but do not import the
  `Sema` facade itself; this avoids import cycles.
- Lowering modules may recurse through `LirBuilder.lowerNode` and emit through
  its public helpers, but do not own LIR storage.
- Backend output modules share allocation and frame state through `X86Gen` so
  text and binary generation keep identical operand assignment.
- Keep facade files below roughly 500 lines. Split a new responsibility before
  it turns into another cross-feature switch body.

## Regression checks

The minimum structural-change verification is:

```sh
zig fmt compiler/bootstrap
zig build
zig build test --summary all
```

Changes to lowering or code generation also compile and execute representative
programs from `tests/`, covering control flow, calls/imports, aggregates, error
unions and generic instantiations.
