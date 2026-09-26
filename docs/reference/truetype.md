# TrueType fonts: `std.truetype`

`std.truetype` reads TrueType fonts, rasterizes glyphs with anti-aliasing,
packs them into an atlas and lays out text. It needs no C library (no
FreeType): it is plain Mlx, and it runs unchanged on x86_64 and on
aarch64 (Android). It began as a port of a Zig TrueType renderer.

```mlx
const tt = @import("std.truetype")

var font: tt.Font = undefined
if tt.loadFile(&font, path, allocator) != tt.Status.ok { ... }
var atlas: tt.Atlas = undefined
if !tt.initAtlas(&atlas, &font, 18, 512, 128, allocator) { ... }  // 18 px lines
var run: tt.Run = undefined
if !tt.initRun(&run, 128, allocator) { ... }
tt.layout(&atlas, "Mlx + Vulkan · AVé€", &run)
tt.drawRun(&atlas, &run, pixels, width, height, stride, x, baseline, 0xFFFFFFFF, 0)
```

## Fonts

`init(font, data, size)` reads a font held in memory and `loadFile(font,
path, allocator)` reads one from disk. Every table read is bounds-checked,
so a truncated or hostile file yields `Status.truncated`,
`Status.missingTable` or missing glyphs, never an out-of-range read.

| Table | Use |
| --- | --- |
| offset table | TrueType (`0x00010000`, `true`); for a `.ttc` collection, its first font; CFF (`OTTO`) is `Status.notTrueType` |
| `head`, `maxp` | units per em, glyph count, short or long `loca` |
| `hhea`, `hmtx` | ascender, descender, line gap, advance widths |
| `cmap` | formats 0, 4, 6 and 12; the full-Unicode subtable (format 12) is preferred, then the BMP one (format 4) |
| `loca`, `glyf` | simple glyphs, and compound glyphs (offsets, uniform and x/y scales, 2x2 transforms, nested up to 8 levels) |
| `kern` | the legacy table, format 0 (horizontal pair kerning) |

`glyphIndex(font, codepoint)`, `advanceWidth(font, glyph)` and
`kerning(font, left, right)` answer in font units.

## Rasterizing

`rasterize(font, glyph, scale, allocator, bitmap)` produces an 8-bit
coverage bitmap; `scaleForPixelHeight(font, pixels)` gives the scale for a
line of that height (ascender to descender). Outlines are decoded from
TrueType's on- and off-curve points (two off-curve points in a row imply
the on-curve point between them), quadratic curves are flattened
adaptively, and each line segment adds its exact signed area and cover to
an accumulation buffer whose running sum is the coverage of each pixel
(after font-rs). The absolute sum, clamped to 1, gives TrueType's non-zero
winding rule. Hinting instructions are not run.

`tools/check_truetype.py` compares the result with FreeType (unhinted)
and with a 16x16-supersampled rendering of the exact outline:

| Font, size | Mean error, std.truetype | Mean error, FreeType |
| --- | --- | --- |
| DejaVu Sans subset, 24 px | 1.08 | 3.01 |
| DejaVu Sans, 13 px | 1.41 | 4.83 |
| DejaVu Sans Mono, 40 px | 0.68 | 2.06 |

(errors in coverage levels out of 255; bitmap sizes and placement are
identical to FreeType's for every glyph checked).

## Atlas, layout and drawing

An `Atlas` holds one font at one size: glyphs are rasterized on first use
and packed in rows into a coverage image (`revision` changes whenever a
glyph is added). `layout(atlas, text, run)` turns one line of UTF-8 into a
`Run`: a `Quad` per visible glyph (its atlas rectangle and its position
relative to the pen origin on the baseline, with advances and kerning
applied), the run's bounding box and its advance. `drawRun` blends a run
into a `0xAARRGGBB` image: each pixel takes the largest coverage among the
glyphs covering it and blends the color at that coverage, in integers.

On the GPU, the `text` compute shader of `examples/vulkan-shared`
(`text.mlx` keeps the atlas and the frame's runs in GPU memory) does the
same arithmetic, so a run drawn by Vulkan equals the CPU's pixel for pixel
(`tests/263_vulkan_text_runtime.mlx`). The Vulkan examples use it: the
Wayland client's label, the compositor's window titles (both renderers,
compared by `tools/check_vulkan_wayland.sh`) and the Android app's label
(from the system font).

## Tests

`tests/262_truetype_runtime.mlx` checks, on
`tests/support/fonts/DejaVuSans-subset.ttf` (a DejaVu Sans subset with short
`loca`, cmap formats 4 and 12, compound glyphs and kerning): the tables and
metrics against fontTools, bitmaps whose placement matches FreeType and
whose coverage is fixed, the atlas, layout with kerning and UTF-8,
`drawRun`'s blending, and rejected input. It runs in `tests/run_vulkan.sh`,
and `tools/diff_aarch64_backend.py` runs it on aarch64 too.

## Limits

- One line of text, left to right: no shaping, bidirectional text,
  ligatures or line breaking.
- Kerning comes from the legacy `kern` table only (OpenType `GPOS` is not
  read); fonts without one are laid out with advances alone.
- No hinting, no CFF outlines, no vertical metrics, no variable-font
  instances (the default instance is drawn); compound glyphs positioned by
  point matching are skipped.
