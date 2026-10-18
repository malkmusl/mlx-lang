/* uloc: IDs, language tags, likely subtags, keywords and the BCP 47 keys
   and types, in cases whose ICU results are known. Prints a line per
   call; locales.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/uloc.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>

typedef int32_t (*Function)(const char *, char *, int32_t, UErrorCode *);

static void call(const char *name, Function f, const char *id)
{
    char buffer[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = f(id, buffer, sizeof buffer, &status);
    printf("%s(\"%s\") = \"%s\" %d %s\n", name, id, U_SUCCESS(status) ? buffer : "", length, u_errorName(status));
}

static void tag(const char *id, int strict)
{
    char buffer[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = uloc_toLanguageTag(id, buffer, sizeof buffer, strict, &status);
    printf("toLanguageTag(\"%s\", %d) = \"%s\" %d %s\n", id, strict, U_SUCCESS(status) ? buffer : "", length, u_errorName(status));
}

static void fromTag(const char *languageTag)
{
    char buffer[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t parsed = -1;
    int32_t length = uloc_forLanguageTag(languageTag, buffer, sizeof buffer, &parsed, &status);
    printf("forLanguageTag(\"%s\") = \"%s\" %d parsed %d %s\n", languageTag, U_SUCCESS(status) ? buffer : "", length, parsed, u_errorName(status));
}

static void keyword(const char *id, const char *key)
{
    char buffer[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = uloc_getKeywordValue(id, key, buffer, sizeof buffer, &status);
    printf("getKeywordValue(\"%s\", \"%s\") = \"%s\" %d %s\n", id, key, U_SUCCESS(status) ? buffer : "", length, u_errorName(status));
}

static void setKeyword(const char *key, const char *value, const char *id)
{
    char buffer[256];
    strcpy(buffer, id);
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = uloc_setKeywordValue(key, value, buffer, sizeof buffer, &status);
    printf("setKeywordValue(\"%s\", \"%s\", \"%s\") = \"%s\" %d %s\n", key, value, id, U_SUCCESS(status) ? buffer : "", length, u_errorName(status));
}

static void keywords(const char *id)
{
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *en = uloc_openKeywords(id, &status);
    printf("openKeywords(\"%s\"):", id);
    if (en) {
        const char *key;
        while ((key = uenum_next(en, 0, &status)))
            printf(" %s", key);
        uenum_close(en);
    } else {
        printf(" none");
    }
    printf(" %s\n", u_errorName(status));
}

static const char *orNull(const char *s) { return s ? s : "(null)"; }

int main(void)
{
    const char *names[] = { "en-us", "EN_us_posix", "de_DE@collation=PHONEBOOK;currency=EUR", "de@currency=EUR;calendar=gregorian;Currency=USD",
        "de-DE-u-co-phonebk", "sr-Latn-RS", "en_US.utf8@calendar=japanese", "fr__POSIX", "deu_DEU", "und_US", "root", "en-US-u-ca-gregory-x-foo",
        "zh_GAN", "de@euro", "i-default", "x-klingon_US", 0 };
    for (int i = 0; names[i]; i++) {
        call("getName", uloc_getName, names[i]);
        call("canonicalize", uloc_canonicalize, names[i]);
        call("getBaseName", uloc_getBaseName, names[i]);
    }
    const char *parts[] = { "eng", "sr_Cyrl_RS_ekavsk", "de-1901", "fr__1694acad", "und_419", "en_USA", "es_419", 0 };
    for (int i = 0; parts[i]; i++) {
        call("getLanguage", uloc_getLanguage, parts[i]);
        call("getScript", uloc_getScript, parts[i]);
        call("getCountry", uloc_getCountry, parts[i]);
        call("getVariant", uloc_getVariant, parts[i]);
    }
    const char *parents[] = { "de_DE_POSIX", "de", "und_US", "zh_Hant_TW", 0 };
    for (int i = 0; parents[i]; i++)
        call("getParent", uloc_getParent, parents[i]);

    const char *tags[] = { "en-US", "de-DE-u-co-phonebk-ca-gregory", "ja-JP-u-ca-japanese", "en-u-kn", "en-US-u-va-posix", "zh-yue-HK",
        "i-klingon", "en-x-foo", "x-foo", "en-a-bbb-x-ccc", "und-u-ca-buddhist", "en-US-POSIX", "fr-1694acad", "en-US-foo_bar",
        "de-u-co-phonebk-x-linux", "en-Latn-US-t-ja-m0-iso-x-private", "sgn-BE-FR", "zh-Hant-TW-u-nu-hanidec", "en-u-attr1-attr2-ca-islamic-civil",
        "de-u-tz-usnyc", "123", "", "en--US", "EN-us-U-CA-GREGORY", "art-lojban", 0 };
    for (int i = 0; tags[i]; i++)
        fromTag(tags[i]);
    const char *ids[] = { "en_US", "de_DE@collation=phonebook", "ja_JP@calendar=japanese;currency=JPY", "en_US_POSIX", "en@colnumeric=yes",
        "iw_IL", "@x=foo", "de__1901", "en@t=ja-m0-iso;x=priv", "th_TH@numbers=thai", "de@timezone=America/New_York", "zh_Hant_TW",
        "en@attribute=attr1-attr2;calendar=islamic-civil", "sr_Latn_RS", "en_US@calendar=gregorian;a=bbb", "en@foo=bar", "123", 0 };
    for (int i = 0; ids[i]; i++) {
        tag(ids[i], 0);
        tag(ids[i], 1);
    }

    const char *likely[] = { "en", "zh_TW", "und", "sr_ME", "und_Cyrl_RO", "de_CH", "und_419", "en_US_POSIX", "ar", "zh_Hant", "und_Hant", "xx", "zz_DE", "ja@calendar=japanese", 0 };
    for (int i = 0; likely[i]; i++)
        call("addLikelySubtags", uloc_addLikelySubtags, likely[i]);
    const char *minimal[] = { "en_Latn_US", "zh_Hant_TW", "sr_Cyrl_RS", "de_Latn_AT", "zh_Hans_CN", "und", "en_US_POSIX", "pt_Latn_BR", "pt_PT", "az_Arab_IR", 0 };
    for (int i = 0; minimal[i]; i++)
        call("minimizeSubtags", uloc_minimizeSubtags, minimal[i]);
    const char *orientations[] = { "ar", "he_IL", "en", "fa", "ur", "az_Arab", "az", "dv", "zh", 0 };
    for (int i = 0; orientations[i]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        printf("getCharacterOrientation(\"%s\") = %d, isRightToLeft %d\n", orientations[i], (int)uloc_getCharacterOrientation(orientations[i], &status), uloc_isRightToLeft(orientations[i]));
    }

    keyword("de_DE@collation=phonebook;currency=EUR", "currency");
    keyword("de_DE@collation=phonebook;currency=EUR", "CURRENCY");
    keyword("de-DE-u-co-phonebk", "collation");
    keyword("de_DE", "collation");
    setKeyword("calendar", "buddhist", "th_TH");
    setKeyword("calendar", "", "th_TH@calendar=buddhist;numbers=thai");
    setKeyword("collation", "phonebook", "de@calendar=gregorian");
    setKeyword("Calendar", "japanese", "ja_JP@calendar=gregorian");
    setKeyword("lb", "strict", "ja");
    keywords("de@currency=EUR;calendar=gregorian");
    keywords("de");
    keywords("en-u-ca-gregory-nu-latn");

    const char *keys[] = { "calendar", "ca", "zz", "not-valid", "colnumeric", "timezone", 0 };
    for (int i = 0; keys[i]; i++)
        printf("toUnicodeLocaleKey(\"%s\") = %s, toLegacyKey = %s\n", keys[i], orNull(uloc_toUnicodeLocaleKey(keys[i])), orNull(uloc_toLegacyKey(keys[i])));
    const char *types[][2] = { { "calendar", "gregorian" }, { "timezone", "America/New_York" }, { "colnumeric", "yes" }, { "ca", "xyzzy" }, { "ca", "a" },
        { "ca", "gregory" }, { "tz", "usnyc" }, { "kn", "true" }, { "co", "phonebook" }, { "kr", "latn-digit" }, { "vt", "0020-0041" }, { "ks", "level1" }, { 0, 0 } };
    for (int i = 0; types[i][0]; i++)
        printf("toUnicodeLocaleType(\"%s\", \"%s\") = %s, toLegacyType = %s\n", types[i][0], types[i][1], orNull(uloc_toUnicodeLocaleType(types[i][0], types[i][1])), orNull(uloc_toLegacyType(types[i][0], types[i][1])));
    printf("getISO3Language(\"de\") = %s, getISO3Country(\"de_DE\") = %s, getISO3Language(\"en_US\") = %s\n", uloc_getISO3Language("de"), uloc_getISO3Country("de_DE"), uloc_getISO3Language("en_US"));

    printf("countAvailable %d, first %s\n", uloc_countAvailable(), uloc_getAvailable(0));
    const char *const *languages = uloc_getISOLanguages();
    int count = 0;
    while (languages[count]) count++;
    printf("ISO languages %d, first %s\n", count, languages[0]);
    return 0;
}
