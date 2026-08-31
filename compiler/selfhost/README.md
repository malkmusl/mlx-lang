# Canonical self-hosted Zin compiler

This directory contains the canonical Zin compiler written entirely in Zin.
`zin0` compiles it into `zin1`; a complete `zin1` will then compile the same
sources into `zin2`. Do not add Zig dependencies here.

The first executable scaffold is intentionally small but already crosses real
bootstrap boundaries:

- `token.zin` defines the stable token representation.
- `lexer.zin` tokenizes identifiers, integers, newlines, and core punctuation.
- `diagnostic.zin` starts the source-offset diagnostic model.
- `main.zin` is the current `zin1` entry point and an end-to-end smoke test.

Build the current scaffold with:

```sh
zig-out/bin/zin0 compiler/selfhost/main.zin -ozin1
./zin1
```
