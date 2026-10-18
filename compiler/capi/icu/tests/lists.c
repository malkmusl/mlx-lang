/* ulistfmt: list formatters as JavaScriptCore's Intl.ListFormat and
   Intl.DurationFormat use them (ulistfmt_openForType, ulistfmt_format,
   ulistfmt_formatStringsToResult and the element fields of the result),
   CLDR's list patterns of each type and width, ICU's special cases
   (Spanish "e" and "u", Hebrew "ו-"), the field spans and the errors.
   Prints a line per call; lists.expected is what it must print. */
#include <stdio.h>
#include <string.h>
#include <unicode/ulistformatter.h>
#include <unicode/uformattedvalue.h>
#include <unicode/ustring.h>

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

/* The strings of ITEMS (UTF-8, NULL-ended) as UTF-16. */
static int32_t prepare(const char **items, UChar storage[][64], const UChar **strings, int32_t *lengths)
{
    int32_t count = 0;
    while (items[count]) {
        UErrorCode e = U_ZERO_ERROR;
        u_strFromUTF8(storage[count], 64, &lengths[count], items[count], -1, &e);
        strings[count] = storage[count];
        count++;
    }
    return count;
}

static void format(const char *locale, UListFormatterType type, UListFormatterWidth width, const char **items)
{
    UErrorCode status = U_ZERO_ERROR;
    UListFormatter *f = ulistfmt_openForType(locale, type, width, &status);
    UChar storage[8][64];
    const UChar *strings[8];
    int32_t lengths[8];
    int32_t count = prepare(items, storage, strings, lengths);
    UChar result[256];
    int32_t length = ulistfmt_format(f, strings, lengths, count, result, 256, &status);
    char out[512];
    utf8(result, length, out, sizeof out);
    printf("%s %d/%d [%d]: \"%s\" %d %s\n", locale, (int)type, (int)width, count, U_SUCCESS(status) ? out : "", length, statusName(status));
    ulistfmt_close(f);
}

static const char *categoryName(int32_t category)
{
    if (category == UFIELD_CATEGORY_LIST) return "list";
    if (category == UFIELD_CATEGORY_LIST_SPAN) return "span";
    return "?";
}

/* The result and every span of it, then the element fields alone (as
   JavaScriptCore reads them), then the spans alone. */
static void fields(const char *locale, UListFormatterType type, UListFormatterWidth width, const char **items, int nulTerminated)
{
    UErrorCode status = U_ZERO_ERROR;
    UListFormatter *f = ulistfmt_openForType(locale, type, width, &status);
    UChar storage[8][64];
    const UChar *strings[8];
    int32_t lengths[8];
    int32_t count = prepare(items, storage, strings, lengths);
    UFormattedList *result = ulistfmt_openResult(&status);
    ulistfmt_formatStringsToResult(f, strings, nulTerminated ? NULL : lengths, count, result, &status);
    const UFormattedValue *value = ulistfmt_resultAsValue(result, &status);
    int32_t length = 0;
    const UChar *s = ufmtval_getString(value, &length, &status);
    char out[512];
    utf8(s, length, out, sizeof out);
    printf("fields %s %d/%d [%d]: \"%s\" %d %s\n", locale, (int)type, (int)width, count, U_SUCCESS(status) ? out : "", length, statusName(status));
    for (int pass = 0; pass < 3; pass++) {
        UConstrainedFieldPosition *position = ucfpos_open(&status);
        if (pass == 1) ucfpos_constrainField(position, UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, &status);
        if (pass == 2) ucfpos_constrainCategory(position, UFIELD_CATEGORY_LIST_SPAN, &status);
        printf("  %s:", pass == 0 ? "all" : pass == 1 ? "elements" : "spans");
        while (ufmtval_nextPosition(value, position, &status)) {
            int32_t start = 0, limit = 0;
            ucfpos_getIndexes(position, &start, &limit, &status);
            printf(" %s.%d[%d,%d)", categoryName(ucfpos_getCategory(position, &status)), ucfpos_getField(position, &status), start, limit);
        }
        printf(" %s\n", statusName(status));
        ucfpos_close(position);
    }
    ulistfmt_closeResult(result);
    ulistfmt_close(f);
}

static const char *none[] = { NULL };
static const char *one[] = { "A", NULL };
static const char *two[] = { "A", "B", NULL };
static const char *three[] = { "A", "B", "C", NULL };
static const char *four[] = { "A", "B", "C", "D", NULL };
static const char *five[] = { "Alpha", "Bravo", "Charlie", "Delta", "Echo", NULL };

static void allWidths(const char *locale, const char **items)
{
    for (int type = 0; type < 3; type++)
        for (int width = 0; width < 3; width++)
            format(locale, (UListFormatterType)type, (UListFormatterWidth)width, items);
}

int main(void)
{
    /* Every type and width, three elements. */
    const char *locales[] = { "en", "en_GB", "de", "fr", "es", "it", "pt", "nl", "ru", "pl", "ja", "zh", "zh_Hant", "ko", "ar", "he", "hi", "th", "tr", "fi", "sv", "el", "uk", "id", "vi", "sw", "am", "root", NULL };
    for (int i = 0; locales[i]; i++) allWidths(locales[i], three);

    /* Lengths 0 to 5. */
    const char **lists[] = { none, one, two, three, four, five };
    for (int i = 0; i < 6; i++) {
        format("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, lists[i]);
        format("en", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_SHORT, lists[i]);
        format("de", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, lists[i]);
        format("ja", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, lists[i]);
    }

    /* ulistfmt_open: and, wide. */
    {
        UErrorCode status = U_ZERO_ERROR;
        UListFormatter *f = ulistfmt_open("fr_CA", &status);
        UChar storage[8][64];
        const UChar *strings[8];
        int32_t lengths[8];
        int32_t count = prepare(four, storage, strings, lengths);
        UChar result[128];
        int32_t length = ulistfmt_format(f, strings, NULL, count, result, 128, &status);
        char out[256];
        utf8(result, length, out, sizeof out);
        printf("ulistfmt_open(fr_CA): \"%s\" %d %s\n", out, length, statusName(status));
        ulistfmt_close(f);
    }

    /* Spanish: y before /i/ is e (not before "hie", "hia"); o before /o/
       is u ("o", "ho", "8", "11"). */
    const char *es1[] = { "agua", "hielo", NULL };
    const char *es2[] = { "padre", "hijo", NULL };
    const char *es3[] = { "Roma", "Italia", NULL };
    const char *es4[] = { "cosa", "hiato", NULL };
    const char *es5[] = { "uno", "dos", "Iván", NULL };
    const char *es6[] = { "siete", "ocho", NULL };
    const char *es7[] = { "uno", "dos", "Holanda", NULL };
    const char *es8[] = { "7", "8", NULL };
    const char *es9[] = { "10", "11", NULL };
    const char *es10[] = { "10", "11 manzanas", NULL };
    const char *es11[] = { "10", "110", NULL };
    const char *es12[] = { "a", "hi", NULL };
    const char *es13[] = { "a", "h", NULL };
    const char **spanish[] = { es1, es2, es3, es4, es5, es6, es7, es8, es9, es10, es11, es12, es13 };
    for (int i = 0; i < 13; i++) {
        format("es", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, spanish[i]);
        format("es", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, spanish[i]);
        format("es_MX", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_SHORT, spanish[i]);
        format("es_419", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_NARROW, spanish[i]);
    }

    /* Hebrew: ו before a letter not Hebrew is ו-. */
    const char *he1[] = { "\xd7\x90", "\xd7\x91", NULL };
    const char *he2[] = { "\xd7\x90", "B", NULL };
    const char *he3[] = { "\xd7\x90", "\xd7\x91", "1", NULL };
    const char *he4[] = { "A", "B", "\xd7\x92", NULL };
    const char **hebrew[] = { he1, he2, he3, he4 };
    for (int i = 0; i < 4; i++) {
        format("he", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, hebrew[i]);
        format("iw", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, hebrew[i]);
        format("he_IL", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_SHORT, hebrew[i]);
        format("he", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, hebrew[i]);
    }

    /* Thai and others with no spaces. */
    const char *th1[] = { "\xe0\xb8\x81", "\xe0\xb8\x82", "\xe0\xb8\x84", NULL };
    format("th", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, th1);
    format("th", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, th1);

    /* Locale IDs: tags, keywords, fallback to the parent. */
    format("en-US", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);
    format("de-AT-u-co-phonebk", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);
    format("en_AU@calendar=buddhist", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_SHORT, three);
    format("en_IN", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);
    format("pt_PT", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, three);
    format("sr_Latn", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);
    format("zh_TW", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);
    format("", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, three);

    /* Fields: element and literal spans and the list spans with their
       element index. */
    fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, none, 0);
    fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, one, 0);
    fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, two, 0);
    fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, five, 1);
    fields("en", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, four, 0);
    fields("ja", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, four, 0);
    fields("es", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, es5, 0);
    fields("he", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, he4, 0);
    fields("zh", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, four, 0);
    {
        const char *empty1[] = { "", "B", "", NULL };
        const char *empty2[] = { "A", "", "", "D", NULL };
        fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, empty1, 0);
        fields("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, empty2, 0);
    }

    /* A result reused for another list. */
    {
        UErrorCode status = U_ZERO_ERROR;
        UListFormatter *f = ulistfmt_openForType("en", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, &status);
        UFormattedList *result = ulistfmt_openResult(&status);
        UChar storage[8][64];
        const UChar *strings[8];
        int32_t lengths[8];
        int32_t count = prepare(five, storage, strings, lengths);
        ulistfmt_formatStringsToResult(f, strings, lengths, count, result, &status);
        count = prepare(two, storage, strings, lengths);
        ulistfmt_formatStringsToResult(f, strings, lengths, count, result, &status);
        const UFormattedValue *value = ulistfmt_resultAsValue(result, &status);
        int32_t length = 0;
        const UChar *s = ufmtval_getString(value, &length, &status);
        char out[256];
        utf8(s, length, out, sizeof out);
        printf("reused: \"%s\" %d %s\n", out, length, statusName(status));
        UConstrainedFieldPosition *position = ucfpos_open(&status);
        int spans = 0;
        while (ufmtval_nextPosition(value, position, &status)) spans++;
        printf("reused spans: %d %s\n", spans, statusName(status));
        ucfpos_close(position);
        ulistfmt_closeResult(result);
        ulistfmt_close(f);
    }

    /* Lengths: -1 for NUL-terminated, explicit shorter lengths. */
    {
        UErrorCode status = U_ZERO_ERROR;
        UListFormatter *f = ulistfmt_openForType("en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, &status);
        UChar a[] = { 'o', 'n', 'e', 0 }, b[] = { 't', 'w', 'o', 0 }, c[] = { 't', 'h', 'r', 'e', 'e', 0 };
        const UChar *strings[] = { a, b, c };
        int32_t lengths[] = { -1, 2, 5 };
        UChar result[64];
        int32_t length = ulistfmt_format(f, strings, lengths, 3, result, 64, &status);
        char out[128];
        utf8(result, length, out, sizeof out);
        printf("lengths: \"%s\" %d %s\n", out, length, statusName(status));

        /* Preflighting, an exact fit, too small. */
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, strings, NULL, 3, NULL, 0, &status);
        printf("preflight: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        for (int i = 0; i < 64; i++) result[i] = 0x2a;
        length = ulistfmt_format(f, strings, NULL, 3, result, 19, &status);
        printf("exact: %d %s %s\n", length, statusName(status), result[18] == 'e' && result[19] == 0x2a ? "unterminated" : "?");
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, strings, NULL, 3, result, 5, &status);
        printf("small: %d %s\n", length, statusName(status));

        /* Errors. */
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, strings, NULL, -1, result, 64, &status);
        printf("count -1: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, NULL, NULL, 2, result, 64, &status);
        printf("strings NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, NULL, NULL, 0, result, 64, &status);
        printf("strings NULL, none: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, strings, NULL, 3, NULL, 10, &status);
        printf("result NULL: %d %s\n", length, statusName(status));
        status = U_ZERO_ERROR;
        length = ulistfmt_format(f, strings, NULL, 3, result, -1, &status);
        printf("capacity -1: %d %s\n", length, statusName(status));
        status = U_ILLEGAL_ARGUMENT_ERROR;
        length = ulistfmt_format(f, strings, NULL, 3, result, 64, &status);
        printf("failed before: %d %s\n", length, statusName(status));

        UFormattedList *r = ulistfmt_openResult(&status);
        printf("openResult after failure: %s %s\n", r ? "result" : "NULL", statusName(status));
        status = U_ZERO_ERROR;
        r = ulistfmt_openResult(&status);
        ulistfmt_formatStringsToResult(f, strings, NULL, -2, r, &status);
        printf("toResult count -2: %s\n", statusName(status));
        status = U_ZERO_ERROR;
        ulistfmt_formatStringsToResult(f, strings, NULL, 3, NULL, &status);
        printf("toResult NULL: %s\n", statusName(status));
        status = U_ZERO_ERROR;
        const UFormattedValue *v = ulistfmt_resultAsValue(NULL, &status);
        printf("resultAsValue NULL: %s %s\n", v ? "value" : "NULL", statusName(status));
        ulistfmt_closeResult(r);
        ulistfmt_closeResult(NULL);
        ulistfmt_close(f);
        ulistfmt_close(NULL);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        UListFormatter *f = ulistfmt_openForType("en", (UListFormatterType)3, ULISTFMT_WIDTH_WIDE, &status);
        printf("type 3: %s %s\n", f ? "formatter" : "NULL", statusName(status));
        status = U_ZERO_ERROR;
        f = ulistfmt_openForType("en", ULISTFMT_TYPE_AND, (UListFormatterWidth)-1, &status);
        printf("width -1: %s %s\n", f ? "formatter" : "NULL", statusName(status));
        status = U_MEMORY_ALLOCATION_ERROR;
        f = ulistfmt_open("en", &status);
        printf("open after failure: %s %s\n", f ? "formatter" : "NULL", statusName(status));
    }
    return 0;
}
