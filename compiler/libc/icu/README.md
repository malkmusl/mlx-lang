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
| `bidi.mlx` | `ubidi.h` (std.unicode.bidi) |
| `search.mlx` | `usearch.h` (std.unicode.search, over std.unicode.collation and breaks) |
| `converters.mlx` | `ucnv.h`, `ucnv_err.h`, `ucnv_cb.h` (std.unicode.converters, over std.unicode.charmaps); the callbacks of `ucnv_err.h` |
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
- BidiTest.txt: 770241 of 770241 cases (each line in each paragraph
  direction its bitset names: the levels and the visual order), and
  BidiCharacterTest.txt: 91707 of 91707 lines (ICU 74 itself fails 30 of
  them, of the rules for brackets that Unicode 16 settles); and
  tests/bidirandom.c, 6000 random texts of every bidi class (brackets,
  marks, controls, isolates, both kinds of digits, paragraph separators,
  surrogates) in every paragraph direction, prints what ICU 74 prints:
  the levels, the direction, the runs in both orders, the maps, the text
  written in visual order with each option, and a line of each
  (bidirandom.expected). Where UAX #9 leaves the choice to the
  implementation the library takes ICU's (a level per UTF-16 unit with a
  lead surrogate as BN, removed characters at the level of the next one,
  text of one direction reported at its paragraph level, the paragraph
  level of a line of one direction), and also ICU's way of resolving the
  brackets and the numbers of a paragraph after the first in text without
  explicit controls (by the level of the paragraph before it)
- string search: tests/search.c, as WebKit's find in page uses it (one
  searcher of `<locale>@collation=search`, secondary strength and the
  pattern's base letters as wildcards for any case, tertiary and exact
  comparison else) and with every strength and element comparison,
  overlap, offsets, first/last, following/preceding, the matched text,
  an ignorable pattern, the errors, over texts of case, accents
  (precomposed and combining), expansions (ß, æ, ﬁ), contractions (Danish
  "aa"), ignorables, Hangul, Arabic and Hebrew, and 1500 random searches
  (forwards: ICU 74's backward search leaves out, in some of them, a
  first match made of an expansion, and takes ignorables after a match
  into it), prints what ICU 74 prints (search.expected)
- converters: tests/converters.c, as WebKit's TextCodecICU uses them
  (open by name, fallbacks, `ucnv_toUnicode` in pieces into a small
  buffer until it stops overflowing, the substitute callback stopping on
  illegal input, `ucnv_fromUnicode` with the XML escape or a callback of
  its own writing with `ucnv_cbFromUWriteBytes`, reset) and the rest of
  the API: names, aliases and `ucnv_compareNames`, whole texts both ways
  with every callback and escape context, ill-formed UTF-8 (maximal
  subparts), UTF-16 and UTF-32 with and without byte order marks, unpaired
  surrogates, the substitution, the invalid input a callback is given,
  what is pending, offsets, one character at a time, `ucnv_convert` and
  `ucnv_convertEx` with a small pivot, signatures, clones, the errors,
  and 1500 random texts of whole and broken characters converted both
  ways in random pieces into random room: prints what ICU 74 prints
  (converters.expected)
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
  - the sets of a collator
    (`ucol_getTailoredSet`, `ucol_getContractionsAndExpansions`);
  - Windows' zone IDs; parsing leap months and cyclic years of the
    Chinese calendars;
  - detecting single-byte charsets other than Latin-1 and windows-1252
    (ICU rates them by n-gram statistics of their languages);
  - sets (`uset`).
  They come next, in Mlx, from the Unicode and CLDR data.
- Of the converters (`ucnv.h`): the East Asian multi-byte charsets
  (Shift_JIS, EUC-JP, GBK, gb18030, Big5, EUC-KR: WebKit has its own),
  EUC-TW, the stateful ones (ISO-2022-*, HZ, UTF-7, SCSU, BOCU-1), EBCDIC
  and CESU-8: opening one fails with `U_FILE_ACCESS_ERROR`. The
  single-byte charsets map as the Encoding Standard's tables do (where
  ICU's tables differ in a few bytes, the browser's reading); a
  converter's name is its own (`ISO-8859-2`, not ICU's
  `ibm-912_P100-1995`), with ICU 74's aliases for the Unicode, Latin-1
  and ASCII converters and the Encoding Standard's labels for the
  others; `ucnv_getStandardName` and `ucnv_getCanonicalName` answer that
  name whatever the standard. The default converter is UTF-8, as ICU's is
  built with `U_CHARSET_IS_UTF8` (`ucnv_setDefaultName` changes nothing).
  Not here: `ucnv_getUnicodeSet`, `ucnv_getStarters`,
  `ucnv_getDisplayName`, the algorithmic and package functions
  (`ucnv_openPackage` fails), `ucnv_openAllNames`, the standards' names;
  a callback is not told of a clone (`UCNV_CLONE`); in random pieces,
  where a call stops when its output overflows is not always ICU's (the
  output is).
- Of `usearch.h`: canonical matching (`USEARCH_CANONICAL_MATCH`, kept
  but not applied, as it takes no part in ICU 74 either); a match goes on
  over at most two clusters that do not match while looking for a
  contraction that would.
- Of `ubidi.h`: the reordering modes other than the default and the
  inverse algorithm (kept, not applied), the options that insert marks
  (`UBIDI_OPTION_INSERT_MARKS`, `UBIDI_INSERT_LRM_FOR_NUMERIC`) and
  streaming; `ubidi_setContext`'s text is kept but takes no part.
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
