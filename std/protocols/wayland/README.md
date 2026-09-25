# Wayland protocol bootstrap inputs

Canonical Wayland protocol XML consumed by the Stage-1 standard-library
bootstrap to materialize `std.wayland`. These files are not user-project
build inputs; applications only `@import("std.wayland")`.

| File | Upstream |
| --- | --- |
| `wayland.xml` | wayland 1.26.0, `protocol/wayland.xml` |
| `xdg-shell.xml` | wayland-protocols 1.49, `stable/xdg-shell/xdg-shell.xml` |

The files are byte-for-byte copies of the upstream releases. `SOURCES`
records each file's URL, release and SHA-256. Bootstrap tooling must preserve
upstream protocol contents exactly and record source/version/hash metadata.

`materialize.mlx` is the bootstrap tool. It parses the XML with `std.xml` and
`std.wayland.protocol`, validates the protocol set, and writes the ordinary
Mlx modules under `std/src/wayland/generated/` through
`std.wayland.materialize`. `materialize.sh` checks the hashes in `SOURCES`,
builds the tool with the self-hosted compiler and runs it:

```sh
std/protocols/wayland/materialize.sh           # regenerate
std/protocols/wayland/materialize.sh --check   # verify the committed output
```

To add an extension protocol, copy its upstream XML here, record it in
`SOURCES`, add it to the list in `materialize.mlx`, and regenerate.
