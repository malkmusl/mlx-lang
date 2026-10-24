/* Numbers: number formatters (unumf_*) of the skeletons JavaScriptCore's
   Intl.NumberFormat, Intl.PluralRules and Intl.DurationFormat make, and
   of ICU's documented options; their results' text, fields and decimal
   numbers; number ranges (unumrf_*); plural rules (uplrules_*);
   currencies (ucurr_*); numbering systems (unumsys_*); the number
   formats of unum.h (styles, patterns, attributes, symbols, parsing,
   UFormattable) as Intl.RelativeTimeFormat sets one up; relative dates
   and times (ureldatefmt_*). Prints a line per call; numbers.expected is
   what it must print. Statuses are printed as numbers, ICU's warnings
   (of resource fallbacks) as 0. */
#include <stdio.h>
#include <string.h>
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>
#include <unicode/upluralrules.h>
#include <unicode/ucurr.h>
#include <unicode/unumsys.h>
#include <unicode/unum.h>
#include <unicode/uformattable.h>
#include <unicode/ureldatefmt.h>
#include <unicode/uformattedvalue.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>

static int status(UErrorCode s) { return s < 0 ? 0 : (int)s; }

static void toU(const char *s, UChar *d)
{
    /* UTF-8 to UTF-16. */
    UErrorCode e = U_ZERO_ERROR;
    int32_t n;
    u_strFromUTF8(d, 512, &n, s, -1, &e);
}

static void put(const UChar *s, int32_t n)
{
    char t[2048];
    UErrorCode e = U_ZERO_ERROR;
    t[0] = 0;
    if (s) u_strToUTF8(t, sizeof t, NULL, s, n, &e);
    printf("\"%s\"", t);
}

static void positions(const UFormattedValue *v)
{
    UErrorCode e = U_ZERO_ERROR;
    UConstrainedFieldPosition *p = ucfpos_open(&e);
    while (ufmtval_nextPosition(v, p, &e)) {
        int32_t a, b;
        ucfpos_getIndexes(p, &a, &b, &e);
        printf(" %d/%d:%d-%d", (int)ucfpos_getCategory(p, &e), (int)ucfpos_getField(p, &e), a, b);
    }
    ucfpos_close(p);
}

/* ------------------------------------------------------------------ */
/* unumf */

static void showNumber(UFormattedNumber *r, UErrorCode st)
{
    if (U_FAILURE(st)) { printf(" error %d\n", status(st)); return; }
    UErrorCode e = U_ZERO_ERROR;
    UChar b[512];
    int32_t n = unumf_resultToString(r, b, 512, &e);
    printf(" ");
    put(b, n);
    const UFormattedValue *v = unumf_resultAsValue(r, &e);
    positions(v);
    char d[128];
    unumf_resultToDecimalNumber(r, d, sizeof d, &e);
    printf(" [%s]\n", d);
}

static const double doubles[] = { 0, -0.0, 1, -1, 0.5, 1.5, 2.5, -2.5, 1234.5678, -9876543.21, 0.000123, 1e21, 123456789012345680000.0 };
static const char *decimals[] = { "1.005", "-12345678901234567890.123456789", "0.00000001", "1E+30", "NaN", "-Infinity", 0 };

static void numbers(const char *locale, const char *skeleton)
{
    UChar sk[512];
    toU(skeleton, sk);
    UErrorCode st = U_ZERO_ERROR;
    UParseError pe;
    UNumberFormatter *f = unumf_openForSkeletonAndLocaleWithError(sk, -1, locale, &pe, &st);
    printf("%s [%s]", locale, skeleton);
    if (U_FAILURE(st)) {
        printf(" open error %d offset %d\n", status(st), pe.offset);
        unumf_close(f);
        return;
    }
    printf("\n");
    UFormattedNumber *r = unumf_openResult(&st);
    for (unsigned i = 0; i < sizeof doubles / sizeof doubles[0]; i++) {
        st = U_ZERO_ERROR;
        unumf_formatDouble(f, doubles[i], r, &st);
        printf("  %.17g", doubles[i]);
        showNumber(r, st);
    }
    for (int i = 0; decimals[i]; i++) {
        st = U_ZERO_ERROR;
        unumf_formatDecimal(f, decimals[i], -1, r, &st);
        printf("  %s", decimals[i]);
        showNumber(r, st);
    }
    st = U_ZERO_ERROR;
    unumf_formatInt(f, -9223372036854775807LL - 1, r, &st);
    printf("  int64 min");
    showNumber(r, st);
    unumf_closeResult(r);
    unumf_close(f);
}

/* The skeletons of JavaScriptCore's Intl.NumberFormat. */
static const char *jscSkeletons[] = {
    "rounding-mode-half-expand integer-width/*0 .### sign-auto group-auto",
    "rounding-mode-half-expand integer-width/*0 .00 sign-auto group-auto currency/USD",
    "rounding-mode-half-expand integer-width/*0 . sign-auto group-auto currency/JPY unit-width-narrow",
    "rounding-mode-half-expand integer-width/*0 .00 sign-auto group-auto currency/EUR unit-width-full-name",
    "rounding-mode-half-expand integer-width/*0 .00 sign-accounting group-auto currency/USD",
    "rounding-mode-half-expand integer-width/*0 .00 sign-auto group-auto currency/EUR unit-width-iso-code",
    "rounding-mode-half-expand integer-width/*0 . sign-auto group-auto percent scale/100",
    "rounding-mode-half-expand integer-width/*0 ./@#r sign-auto group-min2 compact-short",
    "rounding-mode-half-expand integer-width/*0 ./@#r sign-auto group-min2 compact-long",
    "rounding-mode-half-expand integer-width/*0 .### sign-auto group-off scientific",
    "rounding-mode-half-expand integer-width/*0 .### sign-auto group-auto engineering",
    "rounding-mode-half-expand integer-width/*000 .00## sign-always group-auto",
    "rounding-mode-half-expand integer-width/*0 @@@ sign-except-zero group-on-aligned",
    "rounding-mode-half-expand integer-width/*0 .##/@@@@r sign-never group-auto",
    "rounding-mode-half-expand integer-width/*0 .##/@@@s sign-negative group-auto",
    "rounding-mode-half-trunc integer-width/*0 precision-increment/0.05 sign-auto group-auto",
    "rounding-mode-ceiling integer-width/*0 .0 sign-auto group-auto",
    "rounding-mode-floor integer-width/*0 .0 sign-auto group-auto",
    "rounding-mode-half-even integer-width/*0 .00/w sign-auto group-auto",
    "rounding-mode-half-expand integer-width/*0 . sign-auto group-auto measure-unit/length-meter",
    "rounding-mode-half-expand integer-width/*0 .### sign-auto group-auto numbering-system/arab",
    0
};

static void formatters(void)
{
    const char *locales[] = { "en-US", "de-DE", "fr-FR", "ja-JP", "hi-IN", "ar-EG", 0 };
    for (int l = 0; locales[l]; l++)
        for (int s = 0; jscSkeletons[s]; s++) numbers(locales[l], jscSkeletons[s]);
    /* ICU's other options. */
    const char *more[] = {
        "", "precision-integer", ".00+", "@@#", "precision-currency-cash currency/CHF", "currency/CHF precision-increment/0.05",
        "integer-width/##00", "integer-width-trunc", "decimal-always", "sign-accounting-always currency/EUR", "sign-accounting-except-zero currency/GBP",
        "scale/0.5", "scientific/+ee/sign-always", "engineering/*ee", "compact-short currency/EUR", "unit-width-formal currency/TWD", "unit-width-variant currency/TWD",
        "unit-width-hidden currency/USD", "rounding-mode-unnecessary .0", "permille", "percent unit-width-full-name", "latin",
        "foo", ".00/w/x", "currency/US", "precision-increment/0", "measure-unit/length-furlong", 0 };
    for (int s = 0; more[s]; s++) numbers("en", more[s]);
    numbers("de-CH", "currency/CHF");
    numbers("es", "rounding-mode-half-expand integer-width/*0 .### sign-auto group-auto");
    numbers("ar-EG-u-nu-latn", ".00");
    numbers("th-u-nu-thai", "group-thousands");
    numbers("zh-Hant-TW", "compact-long");
    /* Fields one by one (unumf_resultNextFieldPosition). */
    {
        UErrorCode st = U_ZERO_ERROR;
        UChar sk[64];
        toU("currency/EUR .00", sk);
        UNumberFormatter *f = unumf_openForSkeletonAndLocale(sk, -1, "de", &st);
        UFormattedNumber *r = unumf_openResult(&st);
        unumf_formatDouble(f, -1234567.891, r, &st);
        for (int field = 0; field < UNUM_FIELD_COUNT; field++) {
            UFieldPosition p = { field, 0, 0 };
            printf("field %d:", field);
            while (unumf_resultNextFieldPosition(r, &p, &st)) printf(" %d-%d", p.beginIndex, p.endIndex);
            printf(" (%d-%d) %d\n", p.beginIndex, p.endIndex, status(st));
        }
        UFieldPositionIterator *it = ufieldpositer_open(&st);
        unumf_resultGetAllFieldPositions(r, it, &st);
        int32_t a, b, field;
        printf("all:");
        while ((field = ufieldpositer_next(it, &a, &b)) >= 0) printf(" %d:%d-%d", field, a, b);
        printf("\n");
        ufieldpositer_close(it);
        UFieldPosition bad = { 99, 0, 0 };
        st = U_ZERO_ERROR;
        unumf_resultNextFieldPosition(r, &bad, &st);
        printf("bad field %d\n", status(st));
        st = U_ZERO_ERROR;
        UChar small[4];
        int32_t n = unumf_resultToString(r, small, 4, &st);
        printf("small buffer %d %d\n", n, status(st));
        unumf_closeResult(r);
        unumf_close(f);
    }
}

/* ------------------------------------------------------------------ */
/* unumrf */

static void ranges(const char *locale, const char *skeleton, UNumberRangeCollapse collapse, UNumberRangeIdentityFallback fallback)
{
    UChar sk[512];
    toU(skeleton, sk);
    UErrorCode st = U_ZERO_ERROR;
    UNumberRangeFormatter *f = unumrf_openForSkeletonWithCollapseAndIdentityFallback(sk, -1, collapse, fallback, locale, NULL, &st);
    printf("range %s [%s] %d %d", locale, skeleton, collapse, fallback);
    if (U_FAILURE(st)) {
        printf(" open error %d\n", status(st));
        unumrf_close(f);
        return;
    }
    printf("\n");
    static const double pairs[][2] = { { 1, 5 }, { 1, 1 }, { 0.999, 1.001 }, { -5, 5 }, { 1000, 1000000 }, { 3, 2 }, { 1234.5, 1234.5 } };
    UFormattedNumberRange *r = unumrf_openResult(&st);
    UPluralRules *rules = uplrules_open(locale, &st);
    for (unsigned i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
        st = U_ZERO_ERROR;
        unumrf_formatDoubleRange(f, pairs[i][0], pairs[i][1], r, &st);
        printf("  %g..%g", pairs[i][0], pairs[i][1]);
        if (U_FAILURE(st)) { printf(" error %d\n", status(st)); continue; }
        UErrorCode e = U_ZERO_ERROR;
        const UFormattedValue *v = unumrf_resultAsValue(r, &e);
        int32_t n;
        const UChar *s = ufmtval_getString(v, &n, &e);
        printf(" ");
        put(s, n);
        char a[64], b[64];
        unumrf_resultGetFirstDecimalNumber(r, a, 64, &e);
        unumrf_resultGetSecondDecimalNumber(r, b, 64, &e);
        printf(" identity %d [%s, %s]", (int)unumrf_resultGetIdentityResult(r, &e), a, b);
        positions(v);
        UChar k[16];
        n = uplrules_selectForRange(rules, r, k, 16, &e);
        printf(" plural ");
        put(k, n);
        printf("\n");
    }
    st = U_ZERO_ERROR;
    unumrf_formatDecimalRange(f, "1.5", -1, "2.5E3", -1, r, &st);
    printf("  decimals");
    if (U_FAILURE(st)) printf(" error %d\n", status(st));
    else {
        const UFormattedValue *v = unumrf_resultAsValue(r, &st);
        int32_t n;
        const UChar *s = ufmtval_getString(v, &n, &st);
        printf(" ");
        put(s, n);
        printf("\n");
    }
    uplrules_close(rules);
    unumrf_closeResult(r);
    unumrf_close(f);
}

static void rangeFormatters(void)
{
    const char *locales[] = { "en", "de", "ja", "ru", 0 };
    const char *skeletons[] = { "rounding-mode-half-expand integer-width/*0 .### sign-auto group-auto",
        "rounding-mode-half-expand integer-width/*0 .00 sign-auto group-auto currency/EUR",
        "rounding-mode-half-expand integer-width/*0 .00 sign-auto group-auto currency/EUR unit-width-full-name",
        "rounding-mode-half-expand integer-width/*0 ./@#r sign-auto group-min2 compact-short",
        "rounding-mode-half-expand integer-width/*0 . sign-auto group-auto percent scale/100", 0 };
    for (int l = 0; locales[l]; l++)
        for (int s = 0; skeletons[s]; s++) ranges(locales[l], skeletons[s], UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY);
    for (int c = 0; c < 4; c++)
        for (int f = 0; f < 4; f++) ranges("en", "currency/USD", (UNumberRangeCollapse)c, (UNumberRangeIdentityFallback)f);
    ranges("en", "measure-unit/length-meter", UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_RANGE);
}

/* ------------------------------------------------------------------ */
/* uplrules */

static void pluralRules(void)
{
    const char *locales[] = { "en", "fr", "ar", "ru", "pl", "ja", "cy", "he", "lt", "sl", 0 };
    static const double values[] = { 0, 1, 2, 3, 5, 11, 21, 22, 100, 101, 1000000, 0.5, 1.5, 1e6, -1, 1.0e-5, 2.25 };
    for (int l = 0; locales[l]; l++) {
        for (int type = 0; type < 2; type++) {
            UErrorCode st = U_ZERO_ERROR;
            UPluralRules *rules = uplrules_openForType(locales[l], (UPluralType)type, &st);
            printf("plurals %s %s:", locales[l], type ? "ordinal" : "cardinal");
            UEnumeration *keywords = uplrules_getKeywords(rules, &st);
            const char *k;
            while ((k = uenum_next(keywords, NULL, &st))) printf(" %s", k);
            uenum_close(keywords);
            printf("\n ");
            for (unsigned i = 0; i < sizeof values / sizeof values[0]; i++) {
                UChar kw[16];
                st = U_ZERO_ERROR;
                int32_t n = uplrules_select(rules, values[i], kw, 16, &st);
                char t[16];
                u_austrcpy(t, kw);
                t[n] = 0;
                printf(" %g:%s", values[i], t);
            }
            printf("\n");
            uplrules_close(rules);
        }
    }
    /* The keyword of formatted numbers: "1.0" is not "1". */
    const char *skeletons[] = { "", ".0", "precision-integer", "compact-short", "scientific", 0 };
    static const double formattedValues[] = { 1, 1.5, 1000, 1200000, 0.999 };
    for (int s = 0; skeletons[s]; s++) {
        UErrorCode st = U_ZERO_ERROR;
        UChar sk[64];
        toU(skeletons[s], sk);
        UNumberFormatter *f = unumf_openForSkeletonAndLocale(sk, -1, "fr", &st);
        UPluralRules *rules = uplrules_open("fr", &st);
        UFormattedNumber *r = unumf_openResult(&st);
        printf("formatted fr [%s]:", skeletons[s]);
        for (unsigned i = 0; i < sizeof formattedValues / sizeof formattedValues[0]; i++) {
            unumf_formatDouble(f, formattedValues[i], r, &st);
            UChar kw[16], text[64];
            int32_t n = uplrules_selectFormatted(rules, r, kw, 16, &st);
            int32_t m = unumf_resultToString(r, text, 64, &st);
            printf(" ");
            put(text, m);
            printf("=");
            put(kw, n);
        }
        printf(" %d\n", status(st));
        unumf_closeResult(r);
        uplrules_close(rules);
        unumf_close(f);
    }
    UErrorCode st = U_ZERO_ERROR;
    uplrules_openForType("en", (UPluralType)7, &st);
    printf("bad type %d\n", status(st));
}

/* ------------------------------------------------------------------ */
/* ucurr */

static void currencies(void)
{
    const char *codes[] = { "USD", "EUR", "JPY", "CHF", "BHD", "XXX", "TWD", "ZZZ", 0 };
    const char *locales[] = { "en", "de", "ja", "zh_Hant_TW", "ar", 0 };
    for (int l = 0; locales[l]; l++) {
        for (int c = 0; codes[c]; c++) {
            UChar code[4];
            toU(codes[c], code);
            printf("currency %s %s:", locales[l], codes[c]);
            for (int style = 0; style <= UCURR_VARIANT_SYMBOL_NAME; style++) {
                UErrorCode st = U_ZERO_ERROR;
                UBool choice = 0;
                int32_t n = 0;
                const UChar *name = ucurr_getName(code, locales[l], (UCurrNameStyle)style, &choice, &n, &st);
                printf(" ");
                put(name, n);
                printf("%d", status(st));
            }
            const char *forms[] = { "one", "few", "other", 0 };
            for (int f = 0; forms[f]; f++) {
                UErrorCode st = U_ZERO_ERROR;
                UBool choice = 0;
                int32_t n = 0;
                const UChar *name = ucurr_getPluralName(code, locales[l], &choice, forms[f], &n, &st);
                printf(" %s=", forms[f]);
                put(name, n);
            }
            printf("\n");
        }
    }
    for (int c = 0; codes[c]; c++) {
        UChar code[4];
        toU(codes[c], code);
        UErrorCode st = U_ZERO_ERROR;
        printf("digits %s %d %d %g %g %d %d\n", codes[c], ucurr_getDefaultFractionDigits(code, &st), ucurr_getDefaultFractionDigitsForUsage(code, UCURR_USAGE_CASH, &st),
            ucurr_getRoundingIncrement(code, &st), ucurr_getRoundingIncrementForUsage(code, UCURR_USAGE_CASH, &st), ucurr_getNumericCode(code), status(st));
    }
    const char *regions[] = { "en_US", "de_DE", "ja", "es_419", "en_US@currency=JPY", "sr_Latn_RS", "de_DE_PREEURO", 0 };
    for (int r = 0; regions[r]; r++) {
        UChar b[8];
        UErrorCode st = U_ZERO_ERROR;
        int32_t n = ucurr_forLocale(regions[r], b, 8, &st);
        printf("forLocale %s ", regions[r]);
        put(b, n > 0 ? n : 0);
        printf(" %d\n", status(st));
    }
    /* Currencies of a region over time (1 January 1999, 2002, 2024). */
    static const double dates[] = { 915148800000.0, 1009843200000.0, 1704067200000.0 };
    const char *places[] = { "de_DE", "ja_JP", "sr_RS", "es_CU", 0 };
    for (int p = 0; places[p]; p++) {
        for (int d = 0; d < 3; d++) {
            UErrorCode st = U_ZERO_ERROR;
            int32_t count = ucurr_countCurrencies(places[p], dates[d], &st);
            printf("region %s %d:", places[p], d);
            for (int i = 1; i <= count; i++) {
                UChar b[8];
                int32_t n = ucurr_forLocaleAndDate(places[p], dates[d], i, b, 8, &st);
                printf(" ");
                put(b, n);
            }
            printf(" %d\n", status(st));
        }
    }
    UChar code[4];
    toU("DEM", code);
    UErrorCode st = U_ZERO_ERROR;
    printf("available DEM %d %d\n", ucurr_isAvailable(code, 915148800000.0, 915148800000.0, &st), ucurr_isAvailable(code, 1704067200000.0, 1704067200000.0, &st));
    UEnumeration *all = ucurr_openISOCurrencies(UCURR_COMMON | UCURR_NON_DEPRECATED, &st);
    printf("common currencies %d\n", uenum_count(all, &st) > 100);
    uenum_close(all);
    UEnumeration *values = ucurr_getKeywordValuesForLocale("currency", "de_CH", 1, &st);
    printf("keyword values de_CH:");
    const char *v;
    while ((v = uenum_next(values, NULL, &st))) printf(" %s", v);
    printf(" %d\n", status(st));
    uenum_close(values);
}

/* ------------------------------------------------------------------ */
/* unumsys */

static void numberingSystems(void)
{
    const char *locales[] = { "en", "ar", "ar_EG", "fa", "hi_IN@numbers=native", "zh@numbers=traditional", "ja@numbers=finance", "th@numbers=thai", "en@numbers=xyz", 0 };
    for (int l = 0; locales[l]; l++) {
        UErrorCode st = U_ZERO_ERROR;
        UNumberingSystem *ns = unumsys_open(locales[l], &st);
        printf("numbering %s:", locales[l]);
        if (U_FAILURE(st)) { printf(" error %d\n", status(st)); continue; }
        UChar d[256];
        int32_t n = unumsys_getDescription(ns, d, 256, &st);
        printf(" %s radix %d algorithmic %d ", unumsys_getName(ns), unumsys_getRadix(ns), unumsys_isAlgorithmic(ns));
        put(d, n);
        printf("\n");
        unumsys_close(ns);
    }
    UErrorCode st = U_ZERO_ERROR;
    UNumberingSystem *ns = unumsys_openByName("roman", &st);
    printf("roman algorithmic %d %d\n", unumsys_isAlgorithmic(ns), status(st));
    unumsys_close(ns);
    st = U_ZERO_ERROR;
    unumsys_openByName("nonesuch", &st);
    printf("nonesuch %d\n", status(st));
    st = U_ZERO_ERROR;
    UEnumeration *names = unumsys_openAvailableNames(&st);
    int digital = 0;
    const char *name;
    while ((name = uenum_next(names, NULL, &st))) {
        UErrorCode e = U_ZERO_ERROR;
        UNumberingSystem *one = unumsys_openByName(name, &e);
        if (!unumsys_isAlgorithmic(one)) digital++;
        unumsys_close(one);
    }
    printf("available names: %d digital\n", digital > 60);
    uenum_close(names);
}

/* ------------------------------------------------------------------ */
/* unum */

static void unumFormat(UNumberFormat *f, double x)
{
    UChar b[256];
    UErrorCode st = U_ZERO_ERROR;
    UFieldPosition pos = { UNUM_INTEGER_FIELD, 0, 0 };
    int32_t n = unum_formatDouble(f, x, b, 256, &pos, &st);
    printf("  %.17g ->", x);
    if (U_FAILURE(st)) { printf(" error %d\n", status(st)); return; }
    printf(" ");
    put(b, n);
    printf(" integer %d-%d", pos.beginIndex, pos.endIndex);
    UFieldPositionIterator *it = ufieldpositer_open(&st);
    unum_formatDoubleForFields(f, x, b, 256, it, &st);
    int32_t a, z, field;
    while ((field = ufieldpositer_next(it, &a, &z)) >= 0) printf(" %d:%d-%d", field, a, z);
    ufieldpositer_close(it);
    printf("\n");
}

static void unumState(UNumberFormat *f)
{
    static const int attributes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0x1000, 0x1001, 0x1002, 0x1003, 0x1004 };
    printf("  attributes");
    for (unsigned i = 0; i < sizeof attributes / sizeof attributes[0]; i++) printf(" %d", unum_getAttribute(f, (UNumberFormatAttribute)attributes[i]));
    printf(" increment %g\n", unum_getDoubleAttribute(f, UNUM_ROUNDING_INCREMENT));
    UChar b[256];
    UErrorCode st = U_ZERO_ERROR;
    int32_t n = unum_toPattern(f, 0, b, 256, &st);
    printf("  pattern ");
    put(b, n);
    st = U_ZERO_ERROR;
    n = unum_toPattern(f, 1, b, 256, &st);
    printf(" localized ");
    put(b, n);
    printf("\n  text");
    for (int t = 0; t <= UNUM_CURRENCY_CODE; t++) {
        st = U_ZERO_ERROR;
        n = unum_getTextAttribute(f, (UNumberFormatTextAttribute)t, b, 256, &st);
        printf(" ");
        put(b, n);
    }
    printf("\n  symbols");
    for (int s = 0; s < UNUM_FORMAT_SYMBOL_COUNT; s++) {
        st = U_ZERO_ERROR;
        n = unum_getSymbol(f, (UNumberFormatSymbol)s, b, 256, &st);
        printf(" ");
        put(b, n);
    }
    printf("\n");
}

static void unumParse(UNumberFormat *f, const char *text)
{
    UChar u[256];
    toU(text, u);
    UErrorCode st = U_ZERO_ERROR;
    int32_t at = 0;
    double d = unum_parseDouble(f, u, -1, &at, &st);
    printf("  parse \"%s\": %.17g at %d %d", text, d, at, status(st));
    st = U_ZERO_ERROR;
    at = 0;
    int32_t l = unum_parse(f, u, -1, &at, &st);
    printf(" long %d %d", l, status(st));
    st = U_ZERO_ERROR;
    at = 0;
    long long l64 = unum_parseInt64(f, u, -1, &at, &st);
    printf(" int64 %lld %d", l64, status(st));
    char dec[64];
    st = U_ZERO_ERROR;
    at = 0;
    unum_parseDecimal(f, u, -1, &at, dec, 64, &st);
    printf(" decimal %s", U_FAILURE(st) ? "-" : dec);
    UChar cur[4] = { 0 };
    st = U_ZERO_ERROR;
    at = 0;
    d = unum_parseDoubleCurrency(f, u, -1, &at, cur, &st);
    char c8[4];
    u_austrcpy(c8, cur);
    printf(" currency %.17g %s at %d %d", d, c8, at, status(st));
    st = U_ZERO_ERROR;
    at = 0;
    UFormattable *v = unum_parseToUFormattable(f, NULL, u, -1, &at, &st);
    if (v) {
        int32_t n;
        printf(" formattable %d %s", ufmt_getType(v, &st), ufmt_getDecNumChars(v, &n, &st));
        ufmt_close(v);
    }
    printf("\n");
}

static const double unumValues[] = { 0, -0.0, 1, -1, 1234.5678, -1234.5678, 0.001234, 1234567.891, 1e20, 0.5, 2.5 };

static void unumStyle(int style, const char *locale, const char *pattern)
{
    UChar p[256];
    if (pattern) toU(pattern, p);
    UErrorCode st = U_ZERO_ERROR;
    UNumberFormat *f = unum_open((UNumberFormatStyle)style, pattern ? p : NULL, -1, locale, NULL, &st);
    printf("unum %d %s %s", style, locale, pattern ? pattern : "-");
    if (U_FAILURE(st)) { printf(" open error %d\n", status(st)); return; }
    printf("\n");
    for (unsigned i = 0; i < sizeof unumValues / sizeof unumValues[0]; i++) unumFormat(f, unumValues[i]);
    UChar b[256];
    st = U_ZERO_ERROR;
    int32_t n = unum_formatInt64(f, -9223372036854775807LL - 1, b, 256, NULL, &st);
    printf("  int64 min ");
    put(b, n);
    st = U_ZERO_ERROR;
    n = unum_formatDecimal(f, "-123456789012345678901234.5678", -1, b, 256, NULL, &st);
    printf(" decimal ");
    put(b, n);
    printf("\n");
    unumState(f);
    const char *texts[] = { "1,234.5", "-1,234.5", "12%", "$12.50", "1.234,5", "1E3", "abc", "(5)", "-0", "99999999999", "123456.789", 0 };
    for (int t = 0; texts[t]; t++) unumParse(f, texts[t]);
    unum_close(f);
}

/* Intl.RelativeTimeFormat's number format. */
static UNumberFormat *relativeTimeFormat(const char *locale)
{
    UErrorCode st = U_ZERO_ERROR;
    UNumberFormat *f = unum_open(UNUM_DECIMAL, NULL, 0, locale, NULL, &st);
    unum_setAttribute(f, UNUM_MIN_INTEGER_DIGITS, 1);
    unum_setAttribute(f, UNUM_MIN_FRACTION_DIGITS, 0);
    unum_setAttribute(f, UNUM_MAX_FRACTION_DIGITS, 3);
    unum_setAttribute(f, UNUM_GROUPING_USED, 1);
    unum_setAttribute(f, UNUM_GROUPING_SIZE, -2);
    unum_setAttribute(f, UNUM_SECONDARY_GROUPING_SIZE, -2);
    unum_setAttribute(f, UNUM_MINIMUM_GROUPING_DIGITS, -2);
    return f;
}

static void unumFormats(void)
{
    const char *locales[] = { "en_US", "de_DE", "fr", "ja", "en_IN", 0 };
    const int styles[] = { UNUM_DECIMAL, UNUM_CURRENCY, UNUM_PERCENT, UNUM_SCIENTIFIC, UNUM_CURRENCY_ISO, UNUM_CURRENCY_PLURAL, UNUM_CURRENCY_ACCOUNTING,
        UNUM_CASH_CURRENCY, UNUM_DECIMAL_COMPACT_SHORT, UNUM_DECIMAL_COMPACT_LONG, UNUM_CURRENCY_STANDARD, -1 };
    for (int l = 0; locales[l]; l++)
        for (int s = 0; styles[s] >= 0; s++) unumStyle(styles[s], locales[l], NULL);
    const char *patterns[] = { "#,##0.###", "0.00", "#,##0.05", "##0.###E0", "00.###E+00", "@@@", "@@##", "#,##,##0.00", "'#'#", "* #,##0", "*x####0.00;(#)",
        "\xc2\xa4#,##0.00", "#,##0.00 \xc2\xa4\xc2\xa4", "0%", "#\xe2\x80\xb0", "#.##;#.##-", ".00", "0.", "a'b''c'0", "#E0", "", "0.0.0", "'abc", "#*", 0 };
    for (int p = 0; patterns[p]; p++) unumStyle(UNUM_PATTERN_DECIMAL, "en", patterns[p]);
    unumStyle(UNUM_NUMBERING_SYSTEM, "en@numbers=latn", NULL);

    UNumberFormat *f = relativeTimeFormat("en");
    printf("relative time format\n");
    for (unsigned i = 0; i < sizeof unumValues / sizeof unumValues[0]; i++) unumFormat(f, unumValues[i]);
    unumState(f);
    unum_setAttribute(f, UNUM_MAX_INTEGER_DIGITS, 2);
    unumFormat(f, 1234.5);
    unum_setAttribute(f, UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS, 1);
    unumFormat(f, 1234.5);
    unumFormat(f, 12.5);
    unum_setAttribute(f, UNUM_MAX_INTEGER_DIGITS, 20);
    unum_setAttribute(f, UNUM_SIGNIFICANT_DIGITS_USED, 1);
    unumFormat(f, 1234.5678);
    unum_setAttribute(f, UNUM_MAX_SIGNIFICANT_DIGITS, 3);
    unumFormat(f, 1234.5678);
    unum_setAttribute(f, UNUM_SIGNIFICANT_DIGITS_USED, 0);
    unum_setAttribute(f, UNUM_MULTIPLIER, 1000);
    unumFormat(f, 1.5);
    unum_setAttribute(f, UNUM_MULTIPLIER, 7);
    unumFormat(f, 1.5);
    unumParse(f, "21");
    unum_setAttribute(f, UNUM_MULTIPLIER, 1);
    unum_setAttribute(f, UNUM_ROUNDING_MODE, UNUM_ROUND_UP);
    unumFormat(f, 1.0001);
    unum_setAttribute(f, UNUM_ROUNDING_MODE, UNUM_ROUND_UNNECESSARY);
    unumFormat(f, 1.00001);
    unum_setAttribute(f, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFEVEN);
    unum_setDoubleAttribute(f, UNUM_ROUNDING_INCREMENT, 0.25);
    unumFormat(f, 1.3);
    unum_setDoubleAttribute(f, UNUM_ROUNDING_INCREMENT, 0);
    unum_setAttribute(f, UNUM_FORMAT_WIDTH, 10);
    unumFormat(f, 1.5);
    unum_setAttribute(f, UNUM_PADDING_POSITION, UNUM_PAD_AFTER_SUFFIX);
    unumFormat(f, -1.5);
    UChar t[64];
    UErrorCode st = U_ZERO_ERROR;
    toU("_", t);
    unum_setTextAttribute(f, UNUM_PADDING_CHARACTER, t, -1, &st);
    unumFormat(f, 2.5);
    unum_setAttribute(f, UNUM_FORMAT_WIDTH, -1);
    unum_setAttribute(f, UNUM_SIGN_ALWAYS_SHOWN, 1);
    unumFormat(f, 3);
    unum_setAttribute(f, UNUM_SIGN_ALWAYS_SHOWN, 0);
    unum_setAttribute(f, UNUM_DECIMAL_ALWAYS_SHOWN, 1);
    unumFormat(f, 3);
    unum_setAttribute(f, UNUM_DECIMAL_ALWAYS_SHOWN, 0);
    unum_setAttribute(f, UNUM_GROUPING_SIZE, 2);
    unumFormat(f, 1234567);
    unum_setAttribute(f, UNUM_SECONDARY_GROUPING_SIZE, 3);
    unumFormat(f, 1234567);
    unum_setAttribute(f, UNUM_MINIMUM_GROUPING_DIGITS, 2);
    unumFormat(f, 1234);
    unumState(f);
    toU("+", t);
    unum_setTextAttribute(f, UNUM_POSITIVE_PREFIX, t, -1, &st);
    toU("neg-", t);
    unum_setTextAttribute(f, UNUM_NEGATIVE_PREFIX, t, -1, &st);
    toU("%", t);
    unum_setTextAttribute(f, UNUM_NEGATIVE_SUFFIX, t, -1, &st);
    unumFormat(f, 5);
    unumFormat(f, -5);
    unumState(f);
    unumParse(f, "neg-5%");
    toU("EUR", t);
    unum_setTextAttribute(f, UNUM_CURRENCY_CODE, t, -1, &st);
    printf("  currency code %d\n", status(st));
    unumFormat(f, 5.5);
    toU("EU", t);
    st = U_ZERO_ERROR;
    unum_setTextAttribute(f, UNUM_CURRENCY_CODE, t, -1, &st);
    printf("  bad currency code %d\n", status(st));
    st = U_ZERO_ERROR;
    unum_setTextAttribute(f, UNUM_DEFAULT_RULESET, t, -1, &st);
    printf("  ruleset %d\n", status(st));
    st = U_ZERO_ERROR;
    toU(",", t);
    unum_setSymbol(f, UNUM_DECIMAL_SEPARATOR_SYMBOL, t, -1, &st);
    toU(".", t);
    unum_setSymbol(f, UNUM_GROUPING_SEPARATOR_SYMBOL, t, -1, &st);
    toU("\xd9\xa0", t);
    unum_setSymbol(f, UNUM_ZERO_DIGIT_SYMBOL, t, -1, &st);
    unumFormat(f, 1234567.25);
    unumState(f);
    st = U_ZERO_ERROR;
    UNumberFormat *c = unum_clone(f, &st);
    unum_close(f);
    unumFormat(c, 9876.5);
    toU("#,##0.00;(#,##0.00)", t);
    unum_applyPattern(c, 0, t, -1, NULL, &st);
    printf("  apply %d\n", status(st));
    unumFormat(c, -1234.5);
    unumState(c);
    toU("#.##0,00", t);
    unum_applyPattern(c, 1, t, -1, NULL, &st);
    printf("  apply localized %d\n", status(st));
    unumFormat(c, -1234.5);
    toU("#,##0.0.#", t);
    st = U_ZERO_ERROR;
    unum_applyPattern(c, 0, t, -1, NULL, &st);
    printf("  apply bad %d\n", status(st));
    st = U_ZERO_ERROR;
    unum_setContext(c, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &st);
    printf("  context %d %d", status(st), unum_getContext(c, UDISPCTX_TYPE_CAPITALIZATION, &st));
    st = U_ZERO_ERROR;
    unum_setContext(c, UDISPCTX_STANDARD_NAMES, &st);
    printf(" %d\n", status(st));
    printf("  has %d %d\n", unum_hasAttribute(c, UNUM_GROUPING_USED), unum_hasAttribute(c, (UNumberFormatAttribute)999));
    unum_close(c);

    st = U_ZERO_ERROR;
    f = unum_open(UNUM_CURRENCY, NULL, 0, "en_US", NULL, &st);
    UChar b[256], code[4];
    const char *codes[] = { "JPY", "EUR", "USD", 0 };
    for (int i = 0; codes[i]; i++) {
        toU(codes[i], code);
        st = U_ZERO_ERROR;
        int32_t n = unum_formatDoubleCurrency(f, -1234.567, code, b, 256, NULL, &st);
        printf("double currency %s ", codes[i]);
        put(b, n);
        printf(" %d\n", status(st));
    }
    toU("US$", t);
    st = U_ZERO_ERROR;
    unum_setSymbol(f, UNUM_CURRENCY_SYMBOL, t, -1, &st);
    unumFormat(f, 3.5);
    UFormattable *v = ufmt_open(&st);
    int32_t len;
    printf("formattable %d %d %d", ufmt_getType(v, &st), ufmt_isNumeric(v), ufmt_getLong(v, &st));
    st = U_ZERO_ERROR;
    ufmt_getDate(v, &st);
    printf(" date %d", status(st));
    st = U_ZERO_ERROR;
    ufmt_getUChars(v, &len, &st);
    printf(" string %d", status(st));
    st = U_ZERO_ERROR;
    printf(" decimal %s %d\n", ufmt_getDecNumChars(v, &len, &st), status(st));
    toU("$1,234,567,890,123.50", t);
    int32_t at = 0;
    st = U_ZERO_ERROR;
    unum_parseToUFormattable(f, v, t, -1, &at, &st);
    printf("parsed %d %.17g %d", ufmt_getType(v, &st), ufmt_getDouble(v, &st), ufmt_getLong(v, &st));
    printf(" %d", status(st));
    st = U_ZERO_ERROR;
    long long i64 = ufmt_getInt64(v, &st);
    printf(" %lld %s\n", i64, ufmt_getDecNumChars(v, &len, &st));
    st = U_ZERO_ERROR;
    int32_t n = unum_formatUFormattable(f, v, b, 256, NULL, &st);
    printf("format formattable ");
    put(b, n);
    printf(" %d\n", status(st));
    ufmt_close(v);
    unum_close(f);
}

/* ------------------------------------------------------------------ */
/* ureldatefmt */

static void relativeResult(URelativeDateTimeFormatter *r, double x, URelativeDateTimeUnit unit, int numeric, UFormattedRelativeDateTime *result)
{
    UErrorCode st = U_ZERO_ERROR;
    if (numeric) ureldatefmt_formatNumericToResult(r, x, unit, result, &st);
    else ureldatefmt_formatToResult(r, x, unit, result, &st);
    if (U_FAILURE(st)) { printf(" error %d", status(st)); return; }
    const UFormattedValue *v = ureldatefmt_resultAsValue(result, &st);
    int32_t n;
    const UChar *s = ufmtval_getString(v, &n, &st);
    printf(" ");
    put(s, n);
    positions(v);
}

static void relative(const char *locale, UDateRelativeDateTimeFormatterStyle style, UDisplayContext context, int jsc)
{
    UErrorCode st = U_ZERO_ERROR;
    URelativeDateTimeFormatter *r = ureldatefmt_open(locale, jsc ? relativeTimeFormat(locale) : NULL, style, context, &st);
    printf("relative %s style %d context %d\n", locale, style, context);
    if (U_FAILURE(st)) { printf("  open error %d\n", status(st)); return; }
    static const double offsets[] = { -2, -1, -0.0, 0, 1, 2, 3, 1.005, 0.5, 21, -1234.5678 };
    UFormattedRelativeDateTime *result = ureldatefmt_openResult(&st);
    for (int unit = 0; unit < UDAT_REL_UNIT_COUNT; unit++) {
        for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
            UChar b[256];
            st = U_ZERO_ERROR;
            int32_t n = ureldatefmt_formatNumeric(r, offsets[i], (URelativeDateTimeUnit)unit, b, 256, &st);
            printf("  %d %g:", unit, offsets[i]);
            if (U_FAILURE(st)) printf(" error %d", status(st));
            else { printf(" "); put(b, n); }
            st = U_ZERO_ERROR;
            n = ureldatefmt_format(r, offsets[i], (URelativeDateTimeUnit)unit, b, 256, &st);
            if (U_FAILURE(st)) printf(" error %d", status(st));
            else { printf(" "); put(b, n); }
            relativeResult(r, offsets[i], (URelativeDateTimeUnit)unit, 1, result);
            relativeResult(r, offsets[i], (URelativeDateTimeUnit)unit, 0, result);
            printf("\n");
        }
    }
    UChar d[32], t[32], b[128];
    toU("yesterday", d);
    toU("3:45 PM", t);
    st = U_ZERO_ERROR;
    int32_t n = ureldatefmt_combineDateAndTime(r, d, -1, t, -1, b, 128, &st);
    printf("  combined ");
    put(b, n);
    printf(" %d\n", status(st));
    ureldatefmt_closeResult(result);
    ureldatefmt_close(r);
}

static void relativeFormatters(void)
{
    const char *locales[] = { "en", "de", "es", "ru", "ja", 0 };
    for (int l = 0; locales[l]; l++)
        for (int s = 0; s < 3; s++) relative(locales[l], (UDateRelativeDateTimeFormatterStyle)s, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, 1);
    relative("en", UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, 0);
    relative("en", UDAT_STYLE_SHORT, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, 0);
    UErrorCode st = U_ZERO_ERROR;
    ureldatefmt_open("en", NULL, (UDateRelativeDateTimeFormatterStyle)3, UDISPCTX_CAPITALIZATION_NONE, &st);
    printf("bad style %d\n", status(st));
    st = U_ZERO_ERROR;
    ureldatefmt_open("en", NULL, UDAT_STYLE_LONG, UDISPCTX_STANDARD_NAMES, &st);
    printf("bad context %d\n", status(st));
    st = U_ZERO_ERROR;
    URelativeDateTimeFormatter *r = ureldatefmt_open("en", NULL, UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_NONE, &st);
    UChar b[8];
    st = U_ZERO_ERROR;
    int32_t n = ureldatefmt_formatNumeric(r, 5, UDAT_REL_UNIT_DAY, b, 8, &st);
    printf("small buffer %d %d\n", n, status(st));
    st = U_ZERO_ERROR;
    n = ureldatefmt_formatNumeric(r, 5, UDAT_REL_UNIT_DAY, NULL, 0, &st);
    printf("preflight %d %d\n", n, status(st));
    st = U_ZERO_ERROR;
    n = ureldatefmt_formatNumeric(r, 5, UDAT_REL_UNIT_COUNT, b, 8, &st);
    printf("bad unit %d %d\n", n, status(st));
    ureldatefmt_close(r);
}

int main(void)
{
    formatters();
    rangeFormatters();
    pluralRules();
    currencies();
    numberingSystems();
    unumFormats();
    relativeFormatters();
    /* Rule-based formats need data cldr.dat does not have. */
    unumStyle(UNUM_SPELLOUT, "en", NULL);
    return 0;
}
