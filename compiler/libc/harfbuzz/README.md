# mlx-harfbuzz

HarfBuzz's C API in Mlx, for the C and C++ programs built here: WebKit's
WebCore (`ComplexTextControllerHarfBuzz.cpp`, the FreeType font code,
`OpenTypeMathData`) and Skia's `SkShaper_harfbuzz.cpp` include `<hb.h>`,
`<hb-ot.h>`, `<hb-ft.h>` and `<hb-icu.h>` as they would HarfBuzz's
headers, and link `libmlxharfbuzz.so` in place of libharfbuzz. No
HarfBuzz code is used: text is shaped by std.shaping, the standard
library's shaping engine (`std/src/shaping`), written after HarfBuzz
8.3.0's sources so that it gives what HarfBuzz gives, glyph for glyph and
position for position. This directory is the C API over it.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, macros and functions C sees, a module per HarfBuzz
header, with HarfBuzz's names and the layout of its structures.
`mlx-cheaders` (compiler/cc) makes the C headers of them into
`mlx-out/include/mlx-harfbuzz/` (`hb.h`, `hb-ot.h`, `hb-ft.h`,
`hb-icu.h`, `hb-features.h` and the headers they include); there are no
headers written by hand. `exports.mlx` exports each function as std.shaping's
function of that name (`tools/harfbuzz_exports.py` writes it; `--check`
lists what is declared and not defined); `ft.mlx` is hb-ft over
mlx-freetype (`compiler/libc/freetype`), `harfbuzz.mlx` the library's
root.

    ./mlx build harfbuzz         the library (mlx-out/lib/libmlxharfbuzz.so) and the headers
    tools/check_harfbuzz.sh      builds them and compares with HarfBuzz 8.3

## What it does

- Blobs, faces (from blobs, TrueType collections and dfont resource
  forks, from tables, made by the face builder), fonts (scales, ppem,
  ptem, variations and named instances, synthetic bold and slant, sub
  fonts and their parents, font functions of the program's own),
  buffers (UTF-8/16/32, Latin-1 and code points in, contexts, cluster
  levels, flags, serializing and deserializing in text and JSON, diff,
  normalizing glyphs), sets, maps, the Unicode functions (std.unicode's
  data), languages, scripts, features and variations from and to strings.
- Shaping with the OpenType shaper: GSUB and GPOS (every lookup type,
  contexts, extensions, mark filtering sets, feature variations), GDEF,
  the shapers of Arabic (joining, stretching, the fallback of fonts
  without GSUB), Hebrew, Thai and Lao, Hangul, the Indic scripts (old
  and new tags), Khmer, Myanmar and the Universal Shaping Engine;
  normalization, default ignorables, the `kern` table and the fallback
  mark and space positioning, vertical text, hb_shape_justify,
  hb_ot_shape_glyphs_closure.
- The OpenType font functions: `cmap` (formats 0 to 14, symbol fonts),
  `hmtx`/`vmtx`, `HVAR`/`VVAR`/`MVAR`, `VORG`, glyph outlines and extents
  of `glyf` (with `gvar`), `CFF` and `CFF2`, glyph names (`post`, CFF),
  hb_font_draw_glyph and hb_font_paint_glyph (`CPAL`, `COLR` versions 0
  and 1 with variations, `SVG`, `CBDT`, `sbix`).
- hb-ot: layout (scripts, languages, features, lookups, their glyphs,
  closures and alternates, baselines of `BASE`, feature names and
  characters, the `size` feature), `MATH`, metrics, `name`, `STAT` and
  the style values, `meta`, `fvar`/`avar`, the tags of scripts and
  languages, and hb-ft (fonts over FreeType faces) and hb-icu (ICU's
  script codes, ICU's Unicode functions).

Not done: Apple's AAT tables (`morx`, `kerx`, `trak`, `ankr`, `feat`;
fonts that have them and OpenType tables shape with the OpenType ones),
the Uniscribe, CoreText, DirectWrite and Graphite shapers, hb-subset,
buffer messages (hb_buffer_set_message_func keeps the function, shaping
does not call it), and the full sanitizing of GSUB and GPOS (subtables
that point outside their table are not neutered as HarfBuzz neuters
them).

## Compared with HarfBuzz 8.3

`tools/check_harfbuzz.sh` compares C programs (`tests/`) built against
the library with what HarfBuzz 8.3 prints for them (`tests/expected`,
`--update` writes them with the system's libharfbuzz), runs the same
programs against the system's HarfBuzz for every font of the system, and,
with HarfBuzz's source (`HARFBUZZ_SOURCE`), HarfBuzz's own shaping tests
(`test/shape/data`): 6845 cases give the expected glyphs, as the system's
HarfBuzz does, all but those of AAT fonts. Skia's `SkShaper_harfbuzz.cpp`
compiles against the headers, and the functions it and WebCore call are
in the library.

The tables std.shaping takes of HarfBuzz's generated sources (the Indic
and USE categories, the syllable machines, the tag table...) are made by
`tools/harfbuzz/generate_tables.sh HARFBUZZ_SOURCE`.
