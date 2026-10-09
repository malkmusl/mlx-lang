# The Mlx look

The desktop's apps (`projects/desktop`, MLX Observatory) and anything
built on `std.ui` share one design language. It is not GTK's, Qt's or
macOS's: it is made of three things, and the widgets of
[`std.ui`](ui.md) draw it on their own, so an app gets it by using them.

## Three ideas

**Glass.** Surfaces at the window's edges (a side bar, a header or
toolbar) and everything that floats over the window (a popup, a menu)
are translucent, and the compositor blurs what is behind them
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

**Depth from light, not lines.** Nothing has an outline. A surface that
floats casts a soft shadow (`canvas.shadow`: layers of rounded
rectangles, the same pixels on the CPU and the GPU), and a hairline of
light runs along its top edge (`canvas.topHighlight`). What is on, or
has the focus, glows in the accent (`surface.glow`): a switch that is
on, a chosen radio button, a field being typed in, a slider under the
pointer, a chosen row. Cards (`widgets.cardBegin`) group rows on a page
and float on their shadow; popups float higher (twice the elevation).

**One seed, every colour.** A theme is made from one colour
(`theme.fromSeed(seed, dark)`), the way Material You makes a palette
from a wallpaper: the seed's hue carries through everything, strong in
the accent (at a lightness that reads on the surfaces), faint in the
surfaces themselves (a near-neutral with a little of the hue), dark or
light. The desktop takes the seed from mlx-settings (Appearance, "Accent
colour": `accent-color` in `compositor.conf`,
`projects/desktop/shared/look.mlx` reads it for every app); a wallpaper's
colour can take its place later. `theme.dark()` and `theme.light()` are
the palettes of the default seed, an indigo.

## Shapes and sizes

| Thing | Shape |
| --- | --- |
| Buttons, fields, segments, value boxes, tabs | rounded, `theme.radius` (10 logical pixels) |
| Switches, chips, pills, scroll thumbs, a slider's trough and handle | capsules |
| Cards, popups | `theme.largeRadius` (16) |
| Rows (hover, chosen) | `theme.radius` |

A slider is a trough (14 logical pixels tall) the accent fills up to
the value, with a narrow handle standing out of the trough at the end of
the fill; the whole row takes the pointer and the wheel. A chosen
segment is a raised tile in a sunken trough. A chosen tab is a raised
tile in the bar. A selected row glows faintly from within.

Rows of settings are 36 logical pixels tall, 48 with a note under the
title, 6 apart; a card pads them by 8 and the cards are 12 apart
(`std.ui.widgets`). Every size is logical pixels times the screen's
scale (`std.ui.flow`), so one layout serves a laptop and a phone.

## Do and don't

- Do put a window's side bar, header and toolbar on glass
  (`surface.sidePanel`, `surface.bar`) and keep the content opaque.
- Do open menus and other popups as popups of their own
  (`host.openPopup`), glass over the window.
- Do group settings in cards; do not draw lines between rows.
- Do take colours from the theme (`context.theme`), never fixed ones:
  a fixed colour ignores the seed and the light look.
- Do not draw outlines or borders around controls; a focus or an "on"
  state is a glow.
- Do not blur in the window itself: the compositor blurs what is behind
  the window, so content never scrolls under a glass bar (the content
  area starts below the bar).

## Tests

`tests/296_ui_flow_runtime.mlx` checks the surfaces (a soft shadow's
falloff, a raised surface's hairline, a bar's shadow) and the rows,
cards and widgets of `std.ui.flow` and `std.ui.widgets`;
`tests/284_ui_widgets_runtime.mlx` the controls' behaviour under the
pointer. `tools/emulate_android_app.py` taps the widgets of
`examples/android/widgets.mlx` on a model of Android.
