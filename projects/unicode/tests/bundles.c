/* ures: resource bundles over cldr.dat, as JavaScriptCore reads them (the
   time separator of a numbering system) and the rest of ures.h. Prints a
   line per call; bundles.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/ures.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>

static void show(const char *what, const UChar *s, int32_t length, UErrorCode status)
{
    char utf8[256];
    UErrorCode e = U_ZERO_ERROR;
    if (U_SUCCESS(status) && s) u_strToUTF8(utf8, sizeof utf8, NULL, s, length, &e);
    else strcpy(utf8, "");
    printf("%s = \"%s\" %d %s\n", what, utf8, U_SUCCESS(status) ? length : -1, u_errorName(status));
}

static void timeSeparator(const char *locale, const char *numberingSystem)
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *bundle = ures_open(NULL, locale, &status);
    UResourceBundle *elements = ures_getByKey(bundle, "NumberElements", NULL, &status);
    UResourceBundle *system = ures_getByKey(elements, numberingSystem, NULL, &status);
    UResourceBundle *symbols = ures_getByKey(system, "symbols", NULL, &status);
    int32_t length = 0;
    const UChar *s = ures_getStringByKey(symbols, "timeSeparator", &length, &status);
    char what[128];
    snprintf(what, sizeof what, "timeSeparator(%s, %s)", locale, numberingSystem);
    show(what, s, length, status);
    ures_close(symbols);
    ures_close(system);
    ures_close(elements);
    ures_close(bundle);
}

int main(void)
{
    timeSeparator("en", "latn");
    timeSeparator("de_AT", "latn");
    timeSeparator("fi", "latn");
    timeSeparator("ar", "arab");
    timeSeparator("da", "latn");
    timeSeparator("xx_YY", "latn");

    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle *zh = ures_open(NULL, "zh_TW", &status);
    printf("ures_open(zh_TW): actual %s valid %s %s\n", ures_getLocaleByType(zh, ULOC_ACTUAL_LOCALE, &status), ures_getLocaleByType(zh, ULOC_VALID_LOCALE, &status), u_errorName(status));
    UResourceBundle *names = ures_getByKey(zh, "localeDisplayNames", NULL, &status);
    UResourceBundle *languages = ures_getByKey(names, "languages", NULL, &status);
    int32_t length = 0;
    const UChar *s = ures_getStringByKey(languages, "de", &length, &status);
    show("zh_TW languages/de", s, length, status);
    status = U_ZERO_ERROR;
    s = ures_getStringByKey(languages, "no-such-language", &length, &status);
    show("zh_TW languages/no-such-language", s, length, status);
    ures_close(languages);
    ures_close(names);
    ures_close(zh);

    status = U_ZERO_ERROR;
    UResourceBundle *de = ures_open(NULL, "de", &status);
    UResourceBundle *list = ures_getByKey(de, "listPatterns", NULL, &status);
    UResourceBundle *standard = ures_getByKey(list, "listPattern-type-standard", NULL, &status);
    printf("listPattern-type-standard: size %d type %d key %s\n", ures_getSize(standard), ures_getType(standard), ures_getKey(standard));
    ures_resetIterator(standard);
    while (ures_hasNext(standard)) {
        const char *key = NULL;
        s = ures_getNextString(standard, &length, &key, &status);
        char what[64];
        snprintf(what, sizeof what, "  %s", key);
        show(what, s, length, status);
    }
    char utf8[64];
    int32_t capacity = sizeof utf8;
    ures_getUTF8StringByKey(standard, "end", utf8, &capacity, true, &status);
    printf("UTF-8 end = \"%s\" %d %s\n", utf8, capacity, u_errorName(status));
    ures_close(standard);
    ures_close(list);
    ures_close(de);

    status = U_ZERO_ERROR;
    ures_open("some-package", "en", &status);
    printf("ures_open(package): %s\n", u_errorName(status));
    status = U_ZERO_ERROR;
    UEnumeration *available = ures_openAvailableLocales(NULL, &status);
    printf("available: %d, first %s %s\n", uenum_count(available, &status), uenum_next(available, NULL, &status), u_errorName(status));
    uenum_close(available);
    return 0;
}
