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
| `ext-session-lock-v1.xml` | wayland-protocols 1.49, `staging/ext-session-lock/ext-session-lock-v1.xml` (screen lockers such as swaylock) |
| `primary-selection-unstable-v1.xml` | wayland-protocols 1.49, `unstable/primary-selection/primary-selection-unstable-v1.xml` (the selection a middle click pastes) |
| `viewporter.xml` | wayland-protocols 1.49, `stable/viewporter/viewporter.xml` (a buffer cropped and scaled to a surface size: video players, browsers) |
| `xdg-output-unstable-v1.xml` | wayland-protocols 1.49, `unstable/xdg-output/xdg-output-unstable-v1.xml` (outputs' logical size and name) |
| `idle-inhibit-unstable-v1.xml` | wayland-protocols 1.49, `unstable/idle-inhibit/idle-inhibit-unstable-v1.xml` (a video keeps the screen on) |
| `xdg-activation-v1.xml` | wayland-protocols 1.49, `staging/xdg-activation/xdg-activation-v1.xml` (a program passing the focus to another one it starts) |
| `fractional-scale-v1.xml` | wayland-protocols 1.49, `staging/fractional-scale/fractional-scale-v1.xml` (the scale a surface should be drawn at, in 120ths) |
| `tablet-v2.xml` | wayland-protocols 1.49, `stable/tablet/tablet-v2.xml` (drawing tablets; cursor-shape names its tools) |
| `cursor-shape-v1.xml` | wayland-protocols 1.49, `staging/cursor-shape/cursor-shape-v1.xml` (a cursor by name instead of a surface) |
| `ext-data-control-v1.xml` | wayland-protocols 1.49, `staging/ext-data-control/ext-data-control-v1.xml` (clipboard tools such as wl-copy and clipboard managers) |
| `single-pixel-buffer-v1.xml` | wayland-protocols 1.49, `staging/single-pixel-buffer/single-pixel-buffer-v1.xml` (a one-colour buffer, scaled with a viewport) |
| `presentation-time.xml` | wayland-protocols 1.49, `stable/presentation-time/presentation-time.xml` (when a frame reached the screen: video players, browsers) |
| `wlr-data-control-unstable-v1.xml` | wlr-protocols bf4fc79a, `unstable/wlr-data-control-unstable-v1.xml` (ext-data-control's forerunner, which wl-clipboard 2.2 speaks) |
| `xdg-dialog-v1.xml` | wayland-protocols 1.49, `staging/xdg-dialog/xdg-dialog-v1.xml` (modal dialogs: a dialog asks to be kept over its parent) |
| `tearing-control-v1.xml` | wayland-protocols 1.49, `staging/tearing-control/tearing-control-v1.xml` (a game asking to show frames without waiting for vsync) |
| `content-type-v1.xml` | wayland-protocols 1.49, `staging/content-type/content-type-v1.xml` (what a surface shows: photo, video, game) |
| `ext-idle-notify-v1.xml` | wayland-protocols 1.49, `staging/ext-idle-notify/ext-idle-notify-v1.xml` (idle notifications: swayidle and screen lockers) |
| `xdg-foreign-unstable-v2.xml` | wayland-protocols 1.49, `unstable/xdg-foreign/xdg-foreign-unstable-v2.xml` (a window of one program as the parent of another's: portal dialogs) |
| `alpha-modifier-v1.xml` | wayland-protocols 1.49, `staging/alpha-modifier/alpha-modifier-v1.xml` (a surface drawn translucent as a whole) |
| `xdg-toplevel-icon-v1.xml` | wayland-protocols 1.49, `staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml` (a window's own icon) |

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
