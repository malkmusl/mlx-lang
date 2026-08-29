# Zin 1.0 normative conflicts

## Open normative gaps

These are omissions rather than contradictions, but they prevent a conforming
implementation from choosing behavior without designing new language rules.

### Atomic builtin call shapes

`spec/00-language/atomics-tls.xml` names `@atomicLoad`, `@atomicStore`,
`@atomicRmw`, `@cmpxchgWeak`, `@cmpxchgStrong`, and `@fence`, and defines the
available memory orders. It does not define argument order, result types, the
representation of an RMW operation, or the valid order combinations per
builtin. Stage 0 recognizes every name and reports a structured lowering error;
it does not lower an invented calling convention.

### Vector builtin call shapes

`spec/00-language/vectors.xml` names `@splat`, `@shuffle`, `@reduce`, and
`@select`, but does not define their arguments, reduction-operation encoding,
shuffle-mask representation, or exact result typing. Stage 0 recognizes every
name and reports a structured lowering error until those contracts are
normative.

### Reflection metadata schemas

`spec/00-language/comptime.xml` requires `@typeInfo`, `@hasDecl`, and `@decl`,
but does not define the value schema returned by `@typeInfo` or declaration
metadata/lookup behavior. Stage 0 implements reflection operations whose result
is unambiguous from the specification and emits ZIN-E5005 for these unresolved
metadata operations.

### Language-version value

`spec/00-language/modules.xml` requires `@languageVersion`, but does not define
its result type or value format. Stage 0 recognizes the zero-argument builtin
and emits ZIN-E5005 rather than selecting a private representation.

### Conversion builtin signatures

`spec/00-language/types.xml` names the conversion builtins without defining
their call shapes. The only concrete example is target-first
`@ptrFromInt(*volatile u32, address)` in `spec/00-language/unsafe.xml`. Stage 0
provisionally applies that target-first, two-argument shape consistently to the
other conversion builtins; this must not be treated as a finalized normative
contract.
