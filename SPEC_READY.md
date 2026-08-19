# Zin 1.0 Release-Candidate Spec Readiness

This repository is intended to be implementation-ready. An implementation agent must not invent missing language semantics.

## Frozen language decisions

- newline-terminated syntax
- explicit allocators/manual memory
- `@nocopy(TypeHead)` / `@move(value)`
- mandatory enum backing types
- tagged and untagged unions
- arbitrary-width `uN`/`iN`, 1..4096 bits
- complete arithmetic, wrapping, saturating, bitwise, shift, shift-combine and compound-assignment algebra
- structured diagnostics with stable codes
- comptime/reflection
- explicit unsafe boundary
- native multiple returns + error unions
- direct x86_64 backend
- stdlib-owned XML/JSON/Wayland; no compiler Wayland special case

## Implementation rule

If two normative files conflict, the more specific rule wins. If specificity is equal, record the conflict in `SPEC_CONFLICTS.md` rather than inventing behavior.
