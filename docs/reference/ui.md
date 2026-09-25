# Layout: `std.ui`

`std.ui` has the first building blocks for pixel user interfaces:
rectangles, insets, alignment, containers, stacks and a screen whose edges
can be reserved for bars. It is integer layout math on screen pixels (x to
the right, y down) and knows nothing about drawing: a layout hands out
rectangles, and the caller draws into them with what it has (the CPU, or
the Vulkan `text` shader of `examples/vulkan-shared`). `fillRect` is the
one drawing helper.

```mlx
const ui = @import("std.ui")

// The whole surface, minus what the system covers (status bar, cutouts).
var screen = ui.screen(width, height, ui.insets(status_bar, 0, 0, 0))
// A top bar: 48 content pixels below the status bar.
const bar = ui.reserveTop(&screen, 48)
// A title centered in the bar, 8 pixels in from its edges.
const box = ui.container(bar.content, ui.uniform(8), ui.centered())
const title = ui.placeIn(&box, ui.size(title_width, title_height))
// screen.free is what is left for the content under the bar.
```

## Geometry

| Type | Fields |
| --- | --- |
| `Rect` | `x`, `y`, `width`, `height` (`i32`) |
| `Size` | `width`, `height` |
| `Insets` | `top`, `right`, `bottom`, `left`: space kept free at each edge |

`rect`, `size` and `insets` build them; `uniform(n)` is the same inset on
every edge and `symmetric(vertical, horizontal)` one per axis. `right` and
`bottom` give the first column and row past a rectangle, `isEmpty` is true
for a zero or negative size, `contains(area, x, y)` tests a pixel,
`intersect(a, b)` is the overlap (empty when there is none) and
`inset(area, insets)` shrinks a rectangle, never below a size of 0.

## Alignment

`Align` is where a child goes along one axis: `start` (left or top),
`center`, `end`, or `fill` (stretched to the space). An `Alignment` has one
for each axis; `alignment(horizontal, vertical)` builds one, and
`topLeft`, `topCenter`, `topRight`, `centerLeft`, `centered`,
`centerRight`, `bottomLeft`, `bottomCenter`, `bottomRight` and `stretch`
name the usual ones.

`place(outer, size, alignment)` is where a child of `size` goes inside
`outer`. The child keeps its size (except along a `fill` axis) even when it
does not fit; a centered child that is too large overflows both ends
equally (one pixel more at the start when the overflow is odd), and the
caller decides whether to clip it with `intersect`.

## Bands, containers and stacks

`takeTop(&area, n)`, `takeBottom`, `takeLeft` and `takeRight` cut a band of
`n` pixels (at most what is there, never negative) off one edge of `area`,
return it and leave the rest in `area`.

A `Container` holds one child: `container(bounds, padding, alignment)`,
`content(&box)` is the area inside the padding and `placeIn(&box, size)`
the child's rectangle in it.

A `Stack` lays children out one after another along an `Axis`
(`horizontal` or `vertical`) with `spacing` between them, each aligned
across the axis: `stack(area, axis, spacing, cross)`, then
`next(&stack, size)` for each child. `used` and `count` say how much of
the axis is taken; children past the end still get a rectangle, outside the
area.

## Screen

`screen(width, height, safe)` is a whole surface: its `bounds`, the `safe`
insets the system covers, and the `free` area not reserved yet (`bounds`
inside `safe`). `reserveTop(&screen, height)` and `reserveBottom` take a bar
of `height` content pixels from the free area and return a `Band`:
`content` is the bar's part clear of the safe insets, for its children, and
`area` is what to paint as its background. The first bar at an edge covers
that edge's safe inset too (its background goes behind the status bar), and
`area` always spans the full width.

## Drawing

`fillRect(pixels, width, height, stride, area, color)` blends a 0xAARRGGBB
color (straight alpha) over the part of `area` inside a buffer of
0xAARRGGBB pixels: alpha = (255 × a + 127) / 255, each channel
(color × alpha + under × (255 − alpha) + 127) / 255, result opaque. That is
the `text` shader's arithmetic at full coverage, and on the GPU
`text.fill` in `examples/vulkan-shared/text.mlx` draws the same rectangle
(the shader with no glyphs and every pixel starting fully covered);
`tests/263_vulkan_text_runtime.mlx` checks the two pixel for pixel.

`examples/vulkan-android` lays out its frame this way: the frame is a
screen whose safe insets are the system bars and cutouts
(`std.android.windowInsets`, see [android.md](android.md)); only the
background is filled into those bands (`takeTop`/`takeBottom`/`takeLeft`/
`takeRight` of the bounds), and the label goes at the top center of
`screen.free`, clipped to it with `text.drawIn`.

## Tests

`tests/264_ui_layout_runtime.mlx` covers the geometry, every alignment
(including children that do not fit), bands, containers, stacks, a screen
with safe insets and reserved top and bottom bars, and `fillRect`'s
blending and clipping.
