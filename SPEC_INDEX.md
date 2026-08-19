# Zin 1.0 normative specification index

Specific normative rules override general rules. An unresolved contradiction must be recorded in `SPEC_CONFLICTS.md`; the implementation must never silently invent behavior.

1. `spec/00-language/grammar.ebnf` — syntax
2. `spec/00-language/lexical.xml` — lexical/newline rules
3. `spec/00-language/operators.xml` — complete arithmetic/bitwise/shift operator algebra
4. `spec/00-language/types.xml` — primitive/compound type semantics
5. `spec/00-language/aggregates.xml` — structs/enums/unions
6. `spec/00-language/ownership.xml` — `@nocopy`, `@move`, initialization/move state
7. `spec/00-language/unsafe.xml` — explicit unsafe boundary
8. `spec/00-language/control-flow.xml` — control flow and cleanup
9. `spec/00-language/errors.xml` — error sets/unions
10. `spec/00-language/comptime.xml` — CTFE/generics/reflection
11. `spec/00-language/vectors.xml` — SIMD vectors
12. `spec/00-language/atomics-tls.xml` — atomics/TLS
13. `spec/00-language/modules.xml` — modules/visibility/init
14. `spec/00-language/memory-model.xml` — lifetime/UB/overflow
15. `spec/01-abi/*` — internal and foreign ABIs
16. `spec/02-compiler/*` — compiler pipeline/LIR/backend, `LIR_SEMANTICS.md`, and normative diagnostics
17. `spec/03-formats/*` — executable/debug formats
18. `spec/04-stdlib/*` — standard library, including std.json/std.xml/std.wayland
19. `spec/05-build/*` — build/package system
20. `spec/06-wayland/*` — stdlib-bootstrap Wayland protocol materialization/client/server/transport
21. `spec/07-tests/*` — conformance expectations
