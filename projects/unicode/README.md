# mlx-unicode

Unicode for the C and C++ programs built here, in Mlx, with the C API of
ICU. WebKit (projects/webkit) and the others include `<unicode/...>` as
they would ICU's headers. They link `libmlxunicode.so` in place of ICU.
No ICU code or data is used: the library is this repository's own, and
its tables come from the Unicode data alone (the Character Database, the
Collation Algorithm's table, the IDNA mapping table), its time zones
from the IANA time zone database and its locale data from CLDR.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants and functions C sees, and the macros (`U8_NEXT`,
`U16_APPEND`...) are Mlx functions there. `mlx-cheaders` (projects/cc)
makes the C headers of them into `mlx-out/include/mlx-unicode/unicode/`;
there are no headers written by hand.

    ./mlx build unicode        unicode-generate, tz-generate, cldr-generate, collation-generate, the tables, the library, the headers
    tools/check_unicode.sh     builds it and runs the conformance tests

## Parts

| File | What |
|---|---|
| `ucd/` | The Unicode Character Database 16.0.0 (the files used, and the conformance tests) |
| `generate.mlx` | `unicode-generate UCD OUT`: writes `tables.mlx` and `api/ugenerated.mlx` |
| `tables.mlx` | Two tries (property records and case records) and sorted lists (decompositions, compositions, full case mappings and foldings, script extensions, blocks, numeric values) |
| `api/` | The interface in Mlx, a module per header (ICU's names and API level, 74); `ugenerated.mlx` (scripts, blocks) is made by `generate.mlx`; the formatting headers WebKit's Intl includes (`udat.h`, `udatpg.h`, `unum.h`, `unumberformatter.h`, `unumberrangeformatter.h`, `upluralrules.h`, `ulistformatter.h`, `ureldatefmt.h`, `uldnames.h`, `udateintervalformat.h`, `ucurr.h`, `unumsys.h`, `ures.h` and the headers they include) are declared, not implemented yet |
| `uca/`, `idna/` | The Unicode Collation Algorithm's table (DUCET) and its tests; the IDNA mapping table and its tests |
| `tz/` | The IANA time zone database 2026c (`tzdata.zi`, the form zic reads; `zone.tab`, the zones of each country) |
| `cldr/` | CLDR 48 (JSON): likely subtags, the aliases of languages, scripts, regions and variants, the BCP 47 keys and types, ISO 3166 codes, the available locales; `packages/`, CLDR's npm packages (core, dates, numbers, misc, locale names, units); `collation/`: CLDR 48's collation data (its XML, a file a locale) and the root's radical-stroke order of Han (`radicals.txt`, the `[radical]` lines of CLDR's FractionalUCA) |
| `cldrgenerate.mlx` | `cldr-generate CLDR OUT`: writes `locales.mlx`, CLDR's data as sorted tables |
| `cldrdata.mlx` | `cldr-data PACKAGES OUT`: writes `cldr.dat` (`mlx-out/share/mlx-unicode`), the formatters' locale data of CLDR's packages, each locale's keys and values where they differ from its parent's |
| `measureunits.mlx` | Measure units for `unumf`: ICU's built-in units and core unit identifiers (products, powers, SI and binary prefixes, `-per-`), their patterns by plural form from CLDR's units (built-in ones their own; others put together of the per, times, power and prefix patterns, as UTS #35 says) |
| `tzgenerate.mlx` | `tz-generate TZ OUT`: writes `timezones.mlx`, every zone's transitions worked out from its rules as zic does (through 2037, then the rules in force) |
| `unicode.mlx` | The library's root (`--plugin --stack-arena`) |
| `properties.mlx` | `uchar.h`, `uscript.h`: every binary and enumerated property, names of properties and values, classification, simple case mappings |
| `casing.mlx` | Full case mapping (SpecialCasing.txt with its conditions: Final_Sigma, Turkish/Azerbaijani, Lithuanian) and case folding |
| `strings.mlx` | `ustring.h`: lengths, copies, comparisons (code unit, code point, case-insensitive), UTF-8 and UTF-32 conversion |
| `normalize.mlx` | `unorm2.h`: NFC, NFD, NFKC, NFKD, NFKC_Casefold |
| `breaks.mlx` | `ubrk.h`, `utext.h`: grapheme clusters, words, sentences (UAX #29) and lines (UAX #14, with the `lb` and `lw` locale keywords) |
| `collation.mlx` | `ucol.h`, `uiter.h`: the Unicode Collation Algorithm with CLDR's root collation (DUCET's order, CLDR's variable top and Han order; `-u-co-ducet` for DUCET's own) and the tailorings of CLDR's locales and types (`sv`, `de-u-co-phonebk`, `zh-TW`...), or a program's rules (`ucol_openRules`); the attributes (strength, alternate shifted and the variable top, case first and case level, French, numeric), script reordering, the `-u-` keywords, the locales and types there are |
| `collationelements.mlx` | Collation elements (32-bit primaries, room between the root's weights for a tailoring's), the root's (contractions, discontiguous ones too, implicit weights, Han in radical-stroke order), its script groups; a tailoring's mappings (contractions, prefixes) |
| `collationbuilder.mlx` | Rules (UTS #35 Part 5, as ICU reads them: resets, `[before n]`, the special positions, relations, starred lists, prefixes, extensions, `[import]`, `[reorder]`, `[suppressContractions]`, the settings) into a tailoring, as ICU's CollationBuilder makes it; a locale's chain of collation data (likely script, truncation, CLDR's parents for collation) |
| `collationgenerate.mlx` | `collation-generate OUT RADICALS XML...`: writes `collationrules.mlx`, each locale's default type and each type's rules (as ICU has them: Chinese's short ones, no provisional or unconfirmed ones), and the Han order |
| `calendar.mlx` | `ucal.h`: the Gregorian calendar (Julian before its change, 1582 or as set), fields resolved as ICU resolves them, `add`, `roll`, limits, differences, times a transition skips or repeats; zone IDs (canonical, IANA, custom `GMT+hh:mm`), the host's and the default zone, enumerations of zones |
| `zones.mlx` | The time zones at run time: an ID's zone, the offsets at a moment, the transitions |
| `localeid.mlx` | `uloc.h`: ICU's locale IDs (their parts, names, keywords), BCP 47 tags to and from them (the extensions as keywords, legacy tags), likely subtags (UTS #35), the BCP 47 keys and types and their legacy names, orientation, ISO codes |
| `locale.mlx` | The default locale (from the environment), a locale's language, region (its own, `rg`, or the likely one) and keywords, the region's week (CLDR's week data) |
| `idna.mlx` | `uidna.h`: UTS #46 (mapping, NFC, the validity criteria, bidi, CONTEXTJ and CONTEXTO), Punycode, ToASCII and ToUnicode of names and labels, UTF-16 and UTF-8 |
| `charsets.mlx` | `ucsdet.h`: charset detection (the Unicode forms by mark and validity, ISO-2022 by its escapes, Shift_JIS, EUC-JP, EUC-KR, GB18030 and Big5 by their structure, Latin-1 and windows-1252) |
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
  (with `-u-co-ducet`: DUCET's variable elements and Han order)
- collation by locale, type, keyword and rules: tests/tailoring.c prints
  what ICU 74 prints for the same program (tailoring.expected). Against
  ICU 77 (node, CLDR 47, the collation data built from CLDR 47's files
  for the comparison): every collation of every locale ICU has, with
  every Intl option, sorts the characters of its rules (and random strings
  of them, with combining marks) as ICU does, all characters of the
  Chinese pinyin, stroke and zhuyin collations and Japanese's and Korean's
  too, and 20000 Han characters in the root's order
- the time zones: every zone's offsets from 1850 to 2100, on both sides
  of every transition, against the files zic made of the same database
  (where the system has them): no difference
- IdnaTestV2.txt: 19167 of 19167 conversions (toUnicode, toAsciiN,
  toAsciiT of 6389 lines)
- locale IDs and tags: tests/locales.c against ICU's results
  (locales.expected)
- the calendar: tests/calendar.c against what ICU gives (calendar.expected)
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
  - calendars other than the Gregorian; time zone names other than the
    localized GMT format (`GMT+01:00`); Windows' zone IDs;
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
