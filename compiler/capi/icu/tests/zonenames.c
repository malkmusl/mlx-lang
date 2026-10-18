/* ucal_getTimeZoneDisplayName: the names of zones (long and short,
   standard and daylight) in some locales, as ICU 74 gives them (CLDR's
   names of the zone or its metazone now, else the localized GMT format);
   Date's toString takes the long ones. Prints a line per call;
   zonenames.expected is what it must print. */
#include <stdio.h>
#include <unicode/ucal.h>
#include <unicode/ustring.h>

static const char *zoneNames[] = { "UTC", "Etc/UTC", "GMT", "Etc/GMT", "Europe/London", "Europe/Berlin", "Europe/Moscow",
    "America/New_York", "America/Los_Angeles", "America/Sao_Paulo", "America/Phoenix", "Asia/Kolkata", "Asia/Calcutta",
    "Asia/Tokyo", "Asia/Shanghai", "Asia/Kathmandu", "Australia/Sydney", "Australia/Lord_Howe", "Pacific/Auckland",
    "Africa/Casablanca", "Asia/Tehran", "Etc/GMT+5", "GMT+05:30", "Europe/Dublin", "America/Caracas", "Antarctica/Troll",
    "Pacific/Apia", "Asia/Pyongyang", "Etc/Unknown", NULL };

static const char *localeNames[] = { "en", "en_US", "en_GB", "de", "fr", "ja", "zh_Hant", "ar", "hi", "ru", "es_MX", "und", "", NULL };

int main(void)
{
    for (int z = 0; zoneNames[z]; z++) {
        UChar zone[64];
        u_uastrcpy(zone, zoneNames[z]);
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *cal = ucal_open(zone, -1, "en", UCAL_GREGORIAN, &status);
        for (int l = 0; localeNames[l]; l++) {
            printf("%s %s:", zoneNames[z], localeNames[l][0] ? localeNames[l] : "\"\"");
            for (int kind = 0; kind < 4; kind++) {
                UChar name[128];
                char text[512];
                status = U_ZERO_ERROR;
                int32_t length = ucal_getTimeZoneDisplayName(cal, (UCalendarDisplayNameType)kind, localeNames[l], name, 128, &status);
                u_strToUTF8(text, sizeof text, NULL, name, length, &status);
                printf(" \"%s\"%s%s", text, U_FAILURE(status) ? " " : "", U_FAILURE(status) ? u_errorName(status) : "");
            }
            printf("\n");
        }
        ucal_close(cal);
    }
    return 0;
}
