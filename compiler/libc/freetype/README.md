# mlx-freetype

FreeType 2's C API in Mlx, for the C and C++ programs built here:
WebKit's Skia (`SkFontHost_FreeType*.cpp`, the font managers on FreeType)
and WebCore's FreeType code include `<ft2build.h>` and `FT_*_H` as they
would FreeType's headers, and link `libmlxfreetype.so` in place of
libfreetype. No FreeType code is used: the fonts are read and rendered by
std.truetype, the standard library's font engine (`std/src/truetype`,
see `docs/reference/truetype.md`), written after FreeType 2.13.2's
sources so that it computes what FreeType computes, to the bit:
outlines, metrics, advances, renderings. This directory is the C API
over it: FreeType's records and functions.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, macros (`FT_LOAD_TARGET_`, `FT_IS_SCALABLE`...) and
functions C sees, a module per FreeType header, with FreeType's names
and the layout of its structures. `mlx-cheaders` (compiler/cc) makes the
C headers of them into `mlx-out/include/mlx-freetype/` (`ft2build.h`,
`freetype/*.h`, `freetype/config/*.h`); there are no headers written by
hand.

    ./mlx build freetype         the library (mlx-out/lib/libmlxfreetype.so) and the headers
    tools/check_freetype.sh      builds them and compares the tests with FreeType 2.13

## What it does

- Faces of TrueType, OpenType (CFF and CFF2) and their collections, from
  files, memory or a program's stream; CFF fonts of their own (bare CFF
  data, and OpenType fonts without `head`); named instances of variable
  fonts as faces; the face's flags, names, sizes, charmaps (`cmap`
  formats 0 to 14, a CFF font's encoding, and the Unicode charmap
  FreeType makes of the glyph names of a font that has none, with the
  Adobe Glyph List), SFNT tables and names, glyph names (`post`, CFF
  charsets), PostScript names and information, `gasp`, `kern`, CID
  information, font formats.
- Glyphs: TrueType outlines (`glyf`, composites, phantom points), CFF and
  CFF2 charstrings (Type 2, subroutines, seac, CID fonts, CFF2's blend)
  with the CFF driver's own hinter (the Adobe engine: stem hints, hint
  masks, blue zones, flex, darkening); scaling, transforms, vertical
  layout, unscaled loads, FT_Get_Advance(s), kerning, subglyphs.
- Rendering: the anti-aliasing rasterizer (gray, LCD and LCD_V with the
  filters, the face's own weights, overlapping contours) and the
  monochrome one (drop-out control), FT_Outline_Render with spans and
  clipping, FT_Outline_Get_Bitmap; the outline functions, glyph objects,
  FT_Bitmap_*, the synthetic styles (FT_GlyphSlot_Embolden, Oblique...).
- Variable fonts: `fvar`, `avar` (versions 1 and 2), `gvar` (with the
  interpolation of untouched points), `HVAR`, `VVAR`, `MVAR`, CFF2's
  blend; the multiple masters API (FT_Get_MM_Var, design, normalized and
  named-instance coordinates, the PostScript names of instances).
- Color: `CPAL` palettes, `COLR` version 0 layers rendered into BGRA, and
  version 1's paints (`TT_SUPPORT_COLRV1`: FT_Get_Color_Glyph_Paint,
  FT_Get_Paint and the rest, varied by the font's coordinates); embedded
  bitmaps of `EBLC`/`EBDT`, `CBLC`/`CBDT` (PNG) and `sbix` (PNG, `dupe`),
  the PNGs decoded by std.png.

Not done, as FreeType built without them: the TrueType bytecode
interpreter and the auto-hinter (a hinted TrueType load is the outline
unhinted with its metrics on the pixel grid, and FT_FACE_FLAG_HINTER is
not set for TrueType faces), Type 1, CID-keyed Type 1, PCF, BDF, PFR,
Windows FNT and WOFF/WOFF2 fonts, OpenType SVG, the SDF renderer,
compressed streams, FT_Error_String's messages, the Macintosh's
resource-fork fonts (not opened, with FreeType's errors for what is not a
font of them).

## Parts

The C API (this directory):

| File | What |
|---|---|
| `api/` | The interface in Mlx, a module per FreeType header (FreeType 2.13's names and structures) |
| `freetype.mlx` | The library's root (`--plugin --stack-arena`) |
| `records.mlx` | FreeType's records of objects: FT_FaceRec, FT_SizeRec, FT_GlyphSlotRec, FT_CharMapRec |
| `library.mlx` | FT_Library around the engine's library: modules, properties, the LCD filter, versions, the C library's FT_Memory |
| `faces.mlx` | The records of faces, sizes, slots and charmaps over the engine's (pushed after each call that changes them, a slot read back before the engine works on it), opening faces (memory, paths, streams), closing them, sizes, transforms, properties |
| `glyphs.mlx` | FT_Load_Glyph, FT_Load_Char, FT_Render_Glyph, advances, kerning, subglyphs |
| `outlines.mlx`, `bitmap.mlx`, `glyph.mlx`, `calc.mlx` | ftoutln.h, ftbbox.h and FT_Outline_Render; ftbitmap.h and ftsynth.h; ftglyph.h (glyph objects); fixed-point arithmetic and trigonometry |
| `variations.mlx`, `color.mlx`, `api.mlx` | ftmm.h; ftcolor.h; the rest: charmaps, SFNT tables and names, glyph and PostScript names, `gasp`, formats, CID, BDF and patents as FreeType answers them for SFNT faces |
| `tests/` | The test programs, `cases.txt`, their expected outputs (`expected/`), the ABI test (`abi.py` writes `abi.c`), small test fonts (`fonts/`, made by `fonts/make_fonts.py` from free fonts), the sweep (`sweep.c`) |

The engine (`std/src/truetype`): `memory`, `records`, `objects`,
`library` (memory, the engine's records and values, a library), `calc`,
`face` (opening faces, sizes, slots), `sfnt`, `cmap`, `psnames`, `info`
(the SFNT container, charmaps, names), `glyf` (TrueType outlines),
`cffload`, `cf2`, `cff` (CFF and CFF2, the hinter), `variations`,
`sbit`, `color`, `load` (loading glyphs, advances, kerning), `render`,
`raster` (the rasterizers), `outlines`, `bitmap`.

## Tests

`tools/check_freetype.sh` builds the library and the headers, checks that
every declared function is defined and that every header compiles in C
and C++, and runs the test programs (C, built by mlx-cc) on the cases of
`tests/cases.txt`: system fonts of a Debian or Ubuntu system (DejaVu,
Liberation, FreeFont, Carlito and Caladea, IPA Gothic, Inter, Loma,
Unifont, Noto Color Emoji, WenQuanYi Zen Hei) and the small fonts of
`tests/fonts` (variable TrueType and CFF2, COLR versions 0 and 1, sbix,
glyph names without a Unicode charmap), some of them damaged (bytes
changed or the font cut short: `tests/damaged.c`).
Each must print what FreeType 2.13.2 prints for the same program
(`tests/expected`, written by `tools/check_freetype.sh --update` with gcc
and the system's FreeType): every outline point, metric, advance and
rendered pixel the same. With the system's FreeType at hand, it also
sweeps every font of the system (`tests/sweep.c`: every face and named
instance, up to 1500 glyphs a face at several sizes and load flags,
every rendering, names, charmaps, advances, kerning; CFF fonts of their
own made of the CFF fonts too) and fuzzes (`tests/damaged.c` with many
seeds), the library's output compared with FreeType's. It also compiles
Skia's FreeType sources against the headers and checks that the library
has every function they call.
