# The Mlx look

The desktop's apps (`projects/desktop`, MLX Observatory) and anything
built on `std.ui` share one design language. It is not GTK's, Qt's or
macOS's: it is made of three things, and the widgets of
[`std.ui`](ui.md) draw it on their own, so an app gets it by using them.

## Four rules

**Glass at the edges.** Surfaces at the window's edges (a side bar, a
header or toolbar) and everything that floats over the window (a popup,
a menu) are translucent, and the compositor blurs what is behind them
(`ext-background-effect-v1`; the Mlx compositor offers it, see
[wayland.md](wayland.md)). A window is clear (alpha 0) under its glass
and paints the opaque content beside it. In the window, `std.ui.surface`
draws the tint and names the area (`view.blurRegion`); after each frame
the app hands the frame's glass areas to the platform in one call
(`host.applyBlur`), which tells the compositor only when they changed.
A popup is a surface of its own over the window (`host.openPopup`, an
`xdg_popup` with a rounded blur region), so the window's content blurs
through it; where a platform has no popups of its own (a phone)
`std.ui.popup` draws one in the window instead.

**Depth from contrast, not effects.** Surfaces are flat and a clear
step apart: the page, a card on it, a control on the card. Nothing has
an outline, a highlight or a glow; the one shadow is under what floats
(a popup, `surface.raised` with an elevation; `canvas.shadow` draws it
as layers of rounded rectangles, the same pixels on the CPU and the
GPU). A field with the focus has a ring in the accent; that is the only
ring.

**One edge, one grid.** Every size is logical pixels times the screen's
scale (`std.ui.flow`). A page's title, its section labels and the text
of its rows start on one vertical edge, 16 logical pixels in from the
cards' edge (`widgets.TEXT_INSET`); a card pads its rows by 8 and the
controls pad their text by 8, which adds up to the same edge. Rows are
40 logical pixels tall, 52 with a note under the title, 6 apart; cards
are 16 apart. Titles and section labels are bold (`view.setBold`,
`text.loadBoldText`), section labels in the dim colour; everything else
is the regular face.

**One seed, every colour.** A theme is made from one colour
(`theme.fromSeed(seed, dark)`), the way Material You makes a palette
from a wallpaper: the seed's hue is the accent (at a lightness that
reads on the surfaces) and a trace of it tints the near-neutral
surfaces, dark or light. The accent is used sparingly: a switch that is
on, a chosen radio button, a slider's fill, the primary button, the
chosen category's icon. The desktop takes the seed from mlx-settings
(Appearance, "Accent colour": `accent-color` in `compositor.conf`,
`projects/desktop/shared/look.mlx` reads it for every app); a
wallpaper's colour can take its place later. `theme.dark()` and
`theme.light()` are the palettes of the default seed, an indigo.

## Shapes and sizes

| Thing | Shape |
| --- | --- |
| Buttons, fields, segments, value boxes, tabs, rows | rounded, `theme.radius` (8 logical pixels) |
| Switches, chips, pills, scroll thumbs, a slider's trough and handle | capsules |
| Cards, popups | `theme.largeRadius` (12) |

A slider is a trough (12 logical pixels tall) the accent fills up to the
value, with a narrow handle standing out of the trough at the end of the
fill; the whole row takes the pointer and the wheel. A chosen segment is
a lighter tile in a darker trough. A chosen tab is a lighter tile in the
bar. A selected row has the selected fill.

## Do and don't

- Do put a window's side bar, header and toolbar on glass
  (`surface.sidePanel`, `surface.bar`) and keep the content opaque.
- Do open menus and other popups as popups of their own
  (`host.openPopup`), glass over the window.
- Do group settings in cards; do not draw lines between rows.
- Do take colours from the theme (`context.theme`), never fixed ones:
  a fixed colour ignores the seed and the light look.
- Do not draw outlines, highlights, glows or shadows around controls.
- Do not blur in the window itself: the compositor blurs what is behind
  the window, so content never scrolls under a glass bar (the content
  area starts below the bar).

## Tests

`tests/296_ui_flow_runtime.mlx` checks the surfaces (a soft shadow's
falloff, a floating surface's shadow, a shadow cast from an edge) and
the rows, cards and widgets of `std.ui.flow` and `std.ui.widgets`;
`tests/284_ui_widgets_runtime.mlx` the controls' behaviour under the
pointer. `tools/emulate_android_app.py` taps the widgets of
`examples/android/widgets.mlx` on a model of Android.
