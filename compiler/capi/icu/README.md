# mlx-unicode: ICU's C API

Unicode for the C and C++ programs built here, with the C API of ICU.
WebKit (projects/webkit) and the others include `<unicode/...>` as they
would ICU's headers and link `libmlxunicode.so` in place of ICU. No ICU
code or data is used: the implementation is the standard library's
`std.unicode` (std/src/unicode, see
[docs/reference/unicode.md](../../../docs/reference/unicode.md)), with the
time zones of `std.timezone.database`; its tables come from the Unicode
data alone (the Character Database, the Collation Algorithm's table, the
IDNA mapping table), its time zones from the IANA time zone database and
its locale data from CLDR (std/registry/unicode, std/registry/tz).

This directory is the C layer over it. Each part of `std.unicode` keeps
ICU's functions under ICU's names, with C's types and ICU's error codes;
a module here exports one part's functions to C (an exported function
cannot be called from another Mlx module, so the implementation's are
`pub`, and these `export fn`s call them). The interface is written once,
in Mlx: the modules of `api/` declare the types, constants and functions
C sees, and the macros (`U8_NEXT`, `U16_APPEND`...) are Mlx functions
there. `mlx-cheaders` (compiler/cc) makes the C headers of them into
`mlx-out/include/mlx-unicode/unicode/`; there are no headers written by
hand.

    ./mlx build unicode        the generators, the tables, cldr.dat, the library, the headers
    tools/check_unicode.sh     builds it and runs the conformance tests and std.unicode's own

## Parts

| File | What |
|---|---|
| `icu.mlx` | The library's root (`--plugin --stack-arena`): `libmlxunicode.so.1`, importing the export modules |
| `api/` | The interface in Mlx, a module per header (ICU's names and API level, 74); `ugenerated.mlx` (scripts, blocks) is made by `unicode-generate` (std/registry/unicode/generate.mlx) |
| `properties.mlx` | `uchar.h`, `uscript.h` (std.unicode.properties) |
| `casing.mlx`, `strings.mlx` | `ustring.h`: case mapping and folding (std.unicode.casing); lengths, copies, comparisons, conversions (std.unicode.strings) |
| `normalize.mlx` | `unorm2.h` (std.unicode.normalize) |
| `breaks.mlx` | `ubrk.h`, `utext.h` (std.unicode.breaks) |
| `collation.mlx` | `ucol.h`, `uiter.h` (std.unicode.collation) |
| `idna.mlx` | `uidna.h` (std.unicode.idna) |
| `charsets.mlx` | `ucsdet.h` (std.unicode.charsets) |
| `enumeration.mlx` | `uenum.h` (std.unicode.enumeration) |
| `localeid.mlx`, `displaynames.mlx`, `bundles.mlx` | `uloc.h` (std.unicode.locale); `uldnames.h` and `uloc_getDisplay*` (std.unicode.locale.displaynames); `ures.h` over cldr.dat (std.unicode.locale.bundles) |
| `calendar.mlx`, `timezonenames.mlx` | `ucal.h` (std.unicode.calendar); the names of zones (std.unicode.format.date.zonenames) |
| `formatted.mlx` | `uformattedvalue.h`, `ufieldpositer.h` (std.unicode.format) |
| `numberformat.mlx`, `numberrange.mlx`, `decimalformat.mlx`, `numbersystems.mlx`, `currency.mlx` | `unumberformatter.h`, `unumberrangeformatter.h`, `unum.h` and `uformattable.h`, `unumsys.h`, `ucurr.h` (std.unicode.format.number and its parts) |
| `pluralrules.mlx` | `upluralrules.h` (std.unicode.format.plural) |
| `simpledateformat.mlx`, `patterngenerator.mlx`, `dateintervals.mlx` | `udat.h`, `udatpg.h`, `udateintervalformat.h` (std.unicode.format.date and its parts) |
| `listformat.mlx`, `reltime.mlx` | `ulistformatter.h`, `ureldatefmt.h` (std.unicode.format.list, .reltime) |
| `tests/` | The C programs check_unicode.sh builds with mlx-cc against the library, with what they print (`*.expected`); `api.mlx`, the API modules as Mlx |

The data the formatters read at run time is `cldr.dat`
(`mlx-out/share/mlx-unicode/cldr.dat`, made by `cldr-data`): the library
looks for it as `$MLX_UNICODE_DATA`, else `../share/mlx-unicode/cldr.dat`
beside its own directory, else `/usr/share/mlx-unicode/cldr.dat`.

## Conformance

`tools/check_unicode.sh` runs the UCD's own tests against the library
from C programs compiled by mlx-cc:

- NormalizationTest.txt: 19965 of 19965 lines
- GraphemeBreakTest.txt: 1093 of 1093
- WordBreakTest.txt: 1826 of 1826
- SentenceBreakTest.txt: 512 of 512
- LineBreakTest.txt: 16672 of 16672
- CollationTest (non-ignorable and shifted): 206316 and 227831 lines in order
  (with `-u-co-ducet`: DUCET's variable elements and Han order)
- collation by locale, type, keyword and rules: tests/tailoring.c prints
  what ICU 74 prints for the same program (tailoring.expected). Against
  ICU 77 (node, CLDR 47, the collation data built from CLDR 47's files
  for the comparison): every collation of every locale ICU has, with
  every Intl option, sorts the characters of its rules (and random strings
  of them, with combining marks) as ICU does, all characters of the
  Chinese pinyin, stroke and zhuyin collations and Japanese's and Korean's
  too, and 20000 Han characters in the root's order
- the time zones (std.timezone.database): every zone's offsets from 1850
  to 2100, on both sides of every transition, against the files zic made
  of the same database (where the system has them;
  std/registry/tz/compare.mlx): no difference
- IdnaTestV2.txt: 19169 of 19169 conversions (toUnicode, toAsciiN,
  toAsciiT of 6389 lines, and UTF-8)
- locale IDs and tags: tests/locales.c against ICU's results
  (locales.expected)
- the calendar: tests/calendar.c against what ICU gives (calendar.expected)
- the calendars other than the Gregorian: tests/calendars.c (every
  calendar's fields over three thousand years, `add`, `roll`, limits,
  the eras and the leap months) prints what ICU 74 prints but for
  the start of Japanese's Meiji era (CLDR 48's 1868-10-23, ICU 74's
  1868-09-08) and Saudi Arabia's preferred calendar; tests/calendarformats.c, dates of
  every calendar formatted the way JavaScriptCore formats them, against
  ICU 74 the same but where CLDR 48's data is not CLDR 44's (Śaka's
  era, Ethiopic's and Coptic's era names, `Anno Hegirae`, some
  patterns); numbering systems a pattern names for a field (`hanidays`,
  `hebr`) are not used, as JavaScriptCore with ICU loses them too
- zones' names (`ucal_getTimeZoneDisplayName`): tests/zonenames.c, the
  same as ICU 74 but where CLDR 48's names are not CLDR 44's (Japanese's
  American zones, Samoa's, Arabic's digits)
- measure units: tests/units.c (every unit JavaScriptCore's
  Intl.NumberFormat takes, X-per-Y compounds, the stems of units and
  their widths, Intl.DurationFormat's skeletons, ranges) against ICU 74:
  the same but where CLDR 48's data is not ICU 74's CLDR 44 (Arabic's
  digits, some German narrow durations, Polish days)

## Not yet here

- Of measure units: mixed units (`unit/foot-and-inch`), usages (`usage/`,
  CLDR's unit preferences and the conversions they take) and the
  grammatical cases of unit names.
- Words in scripts written without spaces (Thai, Lao, Khmer, Myanmar,
  Chinese and Japanese runs): ICU finds those by dictionary; here they
  break by the UAX #29 and UAX #14 rules alone.
- What WebKit uses beyond these:
  - searching by collation (`usearch`); the sets of a collator
    (`ucol_getTailoredSet`, `ucol_getContractionsAndExpansions`);
  - Windows' zone IDs; parsing leap months and cyclic years of the
    Chinese calendars;
  - formatting for `Intl` (`udat`, `unum`, `unumf`, `uplrules`, `ulistfmt`, `ureldatefmt`), which needs locale data;
  - display names of locales (`uloc_getDisplay*`, `uldn`: the codes stand
    for themselves until CLDR's names are here);
  - bidi (`ubidi`);
  - legacy charset conversion (`ucnv`); detecting single-byte charsets
    other than Latin-1 and windows-1252 (ICU rates them by n-gram
    statistics of their languages);
  - sets (`uset`).
  They come next, in Mlx, from the Unicode and CLDR data.
- Rules of a program's own for a break iterator (`ubrk_openRules`).
- Collation, where it is not as ICU's:
  - CLDR 48's Han order (`radicals.txt`) is Unicode 17's: a few Han
    characters sort otherwise than with ICU 77 (CLDR 47); the locales
    ICU has no collation data for (aa, blo, cu, kk_Arab, sgs, ssy, vo)
    have CLDR's tailorings here;
  - with alternate shifted, ICU's `ucol_strcoll` (it skips an identical
    prefix) counts a primary-ignorable character of combining class 0
    after a variable one (`-ੰ`, Thai's `๛์`) that its sort keys ignore;
    here both ignore it;
  - `ucol_setVariableTop` makes the string's primary the variable top
    itself (ICU 53 and later take the end of its group); `[hiraganaQ
    on]` is ignored (ICU refuses it); U+FFFE is not the merge separator;
    sort keys are not ICU's bytes (they order as `ucol_strcoll` does).
