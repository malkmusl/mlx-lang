/* Measure units: number formatters (unumf_*) of unit skeletons, as
   JavaScriptCore's Intl.NumberFormat (style "unit", every sanctioned
   unit and their X-per-Y compounds in each unit display), its
   Intl.DurationFormat (measure-unit/duration-...) and ICU's documented
   stems make them: measure-unit/TYPE-SUBTYPE, per-measure-unit/,
   unit/IDENTIFIER (built-in units, compounds of products, powers, SI
   and binary prefixes, "-per-"), percent and permille as units, the
   unit widths, with compact and scientific notation, signs and number
   ranges (unumrf_*). Prints a line per call, the text and the fields of
   each result; units.expected is what it must print. Statuses are
   printed as numbers, ICU's warnings as 0. A formatter that fails is
   not asked again (ICU's own fails otherwise from its third call, when
   it compiles itself). */
#include <stdio.h>
#include <string.h>
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>
#include <unicode/uformattedvalue.h>
#include <unicode/ustring.h>

static int status(UErrorCode s) { return s < 0 ? 0 : (int)s; }

static void toU(const char *s, UChar *d)
{
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

static UNumberFormatter *openFormatter(const char *locale, const char *skeleton)
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
        return 0;
    }
    return f;
}

/* The text of each value on one line. */
static void brief(const char *locale, const char *skeleton, const double *values, int count)
{
    UNumberFormatter *f = openFormatter(locale, skeleton);
    if (!f) return;
    UErrorCode st = U_ZERO_ERROR;
    UFormattedNumber *r = unumf_openResult(&st);
    for (int i = 0; i < count; i++) {
        st = U_ZERO_ERROR;
        unumf_formatDouble(f, values[i], r, &st);
        if (U_FAILURE(st)) {
            printf(" error %d", status(st));
            break;
        }
        UChar b[512];
        UErrorCode e = U_ZERO_ERROR;
        int32_t n = unumf_resultToString(r, b, 512, &e);
        printf(" ");
        put(b, n);
    }
    printf("\n");
    unumf_closeResult(r);
    unumf_close(f);
}

/* Each value on a line of its own, with its fields. */
static void detailed(const char *locale, const char *skeleton, const double *values, int count)
{
    UNumberFormatter *f = openFormatter(locale, skeleton);
    if (!f) return;
    printf("\n");
    UErrorCode st = U_ZERO_ERROR;
    UFormattedNumber *r = unumf_openResult(&st);
    for (int i = 0; i < count; i++) {
        st = U_ZERO_ERROR;
        unumf_formatDouble(f, values[i], r, &st);
        printf("  %.17g", values[i]);
        if (U_FAILURE(st)) {
            printf(" error %d\n", status(st));
            break;
        }
        UChar b[512];
        UErrorCode e = U_ZERO_ERROR;
        int32_t n = unumf_resultToString(r, b, 512, &e);
        printf(" ");
        put(b, n);
        positions(unumf_resultAsValue(r, &e));
        printf("\n");
    }
    unumf_closeResult(r);
    unumf_close(f);
}

/* JavaScriptCore's sanctioned simple units (ECMA-402's table). */
static const char *simpleUnits[] = {
    "acre", "bit", "byte", "celsius", "centimeter", "day", "degree", "fahrenheit", "fluid-ounce", "foot",
    "gallon", "gigabit", "gigabyte", "gram", "hectare", "hour", "inch", "kilobit", "kilobyte", "kilogram",
    "kilometer", "liter", "megabit", "megabyte", "meter", "microsecond", "mile", "mile-scandinavian",
    "milliliter", "millimeter", "millisecond", "minute", "month", "nanosecond", "ounce", "percent",
    "petabyte", "pound", "second", "stone", "terabit", "terabyte", "week", "yard", "year", 0
};

static const char *widths[] = { "unit-width-short", "unit-width-narrow", "unit-width-full-name", 0 };

/* X-per-Y as JavaScriptCore writes them: built-in compounds and others. */
static const char *perUnits[] = {
    "kilometer-per-hour", "meter-per-second", "mile-per-hour", "liter-per-kilometer", "mile-per-gallon",
    "kilobyte-per-second", "megabit-per-second", "gram-per-liter", "liter-per-second", "meter-per-hour",
    "kilogram-per-day", "byte-per-millisecond", "percent-per-year", "celsius-per-minute", "foot-per-second",
    "gallon-per-mile", 0
};

static const double few[] = { 1, 2, 5, 1.5 };
static const double many[] = { 0, 1, 2, 3, 5, 11, 21, 101, 1.5, -1, 1234567.5, 0.25 };

/* ICU's stems for units, and what goes with them. */
static const char *skeletons[] = {
    "measure-unit/length-meter",
    "measure-unit/length-meter unit-width-full-name",
    "measure-unit/length-meter unit-width-narrow",
    "measure-unit/length-meter unit-width-iso-code",
    "measure-unit/length-meter unit-width-formal",
    "measure-unit/length-meter unit-width-variant",
    "measure-unit/length-meter unit-width-hidden",
    "measure-unit/length-meter per-measure-unit/duration-second",
    "measure-unit/length-meter per-measure-unit/duration-second unit-width-full-name",
    "measure-unit/length-kilometer per-measure-unit/duration-hour unit-width-full-name",
    "measure-unit/mass-kilogram per-measure-unit/duration-week unit-width-full-name",
    "measure-unit/mass-kilogram per-measure-unit/length-meter",
    "per-measure-unit/duration-second",
    "measure-unit/concentr-percent",
    "measure-unit/concentr-percent unit-width-full-name",
    "measure-unit/concentr-permille",
    "measure-unit/concentr-permille unit-width-full-name",
    "measure-unit/none-percent",
    "measure-unit/none-base",
    "percent",
    "percent unit-width-full-name",
    "permille unit-width-full-name",
    "permille unit-width-narrow",
    "percent compact-short",
    "unit/percent",
    "unit/percent unit-width-full-name",
    "unit/permille unit-width-full-name",
    "unit/kilometer-per-hour",
    "unit/kilometer-per-hour unit-width-full-name",
    "unit/kilowatt-hour unit-width-full-name",
    "unit/square-meter unit-width-full-name",
    "unit/square-second unit-width-full-name",
    "unit/cubic-foot",
    "unit/meter-second unit-width-full-name",
    "unit/meter-second",
    "unit/kilosecond unit-width-full-name",
    "unit/kilosecond",
    "unit/millimeter-per-square-second unit-width-full-name",
    "unit/kibibyte unit-width-full-name",
    "unit/per-second unit-width-full-name",
    "unit/per-second",
    "unit/newton-meter",
    "unit/pow4-meter",
    "unit/permillion unit-width-full-name",
    "measure-unit/concentr-permillion",
    "measure-unit/duration-year-person unit-width-full-name",
    "measure-unit/mass-ton",
    "measure-unit/temperature-celsius unit-width-full-name",
    "measure-unit/temperature-celsius compact-short",
    "measure-unit/digital-byte compact-long unit-width-full-name",
    "measure-unit/length-meter scientific",
    "measure-unit/length-meter sign-always",
    "measure-unit/length-meter sign-accounting",
    "measure-unit/length-meter precision-integer",
    "measure-unit/length-meter .00",
    "measure-unit/length-furlong unit-width-full-name",
    0
};

static const char *locales[] = { "en", "de", "fr", "ar", "ja", "ru", "pl", "es", 0 };

/* The skeletons of JavaScriptCore's Intl.DurationFormat. */
static const char *durationUnits[] = { "year", "month", "week", "day", "hour", "minute", "second", "millisecond",
    "microsecond", "nanosecond", 0 };

static void durations(const char *locale)
{
    char skeleton[256];
    for (int u = 0; durationUnits[u]; u++) {
        for (int w = 0; widths[w]; w++) {
            snprintf(skeleton, sizeof skeleton, "rounding-mode-half-up integer-width/*0 measure-unit/duration-%s %s", durationUnits[u], widths[w]);
            brief(locale, skeleton, few, 4);
        }
    }
    /* Seconds with their fraction, as a decimal. */
    static const char *secondSkeletons[] = {
        "rounding-mode-down .######### integer-width/*0 measure-unit/duration-second unit-width-short",
        "rounding-mode-down .000 integer-width/*0 measure-unit/duration-second unit-width-full-name",
        "rounding-mode-down .######### integer-width/*0 measure-unit/duration-millisecond unit-width-narrow",
        "rounding-mode-half-up integer-width/*00 group-off",
        0
    };
    static const char *decimals[] = { "1.5", "-12.000000001", "0", "86400.123456789", 0 };
    for (int s = 0; secondSkeletons[s]; s++) {
        UNumberFormatter *f = openFormatter(locale, secondSkeletons[s]);
        if (!f) continue;
        UErrorCode st = U_ZERO_ERROR;
        UFormattedNumber *r = unumf_openResult(&st);
        for (int i = 0; decimals[i]; i++) {
            st = U_ZERO_ERROR;
            unumf_formatDecimal(f, decimals[i], -1, r, &st);
            if (U_FAILURE(st)) {
                printf(" error %d", status(st));
                continue;
            }
            UChar b[512];
            UErrorCode e = U_ZERO_ERROR;
            int32_t n = unumf_resultToString(r, b, 512, &e);
            printf(" ");
            put(b, n);
            positions(unumf_resultAsValue(r, &e));
        }
        printf("\n");
        unumf_closeResult(r);
        unumf_close(f);
    }
}

static void ranges(const char *locale, const char *skeleton, double a, double b)
{
    UChar sk[512];
    toU(skeleton, sk);
    UErrorCode st = U_ZERO_ERROR;
    UParseError pe;
    UNumberRangeFormatter *f = unumrf_openForSkeletonWithCollapseAndIdentityFallback(sk, -1,
        UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY, locale, &pe, &st);
    printf("%s range [%s] %g %g", locale, skeleton, a, b);
    if (U_FAILURE(st)) {
        printf(" open error %d\n", status(st));
        unumrf_close(f);
        return;
    }
    UFormattedNumberRange *r = unumrf_openResult(&st);
    unumrf_formatDoubleRange(f, a, b, r, &st);
    if (U_FAILURE(st)) {
        printf(" error %d\n", status(st));
    } else {
        UErrorCode e = U_ZERO_ERROR;
        const UFormattedValue *v = unumrf_resultAsValue(r, &e);
        int32_t n;
        const UChar *s = ufmtval_getString(v, &n, &e);
        printf(" ");
        put(s, n);
        positions(v);
        printf("\n");
    }
    unumrf_closeResult(r);
    unumrf_close(f);
}

int main(void)
{
    char skeleton[256];
    static const char *briefLocales[] = { "en", "de", "ja", "ru", "ar", 0 };
    for (int l = 0; briefLocales[l]; l++) {
        for (int u = 0; simpleUnits[u]; u++) {
            for (int w = 0; widths[w]; w++) {
                snprintf(skeleton, sizeof skeleton, "unit/%s %s rounding-mode-half-up integer-width/*0 .### sign-auto group-auto", simpleUnits[u], widths[w]);
                brief(briefLocales[l], skeleton, few, 4);
            }
        }
        for (int u = 0; perUnits[u]; u++) {
            for (int w = 0; widths[w]; w++) {
                snprintf(skeleton, sizeof skeleton, "unit/%s %s rounding-mode-half-up integer-width/*0 .### sign-auto group-auto", perUnits[u], widths[w]);
                brief(briefLocales[l], skeleton, few, 4);
            }
        }
    }
    for (int l = 0; locales[l]; l++) {
        for (int s = 0; skeletons[s]; s++) detailed(locales[l], skeletons[s], many, sizeof many / sizeof many[0]);
        for (int u = 0; perUnits[u]; u++) {
            snprintf(skeleton, sizeof skeleton, "unit/%s unit-width-full-name", perUnits[u]);
            brief(locales[l], skeleton, many, sizeof many / sizeof many[0]);
        }
    }
    for (int l = 0; locales[l]; l++) durations(locales[l]);
    for (int l = 0; locales[l]; l++) {
        ranges(locales[l], "measure-unit/length-kilometer", 3, 5);
        ranges(locales[l], "measure-unit/length-kilometer unit-width-full-name", 1, 2);
        ranges(locales[l], "measure-unit/length-kilometer unit-width-full-name", 3, 3);
        ranges(locales[l], "unit/kilometer-per-hour unit-width-narrow", 50, 120);
        ranges(locales[l], "measure-unit/temperature-celsius", -5, 25);
        ranges(locales[l], "measure-unit/duration-hour unit-width-full-name", 1.5, 2);
    }
    return 0;
}
