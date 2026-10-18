/* ucal: the calendars other than the Gregorian, as ICU 74 has them (the
   Buddhist, Japanese, ROC, ISO 8601, Persian, Islamic of each kind,
   Hebrew, Chinese, Dangi, Indian, Coptic, Ethiopic and Amete Alem): each
   type's fields of moments over three thousand years (a checksum a range,
   the fields themselves of some), the moments of fields set, months and
   years added and rolled, actual maxima and the limits, the types of
   locales and the calendars of regions. Prints a line per check;
   calendars.expected is what it must print (made by ICU 74 the same way,
   a process a type: ICU keeps the Chinese calendar's solstices and new
   years once worked out whatever the calendar's meridian, so a process
   of both Chinese and Dangi dates would get the second's from the
   first's).

   What ICU 74 prints otherwise, of its data (CLDR 44's, here CLDR 48's):
   the Japanese era Meiji from 1868-09-08 (CLDR 48: from 1868-10-23, so
   the checksums of the Japanese calendar's fields differ), Saudi
   Arabia's calendars (CLDR 48 prefers the Gregorian one there).

   calendars             every type
   calendars TYPE        that one (locales, change: those checks)
   calendars TYPE all    and every date's fields, not a checksum */
#include <stdio.h>
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>

static const char *types[] = {
    "gregorian", "japanese", "buddhist", "roc", "persian", "islamic-civil", "islamic", "hebrew", "chinese",
    "indian", "coptic", "ethiopic", "ethiopic-amete-alem", "iso8601", "dangi", "islamic-umalqura",
    "islamic-tbla", "islamic-rgsa", NULL };

static const char *fieldNames[] = { "era", "year", "month", "woy", "wom", "date", "doy", "dow", "dowim",
    "ampm", "hour", "hod", "minute", "second", "ms", "zone", "dst", "ywoy", "dowl", "eyear", "jd", "msid",
    "leap", "ordinal" };

static int verbose;

static UCalendar *open(const char *type)
{
    char locale[64];
    UErrorCode status = U_ZERO_ERROR;
    snprintf(locale, sizeof locale, "en@calendar=%s", type);
    UCalendar *cal = ucal_open(u"UTC", -1, locale, UCAL_DEFAULT, &status);
    if (U_FAILURE(status))
        printf("ucal_open %s: %s\n", type, u_errorName(status));
    return cal;
}

static int get(UCalendar *cal, UCalendarDateFields field)
{
    UErrorCode status = U_ZERO_ERROR;
    int value = ucal_get(cal, field, &status);
    return U_FAILURE(status) ? -999999 : value;
}

static void show(const char *what, UCalendar *cal)
{
    UErrorCode status = U_ZERO_ERROR;
    double time = ucal_getMillis(cal, &status);
    printf("%s: %.0f era %d %d-%d%s-%d (ordinal %d) doy %d dow %d woy %d/%d wom %d dowim %d%s\n", what, time,
        get(cal, UCAL_ERA), get(cal, UCAL_YEAR), get(cal, UCAL_MONTH) + 1, get(cal, UCAL_IS_LEAP_MONTH) ? "L" : "",
        get(cal, UCAL_DATE), get(cal, UCAL_ORDINAL_MONTH) + 1, get(cal, UCAL_DAY_OF_YEAR), get(cal, UCAL_DAY_OF_WEEK),
        get(cal, UCAL_WEEK_OF_YEAR), get(cal, UCAL_YEAR_WOY), get(cal, UCAL_WEEK_OF_MONTH),
        get(cal, UCAL_DAY_OF_WEEK_IN_MONTH), U_FAILURE(status) ? " (failed)" : "");
}

static unsigned long long mix(unsigned long long hash, long long value)
{
    hash ^= (unsigned long long)value;
    return hash * 1099511628211ULL;
}

/* The fields of the dates FROM to TO (days since 1970) every STEP days,
   at hour HOUR: their checksum, or each date's fields. */
static void range(UCalendar *cal, const char *type, long long from, long long to, long long step, int hour)
{
    unsigned long long hash = 14695981039346656037ULL;
    long long count = 0;
    for (long long day = from; day <= to; day += step) {
        UErrorCode status = U_ZERO_ERROR;
        ucal_setMillis(cal, (double)(day * 86400000LL + hour * 3600000LL), &status);
        if (verbose) {
            char what[64];
            snprintf(what, sizeof what, "%s day %lld", type, day);
            show(what, cal);
        }
        for (int field = 0; field < UCAL_FIELD_COUNT; field++)
            hash = mix(hash, get(cal, (UCalendarDateFields)field));
        count++;
    }
    printf("%s days %lld..%lld by %lld: %lld dates, fields %016llx\n", type, from, to, step, count, hash);
}

/* The fields of each date set again as era, year, month (leap or not),
   day: the moment they name must be the date's. */
static void roundTrip(UCalendar *cal, const char *type, long long from, long long to, long long step)
{
    long long count = 0, wrong = 0;
    for (long long day = from; day <= to; day += step) {
        UErrorCode status = U_ZERO_ERROR;
        double time = (double)(day * 86400000LL);
        ucal_setMillis(cal, time, &status);
        int era = get(cal, UCAL_ERA), year = get(cal, UCAL_YEAR), month = get(cal, UCAL_MONTH);
        int leap = get(cal, UCAL_IS_LEAP_MONTH), date = get(cal, UCAL_DATE), ordinal = get(cal, UCAL_ORDINAL_MONTH);
        int eyear = get(cal, UCAL_EXTENDED_YEAR), doy = get(cal, UCAL_DAY_OF_YEAR);
        ucal_clear(cal);
        ucal_set(cal, UCAL_ERA, era);
        ucal_set(cal, UCAL_YEAR, year);
        ucal_set(cal, UCAL_MONTH, month);
        ucal_set(cal, UCAL_IS_LEAP_MONTH, leap);
        ucal_set(cal, UCAL_DATE, date);
        double back = ucal_getMillis(cal, &status);
        ucal_clear(cal);
        ucal_set(cal, UCAL_EXTENDED_YEAR, eyear);
        ucal_set(cal, UCAL_ORDINAL_MONTH, ordinal);
        ucal_set(cal, UCAL_DATE, date);
        double byOrdinal = ucal_getMillis(cal, &status);
        ucal_clear(cal);
        ucal_set(cal, UCAL_EXTENDED_YEAR, eyear);
        ucal_set(cal, UCAL_DAY_OF_YEAR, doy);
        double byDay = ucal_getMillis(cal, &status);
        if (back != time || byOrdinal != time || byDay != time) {
            if (wrong < 5)
                printf("%s day %lld: %d %d-%d%s-%d back %.0f, ordinal %d %.0f, doy %d %.0f\n", type, day, era, year,
                    month + 1, leap ? "L" : "", date, back / 86400000, ordinal + 1, byOrdinal / 86400000, doy, byDay / 86400000);
            wrong++;
        }
        count++;
    }
    printf("%s round trips %lld..%lld by %lld: %lld dates, %lld differ\n", type, from, to, step, count, wrong);
}

static void arithmetic(UCalendar *cal, const char *type, long long day)
{
    static const int amounts[] = { 1, -1, 2, -2, 12, -12, 13, -13 };
    static const struct { UCalendarDateFields field; const char *name; } fields[] = {
        { UCAL_MONTH, "month" }, { UCAL_ORDINAL_MONTH, "ordinal" }, { UCAL_YEAR, "year" },
        { UCAL_EXTENDED_YEAR, "eyear" }, { UCAL_DATE, "date" }, { UCAL_WEEK_OF_YEAR, "woy" },
        { UCAL_DAY_OF_YEAR, "doy" }, { UCAL_ERA, "era" } };
    char what[96];
    for (int f = 0; f < 8; f++) {
        for (int a = 0; a < 8; a++) {
            for (int roll = 0; roll < 2; roll++) {
                UErrorCode status = U_ZERO_ERROR;
                ucal_clear(cal);
                ucal_setMillis(cal, (double)(day * 86400000LL), &status);
                if (roll)
                    ucal_roll(cal, fields[f].field, amounts[a], &status);
                else
                    ucal_add(cal, fields[f].field, amounts[a], &status);
                snprintf(what, sizeof what, "%s %lld %s %s %d%s%s", type, day, roll ? "roll" : "add", fields[f].name,
                    amounts[a], U_FAILURE(status) ? " " : "", U_FAILURE(status) ? u_errorName(status) : "");
                show(what, cal);
            }
        }
    }
    static const struct { UCalendarDateFields field; const char *name; } maxima[] = {
        { UCAL_DATE, "date" }, { UCAL_DAY_OF_YEAR, "doy" }, { UCAL_MONTH, "month" }, { UCAL_ORDINAL_MONTH, "ordinal" },
        { UCAL_WEEK_OF_YEAR, "woy" }, { UCAL_WEEK_OF_MONTH, "wom" }, { UCAL_YEAR, "year" }, { UCAL_ERA, "era" },
        { UCAL_DAY_OF_WEEK_IN_MONTH, "dowim" } };
    UErrorCode status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_setMillis(cal, (double)(day * 86400000LL), &status);
    printf("%s %lld actual:", type, day);
    for (int f = 0; f < 9; f++) {
        UErrorCode s = U_ZERO_ERROR;
        int min = ucal_getLimit(cal, maxima[f].field, UCAL_ACTUAL_MINIMUM, &s);
        int max = ucal_getLimit(cal, maxima[f].field, UCAL_ACTUAL_MAXIMUM, &s);
        printf(" %s %d..%d", maxima[f].name, min, max);
    }
    printf("\n");
}

static void limits(UCalendar *cal, const char *type)
{
    printf("%s limits:", type);
    for (int field = 0; field < UCAL_FIELD_COUNT; field++) {
        UErrorCode status = U_ZERO_ERROR;
        printf(" %s %d %d %d %d", fieldNames[field], ucal_getLimit(cal, (UCalendarDateFields)field, UCAL_MINIMUM, &status),
            ucal_getLimit(cal, (UCalendarDateFields)field, UCAL_GREATEST_MINIMUM, &status),
            ucal_getLimit(cal, (UCalendarDateFields)field, UCAL_LEAST_MAXIMUM, &status),
            ucal_getLimit(cal, (UCalendarDateFields)field, UCAL_MAXIMUM, &status));
    }
    printf("\n");
}

/* Fields set, of each kind: the moment they name. */
static void setFields(UCalendar *cal, const char *type)
{
    static const int sets[][5] = {
        /* era, year, month, leap, date; -1 not set */
        { -1, 1, 0, -1, 1 }, { -1, 5784, 6, -1, 1 }, { -1, 5784, 5, -1, 30 }, { -1, 5785, 5, -1, 1 },
        { -1, 1445, 8, -1, 1 }, { -1, 1403, 11, -1, 30 }, { -1, 41, 1, 1, 1 }, { 78, 41, 1, 0, 15 },
        { 78, 40, 3, 1, 10 }, { -1, 2017, 11, -1, 3 }, { 0, 100, 0, -1, 1 }, { 236, 6, 0, -1, 1 },
        { 235, 31, 3, -1, 30 }, { 236, 1, 0, -1, -1 }, { 1, 2024, 0, -1, 1 }, { -1, 1946, 12, -1, 5 } };
    for (unsigned i = 0; i < sizeof sets / sizeof sets[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        ucal_clear(cal);
        if (sets[i][0] >= 0) ucal_set(cal, UCAL_ERA, sets[i][0]);
        if (sets[i][1] >= 0) ucal_set(cal, UCAL_YEAR, sets[i][1]);
        if (sets[i][2] >= 0) ucal_set(cal, UCAL_MONTH, sets[i][2]);
        if (sets[i][3] >= 0) ucal_set(cal, UCAL_IS_LEAP_MONTH, sets[i][3]);
        if (sets[i][4] >= 0) ucal_set(cal, UCAL_DATE, sets[i][4]);
        double time = ucal_getMillis(cal, &status);
        char what[96];
        snprintf(what, sizeof what, "%s set %d %d %d %d %d: %.0f%s", type, sets[i][0], sets[i][1], sets[i][2], sets[i][3],
            sets[i][4], time / 86400000, U_FAILURE(status) ? " failed" : "");
        show(what, cal);
    }
    /* Strict: a date the calendar does not have. */
    ucal_setAttribute(cal, UCAL_LENIENT, 0);
    for (int month = 0; month < 14; month++) {
        UErrorCode status = U_ZERO_ERROR;
        ucal_clear(cal);
        ucal_set(cal, UCAL_EXTENDED_YEAR, month < 7 ? 5784 : 5785);
        ucal_set(cal, UCAL_MONTH, month % 13);
        ucal_set(cal, UCAL_DATE, 30);
        double time = ucal_getMillis(cal, &status);
        printf("%s strict %d-%d-30: %s\n", type, month < 7 ? 5784 : 5785, month % 13 + 1, U_FAILURE(status) ? u_errorName(status) : "ok");
        (void)time;
    }
    ucal_setAttribute(cal, UCAL_LENIENT, 1);
}

static void test(const char *type)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open(type);
    if (!cal) return;
    printf("%s: type %s first day %d minimal days %d\n", type, ucal_getType(cal, &status),
        ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK), ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK));
    /* The calendars of astronomy and of tables searched year by year
       less densely. */
    int slow = !strcmp(type, "chinese") || !strcmp(type, "dangi") || !strcmp(type, "islamic") || !strcmp(type, "islamic-rgsa");
    int searched = slow || !strcmp(type, "islamic-umalqura");
    /* 1800 to 2100 densely; -1000 to 3000 sparsely; some days in a row. */
    range(cal, type, -62091, 47482, slow ? 29 : 3, 12);
    range(cal, type, -1084000, 376000, searched ? 1997 : 97, 0);
    range(cal, type, -36000, -35000, slow ? 7 : 1, 23);
    roundTrip(cal, type, -62091, 47482, slow ? 61 : 7);
    static const long long days[] = { 0, 19723, 19800, 19900, 20000, 20089, 20200, 7000, -50000, 30000, -140000 };
    for (unsigned i = 0; i < sizeof days / sizeof days[0]; i++) {
        char what[64];
        UErrorCode s = U_ZERO_ERROR;
        ucal_clear(cal);
        ucal_setMillis(cal, (double)(days[i] * 86400000LL), &s);
        snprintf(what, sizeof what, "%s day %lld", type, days[i]);
        show(what, cal);
    }
    arithmetic(cal, type, 19723);
    arithmetic(cal, type, 20089);
    limits(cal, type);
    setFields(cal, type);
    ucal_close(cal);
}

static void locales(void)
{
    static const char *names[] = { "", "en", "en_US", "th", "th_TH", "fa", "fa_IR", "ar_SA", "ar", "ar_EG", "he_IL",
        "ja_JP", "ja_JP@calendar=japanese", "zh_TW", "ko_KR", "hi_IN", "am_ET", "en@calendar=hebrew", "de@calendar=foo",
        "th_TH@calendar=gregorian", "en-u-ca-ethioaa", "en-u-ca-gregory", "en@calendar=ISLAMIC-civil", "und_AF",
        "en@rg=thzzzz", "en_US@calendar=iso8601", "en_US@calendar=iso8601;fw=sun", NULL };
    for (int i = 0; names[i]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *cal = ucal_open(u"UTC", -1, names[i], UCAL_DEFAULT, &status);
        UCalendar *greg = ucal_open(u"UTC", -1, names[i], UCAL_GREGORIAN, &status);
        printf("locale \"%s\": %s (gregorian: %s) week %d/%d\n", names[i], ucal_getType(cal, &status), ucal_getType(greg, &status),
            ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK), ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK));
        ucal_close(cal);
        ucal_close(greg);
        for (int common = 0; common < 2; common++) {
            UEnumeration *values = ucal_getKeywordValuesForLocale("calendar", names[i], common, &status);
            printf("  %s:", common ? "common" : "all");
            const char *value;
            while ((value = uenum_next(values, NULL, &status)))
                printf(" %s", value);
            printf(" %s\n", u_errorName(status));
            uenum_close(values);
        }
    }
    /* The Gregorian change is the Gregorian calendar's alone. */
    for (int i = 0; types[i]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *cal = open(types[i]);
        ucal_setGregorianChange(cal, -8.64e15, &status);
        UErrorCode get = U_ZERO_ERROR;
        double change = ucal_getGregorianChange(cal, &get);
        printf("%s: setGregorianChange %s, getGregorianChange %.0f %s\n", types[i], u_errorName(status), change, u_errorName(get));
        ucal_close(cal);
    }
}

/* The Gregorian change set (ICU keeps its day counted from 1970 where
   it compares Julian days: dates before the Julian day of that number
   are Julian ones, the years before the change's year leap years by the
   Julian rule). */
static void gregorianChange(void)
{
    static const double changes[] = { 0, -8.64e15, -12219292800000.0, 946080000000.0, -9460800000000.0 };
    static const double days[] = { -1, 0, 1, -100000, -2440000, -2500000, 10000, 20000, -719000, -800000 };
    for (int c = 0; c < 5; c++) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *cal = ucal_open(u"UTC", -1, "en", UCAL_GREGORIAN, &status);
        ucal_setGregorianChange(cal, changes[c], &status);
        printf("change %.0f: %s, %.0f\n", changes[c] / 86400000, u_errorName(status), ucal_getGregorianChange(cal, &status) / 86400000);
        for (int d = 0; d < 10; d++) {
            char what[64];
            snprintf(what, sizeof what, "change %.0f day %.0f", changes[c] / 86400000, days[d]);
            ucal_setMillis(cal, days[d] * 86400000.0, &status);
            show(what, cal);
        }
        static const int ymd[][3] = { { 1700, 1, 29 }, { 1969, 11, 31 }, { 1582, 9, 10 }, { -4712, 0, 1 } };
        for (int k = 0; k < 4; k++) {
            ucal_clear(cal);
            ucal_set(cal, UCAL_EXTENDED_YEAR, ymd[k][0]);
            ucal_set(cal, UCAL_MONTH, ymd[k][1]);
            ucal_set(cal, UCAL_DATE, ymd[k][2]);
            printf("change %.0f %d-%d-%d: %.0f\n", changes[c] / 86400000, ymd[k][0], ymd[k][1] + 1, ymd[k][2], ucal_getMillis(cal, &status) / 86400000);
        }
        ucal_close(cal);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        verbose = argc > 2;
        if (!strcmp(argv[1], "locales"))
            locales();
        else if (!strcmp(argv[1], "change"))
            gregorianChange();
        else
            test(argv[1]);
        return 0;
    }
    for (int i = 0; types[i]; i++)
        test(types[i]);
    locales();
    gregorianChange();
    return 0;
}
