# mlx-unicode

Unicode for the C and C++ programs built here, in Mlx, with the C API of
ICU. WebKit (projects/webkit) and the others include `<unicode/...>` as
they would ICU's headers. They link `libmlxunicode.so` in place of ICU.
No ICU code or data is used: the library is this repository's own, and
its tables come from the Unicode data alone (the Character Database, the
Collation Algorithm's table, the IDNA mapping table) and its time zones
from the IANA time zone database.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants and functions C sees, and the macros (`U8_NEXT`,
`U16_APPEND`...) are Mlx functions there. `mlx-cheaders` (projects/cc)
makes the C headers of them into `mlx-out/include/mlx-unicode/unicode/`;
there are no headers written by hand.

    ./mlx build unicode        unicode-generate, tz-generate, the tables, the library, the headers
    tools/check_unicode.sh     builds it and runs the conformance tests

## Parts

| File | What |
|---|---|
| `ucd/` | The Unicode Character Database 16.0.0 (the files used, and the conformance tests) |
| `generate.mlx` | `unicode-generate UCD OUT`: writes `tables.mlx` and `api/ugenerated.mlx` |
| `tables.mlx` | Two tries (property records and case records) and sorted lists (decompositions, compositions, full case mappings and foldings, script extensions, blocks, numeric values) |
| `api/` | The interface in Mlx, a module per header (ICU's names and API level, 74); `ugenerated.mlx` (scripts, blocks) is made by `generate.mlx` |
| `uca/`, `idna/` | The Unicode Collation Algorithm's table (DUCET) and its tests; the IDNA mapping table and its tests |
| `tz/` | The IANA time zone database 2026c (`tzdata.zi`, the form zic reads; `zone.tab`, the zones of each country) |
| `tzgenerate.mlx` | `tz-generate TZ OUT`: writes `timezones.mlx`, every zone's transitions worked out from its rules as zic does (through 2037, then the rules in force) |
| `unicode.mlx` | The library's root (`--plugin --stack-arena`) |
| `properties.mlx` | `uchar.h`, `uscript.h`: every binary and enumerated property, names of properties and values, classification, simple case mappings |
| `casing.mlx` | Full case mapping (SpecialCasing.txt with its conditions: Final_Sigma, Turkish/Azerbaijani, Lithuanian) and case folding |
| `strings.mlx` | `ustring.h`: lengths, copies, comparisons (code unit, code point, case-insensitive), UTF-8 and UTF-32 conversion |
| `normalize.mlx` | `unorm2.h`: NFC, NFD, NFKC, NFKD, NFKC_Casefold |
| `breaks.mlx` | `ubrk.h`, `utext.h`: grapheme clusters, words, sentences (UAX #29) and lines (UAX #14, with the `lb` and `lw` locale keywords) |
| `collation.mlx` | `ucol.h`, `uiter.h`: the Unicode Collation Algorithm with DUCET (contractions, discontiguous ones too, implicit weights, the attributes: strength, alternate shifted, case first and case level, French, numeric) |
| `calendar.mlx` | `ucal.h`: the Gregorian calendar (Julian before its change, 1582 or as set), fields resolved as ICU resolves them, `add`, `roll`, limits, differences, times a transition skips or repeats; zone IDs (canonical, IANA, custom `GMT+hh:mm`), the host's and the default zone, enumerations of zones |
| `zones.mlx` | The time zones at run time: an ID's zone, the offsets at a moment, the transitions |
| `locale.mlx` | The default locale (from the environment), a locale's language, region (its own, `rg`, or the likely one) and keywords, the region's week (CLDR's week data) |
| `idna.mlx` | `uidna.h`: UTS #46 (mapping, NFC, the validity criteria, bidi, CONTEXTJ and CONTEXTO), Punycode, ToASCII and ToUnicode of names and labels, UTF-16 and UTF-8 |
| `enumeration.mlx` | `uenum.h` |

## Conformance

`tools/check_unicode.sh` runs the UCD's own tests against the library
from C programs compiled by mlx-cc:

- NormalizationTest.txt: 19965 of 19965 lines
- GraphemeBreakTest.txt: 1093 of 1093
- WordBreakTest.txt: 1826 of 1826
- SentenceBreakTest.txt: 512 of 512
- LineBreakTest.txt: 16672 of 16672
- CollationTest (non-ignorable and shifted): 206316 and 227831 lines in order
- the time zones: every zone's offsets from 1850 to 2100, on both sides
  of every transition, against the files zic made of the same database
  (where the system has them): no difference
- IdnaTestV2.txt: 19167 of 19167 conversions (toUnicode, toAsciiN,
  toAsciiT of 6389 lines)
- the calendar: tests/calendar.c against what ICU gives (calendar.expected)

## Not yet here

- Words in scripts written without spaces (Thai, Lao, Khmer, Myanmar,
  Chinese and Japanese runs): ICU finds those by dictionary; here they
  break by the UAX #29 and UAX #14 rules alone.
- What WebKit uses beyond these:
  - searching by collation (`usearch`), collation by locale (tailorings);
  - calendars other than the Gregorian; time zone names other than the
    localized GMT format (`GMT+01:00`); Windows' zone IDs;
  - formatting for `Intl` (`udat`, `unum`, `unumf`, `uplrules`, `ulistfmt`, `ureldatefmt`), which needs locale data;
  - locales (`uloc`);
  - bidi (`ubidi`);
  - legacy charset conversion and detection (`ucnv`, `ucsdet`);
  - sets (`uset`).
  They come next, in Mlx, from the Unicode and CLDR data.
- Rules of a program's own for a break iterator (`ubrk_openRules`).
