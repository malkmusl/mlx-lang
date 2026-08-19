# Zin 1.0 specification rebuild notes

This rebuild incorporates the latest normative decisions:

- `@move(value)` is the explicit value-transfer intrinsic.
- `@nocopy` is a compiler intrinsic type constructor, not a keyword/modifier.
- Required forms include `@nocopy(u8)`, `@nocopy(struct) { ... }`, `@nocopy(enum(u8)) { ... }`, `@nocopy(union) { ... }`, and `@nocopy(union(enum(u8))) { ... }`.
- Copyability propagates through by-value aggregate containment.
- Sema tracks Uninitialized/Initialized/Moved/PartiallyMoved states.
- No copy/move constructors or implicit destructors exist.
- Compiler diagnostics are structured and stable-code based (`ZIN-E####`/`ZIN-W####`) with JSON output and `zin explain`.
- `unsafe {}` defines explicit raw-safety permission boundaries.
- Error-union results cannot be silently discarded; intentional error discard uses `@discardError`.
- JSON and XML are pure Zin stdlib modules (`std.json`, `std.xml`).
- Wayland has no compiler/build schema builtin. Canonical Wayland XML is consumed during Stage-0/1 stdlib construction, producing native `std.wayland` client and server support for Linux/BSD/brixOS.

- Arbitrary-width integers are now normative for every `uN`/`iN`, 1..4096 bits.
- Added `<<`, `>>`, `<<=`, `>>=`, wrapping-count shifts (`<<%`, `>>%`) and saturating left shifts (`<<|`, `<<%|`) with assignment forms.
- Added the complete shift-combine algebra, including `^<<`, `|<<`, `&<<`, `>>|`, `|>>`, `<<^`, arithmetic shift-combine forms, and all valid compound assignments.
- Compound assignments evaluate their destination exactly once.
- Added normative LIR and diagnostics for shift/count/overflow/operator failures.
