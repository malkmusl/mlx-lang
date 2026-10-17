# mlx-unicode

Unicode for the C and C++ programs built here, in Mlx, with the C API of
ICU. WebKit (projects/webkit) and the others include `<unicode/...>` as
they would ICU's headers. They link `libmlxunicode.so` in place of ICU.
No ICU code or data is used: the library is this repository's own, and
its tables come from the Unicode Character Database alone.

    ./mlx build unicode        unicode-generate, the tables, the library
    tools/check_unicode.sh     builds it and runs the conformance tests

## Parts

| File | What |
|---|---|
| `ucd/` | The Unicode Character Database 16.0.0 (the files used, and the conformance tests) |
| `generate.mlx` | `unicode-generate UCD OUT`: writes `tables.mlx` and `include/unicode/ugenerated.h` |
| `tables.mlx` | Two tries (property records and case records) and sorted lists (decompositions, compositions, full case mappings and foldings, script extensions, blocks, numeric values) |
| `include/unicode/` | The C headers: ICU's names and API level (74), our own text |
| `unicode.mlx` | The library's root (`--plugin --stack-arena`) |
| `properties.mlx` | `uchar.h`, `uscript.h`: every binary and enumerated property, names of properties and values, classification, simple case mappings |
| `casing.mlx` | Full case mapping (SpecialCasing.txt with its conditions: Final_Sigma, Turkish/Azerbaijani, Lithuanian) and case folding |
| `strings.mlx` | `ustring.h`: lengths, copies, comparisons (code unit, code point, case-insensitive), UTF-8 and UTF-32 conversion |
| `normalize.mlx` | `unorm2.h`: NFC, NFD, NFKC, NFKD, NFKC_Casefold |
| `breaks.mlx` | `ubrk.h`, `utext.h`: grapheme clusters, words, sentences (UAX #29) and lines (UAX #14, with the `lb` and `lw` locale keywords) |

## Conformance

`tools/check_unicode.sh` runs the UCD's own tests against the library
from C programs compiled by mlx-cc:

- NormalizationTest.txt: 19965 of 19965 lines
- GraphemeBreakTest.txt: 1093 of 1093
- WordBreakTest.txt: 1826 of 1826
- SentenceBreakTest.txt: 512 of 512
- LineBreakTest.txt: 16672 of 16672

## Not yet here

- Words in scripts written without spaces (Thai, Lao, Khmer, Myanmar,
  Chinese and Japanese runs): ICU finds those by dictionary; here they
  break by the UAX #29 and UAX #14 rules alone.
- What WebKit uses beyond these:
  - collation (`ucol`, `usearch`);
  - calendars and time zones (`ucal`);
  - formatting for `Intl` (`udat`, `unum`, `unumf`, `uplrules`, `ulistfmt`, `ureldatefmt`), which needs locale data;
  - locales (`uloc`);
  - bidi (`ubidi`);
  - IDNA (`uidna`, UTS #46);
  - legacy charset conversion and detection (`ucnv`, `ucsdet`);
  - sets (`uset`).
  They come next, in Mlx, from the Unicode and CLDR data.
- Rules of a program's own for a break iterator (`ubrk_openRules`).
