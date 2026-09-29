# Shared Files UI

`ui.mlx` is the responsive layout interface shared by the Wayland file
manager and its Android NativeActivity. Call `layout` with the pixel size,
safe-area insets, current view and density scale. The returned `Layout`
contains every interactive and painted rectangle plus the list/grid metrics.

The logical breakpoints are:

- **wide** (`>= 760 px`): desktop sidebar, toolbar, list header and status;
- **compact** (`>= 520 px`): no sidebar, with the remaining desktop chrome;
- **phone** (`< 520 px`): touch-sized rows and controls, no list header or
  search field.

`view.mlx` paints the shared chrome and glyphs through
`desktop-shared/canvas.mlx`. That canvas supports the desktop and Android
Vulkan paint-command stream as well as packed/padded CPU buffers for the
fallback path. The platform frontends retain their own filesystem and input
adapters; visual decisions stay in this directory.
