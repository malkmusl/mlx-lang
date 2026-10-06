# Shared Files UI

`ui.mlx` is the responsive layout interface shared by the Wayland file
manager (`projects/desktop/files`, whose controls are std.ui widgets) and
its Android NativeActivity (`projects/android/files`). Call `layoutWithSidebar` with the
pixel size, safe-area insets, current view, density scale and the two retained
sidebar preferences. The returned `Layout` contains every interactive and
painted rectangle plus the list/grid metrics. `layout` provides the default
expanded-wide/closed-overlay state for callers that do not retain UI state.
The sidebar toggle lives in the visible sidebar header or icon rail; with a
closed compact/phone drawer it moves before the Back and Forward buttons.

The logical breakpoints are:

- **wide** (`>= 760 px`): expandable desktop sidebar or icon rail, toolbar,
  search, list header and status;
- **compact** (`>= 520 px`): toolbar search and an optional overlay sidebar;
- **phone** (`< 520 px`): touch-sized controls, a second toolbar row for
  search, no list header and an optional overlay sidebar.

`view.mlx` paints the shared chrome and glyphs through `std.ui.canvas`.
That canvas supports the desktop and Android
Vulkan paint-command stream as well as packed/padded CPU buffers for the
fallback path. The platform frontends retain their own filesystem and input
adapters; visual decisions stay in this directory.
