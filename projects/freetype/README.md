# mlx-freetype

FreeType 2's C API in Mlx, for the C and C++ programs built here:
WebKit's Skia (`SkFontHost_FreeType*.cpp`, the font managers on FreeType)
and WebCore's FreeType code include `<ft2build.h>` and `FT_*_H` as they
would FreeType's headers, and link `libmlxfreetype.so` in place of
libfreetype. No FreeType code is used: the library is this repository's
own, written after FreeType 2.13.2's sources so that it computes what
FreeType computes, to the bit: outlines, metrics, advances, renderings.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, macros (`FT_LOAD_TARGET_`, `FT_IS_SCALABLE`...) and
functions C sees, a module per FreeType header, with FreeType's names
and the layout of its structures. `mlx-cheaders` (projects/cc) makes the
C headers of them into `mlx-out/include/mlx-freetype/` (`ft2build.h`,
`freetype/*.h`, `freetype/config/*.h`); there are no headers written by
hand.

    ./mlx build freetype         the library (mlx-out/lib/libmlxfreetype.so) and the headers
    tools/check_freetype.sh      builds them and compares the tests with FreeType 2.13

## What it does

- Faces of TrueType, OpenType (CFF and CFF2) and their collections, from
  files, memory or a program's stream; named instances of variable fonts
  as faces; the face's flags, names, sizes, charmaps (`cmap` formats 0 to
  14, a CFF font's encoding, and the Unicode charmap FreeType makes of
  the glyph names of a font that has none, with the Adobe Glyph List),
  SFNT tables and names, glyph names
  (`post`, CFF charsets), PostScript names and information, `gasp`,
  `kern`, CID information, font formats.
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
compressed streams, FT_Error_String's messages. Nor are CFF fonts out of
an SFNT (bare CFF files, and OpenType fonts without `head`, which
FreeType reads as such) or the Macintosh's resource-fork fonts: these
are not opened (with FreeType's errors for what is not a font of them).

## Parts

| File | What |
|---|---|
| `api/` | The interface in Mlx, a module per FreeType header (FreeType 2.13's names and structures) |
| `freetype.mlx` | The library's root (`--plugin --stack-arena`) |
| `memory.mlx`, `records.mlx`, `objects.mlx` | Memory through FT_Memory; FreeType's public records as the library reads and writes them; the library's own records of faces, sizes, slots and charmaps |
| `calc.mlx` | FreeType's arithmetic (FT_MulFix, FT_MulDiv, FT_DivFix...), its trigonometry (CORDIC) and matrices |
| `library.mlx` | FT_Library: modules, properties, the LCD filter, versions |
| `face.mlx` | Opening and closing faces, sizes and their requests, glyph slots, color layers rendered |
| `sfnt.mlx`, `cmap.mlx`, `psnames.mlx` | The SFNT container and its tables; character maps; Unicode charmaps of glyph names (the Adobe Glyph List) |
| `truetype.mlx` | TrueType outlines |
| `cffload.mlx`, `cf2.mlx`, `cff.mlx` | CFF and CFF2: the tables and DICTs, charsets, encodings, FDSelect, the variation store; the charstring engine and its hinter; the driver's glyph loading |
| `variations.mlx` | Font variations and the multiple masters API |
| `sbit.mlx` | Embedded bitmaps |
| `color.mlx` | CPAL and COLR |
| `load.mlx` | FT_Load_Glyph and FT_Load_Char, advances, kerning |
| `render.mlx`, `raster.mlx` | The anti-aliasing and the monochrome rasterizers, FT_Render_Glyph, FT_Outline_Render |
| `outlines.mlx`, `glyph.mlx`, `bitmap.mlx` | ftoutln.h and ftbbox.h, ftglyph.h, ftbitmap.h and ftsynth.h |
| `api.mlx` | The rest of the API: charmaps, SFNT tables and names, glyph and PostScript names, `gasp`, formats, CID, BDF and patents as FreeType answers them for SFNT faces |
| `tests/` | The test programs, `cases.txt`, their expected outputs (`expected/`), the ABI test (`abi.py` writes `abi.c`), small test fonts (`fonts/`, made by `fonts/make_fonts.py` from free fonts) |

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
rendered pixel the same. It also compiles Skia's FreeType sources against
the headers and checks that the library has every function they call.
