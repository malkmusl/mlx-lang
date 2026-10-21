/* uldn: locale display names as JavaScriptCore's Intl.DisplayNames uses
   them (uldn_openForContext with dialect or standard names, stand-alone
   capitalization, full or short length, no substitution;
   uldn_localeDisplayName, uldn_regionDisplayName, uldn_scriptDisplayName,
   uldn_keyValueDisplayName for calendars) and the rest of uldnames.h:
   dialect names, scripts, regions, variants and keywords in the locale
   pattern, the contexts, the errors. Prints a line per call;
   displaynames.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/uldnames.h>
#include <unicode/udisplaycontext.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>

/* The status, warnings of the resource lookups apart (ICU leaves the
   fallback warnings of its bundles in it, which callers ignore). */
static const char *statusName(UErrorCode status)
{
    if (status == U_USING_FALLBACK_WARNING || status == U_USING_DEFAULT_WARNING) return "U_ZERO_ERROR";
    return u_errorName(status);
}

static void show(const char *what, const char *code, const UChar *s, int32_t length, UErrorCode status)
{
    char out[512];
    UErrorCode e = U_ZERO_ERROR;
    out[0] = 0;
    if (U_SUCCESS(status) && s) u_strToUTF8(out, sizeof out, NULL, s, length, &e);
    printf("  %s(%s) = \"%s\" %d %s\n", what, code, out, U_SUCCESS(status) ? length : -1, statusName(status));
}

#define CALL(what, code, call) do { \
        UChar result[256]; \
        UErrorCode status = U_ZERO_ERROR; \
        int32_t length = call; \
        show(what, code, result, length, status); \
    } while (0)

static void locales(ULocaleDisplayNames *ldn, const char **ids)
{
    for (int i = 0; ids[i]; i++)
        CALL("locale", ids[i], uldn_localeDisplayName(ldn, ids[i], result, 256, &status));
}

static const char *localeIds[] = {
    "en", "en_US", "en_GB", "en-GB", "en_AU", "en_CA", "en_IN", "en_150", "en_US_POSIX", "fr", "fr_CA", "fr_CH",
    "de", "de_AT", "de_CH", "de_DE_1901", "es", "es_419", "es_ES", "es_MX", "pt", "pt_BR", "pt_PT",
    "nl", "nl_BE", "zh", "zh_Hans", "zh_Hant", "zh_Hans_CN", "zh_Hant_TW", "zh_TW", "zh_Hant_HK",
    "sr", "sr_Latn", "sr_Cyrl_RS", "sr_Latn_ME", "uz_Arab_AF", "ar", "ar_001", "ar_EG", "hi_Latn",
    "ro_MD", "sw_CD", "nds_NL", "fa_AF", "yue", "iw", "iw_IL", "in", "mo", "tl", "no", "nb", "und", "und_US",
    "root", "_US", "xx", "xx_YY", "en_ZZ", "en_XA", "en_UK", "en_Zzzz", "en_Qaaa_US",
    "en@calendar=japanese", "en_US@calendar=buddhist;collation=phonebook", "de@collation=phonebook",
    "ja@calendar=japanese;numbers=jpanfin", "en@currency=EUR", "en@currency=xyz", "en@foo=bar",
    "en@calendar=nosuch", "en-u-ca-chinese-nu-arab", "en_US_POSIX@hours=h23", "th_TH@numbers=thai",
    "zh@collation=pinyin", "en@timezone=America/New_York", "en@ms=metric",
    NULL };

static const char *languages[] = { "en", "fr", "de", "zh", "ja", "en_US", "en-US", "en-us", "zh_Hans", "root", "und", "iw", "in", "mo", "tl", "sh", "no", "nb", "yue", "ar_001", "xx", "EN", "", NULL };
static const char *scripts[] = { "Latn", "Cyrl", "Hans", "Hant", "Arab", "Cans", "Zyyy", "Zzzz", "Qaaa", "latn", "Xsux", "", NULL };
static const char *regions[] = { "US", "GB", "DE", "419", "001", "150", "HK", "MO", "CD", "BA", "UK", "DD", "SU", "ZZ", "AA", "us", "", NULL };
static const char *variants[] = { "POSIX", "1901", "1996", "VALENCIA", "FONIPA", "posix", "NOSUCH", "", NULL };
static const char *keys[] = { "calendar", "collation", "currency", "numbers", "hc", "ms", "nosuch", "", NULL };
static const char *keyValues[][2] = {
    { "calendar", "gregorian" }, { "calendar", "japanese" }, { "calendar", "islamic-civil" }, { "calendar", "iso8601" },
    { "calendar", "roc" }, { "calendar", "nosuch" }, { "collation", "phonebook" }, { "collation", "standard" },
    { "numbers", "latn" }, { "numbers", "arab" }, { "numbers", "arabext" }, { "currency", "EUR" }, { "currency", "usd" },
    { "currency", "JPY" }, { "currency", "XXX" }, { "currency", "QQQ" }, { "nosuch", "value" }, { "hc", "h23" },
    { NULL, NULL } };

static void everything(const char *what, ULocaleDisplayNames *ldn)
{
    UErrorCode status = U_ZERO_ERROR;
    printf("%s: locale %s dialect %d contexts %x %x %x %x\n", what, uldn_getLocale(ldn), (int)uldn_getDialectHandling(ldn),
        (int)uldn_getContext(ldn, UDISPCTX_TYPE_DIALECT_HANDLING, &status), (int)uldn_getContext(ldn, UDISPCTX_TYPE_CAPITALIZATION, &status),
        (int)uldn_getContext(ldn, UDISPCTX_TYPE_DISPLAY_LENGTH, &status), (int)uldn_getContext(ldn, UDISPCTX_TYPE_SUBSTITUTE_HANDLING, &status));
    locales(ldn, localeIds);
    for (int i = 0; languages[i]; i++)
        CALL("language", languages[i], uldn_languageDisplayName(ldn, languages[i], result, 256, &status));
    for (int i = 0; scripts[i]; i++)
        CALL("script", scripts[i], uldn_scriptDisplayName(ldn, scripts[i], result, 256, &status));
    UScriptCode codes[] = { USCRIPT_LATIN, USCRIPT_CYRILLIC, USCRIPT_SIMPLIFIED_HAN, USCRIPT_HAN, USCRIPT_ARABIC, USCRIPT_COMMON, USCRIPT_UNKNOWN };
    for (int i = 0; i < 7; i++) {
        char code[32];
        snprintf(code, sizeof code, "%s", uscript_getShortName(codes[i]));
        CALL("scriptCode", code, uldn_scriptCodeDisplayName(ldn, codes[i], result, 256, &status));
    }
    for (int i = 0; regions[i]; i++)
        CALL("region", regions[i], uldn_regionDisplayName(ldn, regions[i], result, 256, &status));
    for (int i = 0; variants[i]; i++)
        CALL("variant", variants[i], uldn_variantDisplayName(ldn, variants[i], result, 256, &status));
    for (int i = 0; keys[i]; i++)
        CALL("key", keys[i], uldn_keyDisplayName(ldn, keys[i], result, 256, &status));
    for (int i = 0; keyValues[i][0]; i++) {
        char code[64];
        snprintf(code, sizeof code, "%s, %s", keyValues[i][0], keyValues[i][1]);
        CALL("keyValue", code, uldn_keyValueDisplayName(ldn, keyValues[i][0], keyValues[i][1], result, 256, &status));
    }
}

/* What JavaScriptCore asks of Intl.DisplayNames: language (dialect or
   standard), region, script, calendar, long or short. */
static void javascriptCore(const char *locale, UDisplayContext dialect, UDisplayContext length)
{
    UDisplayContext contexts[] = { dialect, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, length, UDISPCTX_NO_SUBSTITUTE };
    UErrorCode status = U_ZERO_ERROR;
    ULocaleDisplayNames *ldn = uldn_openForContext(locale, contexts, 4, &status);
    printf("JSC %s %s %s: %s\n", locale, dialect == UDISPCTX_DIALECT_NAMES ? "dialect" : "standard", length == UDISPCTX_LENGTH_FULL ? "long" : "short", statusName(status));
    const char *ids[] = { "en", "en-US", "en-GB", "fr-CA", "de-AT", "zh-Hans", "zh-Hant-TW", "sr-Latn", "es-419", "pt-BR",
        "nl-BE", "ar-001", "en-ZZ", "en-AA", "xx", "xx-US", "en-Latn-US", "ja-Jpan-JP", "und", "zh", "yue-Hant", "ku", "ckb", NULL };
    locales(ldn, ids);
    const char *r[] = { "US", "GB", "419", "HK", "BA", "AA", "ZZ", "UN", "PS", "CD", NULL };
    for (int i = 0; r[i]; i++)
        CALL("region", r[i], uldn_regionDisplayName(ldn, r[i], result, 256, &status));
    const char *s[] = { "Latn", "Hans", "Hant", "Cans", "Arab", "Zzzz", "Aaaa", NULL };
    for (int i = 0; s[i]; i++)
        CALL("script", s[i], uldn_scriptDisplayName(ldn, s[i], result, 256, &status));
    const char *c[] = { "gregory", "gregorian", "japanese", "islamic-umalqura", "ethiopic-amete-alem", "iso8601", "nosuch", NULL };
    for (int i = 0; c[i]; i++)
        CALL("calendar", c[i], uldn_keyValueDisplayName(ldn, "calendar", c[i], result, 256, &status));
    uldn_close(ldn);
}

int main(void)
{
    const char *displayLocales[] = { "en", "de", "fr", "es", "ja", "zh", "ar", "ru", NULL };
    for (int i = 0; displayLocales[i]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_open(displayLocales[i], ULDN_STANDARD_NAMES, &status);
        char what[64];
        snprintf(what, sizeof what, "uldn_open(%s, standard) %s", displayLocales[i], statusName(status));
        everything(what, ldn);
        uldn_close(ldn);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_open("en_US", ULDN_DIALECT_NAMES, &status);
        everything("uldn_open(en_US, dialect)", ldn);
        uldn_close(ldn);
    }
    {
        UDisplayContext contexts[] = { UDISPCTX_DIALECT_NAMES, UDISPCTX_LENGTH_SHORT };
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_openForContext("en", contexts, 2, &status);
        everything("uldn_openForContext(en, dialect short)", ldn);
        uldn_close(ldn);
    }
    {
        UDisplayContext contexts[] = { UDISPCTX_NO_SUBSTITUTE };
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_openForContext("en", contexts, 1, &status);
        everything("uldn_openForContext(en, no substitute)", ldn);
        uldn_close(ldn);
    }

    /* Capitalization. */
    UDisplayContext capitalizations[] = { UDISPCTX_CAPITALIZATION_NONE, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
        UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU, UDISPCTX_CAPITALIZATION_FOR_STANDALONE };
    const char *capitalized[] = { "fr", "es", "it", "ru", "nl", "tr", "de", "sv", "lt", "cs", NULL };
    for (int i = 0; capitalized[i]; i++) {
        for (int k = 0; k < 5; k++) {
            UDisplayContext contexts[] = { capitalizations[k] };
            UErrorCode status = U_ZERO_ERROR;
            ULocaleDisplayNames *ldn = uldn_openForContext(capitalized[i], contexts, 1, &status);
            printf("%s capitalization %x:\n", capitalized[i], (int)capitalizations[k]);
            const char *ids[] = { "en", "en_US", "fr_CA", "ij", "it@calendar=japanese", NULL };
            locales(ldn, ids);
            CALL("language", "es", uldn_languageDisplayName(ldn, "es", result, 256, &status));
            CALL("script", "Latn", uldn_scriptDisplayName(ldn, "Latn", result, 256, &status));
            CALL("region", "BE", uldn_regionDisplayName(ldn, "BE", result, 256, &status));
            CALL("variant", "1901", uldn_variantDisplayName(ldn, "1901", result, 256, &status));
            CALL("key", "calendar", uldn_keyDisplayName(ldn, "calendar", result, 256, &status));
            CALL("keyValue", "calendar, japanese", uldn_keyValueDisplayName(ldn, "calendar", "japanese", result, 256, &status));
            uldn_close(ldn);
        }
    }

    /* JavaScriptCore's Intl.DisplayNames. */
    const char *jsc[] = { "en", "en-GB", "de", "fr", "es", "ja", "zh-Hant", "ko", "he", "ar", NULL };
    for (int i = 0; jsc[i]; i++) {
        javascriptCore(jsc[i], UDISPCTX_DIALECT_NAMES, UDISPCTX_LENGTH_FULL);
        javascriptCore(jsc[i], UDISPCTX_STANDARD_NAMES, UDISPCTX_LENGTH_FULL);
        javascriptCore(jsc[i], UDISPCTX_DIALECT_NAMES, UDISPCTX_LENGTH_SHORT);
    }

    /* Contexts given twice, unknown, none. */
    {
        UDisplayContext contexts[] = { UDISPCTX_LENGTH_SHORT, UDISPCTX_DIALECT_NAMES, UDISPCTX_LENGTH_FULL, (UDisplayContext)0x501 };
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_openForContext("de-CH", contexts, 4, &status);
        printf("openForContext(de-CH): %s %s %d %x %x\n", statusName(status), uldn_getLocale(ldn), (int)uldn_getDialectHandling(ldn),
            (int)uldn_getContext(ldn, UDISPCTX_TYPE_DISPLAY_LENGTH, &status), (int)uldn_getContext(ldn, (UDisplayContextType)7, &status));
        uldn_close(ldn);
        status = U_ZERO_ERROR;
        ldn = uldn_openForContext("en_us@calendar=Japanese", NULL, 3, &status);
        printf("openForContext(NULL contexts): %s %s %d %x\n", statusName(status), uldn_getLocale(ldn), (int)uldn_getDialectHandling(ldn),
            (int)uldn_getContext(ldn, UDISPCTX_TYPE_CAPITALIZATION, &status));
        status = U_ILLEGAL_ARGUMENT_ERROR;
        printf("getContext after failure: %x %s\n", (int)uldn_getContext(ldn, UDISPCTX_TYPE_CAPITALIZATION, &status), statusName(status));
        uldn_close(ldn);
    }

    /* Buffers and errors. */
    {
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames *ldn = uldn_open("en", ULDN_DIALECT_NAMES, &status);
        UChar result[64];
        int32_t length = uldn_localeDisplayName(ldn, "de_CH", NULL, 0, &status);
        printf("preflight: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        for (int i = 0; i < 64; i++) result[i] = 0x2a;
        int32_t exact = uldn_localeDisplayName(ldn, "de_CH", NULL, 0, &status);
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(ldn, "de_CH", result, exact, &status);
        printf("exact: %d %s %s\n", length, statusName(status), result[exact - 1] != 0x2a && result[exact] == 0x2a ? "unterminated" : "?");
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(ldn, "de_CH", result, 4, &status);
        printf("small: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(ldn, "de_CH", NULL, 4, &status);
        printf("result NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(ldn, "de_CH", result, -1, &status);
        printf("capacity -1: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(ldn, NULL, result, 64, &status);
        printf("locale NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_localeDisplayName(NULL, "de", result, 64, &status);
        printf("ldn NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_regionDisplayName(ldn, NULL, result, 64, &status);
        printf("region NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_keyValueDisplayName(ldn, "calendar", NULL, result, 64, &status);
        printf("value NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = uldn_scriptCodeDisplayName(ldn, (UScriptCode)-5, result, 64, &status);
        printf("script code -5: %d %s\n", length, statusName(status));
        status = U_MISSING_RESOURCE_ERROR;
        length = uldn_languageDisplayName(ldn, "de", result, 64, &status);
        printf("failed before: %d %s\n", length, statusName(status));
        uldn_close(ldn);
        uldn_close(NULL);
        printf("NULL: %s %d\n", uldn_getLocale(NULL) ? "locale" : "NULL", (int)uldn_getDialectHandling(NULL));
        status = U_ILLEGAL_ARGUMENT_ERROR;
        ldn = uldn_open("en", ULDN_STANDARD_NAMES, &status);
        printf("open after failure: %s %s\n", ldn ? "names" : "NULL", statusName(status));
    }
    return 0;
}
