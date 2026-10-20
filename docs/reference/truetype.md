# Fonts: `std.truetype`

`std.truetype` is the font engine of the standard library: it reads
TrueType and OpenType fonts (glyf and CFF/CFF2 outlines) and their
collections, variable fonts and color fonts, and renders their glyphs,
as FreeType 2.13 reads and renders them, to the bit (outlines, metrics,
advances, every rendered pixel). It needs no C library: it is plain Mlx,
and it runs unchanged on x86_64 and on aarch64 (Android). FreeType's C
API for C and C++ programs (WebKit's Skia and WebCore) is a layer over
it, `compiler/libc/freetype` (`libmlxfreetype.so`, see its README).

It has two levels: a `Font` with its `Atlas` and `Run`s, for drawing
lines of text, and the engine under it, for everything else.

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
path, allocator)` reads one from disk (`initWith` takes the allocator of
the engine's records; `init` uses the page allocator). The font is a face
of the engine with its Unicode charmap selected: TrueType, OpenType with
CFF or CFF2 outlines, the first font of a collection. Every read is
bounds-checked, so a truncated or hostile file yields `Status.truncated`,
`Status.missingTable` or missing glyphs, never an out-of-range read; a
font without outlines (only embedded bitmaps) is `Status.notTrueType`,
one without a Unicode charmap `Status.noUnicodeMap`.

`glyphIndex(font, codepoint)`, `advanceWidth(font, glyph)` and
`kerning(font, left, right)` answer in font units (advances from `hmtx`,
kerning from the `kern` table). The font keeps its units per em, glyph
count, `hhea` metrics, charmap format and kerning pairs.

`rasterize(font, glyph, scale, allocator, bitmap)` renders a glyph at
`scale` pixels per font unit into an 8-bit coverage bitmap, unhinted,
with FreeType's anti-aliasing rasterizer (the engine's `render` module):
its size, placement and pixels are FreeType's.
`scaleForPixelHeight(font, pixels)` gives the scale for a line of that
height (ascender to descender). `tools/check_truetype.py` renders glyphs
of TrueType and CFF fonts at several sizes with std.truetype and with the
system's FreeType and requires them to be the same, pixel for pixel.

## Atlas, layout and drawing

An `Atlas` holds one font at one size: glyphs are rasterized on first use
and packed in rows into a coverage image (`revision` changes whenever a
glyph is added). `layout(atlas, text, run)` turns one line of UTF-8 into a
`Run`: a `Quad` per visible glyph (its atlas rectangle and its position
relative to the pen origin on the baseline, with advances and kerning
applied), the run's bounding box and its advance. `drawRun` blends a run
into a `0xAARRGGBB` image: each pixel takes the largest coverage among the
glyphs covering it and blends the color at that coverage, in integers.

On the GPU, the `text` compute shader of `std.gpu`
(`text.mlx` keeps the atlas and the frame's runs in GPU memory) does the
same arithmetic, so a run drawn by Vulkan equals the CPU's pixel for pixel
(`tests/263_vulkan_text_runtime.mlx`). The Vulkan examples use it: the
Wayland client's label, the compositor's window titles (both renderers,
compared by `tools/check_vulkan_wayland.sh`) and the Android app's label
(from the system font).

## The engine

The modules of `std/src/truetype/`, each re-exported by `std.truetype`
(`tt.faces`, `tt.load`...), are FreeType's parts in Mlx, computing what
FreeType computes:

| Module | What |
| --- | --- |
| `memory` | Memory by address, big-endian reads; a library's `Memory` record (three functions; `useAllocator` makes one of an `Allocator`) |
| `records`, `objects` | The values (vectors, matrices, boxes, glyph metrics, bitmaps, outlines, size requests: FreeType's layouts) and constants (flags, modes, encodings, FreeType's error numbers); the records of a face, a size, a glyph slot, a charmap |
| `library` | A library: its memory, the LCD filter, the CFF driver's properties (stem darkening, its parameters, the random seed) |
| `face` (`tt.faces`) | Opening a face of memory or a file (`newFace`, `openFace`: the collection's font, a named instance, the TrueType or the CFF driver), its sizes (`setPixelSizes`, `setCharSize`, `requestSize`, `selectSize`: strikes of embedded bitmaps), slots, transform and properties |
| `sfnt`, `cmap`, `psnames`, `info` | The SFNT container and its tables (`head`, `hhea`, `OS/2`, `post`, `name`, `gasp`, `kern`...), charmaps (every `cmap` format, a CFF font's encoding, Unicode charmaps of glyph names by the Adobe Glyph List), what a face tells (characters, variation selectors, SFNT tables and names, glyph and PostScript names, CID) |
| `glyf`, `cffload`, `cf2`, `cff` | TrueType outlines (composites, phantom points); CFF and CFF2 (DICTs, charsets, encodings, subroutines, CID fonts, CFF2's blend) with the CFF driver's hinter (the Adobe engine) |
| `variations` | `fvar`, `avar` (1 and 2), `gvar`, `HVAR`, `VVAR`, `MVAR`: named instances, design and normalized coordinates |
| `sbit`, `color` | Embedded bitmaps (`EBLC`/`EBDT`, `CBLC`/`CBDT` and `sbix`, their PNGs decoded by `std.png`); `CPAL` palettes, `COLR` layers and version 1 paints |
| `load`, `render`, `raster` | Loading a glyph into a slot (`loadGlyph`, `loadChar`, `getAdvances`, `kerning`); the anti-aliasing rasterizer (gray, LCD with the filters) and the monochrome one, `renderGlyph`, `outlineRender` with spans |
| `outlines`, `bitmap` (`tt.bitmaps`), `calc` | Outline functions (decomposing, boxes, transforms, emboldening, orientation), bitmap functions (copies, emboldening, conversion, blending), synthetic styles; FreeType's fixed-point arithmetic and trigonometry |

The engine's records are reached by their addresses (`tt.face(address)`
gives a `*Face`); a face's slot, sizes and charmaps are its own, and its
`user` word is its callers' (the C API keeps its records there). An
`Engine` (`initEngine(engine, allocator)`) holds a library whose memory is
an allocator's.

## Tests

`tests/262_truetype_runtime.mlx` checks, on
`tests/support/fonts/DejaVuSans-subset.ttf` (a DejaVu Sans subset with short
`loca`, cmap formats 4 and 12, compound glyphs and kerning): the tables and
metrics against fontTools, bitmaps against FreeType's (sizes, placement and
coverage), the atlas, layout with kerning and UTF-8, `drawRun`'s blending,
and rejected input. It runs in `tests/run_vulkan.sh`, and
`tools/diff_aarch64_backend.py` runs it on aarch64 too. The engine itself
is checked through FreeType's C API by `tools/check_freetype.sh`: test
programs whose every printed outline point, metric and pixel must be
FreeType 2.13's, a sweep of every glyph of every font of the system at
several sizes and load flags against the system's FreeType, and damaged
fonts.

## Limits

- One line of text, left to right: no shaping, bidirectional text,
  ligatures or line breaking (kerning comes from the `kern` table only;
  OpenType `GPOS` is not read).
- No TrueType bytecode interpreter and no auto-hinter: TrueType glyphs
  load unhinted (a hinted load has the unhinted outline with metrics on
  the pixel grid); CFF glyphs are hinted by the CFF driver's hinter.
- Not read: Type 1 and CID-keyed Type 1 fonts, PCF, BDF, PFR, Windows FNT,
  WOFF and WOFF2, OpenType SVG, bare CFF files, the Macintosh's resource
  forks.
