# Unicode and locales: `std.unicode`

`std.unicode` (`std/src/unicode.mlx`, with its parts in `std/src/unicode/`)
is Unicode and the locale data of CLDR in Mlx: the properties of code
points, case mapping, normalization, text boundaries, collation, IDNA,
charset detection, locale IDs, calendars, and the formatting of numbers,
dates, lists, relative times, plural forms and the names of languages,
scripts and regions. No ICU code or data is used. Its tables come from the
Unicode Character Database 16.0, the Collation Algorithm's table (DUCET)
and the IDNA mapping table, its locale data from CLDR 48, its time zones
(`std.timezone.database`) from the IANA database 2026c.

The same implementation is ICU's C API for C and C++ programs:
`compiler/capi/icu` exports each part's ICU functions as
`libmlxunicode.so`, with headers made of Mlx modules
([its README](../../compiler/capi/icu/README.md), which also has the
conformance results and what is not done yet). WebKit's JavaScriptCore
runs its `Intl` on it.

```mlx
const unicode = @import("std.unicode")
const casing = @import("std.unicode.casing")
const collation = @import("std.unicode.collation")
const number = @import("std.unicode.format.number")

fn example(allocator: unicode.utf.Allocator) -> unicode.Error!void {
    const upper = try casing.toUpper(allocator, "straße", "")   // "STRASSE"
    defer unicode.utf.free8(allocator, upper)
    var swedish = try collation.openCollator("sv")
    defer collation.closeCollator(&swedish)
    const order = collation.compareText(&swedish, "z", "ö")     // .less
    const price = try number.formatNumber(allocator, "de", "currency/EUR", 1234.5)   // "1.234,50 €"
    defer unicode.utf.free8(allocator, price)
}
```

## Conventions

- Each part is imported by its own name; `std.unicode` itself holds only
  what they share: `Error`, `UNICODE_VERSION`, `CLDR_VERSION`, and
  `std.unicode.utf`.
- Text is UTF-8 in slices; an ill-formed sequence reads as U+FFFD for its
  maximal subpart. Code points are `u32`.
- What a function makes is in memory from the `Allocator` it is given and
  is the caller's: `utf.free8(allocator, text)` frees it.
- Failures are `unicode.Error`: `OutOfMemory`, `InvalidArgument` (an
  argument out of range, a locale ID longer than ICU's 157 bytes),
  `InvalidFormat` (a pattern, skeleton, rule or tag that does not parse),
  `InvalidText` (a domain name UTS #46 rejects), `MissingData` (no
  `cldr.dat`), `Unsupported`, `Failed`. Lookups that may find nothing give
  optionals (`properties.numericValue`, `database.lookup`,
  `charsets.detect`).
- Each part also keeps ICU's functions under ICU's names (`u_charType`,
  `ucol_strcoll`, `udat_format`...) with C's types: UTF-16 text and
  `UErrorCode` by address, C strings. They are what `compiler/capi/icu`
  exports; Mlx programs use the native functions.
- `std.unicode.utf` needs neither tables nor the C library. The other
  parts keep their working memory in the C library's heap, so a program
  that imports one of them is linked against the C library.

## The parts

| Module | Native API |
| --- | --- |
| `std.unicode.utf` | `decode8`, `encode8`, `decode16`, `encode16` (a `Decoded` code point and its length), `valid8`, `count8`, `units16Of`, `bytes8Of`, `toUtf16` and `toUtf8` (whole texts, with an allocator), `copy`, `free8`, `free16` |
| `std.unicode.properties` | `generalCategory` (`GeneralCategory`), `eastAsianWidthOf`, `hasProperty` (by ICU's `UProperty` number), `isAlphabetic`, `isLowercase`, `isUppercase`, `isWhiteSpace`, `isDigit`, `isLetter`, `isPunctuation`, `isMark`, `isEmojiPresentation`, `isExtendedPictographic`, `isDefaultIgnorable`, the simple `toUpper`, `toLower`, `toTitle`, `foldCase`, `combiningClassOf`, `decimalValue`, `numericValue`, `scriptOf`, `usesScript` (Script_Extensions), `scriptCodeName`, `scriptName`, `scriptNamed`, `columnWidth` and `textWidth` (the columns a terminal gives, as wcwidth with Unicode's data) |
| `std.unicode.casing` | `toUpper`, `toLower` (full mappings, Final_Sigma, a language's rules: `tr`, `az`, `lt`, `el`), `fold` |
| `std.unicode.normalize` | `normalizeText` and `isNormalized` by `Form` (`nfc`, `nfd`, `nfkc`, `nfkd`, `nfkcCasefold`) |
| `std.unicode.breaks` | `open` (a `Kind`: `grapheme`, `word`, `line`, `sentence`; a locale for the `lb` and `lw` keywords), `next` (the next boundary as a byte offset, null at the end), `close`, `count` |
| `std.unicode.collation` | `openCollator` (a locale with its tailoring and `-u-co-`, `-u-kn-`... keywords), `openRules` (UTS #35 rules over the root), `setStrength`, `setNumeric`, `setIgnorePunctuation`, `compareText` (an `Order`), `closeCollator` |
| `std.unicode.idna` | `toAscii`, `toUnicode` (UTS #46, nontransitional, the bidi and CONTEXTJ rules) |
| `std.unicode.charsets` | `detect`: the likely charset (`Detected`: name, language, confidence) |
| `std.unicode.charmaps` | the single-byte charsets (ISO-8859-2 to -16, the Windows code pages, KOI8-R and -U, IBM866, the Mac OS ones) as the Encoding Standard's tables have them: `find` by name or label (a `Charmap`), `nameOf`, `decode` a byte, `encode` a code point (`NONE` where there is none); no tables beyond its own, no C library |
| `std.unicode.locale` | `canonical`, `toLanguageTag`, `fromLanguageTag`, `maximize` and `minimize` (likely subtags), `defaultLocale` |
| `std.unicode.locale.displaynames` | `languageName`, `scriptName`, `regionName` in a display locale |
| `std.unicode.calendar` | `fieldsAt`: a moment's `Fields` (era, year, month, day, weekday, time, the zone's offsets...) in a locale's calendar (its `calendar` keyword or its region's preference: Buddhist, Japanese, Hebrew, Islamic, Chinese...) and a zone; `calendarType` |
| `std.unicode.format.number` | `openFormatter` (a locale and an ICU number skeleton: `currency/EUR`, `percent scale/100`, `compact-short`, `measure-unit/length-meter unit-width-full-name`...), `formatDouble`, `formatInteger`, `closeFormatter`, and `formatNumber` at once |
| `std.unicode.format.plural` | `category`: the plural `Category` of a number, cardinal or ordinal |
| `std.unicode.format.date` | `formatSkeleton` (the best pattern for a skeleton such as `yMMMMd` or `jm`), `formatPattern` (UTS #35 patterns), in a zone and the locale's calendar |
| `std.unicode.format.list` | `joinList` (`ListKind`: conjunction, disjunction, units; `ListWidth`) |
| `std.unicode.format.reltime` | `formatRelative` ("tomorrow", "in 3 hours"; `RelativeUnit`, numeric or not) |
| `std.timezone.database` | `lookup`, `lookupSetting` (a `$TZ` value or a zone file's path), `offsetOf`, and by ID and zone `find`, `idName`, `zoneOf`, `offsetsAt`, `totalAt`, `nextTransition`, `previousTransition`, `version` |

`std.unicode.format` itself is what the formatters share: formatted values
and the spans of their fields (ICU's `UFormattedValue`).

`tests/344_unicode_utf_runtime.mlx` tests `std.unicode.utf` (a static
program: no tables, no C library); `tests/345_unicode_text_runtime.mlx`
the text parts (properties, casing, normalization, breaks, collation by
locale and by rules, IDNA, charset detection, the single-byte charsets);
`tests/346_unicode_locale_runtime.mlx` the locale parts over `cldr.dat`
(tags, likely subtags, display names, calendar fields in Berlin and in
the Thai Buddhist calendar, numbers, plural categories, dates, lists,
relative times) and `std.timezone.database` by name.
`tools/check_unicode.sh` runs them with the C tests.

## Time zones

`std.timezone` reads the system's zone (`$TZ`, `/etc/localtime`, TZif
files and POSIX rules) and has the civil calendar; `std.timezone.database`
(`std/src/timezone/database.mlx`) is the whole IANA database built in
(`std/src/timezone/tzdata.mlx`, made by `tz-generate`), reckoned with the
same civil calendar. `std.unicode.calendar` and the zone names of date
formats stand on the database; `std/registry/tz/compare.mlx` checks it
against the zone files zic made of the same database.

## Where its parts are

| File | What |
| --- | --- |
| `std/src/unicode.mlx` | The root: `Error`, ICU's error codes as it, the buffers and transforms the native functions are made of |
| `utf.mlx` | UTF-8 and UTF-16 |
| `memory.mlx` | What the ICU-level functions stand on: memory by address, C's heap, ICU's error codes |
| `tables.mlx` | Generated (`unicode-generate`): two tries (property records and case records) and sorted lists (decompositions, compositions, full case mappings and foldings, script extensions, blocks, numeric values) |
| `properties.mlx` | `uchar.h`, `uscript.h`: every binary and enumerated property, names of properties and values, classification, simple case mappings |
| `casing.mlx` | Full case mapping (SpecialCasing.txt with its conditions: Final_Sigma, Turkish/Azerbaijani, Lithuanian) and case folding |
| `strings.mlx` | `ustring.h`: lengths, copies, comparisons (code unit, code point, case-insensitive), UTF-8 and UTF-32 conversion |
| `normalize.mlx` | `unorm2.h`: NFC, NFD, NFKC, NFKD, NFKC_Casefold |
| `breaks.mlx` | `ubrk.h`, `utext.h`: grapheme clusters, words, sentences (UAX #29) and lines (UAX #14, with the `lb` and `lw` locale keywords) |
| `collation.mlx` | `ucol.h`, `uiter.h`: the Unicode Collation Algorithm with CLDR's root collation and the tailorings of CLDR's locales and types, or a program's rules; the attributes, script reordering, the `-u-` keywords |
| `collation/elements.mlx` | Collation elements, the root's (contractions, implicit weights, Han in radical-stroke order), its script groups; a tailoring's mappings |
| `collation/builder.mlx` | Rules (UTS #35 Part 5, as ICU reads them) into a tailoring, as ICU's CollationBuilder makes it |
| `collation/rules.mlx` | Generated (`collation-generate`): each locale's default type and each type's rules, and the Han order |
| `idna.mlx` | `uidna.h`: UTS #46, Punycode, ToASCII and ToUnicode |
| `charsets.mlx` | `ucsdet.h`: charset detection |
| `charmaps.mlx` | The single-byte charsets: names and labels, a byte's code point and back |
| `charmaps/tables.mlx` | Generated (`charset-generate`): the code points of their bytes 0x80 to 0xFF, of the Encoding Standard's indexes and Apple's tables (`std/registry/unicode/charsets`) |
| `enumeration.mlx` | `uenum.h` |
| `locale.mlx` | `uloc.h`: ICU's locale IDs, BCP 47 tags, likely subtags, the BCP 47 keys and types |
| `locale/defaults.mlx` | The default locale (from the environment), a locale's language, region and keywords, the region's week |
| `locale/data.mlx` | Generated (`cldr-generate`): CLDR's likely subtags, aliases, BCP 47 keys and types, available locales |
| `locale/resources.mlx` | `cldr.dat`, mapped at its first use, looked up by locale and key with the locale's parents |
| `locale/bundles.mlx` | `ures.h` over `cldr.dat` |
| `locale/displaynames.mlx` | `uldnames.h` and `uloc_getDisplay*`: names of languages, scripts, regions, variants, keywords |
| `calendar.mlx` | `ucal.h`: the Gregorian calendar (Julian before its change) and ICU's others, fields resolved as ICU resolves them; zone IDs, the host's and the default zone |
| `calendar/systems.mlx`, `calendar/astro.mlx` | The arithmetic of the other calendars as ICU 74 does it; the sun's longitude and the moon's age as ICU's CalendarAstronomer reckons them |
| `format.mlx` | `uformattedvalue.h`, `ufieldpositer.h`: formatted values and their fields |
| `format/number.mlx` | `unumberformatter.h`: ICU's NumberFormatter over CLDR's data; `format/number/` has its parts: `skeleton`, `quantity`, `pattern`, `builder`, `data`, `systems` (`unumsys.h`), `currency` (`ucurr.h`), `units` (measure units), `range` (`unumberrangeformatter.h`), `decimal`, `decimalformat` (`unum.h`) |
| `format/plural.mlx`, `format/plural/rules.mlx` | `upluralrules.h`; CLDR's plural rules |
| `format/date.mlx` | `udat.h`, `udatpg.h`, `udateintervalformat.h`; `format/date/` has `simple`, `generator`, `intervals`, `patterns`, `parse`, `symbols`, `zonenames`, `text` |
| `format/list.mlx`, `format/reltime.mlx` | `ulistformatter.h`, `ureldatefmt.h` |

## The data

`std/registry/unicode` holds the data and the programs that make the
generated modules of it, as `std/registry/vulkan` does for `std.vulkan`:

| Path | What |
| --- | --- |
| `ucd/` | The Unicode Character Database 16.0.0 (the files used, and the conformance tests) |
| `uca/` | The Collation Algorithm's table (DUCET) and its tests |
| `idna/` | The IDNA mapping table and its tests |
| `cldr/` | CLDR 48 (JSON): likely subtags, aliases, BCP 47 keys and types, ISO codes, available locales; `packages/`: CLDR's npm packages (core, dates, numbers, misc, locale names, units, the calendars other than the Gregorian); `collation/`: CLDR 48's collation data (XML) and the root's radical-stroke order of Han (`radicals.txt`) |
| `generate.mlx` | `unicode-generate UCD TABLES API`: `std/src/unicode/tables.mlx` and `compiler/capi/icu/api/ugenerated.mlx` (the scripts and blocks C sees) |
| `cldrgenerate.mlx` | `cldr-generate CLDR OUT`: `std/src/unicode/locale/data.mlx` |
| `collationgenerate.mlx` | `collation-generate OUT RADICALS XML...`: `std/src/unicode/collation/rules.mlx` |
| `cldrdata.mlx` | `cldr-data PACKAGES OUT`: `mlx-out/share/mlx-unicode/cldr.dat`, the formatters' data: each locale's keys and values where they differ from its parent's |

`std/registry/tz` holds the IANA database (`tzdata.zi`, `zone.tab`), its
generator (`generate.mlx`: `tz-generate TZ OUT` makes
`std/src/timezone/tzdata.mlx`, every zone's transitions worked out as zic
does through 2037, then the rules in force) and `compare.mlx`.

`./mlx build unicode` runs the generators when the data or a generator
changes; the generated modules are kept in the tree, and
`tools/check_unicode.sh` checks that they are what the generators make.

`cldr.dat` is found at run time as `$MLX_UNICODE_DATA`, else
`../share/mlx-unicode/cldr.dat` beside the directory of `libmlxunicode.so`
or of the program, else `/usr/share/mlx-unicode/cldr.dat`. Without it the
formatters and the display names give `Error.MissingData`, and the
calendars lack CLDR's calendar data (the regions' preferred calendars
and the eras it keeps there); properties,
casing, normalization, breaks, collation, IDNA and locale IDs do not need
it.
