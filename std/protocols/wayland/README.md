# Wayland protocol bootstrap inputs

Canonical Wayland protocol XML consumed by the Stage-1 standard-library
bootstrap to materialize `std.wayland`. These files are not user-project
build inputs; applications only `@import("std.wayland")`.

| File | Upstream |
| --- | --- |
| `wayland.xml` | wayland 1.26.0, `protocol/wayland.xml` |
| `xdg-shell.xml` | wayland-protocols 1.49, `stable/xdg-shell/xdg-shell.xml` |
| `linux-dmabuf-v1.xml` | wayland-protocols 1.49, `stable/linux-dmabuf/linux-dmabuf-v1.xml` |
| `ext-background-effect-v1.xml` | wayland-protocols 1.49, `staging/ext-background-effect/ext-background-effect-v1.xml` |
| `ext-foreign-toplevel-list-v1.xml` | wayland-protocols 1.49, `staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml` |
| `ext-image-capture-source-v1.xml` | wayland-protocols 1.49, `staging/ext-image-capture-source/ext-image-capture-source-v1.xml` |
| `ext-image-copy-capture-v1.xml` | wayland-protocols 1.49, `staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml` |
| `wlr-layer-shell-unstable-v1.xml` | wlr-protocols bf4fc79a, `unstable/wlr-layer-shell-unstable-v1.xml` |
| `wlr-foreign-toplevel-management-unstable-v1.xml` | wlr-protocols bf4fc79a, `unstable/wlr-foreign-toplevel-management-unstable-v1.xml` |
| `xdg-decoration-unstable-v1.xml` | wayland-protocols 1.49, `unstable/xdg-decoration/xdg-decoration-unstable-v1.xml` |
| `relative-pointer-unstable-v1.xml` | wayland-protocols 1.49, `unstable/relative-pointer/relative-pointer-unstable-v1.xml` |
| `pointer-constraints-unstable-v1.xml` | wayland-protocols 1.49, `unstable/pointer-constraints/pointer-constraints-unstable-v1.xml` |
| `server-decoration.xml` | plasma-wayland-protocols c5ac4db8, `src/protocols/server-decoration.xml` (KDE's, which GTK 3 uses) |
| `keyboard-shortcuts-inhibit-unstable-v1.xml` | wayland-protocols 1.49, `unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml` |
| `xwayland-shell-v1.xml` | wayland-protocols 1.49, `staging/xwayland-shell/xwayland-shell-v1.xml` (Xwayland's windows to wl_surfaces) |
| `pointer-gestures-unstable-v1.xml` | wayland-protocols 1.49, `unstable/pointer-gestures/pointer-gestures-unstable-v1.xml` (touchpad swipes, pinches and holds) |

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
