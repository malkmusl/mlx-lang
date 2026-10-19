# mlx-fontconfig

Fontconfig for the C and C++ programs built here, in Mlx, with
Fontconfig's C API (2.15): WebKit's font code (WebCore's `FontCacheSkia`
and `SkiaSystemFallbackFontCache`, the FreeType path's `FontCacheFreeType`
and its helpers, Skia's `SkFontMgr_fontconfig` and
`SkFontConfigInterface_direct`) includes `<fontconfig/fontconfig.h>` and
`<fontconfig/fcfreetype.h>` as it would Fontconfig's headers and links
`libmlxfontconfig.so` in place of `libfontconfig.so.1`. The work is the
standard library's `std.fontconfig` (std/src/fontconfig/): fonts.conf and
its rules, the fonts read with std.truetype, matching, sorting and
listing as Fontconfig does them. This directory is the C API over it.

The interface is written once, in Mlx: `api/fontconfig.mlx` and
`api/fcfreetype.mlx` declare the types, constants, macros and functions C
sees, and `mlx-cheaders` (compiler/cc) makes
`mlx-out/include/mlx-fontconfig/fontconfig/*.h` of them; there is no
header written by hand. (`fcfreetype.h` includes mlx-freetype's
`<freetype/freetype.h>`: `-I mlx-out/include/mlx-freetype` too.)

    ./mlx build fontconfig            the library, its headers, std.fontconfig's data
    tools/check_fontconfig.sh         builds them, runs the tests against Fontconfig 2.15
    tests/373_fontconfig_runtime.mlx  std.fontconfig itself (tests/run_std_modules.sh)

## The parts

| File | What it has |
|------|-------------|
| `fontconfig.mlx` | The library's root (`--plugin --stack-arena`, linked to libmlxfreetype.so.6), importing the parts |
| `common.mlx` | The C library's memory for std.fontconfig, the environment (read with `getenv`), the configuration a null `FcConfig*` means, C strings, C's variable arguments |
| `patterns.mlx` | `FcPattern*`, `FcValue*`, `FcPatternBuild`, the iterators, `FcName*` (parse, unparse, constants, object types), ranges, matrices, weights, `FcPatternFormat`, the printing of fcdbg.c |
| `sets.mlx` | `FcCharSet*`, `FcLangSet*` and the language functions, `FcFontSet*`, `FcObjectSet*`, `FcStr*`, `FcStrSet*`, `FcStrList*`, UTF-8 and UTF-16 |
| `configs.mlx` | `FcInit*`, `FcFini`, `FcConfig*` (files, directories, fonts, the current configuration, application fonts, substitution, rescans), matching, sorting, listing, `FcDefaultSubstitute`, scanning, `FcFreeTypeQuery*`, the cache functions, blanks, `FcAtomic*` |
| `freetype.mlx` | fcfreetype.h over mlx-freetype's `FT_Face`: `FcFreeTypeCharIndex`, `FcFreeTypeCharSet(AndSpacing)`, `FcFreeTypeQueryFace`, `FcPattern{Add,Get}FTFace` |

## What std.fontconfig does

- **Configuration**: fonts.conf (`FONTCONFIG_FILE`, `FONTCONFIG_PATH`,
  /etc/fonts) and what it includes (conf.d's `[0-9]*.conf` in order,
  `ignore_missing`, the `xdg` prefix, a file read once), or Fontconfig's
  fallback configuration when there is none. `<dir>` (with the `xdg` and
  `relative` prefixes, a directory named again moving to the end),
  `<cachedir>`, `<reset-dirs>`, `<rescan>`, `<alias>` (prefer, accept,
  default, binding), `<match>` with the pattern, font and scan targets:
  `<test>` (qual, compare, ignore-blanks, target) and `<edit>` (every
  mode and binding) over every expression (constants, `<name>`, `<const>`,
  arithmetic, comparisons, `<if>`, `<floor>`..., charsets, langsets,
  ranges, matrices), `<selectfont>` with globs and patterns.
- **Fonts**: the font directories and their subdirectories, in
  Fontconfig's order (files by name, directories after); each face and
  named instance of TrueType, OpenType (CFF and CFF2), their collections
  and variable fonts, read by std.truetype into Fontconfig's pattern:
  families, styles and full names in each language of the `name` table
  (Mac Roman, UTF-16, the legacy CJK encodings read as Fontconfig reads
  them), the PostScript name, weight, width and slant (OS/2, the style
  names' words, `fvar`'s axes as ranges), spacing, the character set
  (`cmap`), the languages it covers (Fontconfig's 279 orthographies, its
  exclusive CJK rules, OS/2 code pages), capabilities (`GSUB`/`GPOS`
  scripts, Graphite), font format, foundry, decorative, color, the
  scan rules applied. A directory's fonts are kept in a cache of this
  library's own (a text file a directory under
  `$XDG_CACHE_HOME/mlx-fontconfig`, used while the directory's time is
  the one it has).
- **Matching**: `FcConfigSubstitute` and `FcDefaultSubstitute` as
  Fontconfig's; `FcFontMatch` and `FcFontSetMatch` with its priorities
  (family by position and binding, PostScript names, languages,
  character coverage, sizes, ranges...), `FcFontRenderPrepare`;
  `FcFontSort` (trimmed by coverage, its charset); `FcFontList` with its
  hash order; names compared with Fontconfig's case folding.

On this machine (fontconfig 2.15.0, 95 TrueType and OpenType fonts and 8
Type 1 ones) the fonts' patterns are the system Fontconfig's (their
`FcNameUnparse` texts are equal) and so are the matches and sorts of the
42 queries of `tests/system.c` and its lists.

## What it does not do

- Type 1 (`.pfb`), PCF, BDF and the other bitmap formats are not read:
  std.truetype reads TrueType and OpenType. Fontconfig lists them, this
  library does not.
- Fontconfig's own cache files (`/var/cache/fontconfig`, the
  `<cachedir>`s) are neither read nor written: `FcDirCacheLoadFile`
  returns null, the `FcCache*` of `FcDirCacheRead` is this library's.
  `FcDirCacheClean` and the UUID functions do nothing.
- `FcPatternFormat` knows the common part of fcformat.c's language (text,
  `%{object}`, `%{:object}`, `%{object=}`, `%{object[n]}`,
  `%{object:-default}`, widths, `%{=unparse}`, `%{+a,b{...}}` and
  `%{-a,b{...}}`); conditions, enumerations, converters and the other
  builtins make it return null.
- `FcConfigSetSysRoot` keeps the root and hands it back, but files are
  read where they are. Blanks are gone, as in Fontconfig 2.15.
- `FcFreeTypeQueryFace` reads the face's file again; a face opened from
  memory gets a smaller pattern of what `FT_Face` says. A bitmap-only
  face's spacing is taken from its own advances, not from the strike
  nearest 16 pixels.
- Fontconfig's warnings on standard error (a bad value, a bad language
  tag) are not printed. The case folding is of the Unicode Character
  Database in std/registry/unicode (16.0; Fontconfig 2.15's is 15.1).
