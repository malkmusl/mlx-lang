# User interfaces: `std.ui`

`std.ui` is user interfaces on any platform (a Wayland buffer, an Android
window): layout (this module), drawing (`std.ui.canvas`, `std.ui.text`,
on the CPU or the GPU) and widgets (`std.ui.view` and the modules after
it, see [Widgets](#widgets)). Its first building blocks are
rectangles, insets, alignment, containers, stacks and a screen whose edges
can be reserved for bars. It is integer layout math on screen pixels (x to
the right, y down) and knows nothing about drawing: a layout hands out
rectangles, and the caller draws into them with what it has (the CPU, or
the Vulkan `text` shader of `std.gpu`). `fillRect` is the
one drawing helper.

```mlx
const ui = @import("std.ui")

// The whole surface, minus what the system covers (status bar, navigation bar).
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
`text.fill` in `std/src/gpu/text.mlx` draws the same rectangle
(the shader with no glyphs and every pixel starting fully covered);
`tests/263_vulkan_text_runtime.mlx` checks the two pixel for pixel.

On Android, the safe insets are the space `std.android.reservedInsets`
reserves: the status and navigation bars for an app that is not fullscreen,
nothing for a fullscreen one (see [android.md](android.md#reserved-space)).
Every component is laid out in `screen.free`.
`examples/vulkan-android` lays out its frame this way: only the
background is filled into the reserved bands (`takeTop`/`takeBottom`/`takeLeft`/
`takeRight` of the bounds), and the label goes at the top center of
`screen.free`, clipped to it with `text.drawIn`.

## Canvas and text

`std.ui.canvas` draws in premultiplied ARGB: `clear`, `fillRect`,
`fillRoundedRect`, `strokeRoundedRect` (smooth corners, the coverage of
`ui.roundedCoverage`), `drawImage`/`drawImageRect` (`std.png` images,
bilinear) and `setOpacity` (what is drawn fades). A `Canvas` is pixels, or
with `device` a `std.ui.gpu_canvas.Device`: then every call is a command
the `paint` compute shader draws, the same pixels as the CPU
(`std.ui.gpu_upload` takes images and glyph atlases to the GPU).
`std.ui.text` loads a font (`loadText`, `loadMonoText`: the usual places
on Linux desktops and on Android), `measure`s and `drawText`s.
`std.ui.glyphs` has the small marks widgets use (check mark, arrows,
chevron, cross, dot, magnifier, plus).

## Widgets

`std.ui.view` is the context widgets draw in (`Ui`: the canvas, a
`std.ui.theme.Theme`, three fonts, the pointer, focus). Widgets are
functions called every frame between `view.begin` and `view.end`; each
draws itself and names its rectangle with an id (`view.id("save")`,
`view.idOf(list, row)`, or the app's own numbers). What is under the
pointer comes from those rectangles (the last named lies on top, cut to
`pushClip`), so the app tests nothing against its layout:

```mlx
view.begin(&context, &canvas)
if controls.button(&context, view.id("save"), area, "Save", controls.BUTTON_PRIMARY, true) { save() }
if controls.checkRow(&context, view.id("names"), row, "Names", state) { toggle() }
const redraw = view.end(&context)          // true: draw again

// From the platform's events:
if view.pointerMove(&context, x, y) { redraw() }
const pressed = view.pointerButton(&context, true)   // the id under the pointer
const clicked = view.pointerButton(&context, false)  // the id clicked, kept for its widget
```

A click is kept until the widget of its id takes it (the widget returns
true) on the next frame; `view.end` says to draw again then. An app with
its own input handling can use the ids returned by `pointerButton`, or
`view.hitAt`, instead (MLX Observatory uses its hit codes + 1 as ids).
When what a click starts must happen before the next frame (mlx-settings
records the keys typed right after a click on a hotkey row), the event
handler runs the frame once on `view.layoutCanvas()` (a 0 x 0 canvas: the
widgets take the click, nothing is drawn). With `projects/desktop/shared/
panel.mlx`, a draw that sets `panel.dirty` (when `view.end` says so) gets
the next frame.

| Module | Widgets |
| --- | --- |
| `std.ui.controls` | `button` (plain, normal, primary; disabled), `smallButton`, `toggleButton`, `iconButton`, `miniButton` (a small button in a row), `menuButton` (opens a list), `splitButton` (an action and an arrow for its list), `closeButton` (round, a cross), `chevronButton` (back, forward), `link` (underlined under the pointer), `chip` (a small button in a line of text), `crumb` (a step of a path), `swatch` (a colour to choose), `valueRow` (a row with a value box at its right, such as a key binding being recorded), `checkBox`/`checkRow`/`checkRowIn` (on, off, mixed; a colour of its own), `radioRow`, `switchToggle`/`switchRow` (a settings row), `heading` (a section that folds), `segment` (a choice), `pill`, `progress`, `sparkline`, labels |
| `std.ui.field` | a one-line text field: `Field` (text, caret, selection), `fieldKey` (typing, Backspace/Delete by character or word, arrows, Home/End, Shift selecting, Ctrl+A, Enter, Escape), `textField` (drawn, a click places the caret; without the focus it shows the text's start), `notedField` (a note at its right, such as a count, and a text colour of its own) |
| `std.ui.scroll` | `Scroll` (offset, content, view), the wheel over an area, `bar` (drag the thumb, page by a click), `indicator` (the same as a thin bar over the content, without a track), `mark`s on the bar, the rows in view |
| `std.ui.lists` | list and side bar rows (lit, chosen, indent, note, room for an icon), `rowBackground` and `part` (a row the app fills, whose parts have ids of their own), `columnHeader` (a sortable column's title), a fold arrow, a divider |
| `std.ui.tabs` | a tab bar: tabs with a cross that closes them, a dot for unsaved changes |
| `std.ui.popup` | a popup's frame (it takes the pointer from what is under it), menu items and separators, tooltips, wrapped text |
| `std.ui.theme` | colours and sizes: `dark()` (the desktop apps), `light()` |
| `std.ui.keys` | keys as widgets take them: Linux input codes and modifier bits on every platform |
| `std.ui.app` | what a platform gives a std.ui app: the events (pointer, buttons, wheel, keys, configured, closed, tick; drags and drops where there are any), `Draw` and `Handler`, and `feed` (a pointer event to the widgets) |
| `std.ui.host` | the platform itself, one API for all: a Wayland window on Linux, a NativeActivity on Android (see Platforms) |

## Platforms

A std.ui app is a `Draw` and a `Handler` (`std.ui.app`) over the app's
own state; the platform calls them. `std.ui.host` is the platform: the
compiler takes `std/src/ui/host.linux.mlx` for Linux targets and
`std/src/ui/host.android.mlx` for `--target=aarch64-android` (see
`std.platform`), and both have the same functions, so one source builds
for both. A program has both entries; the other platform's does nothing:

```mlx
const host = @import("std.ui.host")

pub fn main(arguments: [][*]const u8) -> !void {
    const state = newState()     // its std.ui.view.Ui, theme, fonts
    state.*.host = host.create(options(), @ptrCast(*anyopaque, state), draw, onEvent).?
    try host.open(state.*.host, arguments)
    try host.run(state.*.host)
}

export fn ANativeActivity_onCreate(activity: usize, saved_state: usize, saved_state_size: usize) -> void {
    const state = newState()
    state.*.host = host.create(options(), @ptrCast(*anyopaque, state), draw, onEvent).?
    const opened = host.openActivity(state.*.host, activity, saved_state, saved_state_size)
}
```

`create` only allocates, so the app has its host before the first event.
The app asks the platform through the host: `redraw`, `setTick`,
`width`/`height`, `insets` (the system bars' space), `scale` (pixels per
logical pixel), `typed` and `modifiers` (the current key), `wheelPixels`,
`showKeyboard`/`hideKeyboard`, `home` (where its files are),
`environmentValue`, `log`; what only a desktop has (`setTitle`,
`setBlur`, `acceptDrops`, `startDrag`) does nothing on a phone.

| Platform | Drawn by | Input |
| --- | --- | --- |
| Linux (`std.ui.wayland.panel`) | the GPU (linux-dmabuf or shared memory) or the CPU | pointer, wheel, keyboard (`std.xkb`), drag and drop |
| Android | the GPU (`std.ui.gpu_canvas` into a `VK_KHR_android_surface` swapchain, frames by `AChoreographer`), the CPU into the locked window without Vulkan | the finger is the left button; dragged past a slop it scrolls (`EVENT_WHEEL`, no click), held still it is the right button; keys (a hardware or the on-screen keyboard) as Linux codes; `$HOME` is the app's `internalDataPath` |

```mlx
fn onEvent(context: *anyopaque, kind: u32, a: u32, b: i32) -> void {
    if app.pressing(kind, a, b) { layOut(context) }   // widgets where they are now
    if app.feed(&state.*.ui, kind, a, b) == app.FEED_CLICK { layOut(context) }   // taken now
}

fn layOut(context: *anyopaque) -> void {
    var nowhere = view.layoutCanvas()
    draw(context, &nowhere)
}
```

Laid out before a press, the widgets are where the app's state puts them
even when the press comes before the first frame (a platform may deliver
it then) or after a change not drawn yet.

`examples/android/widgets.mlx` is a settings screen of switch rows, a
check row, segments and a button, from one source on Android and on the
desktop; mlx-settings is one on Wayland.

## Tests

`tests/284_ui_widgets_runtime.mlx` drives the widgets on a CPU canvas as
a platform does (hover, clicks, a press let go elsewhere, a popup over a
button, check rows, switches, segments, a row whose parts take their own
clicks, a split button, a disabled back button, a crumb, a link, a
swatch, a value row, a column header, a thin indicator, events through
`std.ui.app.feed`, a text field edited by keys, a list scrolled by the
wheel and by its bar, a tab closed, clipping, wrapping).
`tools/emulate_android_app.py` runs `examples/android/widgets.mlx` against
a model of Android (Unicorn), drawn by a mock Vulkan driver and again on
the CPU, and taps a switch row and a segment and drags over a row.
`tests/264_ui_layout_runtime.mlx` covers the geometry, every alignment
(including children that do not fit), bands, containers, stacks, a screen
with safe insets and reserved top and bottom bars, and `fillRect`'s
blending and clipping.
