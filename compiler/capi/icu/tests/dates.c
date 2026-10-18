/* udat, udatpg, udtitvfmt: dates formatted as JavaScriptCore's
   Intl.DateTimeFormat formats them (udatpg_getBestPatternWithOptions with
   UDATPG_MATCH_HOUR_FIELD_LENGTH, udatpg_getSkeleton, udat_open of a
   pattern and a zone, udat_formatForFields and its fields, udat_toPattern,
   udtitvfmt_formatToResult and its spans, the "practically equal" range),
   the locales' keywords (-u-hc-, -u-nu-, -u-ca-, -u-rg-), the styles and
   their relative forms, every pattern letter, the time zone names, the
   symbols, the contexts, parsing, the generator's other calls and the
   errors. Prints a line per call; dates.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/udateintervalformat.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uformattedvalue.h>
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>
#include <unicode/uloc.h>

/* The status, warnings of the resource lookups apart (ICU leaves the
   fallback warnings of its bundles in it, which callers ignore). */
static const char *statusName(UErrorCode status)
{
    if (status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING) return "U_ZERO_ERROR";
    return u_errorName(status);
}

static void utf8(const UChar *s, int32_t length, char *out, int32_t capacity)
{
    UErrorCode e = U_ZERO_ERROR;
    out[0] = 0;
    if (s) u_strToUTF8(out, capacity, NULL, s, length, &e);
}

static int32_t utf16(const char *s, UChar *out, int32_t capacity)
{
    UErrorCode e = U_ZERO_ERROR;
    int32_t length = 0;
    u_strFromUTF8(out, capacity, &length, s, -1, &e);
    return length;
}

/* 2023-11-14 16:30:05.250 UTC, a Tuesday: 08:30 in Los Angeles. */
static const UDate NOV14 = 1699979405250.0;
/* Other moments: 2024-07-04 00:05 UTC, 2001-01-01 12:00 UTC (Monday),
   1969-12-31 23:59:59 UTC, 0044-03-15 BC. */
static const UDate JUL4 = 1720051500000.0;
static const UDate JAN1 = 978350400000.0;
static const UDate EPOCH_BEFORE = -1000.0;
static const UDate ANCIENT = -63549158400000.0;

/* ------------------------------------------------------------------ */
/* udatpg */

static void best(UDateTimePatternGenerator *g, const char *locale, const char *skeleton, UDateTimePatternMatchOptions options)
{
    UChar s[64], pattern[128];
    char out[512];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udatpg_getBestPatternWithOptions(g, s, utf16(skeleton, s, 64), options, pattern, 128, &status);
    utf8(pattern, length, out, sizeof out);
    printf("best %s %s/%d: \"%s\" %s\n", locale, skeleton, (int)options, out, statusName(status));
}

static void generator(const char *locale, int names)
{
    static const char *skeletons[] = {
        "yMd", "yMMMd", "yMMMMEEEEd", "yMMMM", "MMMd", "MMMMd", "Md", "MEd", "yM", "yQQQ", "yQQQQ", "y",
        "GyMMMd", "GGGGGyMd", "jm", "jms", "jmm", "jjmm", "hm", "Hm", "Hms", "hms", "Kmm", "kmm", "ms",
        "jmz", "jmzzzz", "jmv", "jmvvvv", "jmO", "jmOOOO", "jmVV", "jmX", "yMMMdjm", "yMdjms", "EEEEjm",
        "MMMMdjmm", "yMMMEEEEdjmszzzz", "Bhm", "Bh", "bhm", "ahm", "jmmssSSS", "ESSS", "yw", "MMMMW",
        "yMMMdE", "MMMEd", "LLL", "ccc", "eeee", "QQQ", "U", "r", "jC", "C", "CCCC", "Cm", "J", "Jmm",
        "yMMMMd'x'", "yyyyMMdd", "MMMMMd", "EEEEE", "EEEEEE", "dhhmm", "HHmmss", "a", "z", "G", NULL };
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *g = udatpg_open(locale, &status);
    printf("udatpg_open %s: %s\n", locale, statusName(status));
    if (U_FAILURE(status)) return;
    /* With the hour's length kept, and without options where that
       differs. */
    for (int i = 0; skeletons[i]; i++) {
        UChar s[64], a[128], b[128];
        UErrorCode e = U_ZERO_ERROR;
        int32_t sl = utf16(skeletons[i], s, 64);
        int32_t al = udatpg_getBestPatternWithOptions(g, s, sl, UDATPG_MATCH_HOUR_FIELD_LENGTH, a, 128, &e);
        int32_t bl = udatpg_getBestPatternWithOptions(g, s, sl, UDATPG_MATCH_NO_OPTIONS, b, 128, &e);
        best(g, locale, skeletons[i], UDATPG_MATCH_HOUR_FIELD_LENGTH);
        if (al != bl || u_strncmp(a, b, al) != 0) best(g, locale, skeletons[i], UDATPG_MATCH_NO_OPTIONS);
    }
    best(g, locale, "yMMMMdjjmm", UDATPG_MATCH_ALL_FIELDS_LENGTH);
    best(g, locale, "jjmmss", UDATPG_MATCH_ALL_FIELDS_LENGTH);
    UChar result[128];
    char out[512];
    status = U_ZERO_ERROR;
    printf("hour cycle %s: %d %s\n", locale, (int)udatpg_getDefaultHourCycle(g, &status), statusName(status));
    int32_t length = 0;
    const UChar *dt = udatpg_getDateTimeFormat(g, &length);
    utf8(dt, length, out, sizeof out);
    printf("dateTimeFormat %s: \"%s\"\n", locale, out);
    for (int style = UDAT_FULL; style <= UDAT_SHORT; style++) {
        status = U_ZERO_ERROR;
        dt = udatpg_getDateTimeFormatForStyle(g, (UDateFormatStyle)style, &length, &status);
        utf8(dt, length, out, sizeof out);
        printf("dateTimeFormat %s %d: \"%s\" %s\n", locale, style, out, statusName(status));
    }
    dt = udatpg_getDecimal(g, &length);
    utf8(dt, length, out, sizeof out);
    printf("decimal %s: \"%s\"\n", locale, out);
    for (int field = 0; names && field < UDATPG_FIELD_COUNT; field++) {
        for (int width = UDATPG_WIDE; width <= UDATPG_NARROW; width++) {
            status = U_ZERO_ERROR;
            length = udatpg_getFieldDisplayName(g, (UDateTimePatternField)field, (UDateTimePGDisplayWidth)width, result, 128, &status);
            utf8(result, length, out, sizeof out);
            printf("field %s %d/%d: \"%s\" %s\n", locale, field, width, out, statusName(status));
        }
        dt = udatpg_getAppendItemFormat(g, (UDateTimePatternField)field, &length);
        utf8(dt, length, out, sizeof out);
        printf("appendItem %s %d: \"%s\"", locale, field, out);
        dt = udatpg_getAppendItemName(g, (UDateTimePatternField)field, &length);
        utf8(dt, length, out, sizeof out);
        printf(" \"%s\"\n", out);
    }
    udatpg_close(g);
}

static void skeletonOf(const char *pattern)
{
    UChar p[128], result[128];
    char skeleton[256], base[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t patternLength = utf16(pattern, p, 128);
    int32_t length = udatpg_getSkeleton(NULL, p, patternLength, result, 128, &status);
    utf8(result, length, skeleton, sizeof skeleton);
    length = udatpg_getBaseSkeleton(NULL, p, patternLength, result, 128, &status);
    utf8(result, length, base, sizeof base);
    printf("skeleton \"%s\": \"%s\" \"%s\" %s\n", pattern, skeleton, base, statusName(status));
}

static void replaceTypes(const char *locale, const char *pattern, const char *skeleton)
{
    UChar p[128], s[64], result[128];
    char out[256];
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *g = udatpg_open(locale, &status);
    int32_t length = udatpg_replaceFieldTypes(g, p, utf16(pattern, p, 128), s, utf16(skeleton, s, 64), result, 128, &status);
    utf8(result, length, out, sizeof out);
    printf("replace %s \"%s\" %s: \"%s\"", locale, pattern, skeleton, out);
    length = udatpg_replaceFieldTypesWithOptions(g, p, utf16(pattern, p, 128), s, utf16(skeleton, s, 64), UDATPG_MATCH_ALL_FIELDS_LENGTH, result, 128, &status);
    utf8(result, length, out, sizeof out);
    printf(" \"%s\" %s\n", out, statusName(status));
    udatpg_close(g);
}

static void emptyGenerator(void)
{
    static const char *patterns[] = { "d MMM y", "HH:mm", "h:mm a", "MMM d", "MMMM d, y", "y-MM-dd", "d MMM y", "EEE, d MMM", NULL };
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *g = udatpg_openEmpty(&status);
    printf("udatpg_openEmpty: %s\n", statusName(status));
    UChar p[64], conflict[64];
    char out[128];
    for (int i = 0; patterns[i]; i++) {
        for (int override = 0; override <= 1; override++) {
            status = U_ZERO_ERROR;
            int32_t length = 0;
            UDateTimePatternConflict c = udatpg_addPattern(g, p, utf16(patterns[i], p, 64), override, conflict, 64, &length, &status);
            utf8(conflict, length, out, sizeof out);
            printf("addPattern \"%s\" %d: %d \"%s\" %s\n", patterns[i], override, (int)c, out, statusName(status));
        }
    }
    UChar decimal[] = { ',', 0 };
    udatpg_setDecimal(g, decimal, 1);
    UChar dtf[] = { '{', '1', '}', ' ', '@', ' ', '{', '0', '}', 0 };
    udatpg_setDateTimeFormat(g, dtf, -1);
    UChar item[] = { '{', '0', '}', ' ', '(', '{', '2', '}', ':', ' ', '{', '1', '}', ')', 0 };
    udatpg_setAppendItemFormat(g, UDATPG_ERA_FIELD, item, -1);
    UChar name[] = { 'E', 'r', 'a', 0 };
    udatpg_setAppendItemName(g, UDATPG_ERA_FIELD, name, -1);
    UDateTimePatternGenerator *copy = udatpg_clone(g, &status);
    best(copy, "empty", "yMMMd", UDATPG_MATCH_NO_OPTIONS);
    best(copy, "empty", "yMMMdHHmm", UDATPG_MATCH_NO_OPTIONS);
    best(copy, "empty", "GyMMMd", UDATPG_MATCH_NO_OPTIONS);
    best(copy, "empty", "jmmssSS", UDATPG_MATCH_NO_OPTIONS);
    best(copy, "empty", "MMMMd", UDATPG_MATCH_ALL_FIELDS_LENGTH);
    best(copy, "empty", "EEEEMMMd", UDATPG_MATCH_NO_OPTIONS);
    UEnumeration *e = udatpg_openSkeletons(copy, &status);
    printf("skeletons:");
    const UChar *s;
    int32_t length;
    while ((s = uenum_unext(e, &length, &status))) {
        utf8(s, length, out, sizeof out);
        const UChar *pattern = udatpg_getPatternForSkeleton(copy, s, length, &length);
        char pout[128];
        utf8(pattern, length, pout, sizeof pout);
        printf(" %s=\"%s\"", out, pout);
    }
    printf(" %s\n", statusName(status));
    uenum_close(e);
    e = udatpg_openBaseSkeletons(copy, &status);
    printf("base skeletons:");
    while ((s = uenum_unext(e, &length, &status))) {
        utf8(s, length, out, sizeof out);
        printf(" %s", out);
    }
    printf(" %s\n", statusName(status));
    uenum_close(e);
    udatpg_close(copy);
    udatpg_close(g);
}

static void generatorErrors(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *g = udatpg_open("en", &status);
    UChar s[32], result[8];
    status = U_ZERO_ERROR;
    int32_t length = udatpg_getBestPattern(g, s, utf16("yMMMMEEEEd", s, 32), result, 8, &status);
    printf("small buffer: %d %s\n", length, statusName(status));
    status = U_ZERO_ERROR;
    length = udatpg_getBestPattern(g, s, utf16("yMMMMEEEEd", s, 32), NULL, 0, &status);
    printf("preflight: %d %s\n", length, statusName(status));
    status = U_ZERO_ERROR;
    length = udatpg_getBestPattern(g, NULL, 2, result, 8, &status);
    printf("null skeleton: %d %s\n", length, statusName(status));
    status = U_ZERO_ERROR;
    UChar p[] = { 'y', 'y', '\'', 'x', 0 };
    length = udatpg_getSkeleton(NULL, p, -1, result, 8, &status);
    printf("unterminated quote: %d %s\n", length, statusName(status));
    udatpg_close(g);
    status = U_ZERO_ERROR;
    g = udatpg_open("xx-YY", &status);
    best(g, "xx-YY", "yMMMdjm", UDATPG_MATCH_NO_OPTIONS);
    udatpg_close(g);
}

/* ------------------------------------------------------------------ */
/* udat */

static const char *fieldName(int field)
{
    static const char *names[] = { "era", "year", "month", "date", "hour1", "hour0", "minute", "second", "fraction",
        "weekday", "dayOfYear", "dayOfWeekInMonth", "weekOfYear", "weekOfMonth", "ampm", "hour1", "hour0", "zone",
        "yearWOY", "dowLocal", "extendedYear", "julianDay", "millisInDay", "zoneRFC", "zoneGeneric", "standaloneDay",
        "standaloneMonth", "quarter", "standaloneQuarter", "zoneSpecial", "yearName", "zoneGMT", "zoneISO",
        "zoneISOLocal", "relatedYear", "ampmMidnightNoon", "dayPeriod", "timeSeparator" };
    if (field >= 0 && field < (int)(sizeof names / sizeof names[0])) return names[field];
    return "?";
}

/* Formats DATE with F, the fields with their spans. */
static void formatFields(UDateFormat *f, const char *label, UDate date)
{
    UChar result[256];
    char out[512];
    UErrorCode status = U_ZERO_ERROR;
    UFieldPositionIterator *positions = ufieldpositer_open(&status);
    int32_t length = udat_formatForFields(f, date, result, 256, positions, &status);
    utf8(result, length, out, sizeof out);
    printf("%s: \"%s\" %s [", label, out, statusName(status));
    int32_t start, limit, field;
    while ((field = ufieldpositer_next(positions, &start, &limit)) >= 0) printf(" %s %d-%d", fieldName(field), start, limit);
    printf(" ]\n");
    ufieldpositer_close(positions);
}

static void formatDate(UDateFormat *f, const char *label, UDate date)
{
    UChar result[256];
    char out[512];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udat_format(f, date, result, 256, NULL, &status);
    utf8(result, length, out, sizeof out);
    printf("%s: \"%s\" %s\n", label, out, statusName(status));
}

static void printPattern(UDateFormat *f, const char *label)
{
    UChar result[256];
    char out[512], localized[512];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = udat_toPattern(f, false, result, 256, &status);
    utf8(result, length, out, sizeof out);
    length = udat_toPattern(f, true, result, 256, &status);
    utf8(result, length, localized, sizeof localized);
    printf("%s pattern: \"%s\" \"%s\" %s\n", label, out, localized, statusName(status));
}

/* The way JavaScriptCore formats: the best pattern of the skeleton, the
   format of the pattern in the zone, its fields. */
static void intlFormat(const char *locale, const char *skeleton, const char *zone)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *g = udatpg_open(locale, &status);
    UChar s[64], pattern[128], z[64];
    int32_t patternLength = udatpg_getBestPatternWithOptions(g, s, utf16(skeleton, s, 64), UDATPG_MATCH_HOUR_FIELD_LENGTH, pattern, 128, &status);
    udatpg_close(g);
    UDateFormat *f = udat_open(UDAT_PATTERN, UDAT_PATTERN, locale, z, utf16(zone, z, 64), pattern, patternLength, &status);
    if (U_FAILURE(status)) {
        printf("intl %s %s %s: %s\n", locale, skeleton, zone, statusName(status));
        return;
    }
    UCalendar *cal = (UCalendar *)udat_getCalendar(f);
    UCalendar *copy = ucal_clone(cal, &status);
    ucal_setGregorianChange(copy, -8.64e15, &status);
    udat_setCalendar(f, copy);
    ucal_close(copy);
    char label[256];
    snprintf(label, sizeof label, "intl %s %s %s", locale, skeleton, zone);
    printPattern(f, label);
    formatFields(f, label, NOV14);
    formatFields(f, label, JUL4);
    formatFields(f, label, ANCIENT);
    udat_close(f);
}

static void styles(const char *locale, const char *zone)
{
    UChar z[64];
    int32_t zoneLength = utf16(zone, z, 64);
    static const int list[] = { UDAT_FULL, UDAT_LONG, UDAT_MEDIUM, UDAT_SHORT, UDAT_NONE };
    for (int t = 0; t < 5; t++) {
        for (int d = 0; d < 5; d++) {
            int time = list[t], date = list[d];
            if (time == UDAT_NONE && date == UDAT_NONE) continue;
            if (time != date && time != UDAT_NONE && date != UDAT_NONE && time != UDAT_SHORT) continue;
            UErrorCode status = U_ZERO_ERROR;
            UDateFormat *f = udat_open((UDateFormatStyle)time, (UDateFormatStyle)date, locale, z, zoneLength, NULL, 0, &status);
            char label[128];
            snprintf(label, sizeof label, "style %s %s %d/%d", locale, zone, time, date);
            if (U_FAILURE(status)) {
                printf("%s: %s\n", label, statusName(status));
                continue;
            }
            printPattern(f, label);
            formatDate(f, label, NOV14);
            if (time == UDAT_FULL || date == UDAT_FULL) formatDate(f, label, JUL4);
            udat_close(f);
        }
    }
}

static void pattern(const char *locale, const char *zone, const char *p, UDate date)
{
    UChar z[64], pat[256];
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *f = udat_open(UDAT_PATTERN, UDAT_PATTERN, locale, z, utf16(zone, z, 64), pat, utf16(p, pat, 256), &status);
    char label[512];
    snprintf(label, sizeof label, "pattern %s %s \"%s\"", locale, zone, p);
    if (U_FAILURE(status)) {
        printf("%s: %s\n", label, statusName(status));
        return;
    }
    formatFields(f, label, date);
    udat_close(f);
}

static void letters(void)
{
    static const char *patterns[] = {
        "G GGGG GGGGG", "y yy yyy yyyy yyyyy", "Y YY YYYY", "u uu uuuu", "U", "r rrrr",
        "Q QQ QQQ QQQQ QQQQQ", "q qq qqq qqqq qqqqq", "M MM MMM MMMM MMMMM", "L LL LLL LLLL LLLLL",
        "w ww W", "d dd D DDD F g", "E EE EEE EEEE EEEEE EEEEEE", "e ee eee eeee eeeee eeeeee",
        "c cc ccc cccc ccccc cccccc", "a aa aaa aaaa aaaaa", "b bbbb bbbbb", "B BBBB BBBBB",
        "h hh H HH K KK k kk", "m mm s ss", "S SS SSS SSSS SSSSSS", "A AAAAAAAA",
        "z zz zzz zzzz", "Z ZZ ZZZ ZZZZ ZZZZZ", "O OOOO", "v vvvv", "V VV VVV VVVV",
        "X XX XXX XXXX XXXXX", "x xx xxx xxxx xxxxx", "'quoted ''text''' '' y", "yyyy.MM.dd G 'at' HH:mm:ss zzz",
        "EEE, MMM d, ''yy", "h:mm a", "hh 'o''clock' a, zzzz", "K:mm a, z", "yyyyy.MMMMM.dd GGG hh:mm aaa",
        "EEE, d MMM yyyy HH:mm:ss Z", "yyMMddHHmmssZ", "yyyy-MM-dd'T'HH:mm:ss.SSSZ", "YYYY-'W'ww-u", NULL };
    for (int i = 0; patterns[i]; i++) {
        pattern("en", "America/Los_Angeles", patterns[i], NOV14);
        pattern("en", "Europe/Paris", patterns[i], JUL4);
    }
    static const UDate noonish[] = { 0.0, 21600000.0, 43200000.0, 46800000.0, 64800000.0, 75600000.0, 82800000.0 };
    for (int i = 0; i < 7; i++) {
        pattern("en", "UTC", "h:mm b | h:mm B | h:mm BBBB | a", noonish[i]);
        pattern("fr", "UTC", "h:mm B | BBBB", noonish[i]);
        pattern("zh", "UTC", "Bh:mm | b", noonish[i]);
    }
    pattern("en", "UTC", "y G", ANCIENT);
    pattern("en", "UTC", "u r Y", ANCIENT);
    pattern("en", "UTC", "yyyy-MM-dd HH:mm:ss.SSS", EPOCH_BEFORE);
    pattern("en", "UTC", "y-MM-dd j", NOV14);
    pattern("en", "UTC", "y-MM-dd l", NOV14);
}

static void zones(void)
{
    static const char *zoneList[] = { "America/Los_Angeles", "America/New_York", "Europe/London", "Europe/Berlin",
        "Asia/Tokyo", "Asia/Kolkata", "Australia/Sydney", "America/Sao_Paulo", "Pacific/Chatham", "Africa/Cairo",
        "America/Indiana/Knox", "Asia/Kathmandu", "UTC", "Etc/GMT+5", "GMT", "America/Argentina/Buenos_Aires",
        "US/Pacific", "Asia/Calcutta", "Europe/Kiev", "Antarctica/Troll", "America/Phoenix", NULL };
    static const char *locales[] = { "en", "de", "ja", "es-MX", "zh-Hant", NULL };
    for (int l = 0; locales[l]; l++) {
        for (int i = 0; zoneList[i]; i++) {
            pattern(locales[l], zoneList[i], "z | zzzz | v | vvvv | VVV | VVVV | O | OOOO", NOV14);
            pattern(locales[l], zoneList[i], "z | zzzz | v | vvvv", JUL4);
        }
    }
    pattern("en", "Invalid/Zone", "zzzz | VV | VVV", NOV14);
    pattern("en", "GMT+05:30", "zzzz | z | VV", NOV14);
}

static void keywords(void)
{
    static const char *locales[] = { "en-u-hc-h23", "en-US-u-hc-h24", "ja-JP-u-hc-h11", "de-u-hc-h12", "en-u-nu-arab",
        "ar", "ar-EG", "ar-u-nu-latn", "fa", "hi-IN-u-nu-deva", "th-u-nu-thai", "zh-u-nu-hanidec", "en-u-ca-japanese",
        "th", "en-u-ca-buddhist", "en-u-rg-gbzzzz", "en-GB-u-hc-h12", "es-419", "en-001", "zh-Hant-HK", "sr-Latn",
        "und", "root", "en_US_POSIX", "de@calendar=gregorian;hours=h12", NULL };
    for (int i = 0; locales[i]; i++) {
        intlFormat(locales[i], "yMMMdjmm", "Asia/Tokyo");
        styles(locales[i], "Asia/Tokyo");
        UErrorCode status = U_ZERO_ERROR;
        UDateFormat *f = udat_open(UDAT_SHORT, UDAT_SHORT, locales[i], NULL, 0, NULL, 0, &status);
        printf("locale %s: valid %s actual %s %s\n", locales[i], udat_getLocaleByType(f, ULOC_VALID_LOCALE, &status),
               udat_getLocaleByType(f, ULOC_ACTUAL_LOCALE, &status), statusName(status));
        const UCalendar *cal = udat_getCalendar(f);
        printf("calendar %s: %s\n", locales[i], ucal_getType(cal, &status));
        udat_close(f);
    }
}

static void relative(void)
{
    /* Days around now: the relative names; noon of each, in UTC. */
    UErrorCode status = U_ZERO_ERROR;
    UDate now = ucal_getNow();
    UDate noon = (double)(long long)(now / 86400000.0) * 86400000.0 + 43200000.0;
    static const char *locales[] = { "en", "de", "fr", "es", "ja", "cs", "ar", NULL };
    UChar z[] = { 'U', 'T', 'C', 0 };
    for (int l = 0; locales[l]; l++) {
        for (int style = UDAT_FULL_RELATIVE; style <= UDAT_SHORT_RELATIVE; style++) {
            UDateFormat *f = udat_open(UDAT_NONE, (UDateFormatStyle)style, locales[l], z, -1, NULL, 0, &status);
            UDateFormat *g = udat_open(UDAT_NONE, (UDateFormatStyle)(style & ~UDAT_RELATIVE), locales[l], z, -1, NULL, 0, &status);
            for (int day = -3; day <= 3; day++) {
                UChar a[128], b[128];
                char out[256];
                int32_t length = udat_format(f, noon + day * 86400000.0, a, 128, NULL, &status);
                int32_t plain = udat_format(g, noon + day * 86400000.0, b, 128, NULL, &status);
                if (length == plain && u_strncmp(a, b, length) == 0) {
                    printf("relative %s %d %d: (absolute) %s\n", locales[l], style, day, statusName(status));
                } else {
                    utf8(a, length, out, sizeof out);
                    printf("relative %s %d %d: \"%s\" %s\n", locales[l], style, day, out, statusName(status));
                }
            }
            udat_setContext(f, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, &status);
            UChar a[128];
            char out[256];
            int32_t length = udat_format(f, noon, a, 128, NULL, &status);
            utf8(a, length, out, sizeof out);
            printf("relative %s %d sentence: \"%s\" %s\n", locales[l], style, out, statusName(status));
            udat_close(f);
            udat_close(g);
        }
        UDateFormat *f = udat_open(UDAT_SHORT, UDAT_MEDIUM_RELATIVE, locales[l], z, -1, NULL, 0, &status);
        UChar a[128];
        char out[256];
        int32_t length = udat_format(f, noon - 86400000.0, a, 128, NULL, &status);
        utf8(a, length, out, sizeof out);
        printf("relative %s with time: \"%s\" %s\n", locales[l], out, statusName(status));
        length = udat_toPattern(f, false, a, 128, &status);
        utf8(a, length, out, sizeof out);
        printf("relative %s pattern: \"%s\" %s\n", locales[l], out, statusName(status));
        udat_close(f);
    }
}

static void symbols(const char *locale)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *f = udat_open(UDAT_MEDIUM, UDAT_MEDIUM, locale, NULL, 0, NULL, 0, &status);
    for (int type = UDAT_ERAS; type <= UDAT_STANDALONE_NARROW_QUARTERS; type++) {
        int32_t count = udat_countSymbols(f, (UDateFormatSymbolType)type);
        printf("symbols %s %d [%d]:", locale, type, count);
        for (int i = 0; i < count; i++) {
            UChar result[128];
            char out[256];
            status = U_ZERO_ERROR;
            int32_t length = udat_getSymbols(f, (UDateFormatSymbolType)type, i, result, 128, &status);
            utf8(result, length, out, sizeof out);
            printf(" \"%s\"", out);
        }
        printf(" %s\n", statusName(status));
    }
    udat_close(f);
}

static void setSymbols(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UChar z[] = { 'U', 'T', 'C', 0 }, p[64];
    UDateFormat *f = udat_open(UDAT_PATTERN, UDAT_PATTERN, "en", z, -1, p, utf16("EEEE d MMMM y G, h:mm a", p, 64), &status);
    UChar month[64], day[64], am[64], era[64];
    udat_setSymbols(f, UDAT_MONTHS, 10, month, utf16("Brumaire", month, 64), &status);
    udat_setSymbols(f, UDAT_WEEKDAYS, 3, day, utf16("Mardi", day, 64), &status);
    udat_setSymbols(f, UDAT_AM_PMS, 0, am, utf16("matin", am, 64), &status);
    udat_setSymbols(f, UDAT_ERAS, 1, era, utf16("CE", era, 64), &status);
    udat_setSymbols(f, UDAT_ERA_NAMES, 1, era, utf16("Common Era", era, 64), &status);
    printf("setSymbols: %s\n", statusName(status));
    formatFields(f, "setSymbols", NOV14 - 16 * 3600000.0);
    status = U_ZERO_ERROR;
    udat_setSymbols(f, UDAT_MONTHS, 12, month, 3, &status);
    printf("setSymbols out of range: %s\n", statusName(status));
    status = U_ZERO_ERROR;
    UDateFormat *copy = udat_clone(f, &status);
    udat_close(f);
    formatFields(copy, "clone", NOV14);
    UChar result[64];
    char out[128];
    int32_t length = udat_getSymbols(copy, UDAT_MONTHS, 10, result, 64, &status);
    utf8(result, length, out, sizeof out);
    printf("getSymbols after set: \"%s\" %s\n", out, statusName(status));
    udat_close(copy);
}

static void contexts(void)
{
    static const char *locales[] = { "cs", "es", "fr", "ru", "en", "nb", "uk", NULL };
    static const UDisplayContext caps[] = { UDISPCTX_CAPITALIZATION_NONE, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
        UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,
        UDISPCTX_CAPITALIZATION_FOR_STANDALONE };
    static const char *patterns[] = { "LLLL y", "MMMM y", "EEEE d. MMMM", "cccc", "d MMMM", NULL };
    UChar z[] = { 'U', 'T', 'C', 0 };
    for (int l = 0; locales[l]; l++) {
        for (int p = 0; patterns[p]; p++) {
            UChar pat[64];
            UErrorCode status = U_ZERO_ERROR;
            UDateFormat *f = udat_open(UDAT_PATTERN, UDAT_PATTERN, locales[l], z, -1, pat, utf16(patterns[p], pat, 64), &status);
            printf("context %s \"%s\":", locales[l], patterns[p]);
            for (int c = 0; c < 5; c++) {
                udat_setContext(f, caps[c], &status);
                UChar result[128];
                char out[256];
                int32_t length = udat_format(f, NOV14, result, 128, NULL, &status);
                utf8(result, length, out, sizeof out);
                printf(" \"%s\"", out);
            }
            printf(" %d %s\n", (int)udat_getContext(f, UDISPCTX_TYPE_CAPITALIZATION, &status), statusName(status));
            udat_close(f);
        }
    }
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *f = udat_open(UDAT_SHORT, UDAT_SHORT, "en", z, -1, NULL, 0, &status);
    udat_setContext(f, UDISPCTX_LENGTH_SHORT, &status);
    printf("context length: %s\n", statusName(status));
    status = U_ZERO_ERROR;
    udat_setContext(f, UDISPCTX_DIALECT_NAMES, &status);
    printf("context dialect: %s\n", statusName(status));
    udat_close(f);
}

static void parse(const char *locale, const char *p, const char *text)
{
    UChar z[] = { 'A', 'm', 'e', 'r', 'i', 'c', 'a', '/', 'L', 'o', 's', '_', 'A', 'n', 'g', 'e', 'l', 'e', 's', 0 };
    UChar pat[128], t[128];
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *f;
    if (p[0] == '@') f = udat_open((UDateFormatStyle)(p[2] - '0'), (UDateFormatStyle)(p[1] - '0'), locale, z, -1, NULL, 0, &status);
    else f = udat_open(UDAT_PATTERN, UDAT_PATTERN, locale, z, -1, pat, utf16(p, pat, 128), &status);
    udat_set2DigitYearStart(f, -1262304000000.0, &status);
    int32_t length = utf16(text, t, 128);
    for (int lenient = 1; lenient >= 0; lenient--) {
        udat_setLenient(f, lenient);
        status = U_ZERO_ERROR;
        int32_t position = 0;
        UDate d = udat_parse(f, t, length, &position, &status);
        printf("parse %s \"%s\" %s \"%s\": %.0f %d %s\n", locale, p, lenient ? "lenient" : "strict", text, d, position, statusName(status));
    }
    status = U_ZERO_ERROR;
    UCalendar *cal = ucal_open(NULL, 0, "en", UCAL_GREGORIAN, &status);
    ucal_setMillis(cal, 0.0, &status);
    int32_t position = 0;
    status = U_ZERO_ERROR;
    udat_setLenient(f, 1);
    udat_parseCalendar(f, cal, t, length, &position, &status);
    printf("parseCalendar %s \"%s\": %.0f %d %s\n", locale, text, ucal_getMillis(cal, &status), position, statusName(status));
    ucal_close(cal);
    udat_close(f);
}

static void parsing(void)
{
    parse("en", "yyyy-MM-dd HH:mm:ss", "2023-11-14 08:30:05");
    parse("en", "yyyy-MM-dd HH:mm:ss", "2023/11/14 08:30:05");
    parse("en", "yyyy-MM-dd HH:mm:ss", "x2023-11-14 08:30:05");
    parse("en", "yyyy-MM-dd", "2023-13-45");
    parse("en", "yyyy-MM-dd", "2023-02-30");
    parse("en", "yyyy-MM-dd", "2023 - 11 - 14");
    parse("en", "MM/dd/yy", "11/14/23");
    parse("en", "MM/dd/yy", "11/14/29");
    parse("en", "MM/dd/yy", "01/01/30");
    parse("en", "MM/dd/yy", "1/1/1930");
    parse("en", "M/d/yy", "1/2/3");
    parse("en", "MMM d, y h:mm a", "nov 14, 2023 8:30 pm");
    parse("en", "MMM d, y h:mm a", "November 14, 2023 8:30 AM");
    parse("en", "MMM d", "Sept 14");
    parse("en", "EEEE, MMMM d, y", "Tue, Nov 14, 2023");
    parse("en", "yyyyMMddHHmmss", "20231114083005");
    parse("en", "HH:mm:ss.SSS", "08:30:05.123456");
    parse("en", "yyyy-MM-dd'T'HH:mm:ssXXX", "2023-11-14T08:30:05+05:30");
    parse("en", "yyyy-MM-dd'T'HH:mm:ssXXX", "2023-11-14T08:30:05Z");
    parse("en", "yyyy-MM-dd HH:mm z", "2023-11-14 08:30 PDT");
    parse("en", "yyyy-MM-dd HH:mm z", "2023-11-14 08:30 GMT+2");
    parse("en", "yyyy-MM-dd HH:mm zzzz", "2023-11-14 08:30 Pacific Standard Time");
    parse("en", "yyyy-MM-dd HH:mm Z", "2023-11-14 08:30 +0130");
    parse("en", "yyyy-MM-dd HH:mm VV", "2023-11-14 08:30 America/New_York");
    parse("en", "@33", "11/14/23, 8:30\xe2\x80\xafPM");
    parse("en", "@22", "Nov 14, 2023, 8:30:05\xe2\x80\xafPM");
    parse("en", "@00", "Tuesday, November 14, 2023 at 8:30:05\xe2\x80\xafPM Pacific Standard Time");
    parse("de", "EEEE, d. MMMM y 'um' HH:mm", "Dienstag, 14. November 2023 um 08:30");
    parse("ru", "d MMMM y", "14 ноября 2023");
    parse("ru", "d MMMM y", "14 ноябрь 2023");
    parse("ar", "d MMMM y", "١٤ نوفمبر ٢٠٢٣");
    parse("ja", "y年M月d日 H時m分", "2023年11月14日 8時30分");
    parse("en", "G y", "BC 44");
    parse("en", "h:mm B", "8:30 in the evening");
    parse("en", "h b", "12 noon");
    parse("en", "K:mm a", "0:30 PM");
    parse("en", "k:mm", "24:30");
    parse("en", "u-MM-dd", "-0043-03-15");
}

static void attributes(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *f = udat_open(UDAT_SHORT, UDAT_SHORT, "en", NULL, 0, NULL, 0, &status);
    printf("lenient: %d\n", (int)udat_isLenient(f));
    for (int a = 0; a < UDAT_BOOLEAN_ATTRIBUTE_COUNT; a++) printf("attribute %d: %d\n", a, (int)udat_getBooleanAttribute(f, (UDateFormatBooleanAttribute)a, &status));
    udat_setLenient(f, false);
    printf("lenient after set: %d", (int)udat_isLenient(f));
    for (int a = 0; a < UDAT_BOOLEAN_ATTRIBUTE_COUNT; a++) printf(" %d", (int)udat_getBooleanAttribute(f, (UDateFormatBooleanAttribute)a, &status));
    printf(" %s\n", statusName(status));
    udat_setBooleanAttribute(f, UDAT_PARSE_ALLOW_WHITESPACE, true, &status);
    printf("whitespace: %d %d\n", (int)udat_getBooleanAttribute(f, UDAT_PARSE_ALLOW_WHITESPACE, &status), (int)udat_isLenient(f));
    printf("2-digit year start: %.0f %s\n", udat_get2DigitYearStart(f, &status) > 0 ? 1.0 : 0.0, statusName(status));
    udat_set2DigitYearStart(f, 0.0, &status);
    printf("2-digit year start set: %.0f %s\n", udat_get2DigitYearStart(f, &status), statusName(status));
    UChar result[128];
    char out[256];
    UChar p[64];
    udat_applyPattern(f, false, p, utf16("dd.MM.yyyy HH:mm", p, 64));
    printPattern(f, "applyPattern");
    udat_applyPattern(f, true, p, utf16("dd.MM.yyyy", p, 64));
    printPattern(f, "applyPattern localized");
    int32_t length = udat_format(f, NOV14, result, 128, NULL, &status);
    utf8(result, length, out, sizeof out);
    printf("applied: \"%s\"\n", out);
    UFieldPosition position = { UDAT_MONTH_FIELD, 0, 0 };
    length = udat_format(f, NOV14, result, 128, &position, &status);
    printf("field position month: %d-%d %s\n", position.beginIndex, position.endIndex, statusName(status));
    status = U_ZERO_ERROR;
    length = udat_format(f, NOV14, result, 4, NULL, &status);
    printf("small buffer: %d %s\n", length, statusName(status));
    status = U_ZERO_ERROR;
    length = udat_format(f, NOV14, NULL, 0, NULL, &status);
    printf("preflight: %d %s\n", length, statusName(status));
    status = U_ZERO_ERROR;
    UCalendar *cal = ucal_open(NULL, 0, "en", UCAL_GREGORIAN, &status);
    ucal_setDateTime(cal, 2020, UCAL_FEBRUARY, 29, 13, 45, 0, &status);
    status = U_ZERO_ERROR;
    length = udat_formatCalendar(f, cal, result, 128, NULL, &status);
    utf8(result, length, out, sizeof out);
    printf("formatCalendar: \"%s\" %s\n", out, statusName(status));
    UFieldPositionIterator *positions = ufieldpositer_open(&status);
    length = udat_formatCalendarForFields(f, cal, result, 128, positions, &status);
    utf8(result, length, out, sizeof out);
    printf("formatCalendarForFields: \"%s\" %s [", out, statusName(status));
    int32_t start, limit, field;
    while ((field = ufieldpositer_next(positions, &start, &limit)) >= 0) printf(" %s %d-%d", fieldName(field), start, limit);
    printf(" ]\n");
    ufieldpositer_close(positions);
    ucal_close(cal);
    for (int field = 0; field <= UDAT_TIME_SEPARATOR_FIELD; field++) printf("%d ", (int)udat_toCalendarDateField((UDateFormatField)field));
    printf("\n");
    printf("number format: %s\n", udat_getNumberFormat(f) ? "some" : "none");
    udat_close(f);
    printf("available: %s %s\n", udat_countAvailable() > 100 ? "many" : "few", udat_getAvailable(0) ? "first" : "none");
    status = U_ZERO_ERROR;
    f = udat_open(UDAT_PATTERN, UDAT_PATTERN, "en", NULL, 0, p, utf16("yyyy-MM-dd jj", p, 64), &status);
    printf("pattern with j: %s\n", statusName(status));
    if (U_SUCCESS(status)) {
        length = udat_format(f, NOV14, result, 128, NULL, &status);
        printf("format with j: %s\n", statusName(status));
        udat_close(f);
    }
    status = U_ZERO_ERROR;
    f = udat_open(UDAT_PATTERN, UDAT_SHORT, "en", NULL, 0, NULL, 0, &status);
    printf("pattern style without pattern: %s\n", statusName(status));
    status = U_ZERO_ERROR;
    f = udat_open((UDateFormatStyle)7, UDAT_SHORT, "en", NULL, 0, NULL, 0, &status);
    printf("style 7: %s\n", statusName(status));
    if (f) udat_close(f);
}

/* ------------------------------------------------------------------ */
/* udtitvfmt */

static const char *categoryName(int32_t category)
{
    if (category == UFIELD_CATEGORY_DATE) return "date";
    if (category == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) return "span";
    return "?";
}

static void interval(const char *locale, const char *skeleton, const char *zone, UDate from, UDate to, int withPosition)
{
    UChar s[64], z[64];
    UErrorCode status = U_ZERO_ERROR;
    UDateIntervalFormat *f = udtitvfmt_open(locale, s, utf16(skeleton, s, 64), z, utf16(zone, z, 64), &status);
    if (U_FAILURE(status)) {
        printf("interval %s %s: %s\n", locale, skeleton, statusName(status));
        return;
    }
    UFormattedDateInterval *result = udtitvfmt_openResult(&status);
    udtitvfmt_formatToResult(f, from, to, result, &status);
    const UFormattedValue *value = udtitvfmt_resultAsValue(result, &status);
    int32_t length = 0;
    const UChar *string = ufmtval_getString(value, &length, &status);
    char out[512];
    utf8(string, length, out, sizeof out);
    printf("interval %s %s %s: \"%s\" %s [", locale, skeleton, zone, out, statusName(status));
    UConstrainedFieldPosition *position = ucfpos_open(&status);
    int spans = 0;
    while (ufmtval_nextPosition(value, position, &status)) {
        int32_t start, limit;
        ucfpos_getIndexes(position, &start, &limit, &status);
        int32_t category = ucfpos_getCategory(position, &status);
        int32_t field = ucfpos_getField(position, &status);
        if (category == UFIELD_CATEGORY_DATE_INTERVAL_SPAN) spans++;
        printf(" %s/%s %d-%d", categoryName(category), category == UFIELD_CATEGORY_DATE ? fieldName(field) : (field ? "1" : "0"), start, limit);
    }
    printf(" ]%s %s\n", spans ? "" : " equal", statusName(status));
    ucfpos_close(position);
    udtitvfmt_closeResult(result);
    if (withPosition) {
        UChar buffer[256];
        UFieldPosition fp = { UDAT_DATE_FIELD, 0, 0 };
        length = udtitvfmt_format(f, from, to, buffer, 256, &fp, &status);
        utf8(buffer, length, out, sizeof out);
        printf("interval format %s %s: \"%s\" %d-%d %s\n", locale, skeleton, out, fp.beginIndex, fp.endIndex, statusName(status));
    }
    udtitvfmt_close(f);
}

static void intervals(void)
{
    static const char *skeletons[] = { "yMMMd", "yMd", "yMMMMEEEEd", "MMMd", "yMMM", "y", "jm", "hm", "Hm", "jmz",
        "yMMMdjm", "yMdjms", "MMMEdjm", "jmv", "h", "H", "d", "MMMMd", "yMMMMd", "Bhm", "GyMMMd", NULL };
    static const char *locales[] = { "en", "de", "en-GB", "fr", "ja", "zh", "ar", "fi", "ja-u-hc-h11", NULL };
    /* The same moment, minutes apart, hours apart (across noon), a day,
       a month, a year, an era. */
    static const UDate tos[] = { 0.0, 600000.0, 21600000.0, 86400000.0, 2678400000.0, 31536000000.0 };
    for (int l = 0; locales[l]; l++) {
        for (int s = 0; skeletons[s]; s++) {
            for (int t = 0; t < 6; t++) interval(locales[l], skeletons[s], "America/Los_Angeles", NOV14, NOV14 + tos[t], l < 2 && (t == 0 || t == 3));
        }
        interval(locales[l], "yMMMd", "UTC", ANCIENT, NOV14, 1);
    }
    interval("en", "yMMMd", "UTC", NOV14 + 86400000.0, NOV14, 1);
    interval("en", "yMMMdjmmss", "UTC", NOV14, NOV14 + 999.0, 1);
    interval("en", "jmmssSSS", "UTC", NOV14, NOV14 + 999.0, 1);
    interval("en", "yMMMd", "Invalid/Zone", NOV14, JUL4, 1);
    interval("en", "", "UTC", NOV14, JUL4, 1);
    interval("zh-Hant", "yMMMMEEEEd", "Asia/Taipei", NOV14, JUL4, 1);

    UChar s[] = { 'y', 'M', 'M', 'M', 'd', 0 }, z[] = { 'U', 'T', 'C', 0 };
    UErrorCode status = U_ZERO_ERROR;
    UDateIntervalFormat *f = udtitvfmt_open("cs", s, -1, z, -1, &status);
    printf("interval context: %d %s\n", (int)udtitvfmt_getContext(f, UDISPCTX_TYPE_CAPITALIZATION, &status), statusName(status));
    UChar p[] = { 'L', 'L', 'L', 'L', 0 };
    udtitvfmt_close(f);
    f = udtitvfmt_open("cs", p, -1, z, -1, &status);
    for (int c = 0; c < 3; c++) {
        static const UDisplayContext caps[] = { UDISPCTX_CAPITALIZATION_NONE, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
            UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU };
        status = U_ZERO_ERROR;
        udtitvfmt_setContext(f, caps[c], &status);
        UChar buffer[128];
        char out[256];
        int32_t length = udtitvfmt_format(f, NOV14, JUL4, buffer, 128, NULL, &status);
        utf8(buffer, length, out, sizeof out);
        printf("interval cs %d: \"%s\" %d %s\n", c, out, (int)udtitvfmt_getContext(f, UDISPCTX_TYPE_CAPITALIZATION, &status), statusName(status));
    }
    udtitvfmt_close(f);

    /* Calendars of their own zones and gregorian changes. */
    f = udtitvfmt_open("en", s, -1, z, -1, &status);
    UCalendar *from = ucal_open(z, -1, "en", UCAL_GREGORIAN, &status);
    UCalendar *to = ucal_open(z, -1, "en", UCAL_GREGORIAN, &status);
    ucal_setDateTime(from, 1582, UCAL_OCTOBER, 4, 0, 0, 0, &status);
    ucal_setMillis(to, ucal_getMillis(from, &status) + 86400000.0, &status);
    UFormattedDateInterval *result = udtitvfmt_openResult(&status);
    udtitvfmt_formatCalendarToResult(f, from, to, result, &status);
    int32_t length = 0;
    const UChar *string = ufmtval_getString(udtitvfmt_resultAsValue(result, &status), &length, &status);
    char out[256];
    utf8(string, length, out, sizeof out);
    printf("interval calendars: \"%s\" %s\n", out, statusName(status));
    ucal_setGregorianChange(from, -8.64e15, &status);
    ucal_setGregorianChange(to, -8.64e15, &status);
    udtitvfmt_formatCalendarToResult(f, from, to, result, &status);
    string = ufmtval_getString(udtitvfmt_resultAsValue(result, &status), &length, &status);
    utf8(string, length, out, sizeof out);
    printf("interval proleptic calendars: \"%s\" %s\n", out, statusName(status));
    udtitvfmt_closeResult(result);
    ucal_close(from);
    ucal_close(to);
    status = U_ZERO_ERROR;
    UChar buffer[8];
    length = udtitvfmt_format(f, NOV14, JUL4, buffer, 8, NULL, &status);
    printf("interval small buffer: %d %s\n", length, statusName(status));
    udtitvfmt_close(f);
}

int main(void)
{
    /* The defaults the calls without a locale or zone take. */
    UErrorCode status = U_ZERO_ERROR;
    UChar zone[] = { 'A', 'm', 'e', 'r', 'i', 'c', 'a', '/', 'L', 'o', 's', '_', 'A', 'n', 'g', 'e', 'l', 'e', 's', 0 };
    ucal_setDefaultTimeZone(zone, &status);
    uloc_setDefault("en_US", &status);
    static const char *locales[] = { "en", "en-GB", "en-AU", "de", "fr", "fr-CA", "es", "it", "pt-BR", "nl", "sv", "fi",
        "ru", "pl", "cs", "ja", "zh", "zh-Hant", "ko", "ar", "he", "hi", "th", "tr", "vi", "id", "fa", "und",
        "en-u-hc-h23", "ja-u-hc-h11", "en-GB-u-hc-h12", NULL };
    for (int i = 0; locales[i]; i++) generator(locales[i], i < 8 || i == 15 || i == 17 || i == 19);
    static const char *patterns[] = { "yyyy-MM-dd", "d MMM y", "EEEE, MMMM d, y 'at' h:mm:ss a zzzz", "HH:mm",
        "hh:mm a", "K:mm", "kk:mm", "MMMMEEEEd", "QQQ y", "LLL", "'o''clock' h", "jmm", "ccc LLLL", "B h:mm",
        "yyMMdd", "GGGGG y.M.d", "a h", "''", "VV", "H.mm 'Uhr'", NULL };
    for (int i = 0; patterns[i]; i++) skeletonOf(patterns[i]);
    replaceTypes("en", "d-M H:m", "MMMMddHHmm");
    replaceTypes("en", "dd.MM.yyyy", "yMMMd");
    replaceTypes("de", "h:mm a", "Hm");
    replaceTypes("en", "HH:mm", "hmm");
    replaceTypes("en", "EEE d MMM", "EEEEdMMMM");
    emptyGenerator();
    generatorErrors();

    static const char *intlLocales[] = { "en", "en-GB", "de", "fr", "ja", "zh-Hant", "ar", "ko", "es-MX", "ru", "he", "hi", NULL };
    static const char *intlSkeletons[] = { "yMd", "yMMMd", "yMMMMEEEEd", "jm", "jms", "yMdjms", "yMMMdjmz", "jmzzzz",
        "jmO", "jmv", "Ejm", "GyMMMMd", "yMMMMdjmmssSSS", "hmB", "MMMMd", "yQQQQ", NULL };
    for (int l = 0; intlLocales[l]; l++)
        for (int s = 0; intlSkeletons[s]; s++) intlFormat(intlLocales[l], intlSkeletons[s], "America/New_York");
    for (int i = 0; locales[i]; i++) if (i % 2 == 0) styles(locales[i], "America/Los_Angeles");
    letters();
    zones();
    keywords();
    relative();
    symbols("en");
    symbols("de");
    symbols("ja");
    setSymbols();
    contexts();
    parsing();
    attributes();
    intervals();
    return 0;
}
