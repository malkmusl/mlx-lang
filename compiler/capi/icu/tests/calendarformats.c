/* udat, udatpg, udtitvfmt with calendars other than the Gregorian, as
   JavaScriptCore's Intl.DateTimeFormat uses them: a locale with a
   -u-ca- type (or its region's calendar), a skeleton's best pattern, a
   format of that pattern (its calendar's Gregorian change set as JSC
   sets it), dates formatted with their fields; the styles' patterns; the
   symbols (eras, months, leap month patterns, cyclic years); intervals.
   Prints a line per call; calendarformats.expected is what it must print.

   calendarformats              English in every calendar, other locales
                                in theirs
   calendarformats all          every locale in every calendar
   calendarformats LOCALE       one locale's lines, every calendar */
#include <stdio.h>
#include <string.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/udateintervalformat.h>
#include <unicode/ufieldpositer.h>
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>
#include <unicode/uloc.h>

static const char *statusName(UErrorCode status)
{
    if (status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING) return "U_ZERO_ERROR";
    return u_errorName(status);
}

static void utf8(const UChar *s, int32_t length, char *out, int32_t capacity)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t written = 0;
    u_strToUTF8(out, capacity, &written, s, length, &status);
    if (U_FAILURE(status)) snprintf(out, capacity, "(%s)", u_errorName(status));
}

static void ascii(const char *text, UChar *out)
{
    while ((*out++ = (UChar)*text++))
        ;
}

static const char *calendars[] = { "", "gregory", "japanese", "buddhist", "roc", "persian", "islamic", "islamic-civil",
    "islamic-umalqura", "islamic-tbla", "islamic-rgsa", "hebrew", "chinese", "dangi", "indian", "coptic", "ethiopic",
    "ethioaa", "iso8601", NULL };

static const char *localeNames[] = { "en", "en-GB", "ja", "zh", "zh-Hant", "ko", "th", "ar", "ar-SA", "fa", "he", "hi",
    "am", "de", "fr", "ru", "es", "und", NULL };

static const char *skeletons[] = { "yMd", "yMMMd", "yMMMMEEEEd", "GyMMMd", "GGGGyMMMMd", "yMMM", "Gy", "y", "MMMM",
    "yMMMMEEEEdjmszzzz", "GGGGGyMd", NULL };

/* UTC: 2024-01-01, 2023-06-21 12:00, 2020-05-23 (a leap month of the
   Chinese year), 2019-04-30, 2019-05-01, 1989-01-07, 1989-01-08,
   1912-02-18, 2000-02-29, 1868-10-23, 1700-03-01, -0500-07-01. */
static const double dates[] = { 1704067200000.0, 1687348800000.0, 1590192000000.0, 1556582400000.0, 1556668800000.0,
    600134400000.0, 600220800000.0, -1825286400000.0, 951782400000.0, -3194208000000.0, -8509651200000.0,
    -77909443200000.0 };
#define DATE_COUNT (sizeof dates / sizeof dates[0])

static void localeWith(const char *locale, const char *calendar, char *out, size_t capacity)
{
    if (calendar[0])
        snprintf(out, capacity, "%s-u-ca-%s", locale, calendar);
    else
        snprintf(out, capacity, "%s", locale);
}

static UDateFormat *openPattern(const char *locale, const UChar *pattern, int32_t length, UErrorCode *status)
{
    UDateFormat *format = udat_open(UDAT_PATTERN, UDAT_PATTERN, locale, u"UTC", 3, pattern, length, status);
    if (U_SUCCESS(*status)) {
        UErrorCode ignored = U_ZERO_ERROR;
        ucal_setGregorianChange((UCalendar *)udat_getCalendar(format), -8.64e15, &ignored);
    }
    return format;
}

static void formatDates(const char *what, UDateFormat *format, int all)
{
    for (unsigned d = 0; d < DATE_COUNT; d++) {
        if (!all && d != 0 && d != 2 && d != 6 && d != 10) continue;
        UErrorCode status = U_ZERO_ERROR;
        UChar result[256];
        char text[768];
        UFieldPositionIterator *fields = ufieldpositer_open(&status);
        int32_t length = udat_formatForFields(format, dates[d], result, 256, fields, &status);
        utf8(result, length, text, sizeof text);
        printf("%s %u: \"%s\" %s [", what, d, text, statusName(status));
        int32_t start, limit, field;
        while ((field = ufieldpositer_next(fields, &start, &limit)) >= 0)
            printf(" %d:%d-%d", field, start, limit);
        printf(" ]\n");
        ufieldpositer_close(fields);
    }
}

static void skeletonFormats(const char *locale, const char *calendar)
{
    char name[96], text[512], what[256];
    localeWith(locale, calendar, name, sizeof name);
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator *generator = udatpg_open(name, &status);
    if (U_FAILURE(status)) {
        printf("udatpg_open %s: %s\n", name, u_errorName(status));
        return;
    }
    for (int k = 0; skeletons[k]; k++) {
        UChar skeleton[64], pattern[256];
        ascii(skeletons[k], skeleton);
        status = U_ZERO_ERROR;
        int32_t length = udatpg_getBestPatternWithOptions(generator, skeleton, -1, UDATPG_MATCH_HOUR_FIELD_LENGTH, pattern, 256, &status);
        utf8(pattern, length, text, sizeof text);
        printf("%s %s: \"%s\" %s\n", name, skeletons[k], text, statusName(status));
        UDateFormat *format = openPattern(name, pattern, length, &status);
        if (U_FAILURE(status)) {
            printf("  udat_open: %s\n", u_errorName(status));
            continue;
        }
        snprintf(what, sizeof what, "%s %s", name, skeletons[k]);
        formatDates(what, format, k == 1);
        udat_close(format);
    }
    udatpg_close(generator);
}

static void styles(const char *locale, const char *calendar)
{
    char name[96], text[512], what[256];
    localeWith(locale, calendar, name, sizeof name);
    for (int style = 0; style < 4; style++) {
        for (int withTime = 0; withTime < 2; withTime++) {
            UErrorCode status = U_ZERO_ERROR;
            UDateFormat *styled = udat_open(withTime ? (UDateFormatStyle)style : UDAT_NONE, (UDateFormatStyle)style, name, u"UTC", 3, NULL, -1, &status);
            if (U_FAILURE(status)) {
                printf("%s style %d/%d: %s\n", name, style, withTime, u_errorName(status));
                continue;
            }
            UChar pattern[256];
            int32_t length = udat_toPattern(styled, 0, pattern, 256, &status);
            utf8(pattern, length, text, sizeof text);
            printf("%s style %d/%d pattern: \"%s\" %s calendar %s\n", name, style, withTime, text, statusName(status),
                ucal_getType(udat_getCalendar(styled), &status));
            UDateFormat *format = openPattern(name, pattern, length, &status);
            snprintf(what, sizeof what, "%s style %d/%d", name, style, withTime);
            if (U_SUCCESS(status)) {
                formatDates(what, format, style == 1 && !withTime);
                udat_close(format);
            }
            udat_close(styled);
        }
    }
}

static void symbols(const char *locale, const char *calendar)
{
    char name[96], text[512];
    localeWith(locale, calendar, name, sizeof name);
    static const struct { UDateFormatSymbolType type; const char *name; } sets[] = {
        { UDAT_ERAS, "eras" }, { UDAT_ERA_NAMES, "era names" },
        { UDAT_MONTHS, "months" }, { UDAT_SHORT_MONTHS, "short months" }, { UDAT_STANDALONE_NARROW_MONTHS, "narrow months" },
        { UDAT_CYCLIC_YEARS_ABBREVIATED, "cyclic years" }, { UDAT_ZODIAC_NAMES_ABBREVIATED, "zodiac" } };
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *format = udat_open(UDAT_PATTERN, UDAT_PATTERN, name, u"UTC", 3, u"y", 1, &status);
    if (U_FAILURE(status)) return;
    for (unsigned k = 0; k < sizeof sets / sizeof sets[0]; k++) {
        int32_t count = udat_countSymbols(format, sets[k].type);
        printf("%s %s %d:", name, sets[k].name, count);
        for (int32_t i = 0; i < count; i++) {
            if (count > 20 && i > 3 && i < count - 3) {
                if (i == 4) printf(" ...");
                continue;
            }
            UChar value[128];
            status = U_ZERO_ERROR;
            int32_t length = udat_getSymbols(format, sets[k].type, i, value, 128, &status);
            utf8(value, length, text, sizeof text);
            printf(" \"%s\"", text);
        }
        printf("\n");
    }
    udat_close(format);
}

static void intervals(const char *locale, const char *calendar)
{
    char name[96], text[512];
    localeWith(locale, calendar, name, sizeof name);
    static const char *intervalSkeletons[] = { "yMMMd", "yMd", "yMMM", "GyMMMMd", NULL };
    for (int k = 0; intervalSkeletons[k]; k++) {
        UChar skeleton[32];
        ascii(intervalSkeletons[k], skeleton);
        UErrorCode status = U_ZERO_ERROR;
        UDateIntervalFormat *format = udtitvfmt_open(name, skeleton, -1, u"UTC", 3, &status);
        if (U_FAILURE(status)) {
            printf("%s interval %s: %s\n", name, intervalSkeletons[k], u_errorName(status));
            continue;
        }
        for (int pair = 0; pair < 3; pair++) {
            double from = dates[0], to = pair == 0 ? dates[0] + 86400000.0 * 10 : pair == 1 ? dates[0] + 86400000.0 * 40 : dates[1];
            UChar result[256];
            status = U_ZERO_ERROR;
            int32_t length = udtitvfmt_format(format, from, to, result, 256, NULL, &status);
            utf8(result, length, text, sizeof text);
            printf("%s interval %s %d: \"%s\" %s\n", name, intervalSkeletons[k], pair, text, statusName(status));
        }
        udtitvfmt_close(format);
    }
}

static void locale(const char *locale)
{
    for (int c = 0; calendars[c]; c++) {
        skeletonFormats(locale, calendars[c]);
        styles(locale, calendars[c]);
        symbols(locale, calendars[c]);
        intervals(locale, calendars[c]);
    }
}

static void one(const char *locale, const char *calendar)
{
    skeletonFormats(locale, calendar);
    styles(locale, calendar);
    symbols(locale, calendar);
    intervals(locale, calendar);
}

/* What is printed without arguments: English in every calendar, other
   locales in theirs. */
static const char *pairs[][2] = { { "ja", "" }, { "ja", "japanese" }, { "ja", "chinese" }, { "zh", "" },
    { "zh", "chinese" }, { "zh-Hant", "roc" }, { "zh-Hant", "chinese" }, { "ko", "dangi" }, { "th", "" },
    { "th", "gregory" }, { "ar-SA", "" }, { "ar-SA", "islamic-umalqura" }, { "ar", "islamic" }, { "ar", "coptic" },
    { "fa", "" }, { "he", "hebrew" }, { "hi", "indian" }, { "am", "ethiopic" }, { "am", "ethioaa" },
    { "de", "buddhist" }, { "de", "japanese" }, { "fr", "hebrew" }, { "ru", "islamic-civil" }, { NULL, NULL } };

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "all")) {
            for (int i = 0; localeNames[i]; i++)
                locale(localeNames[i]);
        } else {
            locale(argv[1]);
        }
        return 0;
    }
    locale("en");
    for (int i = 0; pairs[i][0]; i++)
        one(pairs[i][0], pairs[i][1]);
    return 0;
}
