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
