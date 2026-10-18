/* ucal: what ICU's Gregorian calendar gives, in cases whose answers are
   known: wall-clock times a transition skips or repeats (each option),
   weeks by locale, the change from the Julian calendar in 1582, add, roll,
   differences, limits, transitions, zone IDs. Prints a line per check;
   calendar.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/ustring.h>

static UChar *u(const char *text)
{
    static UChar buffers[4][64];
    static int next;
    UChar *buffer = buffers[next++ & 3];
    int i = 0;
    for (; text[i] && i < 63; i++)
        buffer[i] = (UChar)text[i];
    buffer[i] = 0;
    return buffer;
}

static const char *a(const UChar *text, int length)
{
    static char buffer[128];
    int i = 0;
    for (; i < length && i < 127; i++)
        buffer[i] = (char)text[i];
    buffer[i] = 0;
    return buffer;
}

static UCalendar *open(const char *zone, const char *locale)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = ucal_open(u(zone), -1, locale, UCAL_GREGORIAN, &status);
    if (U_FAILURE(status))
        printf("ucal_open %s: %s\n", zone, u_errorName(status));
    return cal;
}

static void fields(const char *what, UCalendar *cal)
{
    UErrorCode status = U_ZERO_ERROR;
    printf("%s: %d-%02d-%02d %02d:%02d:%02d.%03d era %d dow %d doy %d woy %d/%d wom %d dowim %d zone %d dst %d\n", what,
        ucal_get(cal, UCAL_EXTENDED_YEAR, &status), ucal_get(cal, UCAL_MONTH, &status) + 1, ucal_get(cal, UCAL_DATE, &status),
        ucal_get(cal, UCAL_HOUR_OF_DAY, &status), ucal_get(cal, UCAL_MINUTE, &status), ucal_get(cal, UCAL_SECOND, &status),
        ucal_get(cal, UCAL_MILLISECOND, &status), ucal_get(cal, UCAL_ERA, &status), ucal_get(cal, UCAL_DAY_OF_WEEK, &status),
        ucal_get(cal, UCAL_DAY_OF_YEAR, &status), ucal_get(cal, UCAL_WEEK_OF_YEAR, &status), ucal_get(cal, UCAL_YEAR_WOY, &status),
        ucal_get(cal, UCAL_WEEK_OF_MONTH, &status), ucal_get(cal, UCAL_DAY_OF_WEEK_IN_MONTH, &status),
        ucal_get(cal, UCAL_ZONE_OFFSET, &status) / 60000, ucal_get(cal, UCAL_DST_OFFSET, &status) / 60000);
    if (U_FAILURE(status))
        printf("  %s\n", u_errorName(status));
}

static void millis(const char *what, UCalendar *cal)
{
    UErrorCode status = U_ZERO_ERROR;
    double time = ucal_getMillis(cal, &status);
    printf("%s: %.0f%s%s\n", what, time, U_FAILURE(status) ? " " : "", U_FAILURE(status) ? u_errorName(status) : "");
}

static void wall(const char *zone, int year, int month, int day, int hour, int minute, int skipped, int repeated)
{
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *cal = open(zone, "en_US");
    ucal_setAttribute(cal, UCAL_SKIPPED_WALL_TIME, skipped);
    ucal_setAttribute(cal, UCAL_REPEATED_WALL_TIME, repeated);
    ucal_clear(cal);
    ucal_setDateTime(cal, year, month, day, hour, minute, 0, &status);
    char what[96];
    snprintf(what, sizeof what, "%s %d-%02d-%02d %02d:%02d skipped %d repeated %d", zone, year, month + 1, day, hour, minute, skipped, repeated);
    millis(what, cal);
    ucal_close(cal);
}

int main(void)
{
    UErrorCode status = U_ZERO_ERROR;

    UCalendar *cal = open("America/New_York", "en_US");
    ucal_setMillis(cal, 0.0, &status);
    fields("New York at 0", cal);
    ucal_setMillis(cal, 1704085200000.0, &status);
    fields("New York 2024-01-01", cal);
    UDate transition = 0;
    UBool has = ucal_getTimeZoneTransitionDate(cal, UCAL_TZ_TRANSITION_NEXT, &transition, &status);
    printf("next transition: %d %.0f\n", has, transition);
    has = ucal_getTimeZoneTransitionDate(cal, UCAL_TZ_TRANSITION_PREVIOUS, &transition, &status);
    printf("previous transition: %d %.0f\n", has, transition);
    ucal_setMillis(cal, 1710054000000.0, &status);
    has = ucal_getTimeZoneTransitionDate(cal, UCAL_TZ_TRANSITION_NEXT_INCLUSIVE, &transition, &status);
    printf("next inclusive at it: %d %.0f\n", has, transition);
    has = ucal_getTimeZoneTransitionDate(cal, UCAL_TZ_TRANSITION_PREVIOUS, &transition, &status);
    printf("previous before it: %d %.0f\n", has, transition);
    printf("in daylight time: %d\n", ucal_inDaylightTime(cal, &status));
    ucal_close(cal);

    wall("Europe/Berlin", 2024, 2, 31, 2, 30, UCAL_WALLTIME_LAST, UCAL_WALLTIME_LAST);
    wall("Europe/Berlin", 2024, 2, 31, 2, 30, UCAL_WALLTIME_FIRST, UCAL_WALLTIME_LAST);
    wall("Europe/Berlin", 2024, 2, 31, 2, 30, UCAL_WALLTIME_NEXT_VALID, UCAL_WALLTIME_LAST);
    wall("Europe/Berlin", 2024, 9, 27, 2, 30, UCAL_WALLTIME_LAST, UCAL_WALLTIME_LAST);
    wall("Europe/Berlin", 2024, 9, 27, 2, 30, UCAL_WALLTIME_LAST, UCAL_WALLTIME_FIRST);
    wall("Australia/Lord_Howe", 2024, 3, 7, 1, 45, UCAL_WALLTIME_LAST, UCAL_WALLTIME_LAST);
    wall("Australia/Lord_Howe", 2024, 3, 7, 1, 45, UCAL_WALLTIME_LAST, UCAL_WALLTIME_FIRST);

    /* A skipped time is an error when not lenient. */
    cal = open("Europe/Berlin", "de_DE");
    ucal_setAttribute(cal, UCAL_LENIENT, 0);
    ucal_clear(cal);
    ucal_setDateTime(cal, 2024, 2, 31, 2, 30, 0, &status);
    ucal_getMillis(cal, &status);
    printf("strict skipped: %s\n", u_errorName(status));
    status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_setDateTime(cal, 2024, 1, 30, 12, 0, 0, &status);
    ucal_getMillis(cal, &status);
    printf("strict February 30: %s\n", u_errorName(status));
    status = U_ZERO_ERROR;
    ucal_close(cal);

    /* Weeks: Sunday and one day (en_US), Monday and four days (de_DE). */
    cal = open("UTC", "en_US");
    printf("en_US first day %d minimal days %d\n", ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK), ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK));
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 0, 1, &status);
    fields("en_US 2024-01-01", cal);
    ucal_clear(cal);
    ucal_setDate(cal, 2021, 0, 1, &status);
    fields("en_US 2021-01-01", cal);
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 11, 31, &status);
    fields("en_US 2024-12-31", cal);
    ucal_close(cal);
    cal = open("UTC", "de_DE");
    printf("de_DE first day %d minimal days %d\n", ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK), ucal_getAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK));
    ucal_clear(cal);
    ucal_setDate(cal, 2021, 0, 1, &status);
    fields("de_DE 2021-01-01", cal);
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 11, 30, &status);
    fields("de_DE 2024-12-30", cal);
    printf("de_DE weeks in 2020: %d\n", (ucal_clear(cal), ucal_setDate(cal, 2020, 5, 1, &status), ucal_getLimit(cal, UCAL_WEEK_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status)));
    printf("de_DE weeks in 2021: %d\n", (ucal_clear(cal), ucal_setDate(cal, 2021, 5, 1, &status), ucal_getLimit(cal, UCAL_WEEK_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status)));
    /* Week 1 of 2025, Monday. */
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR_WOY, 2025);
    ucal_set(cal, UCAL_WEEK_OF_YEAR, 1);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_MONDAY);
    fields("de_DE week 1 of 2025, Monday", cal);
    ucal_close(cal);

    /* The last Monday of May 2024; the second Sunday of March 2024. */
    cal = open("UTC", "en_US");
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 2024);
    ucal_set(cal, UCAL_MONTH, UCAL_MAY);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_MONDAY);
    ucal_set(cal, UCAL_DAY_OF_WEEK_IN_MONTH, -1);
    fields("last Monday of May 2024", cal);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 2024);
    ucal_set(cal, UCAL_MONTH, UCAL_MARCH);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_SUNDAY);
    ucal_set(cal, UCAL_DAY_OF_WEEK_IN_MONTH, 2);
    fields("second Sunday of March 2024", cal);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 2024);
    ucal_set(cal, UCAL_DAY_OF_YEAR, 256);
    fields("day 256 of 2024", cal);

    /* add, roll. */
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 0, 31, &status);
    ucal_add(cal, UCAL_MONTH, 1, &status);
    fields("2024-01-31 + 1 month", cal);
    ucal_add(cal, UCAL_YEAR, 1, &status);
    fields("+ 1 year", cal);
    ucal_add(cal, UCAL_DATE, -400, &status);
    fields("- 400 days", cal);
    ucal_add(cal, UCAL_HOUR, 30, &status);
    fields("+ 30 hours", cal);
    ucal_add(cal, UCAL_WEEK_OF_YEAR, 10, &status);
    fields("+ 10 weeks", cal);
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 0, 31, &status);
    ucal_roll(cal, UCAL_MONTH, 13, &status);
    fields("2024-01-31 roll 13 months", cal);
    ucal_roll(cal, UCAL_DATE, 1, &status);
    fields("roll 1 day", cal);
    ucal_roll(cal, UCAL_HOUR_OF_DAY, -1, &status);
    fields("roll -1 hour", cal);
    ucal_roll(cal, UCAL_DAY_OF_WEEK, 3, &status);
    fields("roll 3 days of the week", cal);
    ucal_roll(cal, UCAL_WEEK_OF_YEAR, 60, &status);
    fields("roll 60 weeks", cal);
    ucal_roll(cal, UCAL_YEAR, -2025, &status);
    fields("roll -2025 years", cal);
    if (U_FAILURE(status))
        printf("add and roll: %s\n", u_errorName(status));
    status = U_ZERO_ERROR;

    /* Differences. */
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 0, 31, &status);
    ucal_setDateTime(cal, 2024, 2, 31, 0, 0, 0, &status);
    double target = ucal_getMillis(cal, &status);
    ucal_clear(cal);
    ucal_setDate(cal, 2020, 0, 31, &status);
    printf("years to 2024-03-31: %d\n", ucal_getFieldDifference(cal, target, UCAL_YEAR, &status));
    printf("then months: %d\n", ucal_getFieldDifference(cal, target, UCAL_MONTH, &status));
    printf("then days: %d\n", ucal_getFieldDifference(cal, target, UCAL_DATE, &status));
    ucal_clear(cal);
    ucal_setDate(cal, 2030, 5, 15, &status);
    printf("days back to 2024-03-31: %d\n", ucal_getFieldDifference(cal, target, UCAL_DATE, &status));

    /* Limits. */
    ucal_clear(cal);
    ucal_setDate(cal, 2024, 1, 10, &status);
    printf("February 2024: %d days, year %d days, weeks of the month %d, dowim max %d\n",
        ucal_getLimit(cal, UCAL_DATE, UCAL_ACTUAL_MAXIMUM, &status), ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status),
        ucal_getLimit(cal, UCAL_WEEK_OF_MONTH, UCAL_ACTUAL_MAXIMUM, &status), ucal_getLimit(cal, UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_ACTUAL_MAXIMUM, &status));
    printf("limits of DATE: %d %d %d %d, of WEEK_OF_MONTH: %d %d %d %d, of HOUR: %d %d\n",
        ucal_getLimit(cal, UCAL_DATE, UCAL_MINIMUM, &status), ucal_getLimit(cal, UCAL_DATE, UCAL_GREATEST_MINIMUM, &status),
        ucal_getLimit(cal, UCAL_DATE, UCAL_LEAST_MAXIMUM, &status), ucal_getLimit(cal, UCAL_DATE, UCAL_MAXIMUM, &status),
        ucal_getLimit(cal, UCAL_WEEK_OF_MONTH, UCAL_MINIMUM, &status), ucal_getLimit(cal, UCAL_WEEK_OF_MONTH, UCAL_GREATEST_MINIMUM, &status),
        ucal_getLimit(cal, UCAL_WEEK_OF_MONTH, UCAL_LEAST_MAXIMUM, &status), ucal_getLimit(cal, UCAL_WEEK_OF_MONTH, UCAL_MAXIMUM, &status),
        ucal_getLimit(cal, UCAL_HOUR, UCAL_MINIMUM, &status), ucal_getLimit(cal, UCAL_HOUR, UCAL_MAXIMUM, &status));

    /* The change of calendar: October 4, 1582 (Julian) is followed by
       October 15 (Gregorian); before, the Julian calendar. */
    ucal_clear(cal);
    ucal_setDate(cal, 1582, 9, 4, &status);
    fields("1582-10-04", cal);
    ucal_add(cal, UCAL_DATE, 1, &status);
    fields("+ 1 day", cal);
    millis("its moment", cal);
    printf("October 1582: %d days? year 1582: %d days\n", ucal_getLimit(cal, UCAL_DATE, UCAL_ACTUAL_MAXIMUM, &status), ucal_getLimit(cal, UCAL_DAY_OF_YEAR, UCAL_ACTUAL_MAXIMUM, &status));
    ucal_clear(cal);
    ucal_setDate(cal, 1000, 1, 29, &status);
    fields("1000-02-29 (Julian)", cal);
    millis("its moment", cal);
    ucal_clear(cal);
    ucal_set(cal, UCAL_ERA, 0);
    ucal_set(cal, UCAL_YEAR, 44);
    ucal_set(cal, UCAL_MONTH, 2);
    ucal_set(cal, UCAL_DATE, 15);
    fields("44 BC-03-15", cal);
    ucal_setGregorianChange(cal, -8.64e15, &status);
    ucal_clear(cal);
    ucal_setDate(cal, 1000, 1, 29, &status);
    fields("proleptic 1000-02-29", cal);
    millis("its moment", cal);
    ucal_setMillis(cal, -8.64e15, &status);
    fields("proleptic at -8.64e15", cal);
    ucal_close(cal);

    /* Zone IDs. */
    UChar buffer[64];
    UBool system = 0;
    const char *ids[] = { "Asia/Calcutta", "US/Eastern", "UTC", "Etc/GMT+5", "GMT+5:30", "gmt-0130", "GMT+12:34:56", "Etc/Unknown", "Mars/Olympus", "america/new_york", 0 };
    for (int i = 0; ids[i]; i++) {
        status = U_ZERO_ERROR;
        system = 9;
        int length = ucal_getCanonicalTimeZoneID(u(ids[i]), -1, buffer, 64, &system, &status);
        printf("canonical %s: %s system %d %s", ids[i], U_SUCCESS(status) ? a(buffer, length) : "-", system, u_errorName(status));
        status = U_ZERO_ERROR;
        length = ucal_getIanaTimeZoneID(u(ids[i]), -1, buffer, 64, &status);
        printf(", IANA %s %s\n", U_SUCCESS(status) ? a(buffer, length) : "-", u_errorName(status));
    }
    status = U_ZERO_ERROR;
    cal = open("GMT-0130", "en");
    int length = ucal_getTimeZoneID(cal, buffer, 64, &status);
    printf("calendar of GMT-0130: %s\n", a(buffer, length));
    ucal_setMillis(cal, 0.0, &status);
    fields("  at 0", cal);
    ucal_close(cal);
    cal = open("Nowhere/Special", "en");
    length = ucal_getTimeZoneID(cal, buffer, 64, &status);
    printf("calendar of Nowhere/Special: %s %s\n", a(buffer, length), u_errorName(status));
    ucal_close(cal);

    cal = open("Europe/Berlin", "de");
    ucal_setMillis(cal, 1704085200000.0, &status);
    const UCalendarDisplayNameType kinds[] = { UCAL_STANDARD, UCAL_SHORT_STANDARD, UCAL_DST, UCAL_SHORT_DST };
    for (int i = 0; i < 4; i++) {
        length = ucal_getTimeZoneDisplayName(cal, kinds[i], "de", buffer, 64, &status);
        printf("Berlin display name %d: %s\n", i, a(buffer, length));
    }
    ucal_close(cal);
    cal = open("Asia/Kolkata", "hi");
    length = ucal_getTimeZoneDisplayName(cal, UCAL_SHORT_STANDARD, "hi", buffer, 64, &status);
    printf("Kolkata short: %s\n", a(buffer, length));
    printf("hi first day %d\n", ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK));
    ucal_close(cal);

    printf("DST savings: New York %d, Kolkata %d, Lord Howe %d\n", ucal_getDSTSavings(u("America/New_York"), &status), ucal_getDSTSavings(u("Asia/Kolkata"), &status), ucal_getDSTSavings(u("Australia/Lord_Howe"), &status));

    /* Weekends. */
    cal = open("UTC", "en_US");
    printf("en_US Saturday %d Sunday %d Monday %d\n", ucal_getDayOfWeekType(cal, UCAL_SATURDAY, &status), ucal_getDayOfWeekType(cal, UCAL_SUNDAY, &status), ucal_getDayOfWeekType(cal, UCAL_MONDAY, &status));
    printf("2024-01-06 12:00 weekend %d, 2024-01-08 weekend %d\n", ucal_isWeekend(cal, 1704542400000.0, &status), ucal_isWeekend(cal, 1704715200000.0, &status));
    ucal_close(cal);
    cal = open("Asia/Riyadh", "ar_SA");
    printf("ar_SA Friday %d Saturday %d Sunday %d first day %d\n", ucal_getDayOfWeekType(cal, UCAL_FRIDAY, &status), ucal_getDayOfWeekType(cal, UCAL_SATURDAY, &status), ucal_getDayOfWeekType(cal, UCAL_SUNDAY, &status), ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK));
    ucal_close(cal);

    /* Offsets for a local time. */
    cal = open("America/New_York", "en_US");
    ucal_setMillis(cal, 1730601000000.0, &status); /* 2024-11-03 02:30 as if UTC: 01:30 is repeated */
    int32_t raw = 0, dst = 0;
    ucal_setMillis(cal, 1730597400000.0, &status); /* 01:30 */
    ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_FORMER, &raw, &dst, &status);
    printf("01:30 on 2024-11-03 former: %d %d\n", raw / 60000, dst / 60000);
    ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_LATTER, &raw, &dst, &status);
    printf("latter: %d %d\n", raw / 60000, dst / 60000);
    ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_STANDARD_FORMER, &raw, &dst, &status);
    printf("standard: %d %d\n", raw / 60000, dst / 60000);
    ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_DAYLIGHT_LATTER, &raw, &dst, &status);
    printf("daylight: %d %d\n", raw / 60000, dst / 60000);
    ucal_close(cal);

    /* Enumerations. */
    UEnumeration *en = ucal_openCountryTimeZones("DE", &status);
    printf("zones of DE:");
    const char *name;
    while ((name = uenum_next(en, 0, &status)))
        printf(" %s", name);
    printf("\n");
    uenum_close(en);
    en = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL_LOCATION, "CH", 0, &status);
    printf("canonical location zones of CH:");
    while ((name = uenum_next(en, 0, &status)))
        printf(" %s", name);
    printf("\n");
    uenum_close(en);
    en = ucal_openTimeZones(&status);
    printf("zones: %d, ", uenum_count(en, &status));
    uenum_close(en);
    en = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, 0, 0, &status);
    printf("canonical: %d\n", uenum_count(en, &status));
    uenum_close(en);
    printf("tz data version: %s\n", ucal_getTZDataVersion(&status));
    printf("status at the end: %s\n", u_errorName(status));
    return 0;
}
