/* ucnv.h, ucnv_err.h, ucnv_cb.h: converters as WebKit's TextCodecICU uses
   them (open by name, fallbacks on, ucnv_toUnicode in pieces into a small
   buffer until it stops overflowing, the substitute callback stopping on
   illegal input when asked, ucnv_fromUnicode with the XML escape or a
   callback of its own that writes with ucnv_cbFromUWriteBytes, ucnv_reset)
   and the rest of the API: names and aliases, whole texts both ways with
   every callback and context, the substitution, the invalid input a
   callback is given, what is pending, one character at a time, from one
   charset to another, signatures. The converters: UTF-8, UTF-16 and
   UTF-32 (with a byte order mark and without, BE and LE), ISO-8859-1,
   US-ASCII, and a few single-byte charsets where ICU's tables and the
   Encoding Standard's agree; and 1500 random texts, whole characters and
   broken ones, in random pieces into random room. converters.expected is what ICU 74 prints
   (the program built with gcc against the system's ICU). */
#include <stdio.h>
#include <string.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ustring.h>

static void showUnits(const UChar *units, int32_t length)
{
    for (int32_t i = 0; i < length; i++)
        printf(" %04X", units[i]);
}

static void showBytes(const char *bytes, int32_t length)
{
    for (int32_t i = 0; i < length; i++)
        printf(" %02X", (unsigned char)bytes[i]);
}

static int32_t fromUTF8(const char *utf8, UChar *out, int32_t capacity)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = 0;
    u_strFromUTF8(out, capacity, &length, utf8, -1, &status);
    return length;
}

/* ------------------------------------------------------------------ */
/* Names */

static void names(void)
{
    static const char *pairs[][2] = {
        { "UTF-8", "utf8" }, { "utf-8", "UTF_8" }, { "ISO-8859-1", "iso88591" },
        { "iso-8859-01", "ISO-8859-1" }, { "latin1", "Latin-2" }, { "a", "b" },
        { "UTF-16BE", "UTF-16" }, { "", "-" }, { "abc", "ab" }, { "8859_1", "885901" },
    };
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
        int result = ucnv_compareNames(pairs[i][0], pairs[i][1]);
        printf("compare %s %s: %d\n", pairs[i][0], pairs[i][1], result < 0 ? -1 : result > 0);
    }

    static const char *opened[] = {
        "UTF-8", "utf8", "unicode-1-1-utf-8", "windows-65001", "UTF-16", "utf16",
        "ucs-2", "unicode", "UTF-16BE", "UnicodeBigUnmarked", "x-utf-16le", "UTF16_LittleEndian",
        "UTF-32", "ucs-4", "UTF-32BE", "UTF-32LE", "ISO-8859-1", "latin1", "l1", "ISO_8859-1:1987",
        "cp819", "US-ASCII", "ascii", "us", "ANSI_X3.4-1968", "ISO646-US", "iso-ir-6", "ibm-367",
    };
    for (size_t i = 0; i < sizeof opened / sizeof opened[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UConverter *cnv = ucnv_open(opened[i], &status);
        printf("open %s: %s", opened[i], u_errorName(status));
        if (cnv) {
            UErrorCode s = U_ZERO_ERROR;
            printf(" name %s type %d sizes %d-%d fixed %d ccsid %d ambiguous %d", ucnv_getName(cnv, &s), ucnv_getType(cnv),
                ucnv_getMinCharSize(cnv), ucnv_getMaxCharSize(cnv), ucnv_isFixedWidth(cnv, &s), ucnv_getCCSID(cnv, &s), ucnv_isAmbiguous(cnv));
            ucnv_close(cnv);
        }
        printf("\n");
    }

    static const char *aliased[] = { "UTF-8", "utf-16", "UTF-32LE", "latin1", "ascii" };
    for (size_t i = 0; i < sizeof aliased / sizeof aliased[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        uint16_t count = ucnv_countAliases(aliased[i], &status);
        printf("aliases of %s (%d):", aliased[i], count);
        for (uint16_t n = 0; n < count; n++)
            printf(" %s", ucnv_getAlias(aliased[i], n, &status));
        const char *all[32];
        ucnv_getAliases(aliased[i], all, &status);
        printf(" | %s %s %s\n", all[0], all[count - 1], u_errorName(status));
        status = U_ZERO_ERROR;
        ucnv_getAlias(aliased[i], count, &status);
        printf("  alias %d: %s\n", count, u_errorName(status));
    }

    static const char *unknown[] = { "no-such-charset", "UTF-9", "" };
    for (size_t i = 0; i < sizeof unknown / sizeof unknown[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UConverter *cnv = ucnv_open(unknown[i], &status);
        printf("open \"%s\": %s %s\n", unknown[i], u_errorName(status), cnv ? "converter" : "null");
        status = U_ZERO_ERROR;
        printf("  aliases %d %s\n", ucnv_countAliases(unknown[i], &status), u_errorName(status));
    }

    static const UChar utf16Name[] = { 'u', 't', 'f', '-', '1', '6', 'l', 'e', 0 };
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_openU(utf16Name, &status);
    printf("openU utf-16le: %s %s\n", u_errorName(status), ucnv_getName(cnv, &status));
    ucnv_close(cnv);
    static const int32_t ccsids[] = { 1208, 1200, 1202, 1204, 1232, 819, 367, 99999 };
    for (size_t i = 0; i < sizeof ccsids / sizeof ccsids[0]; i++) {
        status = U_ZERO_ERROR;
        cnv = ucnv_openCCSID(ccsids[i], UCNV_IBM, &status);
        printf("openCCSID %d: %s %s\n", ccsids[i], u_errorName(status), cnv ? ucnv_getName(cnv, &status) : "-");
        ucnv_close(cnv);
    }

    ucnv_setDefaultName("latin1");
    printf("default after latin1: %s\n", ucnv_getDefaultName());
    status = U_ZERO_ERROR;
    cnv = ucnv_open(NULL, &status);
    printf("open NULL: %s %s\n", u_errorName(status), ucnv_getName(cnv, &status));
    ucnv_close(cnv);
    ucnv_setDefaultName("UTF-8");
    printf("default after UTF-8: %s\n", ucnv_getDefaultName());
    printf("max bytes for 5 units at 3: %d\n", UCNV_GET_MAX_BYTES_FOR_STRING(5, 3));
}

/* ------------------------------------------------------------------ */
/* Bytes to UTF-16, whole */

static const char *contextName(const void *context)
{
    return context ? (const char *)context : "none";
}

static void decodeWith(const char *charset, const char *bytes, int32_t length, UConverterToUCallback callback, const void *context, const char *label)
{
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open(charset, &status);
    if (callback)
        ucnv_setToUCallBack(cnv, callback, context, NULL, NULL, &status);
    UChar out[256];
    int32_t n = ucnv_toUChars(cnv, out, 256, bytes, length, &status);
    printf("decode %s %s %s:", charset, label, contextName(context));
    showBytes(bytes, length);
    printf(" ->");
    showUnits(out, n);
    printf(" (%d) %s\n", n, u_errorName(status));
    ucnv_close(cnv);
}

static void decodeAll(const char *charset, const char *bytes, int32_t length)
{
    decodeWith(charset, bytes, length, NULL, NULL, "default");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_STOP, NULL, "stop");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_SKIP, NULL, "skip");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_SKIP, UCNV_SKIP_STOP_ON_ILLEGAL, "skip");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, "substitute");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_ESCAPE, NULL, "escape");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, "escape");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_HEX, "escape");
    decodeWith(charset, bytes, length, UCNV_TO_U_CALLBACK_ESCAPE, UCNV_ESCAPE_C, "escape");
}

static void decoding(void)
{
    static const struct { const char *charset; const char *bytes; int32_t length; } cases[] = {
        { "UTF-8", "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z", 11 },
        { "UTF-8", "\xc0\x80" "a\xc1\xbf", 5 },
        { "UTF-8", "\xe0\x80\x80" "b\xed\xa0\x80" "c\xed\xbf\xbf", 11 },
        { "UTF-8", "\xf4\x90\x80\x80" "d\xf5\x80" "e\xff\xfe", 10 },
        { "UTF-8", "\xe2\x82" "f\xf0\x9f\x98" "g\x80\xbf", 10 },
        { "UTF-8", "h\xe2\x82", 3 },
        { "UTF-8", "h\xf0\x9f", 3 },
        { "UTF-8", "\xef\xbb\xbfi", 4 },
        { "UTF-16BE", "\x00" "a\xd8\x3d\xde\x00\x00" "b", 8 },
        { "UTF-16BE", "\xd8\x3d\x00" "a\xdc\x00\x00" "b", 8 },
        { "UTF-16BE", "\x00" "a\xd8\x3d", 4 },
        { "UTF-16BE", "\x00" "a\x00", 3 },
        { "UTF-16LE", "a\x00\x3d\xd8\x00\xde" "b\x00", 8 },
        { "UTF-16LE", "a\x00\x00\xdc" "b\x00\x3d\xd8", 8 },
        { "UTF-16", "\xfe\xff\x00" "a\x00" "b", 6 },
        { "UTF-16", "\xff\xfe" "a\x00" "b\x00", 6 },
        { "UTF-16", "\x00" "a\x00" "b", 4 },
        { "UTF-16", "\xfe\xff\xfe\xff\x00" "a", 6 },
        { "UTF-32BE", "\x00\x00\x00" "a\x00\x01\xf6\x00\x00\x11\x00\x00\x00\x00\xd8\x00\x00\x00\x00" "b", 20 },
        { "UTF-32LE", "a\x00\x00\x00\x00\xf6\x01\x00" "b\x00\x00", 11 },
        { "UTF-32", "\x00\x00\xfe\xff\x00\x00\x00" "a", 8 },
        { "UTF-32", "\xff\xfe\x00\x00" "a\x00\x00\x00", 8 },
        { "UTF-32", "\x00\x00\x00" "a", 4 },
        { "ISO-8859-1", "a\x80\xa9\xff", 4 },
        { "US-ASCII", "a\x7f\x80\xffz", 5 },
        { "ISO-8859-2", "a\xa1\xb1\xc8\xe8\xff", 6 },
        { "ISO-8859-3", "a\xa5\xa6\xae", 4 },
        { "ISO-8859-5", "\xb0\xd0\xf0\xfd", 4 },
        { "KOI8-R", "\xc1\xe1\xff", 3 },
        { "ISO-8859-15", "\xa4\xbc\xbe", 3 },
        { "", "", 0 },
    };
    for (size_t i = 0; cases[i].charset[0]; i++)
        decodeAll(cases[i].charset, cases[i].bytes, cases[i].length);

    /* Preflighting, a NUL-terminated source, the target too small. */
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("UTF-8", &status);
    int32_t n = ucnv_toUChars(cnv, NULL, 0, "abc\xe2\x82\xac", -1, &status);
    printf("preflight: %d %s\n", n, u_errorName(status));
    UChar small[3];
    status = U_ZERO_ERROR;
    n = ucnv_toUChars(cnv, small, 3, "abc", 3, &status);
    printf("exact: %d %s\n", n, u_errorName(status));
    status = U_ZERO_ERROR;
    n = ucnv_toUChars(cnv, small, 2, "abc\xf0\x9f\x98\x80", 7, &status);
    printf("small: %d %s", n, u_errorName(status));
    showUnits(small, 2);
    printf("\n");
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* UTF-16 to bytes, whole */

static void encodeWith(const char *charset, const UChar *units, int32_t length, UConverterFromUCallback callback, const void *context, const char *label)
{
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open(charset, &status);
    if (callback)
        ucnv_setFromUCallBack(cnv, callback, context, NULL, NULL, &status);
    char out[512];
    int32_t n = ucnv_fromUChars(cnv, out, 512, units, length, &status);
    printf("encode %s %s %s:", charset, label, contextName(context));
    showUnits(units, length);
    printf(" ->");
    showBytes(out, n);
    printf(" (%d) %s\n", n, u_errorName(status));
    ucnv_close(cnv);
}

static void encodeAll(const char *charset, const UChar *units, int32_t length)
{
    encodeWith(charset, units, length, NULL, NULL, "default");
    encodeWith(charset, units, length, UCNV_FROM_U_CALLBACK_STOP, NULL, "stop");
    encodeWith(charset, units, length, UCNV_FROM_U_CALLBACK_SKIP, NULL, "skip");
    encodeWith(charset, units, length, UCNV_FROM_U_CALLBACK_SKIP, UCNV_SKIP_STOP_ON_ILLEGAL, "skip");
    encodeWith(charset, units, length, UCNV_FROM_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, "substitute");
    static const char *escapes[] = { UCNV_ESCAPE_ICU, UCNV_ESCAPE_JAVA, UCNV_ESCAPE_C, UCNV_ESCAPE_XML_DEC, UCNV_ESCAPE_XML_HEX, UCNV_ESCAPE_UNICODE, UCNV_ESCAPE_CSS2 };
    for (size_t i = 0; i < sizeof escapes / sizeof escapes[0]; i++)
        encodeWith(charset, units, length, UCNV_FROM_U_CALLBACK_ESCAPE, escapes[i], "escape");
}

static void encoding(void)
{
    static const char *texts[] = { "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z", "\xc3\xbf\xc4\x80~\x7f", "\xd0\x96\xd0\xb6\xc2\xa4", "" };
    static const char *charsets[] = { "UTF-8", "UTF-16BE", "UTF-16LE", "UTF-16", "UTF-32BE", "UTF-32LE", "UTF-32", "ISO-8859-1", "US-ASCII", "ISO-8859-5", "KOI8-R", "ISO-8859-15" };
    for (size_t t = 0; texts[t][0]; t++) {
        UChar units[64];
        int32_t length = fromUTF8(texts[t], units, 64);
        for (size_t c = 0; c < sizeof charsets / sizeof charsets[0]; c++)
            encodeAll(charsets[c], units, length);
    }
    /* Unpaired surrogates. */
    static const UChar lone[][4] = { { 'a', 0xd800, 'b' }, { 'a', 0xdc00, 'b' }, { 'a', 0xd83d } };
    static const int32_t loneLengths[] = { 3, 3, 2 };
    static const char *loneCharsets[] = { "UTF-8", "UTF-16LE", "UTF-32BE", "ISO-8859-1", "US-ASCII" };
    for (size_t t = 0; t < 3; t++)
        for (size_t c = 0; c < sizeof loneCharsets / sizeof loneCharsets[0]; c++)
            encodeAll(loneCharsets[c], lone[t], loneLengths[t]);

    /* Preflighting and the target too small. */
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("UTF-8", &status);
    static const UChar abc[] = { 'a', 0x20ac, 'c', 0 };
    int32_t n = ucnv_fromUChars(cnv, NULL, 0, abc, -1, &status);
    printf("preflight: %d %s\n", n, u_errorName(status));
    char small[4];
    status = U_ZERO_ERROR;
    n = ucnv_fromUChars(cnv, small, 4, abc, 2, &status);
    printf("exact: %d %s\n", n, u_errorName(status));
    status = U_ZERO_ERROR;
    n = ucnv_fromUChars(cnv, small, 2, abc, 3, &status);
    printf("small: %d %s", n, u_errorName(status));
    showBytes(small, 2);
    printf("\n");
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* The substitution */

static void substitution(void)
{
    static const char *charsets[] = { "UTF-8", "UTF-16BE", "UTF-16LE", "UTF-16", "UTF-32", "UTF-32LE", "ISO-8859-1", "US-ASCII", "ISO-8859-2" };
    for (size_t c = 0; c < sizeof charsets / sizeof charsets[0]; c++) {
        UErrorCode status = U_ZERO_ERROR;
        UConverter *cnv = ucnv_open(charsets[c], &status);
        char sub[8];
        int8_t length = 8;
        ucnv_getSubstChars(cnv, sub, &length, &status);
        printf("substitution %s:", charsets[c]);
        showBytes(sub, length);
        length = 0;
        ucnv_getSubstChars(cnv, sub, &length, &status);
        printf(" | room 0: %s\n", u_errorName(status));
        ucnv_close(cnv);
    }
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("US-ASCII", &status);
    static const UChar text[] = { 'a', 0xe9, 'b', 0xd83d, 0xde00, 'c' };
    ucnv_setSubstChars(cnv, "?", 1, &status);
    char out[64];
    int32_t n = ucnv_fromUChars(cnv, out, 64, text, 6, &status);
    printf("subst ?:");
    showBytes(out, n);
    printf(" %s\n", u_errorName(status));
    ucnv_setSubstChars(cnv, "", 0, &status);
    printf("subst empty: %s\n", u_errorName(status));
    status = U_ZERO_ERROR;
    static const UChar replacement[] = { '[', '?', ']' };
    ucnv_setSubstString(cnv, replacement, 3, &status);
    n = ucnv_fromUChars(cnv, out, 64, text, 6, &status);
    printf("subst string:");
    showBytes(out, n);
    printf(" %s\n", u_errorName(status));
    static const UChar unencodable[] = { 0xe9 };
    ucnv_setSubstString(cnv, unencodable, 1, &status);
    printf("subst unencodable: %s\n", u_errorName(status));
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* Callbacks of a program's own */

static int32_t calls;

static void toUOwn(const void *context, UConverterToUnicodeArgs *args, const char *codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode *err)
{
    calls++;
    printf("  toU %s reason %d err %s length %d:", (const char *)context, reason, u_errorName(*err), length);
    if (codeUnits)
        showBytes(codeUnits, length);
    if (reason <= UCNV_IRREGULAR) {
        char invalid[32];
        int8_t invalidLength = 32;
        UErrorCode status = U_ZERO_ERROR;
        ucnv_getInvalidChars(args->converter, invalid, &invalidLength, &status);
        printf(" | invalid");
        showBytes(invalid, invalidLength);
        printf(" | flush %d left %d", args->flush, (int)(args->sourceLimit - args->source));
        *err = U_ZERO_ERROR;
        static const UChar mark[] = { '<', '>' };
        ucnv_cbToUWriteUChars(args, mark, 2, 0, err);
        ucnv_cbToUWriteSub(args, 0, err);
    }
    printf("\n");
}

static void fromUOwn(const void *context, UConverterFromUnicodeArgs *args, const UChar *codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode *err)
{
    calls++;
    printf("  fromU %s reason %d err %s length %d code point %X:", (const char *)context, reason, u_errorName(*err), length, codePoint);
    if (codeUnits)
        showUnits(codeUnits, length);
    if (reason == UCNV_UNASSIGNED) {
        /* As WebKit's urlEscapedEntityCallback: %26%23<decimal>%3B. */
        char entity[32];
        int written = snprintf(entity, sizeof entity, "%%26%%23%d%%3B", (int)codePoint);
        *err = U_ZERO_ERROR;
        ucnv_cbFromUWriteBytes(args, entity, written, 0, err);
        UChar invalid[8];
        int8_t invalidLength = 8;
        UErrorCode status = U_ZERO_ERROR;
        ucnv_getInvalidUChars(args->converter, invalid, &invalidLength, &status);
        printf(" | invalid");
        showUnits(invalid, invalidLength);
    } else if (reason <= UCNV_IRREGULAR) {
        UCNV_FROM_U_CALLBACK_ESCAPE(UCNV_ESCAPE_XML_HEX, args, codeUnits, length, codePoint, reason, err);
        static const UChar text[] = { '(', 0xe9, 0x416, ')' };
        const UChar *source = text;
        ucnv_cbFromUWriteUChars(args, &source, text + 4, 0, err);
        ucnv_cbFromUWriteSub(args, 0, err);
    }
    printf("\n");
}

static void ownCallbacks(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("ISO-8859-5", &status);
    UConverterToUCallback oldToU;
    const void *oldToUContext;
    ucnv_getToUCallBack(cnv, &oldToU, &oldToUContext);
    printf("default toU is SUBSTITUTE: %d context %d\n", oldToU == UCNV_TO_U_CALLBACK_SUBSTITUTE, oldToUContext == NULL);
    UConverterFromUCallback oldFromU;
    const void *oldFromUContext;
    ucnv_getFromUCallBack(cnv, &oldFromU, &oldFromUContext);
    printf("default fromU is SUBSTITUTE: %d context %d\n", oldFromU == UCNV_FROM_U_CALLBACK_SUBSTITUTE, oldFromUContext == NULL);
    ucnv_setFromUCallBack(cnv, fromUOwn, "mine", &oldFromU, &oldFromUContext, &status);
    printf("old fromU is SUBSTITUTE: %d\n", oldFromU == UCNV_FROM_U_CALLBACK_SUBSTITUTE);
    static const UChar text[] = { 'a', 0x416, 0xe9, 0xd800, 'b', 0xd83d, 0xde00, 'c' };
    char out[256];
    int32_t n = ucnv_fromUChars(cnv, out, 256, text, 8, &status);
    printf("encode own:");
    showBytes(out, n);
    printf(" %s\n", u_errorName(status));
    printf("reset:\n");
    ucnv_reset(cnv);
    printf("resetToUnicode:\n");
    ucnv_resetToUnicode(cnv);
    printf("resetFromUnicode:\n");
    ucnv_resetFromUnicode(cnv);
    printf("close:\n");
    ucnv_close(cnv);

    status = U_ZERO_ERROR;
    cnv = ucnv_open("UTF-8", &status);
    ucnv_setToUCallBack(cnv, toUOwn, "own", NULL, NULL, &status);
    UChar units[64];
    n = ucnv_toUChars(cnv, units, 64, "a\xc3" "b\xe2\x82", 5, &status);
    printf("decode own:");
    showUnits(units, n);
    printf(" %s\n", u_errorName(status));
    printf("close:\n");
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* Streaming */

/* As TextCodecICU::decode: pieces of `chunk` bytes into a buffer of
   `room` units, until it stops overflowing; flush with the last piece.
   With stopOnError, the substitute callback stopping on illegal input,
   and after an error the rest flushed with the callback put back. */
static void decodeStream(const char *charset, const char *bytes, int32_t length, int32_t chunk, int32_t room, int stopOnError)
{
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open(charset, &status);
    ucnv_setFallback(cnv, 1);
    UChar result[256];
    int32_t total = 0;
    const char *source = bytes;
    const char *end = bytes + length;
    int sawError = 0;
    while (source < end || !sawError) {
        const char *pieceEnd = end - source > chunk ? source + chunk : end;
        UBool flush = pieceEnd == end;
        UConverterToUCallback savedAction = NULL;
        const void *savedContext = NULL;
        if (stopOnError) {
            UErrorCode s = U_ZERO_ERROR;
            ucnv_setToUCallBack(cnv, UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, &savedAction, &savedContext, &s);
        }
        UErrorCode err;
        do {
            UChar buffer[16];
            UChar *target = buffer;
            err = U_ZERO_ERROR;
            ucnv_toUnicode(cnv, &target, buffer + room, &source, pieceEnd, NULL, flush, &err);
            for (UChar *u = buffer; u < target && total < 256; u++)
                result[total++] = *u;
        } while (err == U_BUFFER_OVERFLOW_ERROR);
        if (U_FAILURE(err)) {
            printf(" [%s at %d, pending %d]", u_errorName(err), (int)(source - bytes), ucnv_toUCountPending(cnv, &status));
            sawError = 1;
            do {
                UChar buffer[16];
                UChar *target = buffer;
                err = U_ZERO_ERROR;
                ucnv_toUnicode(cnv, &target, buffer + room, &source, pieceEnd, NULL, 1, &err);
            } while (source < pieceEnd);
        }
        if (stopOnError) {
            UErrorCode s = U_ZERO_ERROR;
            UConverterToUCallback oldAction;
            const void *oldContext;
            ucnv_setToUCallBack(cnv, savedAction, savedContext, &oldAction, &oldContext, &s);
        }
        if (source >= end)
            break;
    }
    printf("stream %s chunk %d room %d stop %d:", charset, chunk, room, stopOnError);
    showUnits(result, total);
    printf("\n");
    ucnv_reset(cnv);
    ucnv_close(cnv);
}

/* As TextCodecICU::encode: into a buffer of `room` bytes until it stops
   overflowing, the XML escape for what the charset has not. */
static void encodeStream(const char *charset, const char *utf8, int32_t room)
{
    UChar units[128];
    int32_t length = fromUTF8(utf8, units, 128);
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open(charset, &status);
    ucnv_setFromUCallBack(cnv, UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_DEC, 0, 0, &status);
    const UChar *source = units;
    char result[512];
    int32_t total = 0;
    UErrorCode err;
    do {
        char buffer[32];
        char *target = buffer;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(cnv, &target, buffer + room, &source, units + length, 0, 1, &err);
        for (char *b = buffer; b < target && total < 512; b++)
            result[total++] = *b;
    } while (err == U_BUFFER_OVERFLOW_ERROR);
    printf("encode stream %s room %d: %.*s %s\n", charset, room, (int)total, result, u_errorName(err));
    ucnv_close(cnv);
}

static void streaming(void)
{
    static const char utf8[] = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z\xe4\xb8\xad\xe6\x96\x87";
    static const char bad[] = "a\xc3\xa9\xff" "b\xe2\x82" "c\xf0\x9f\x98\x80";
    for (int32_t chunk = 1; chunk <= 5; chunk++)
        for (int32_t room = 1; room <= 4; room += 3) {
            decodeStream("UTF-8", utf8, (int32_t)strlen(utf8), chunk, room, 0);
            decodeStream("UTF-8", bad, (int32_t)strlen(bad), chunk, room, 0);
            decodeStream("UTF-8", bad, (int32_t)strlen(bad), chunk, room, 1);
        }
    static const char utf16[] = "\xff\xfe" "a\x00\x3d\xd8\x00\xde" "b\x00";
    for (int32_t chunk = 1; chunk <= 3; chunk++) {
        decodeStream("UTF-16", utf16, 10, chunk, 2, 0);
        decodeStream("UTF-16LE", utf16 + 2, 8, chunk, 1, 0);
        decodeStream("UTF-32LE", "a\x00\x00\x00\x00\xf6\x01\x00", 8, chunk, 1, 0);
    }
    decodeStream("ISO-8859-5", "\xb0\xd0\xf0\xfd", 4, 1, 1, 1);
    decodeStream("US-ASCII", "ab\x80" "cd", 5, 2, 4, 1);
    decodeStream("US-ASCII", "ab\x80" "cd", 5, 2, 4, 0);

    for (int32_t room = 1; room <= 9; room += 4) {
        encodeStream("ISO-8859-1", "caf\xc3\xa9 \xe2\x82\xac 5 \xf0\x9f\x98\x80!", room);
        encodeStream("US-ASCII", "caf\xc3\xa9 \xe2\x82\xac 5 \xf0\x9f\x98\x80!", room);
        encodeStream("UTF-8", "caf\xc3\xa9", room);
    }

    /* A surrogate pair split between calls, the pending lead, its end. */
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("UTF-8", &status);
    static const UChar pair[] = { 'a', 0xd83d, 0xde00, 'b', 0xd83d };
    char out[32];
    char *target = out;
    const UChar *source = pair;
    ucnv_fromUnicode(cnv, &target, out + 32, &source, pair + 2, NULL, 0, &status);
    printf("split: %d bytes, pending %d %s\n", (int)(target - out), ucnv_fromUCountPending(cnv, &status), u_errorName(status));
    ucnv_fromUnicode(cnv, &target, out + 32, &source, pair + 5, NULL, 0, &status);
    printf("joined: %d bytes, pending %d %s\n", (int)(target - out), ucnv_fromUCountPending(cnv, &status), u_errorName(status));
    ucnv_fromUnicode(cnv, &target, out + 32, &source, pair + 5, NULL, 1, &status);
    printf("flushed:");
    showBytes(out, (int32_t)(target - out));
    printf(" %s\n", u_errorName(status));

    /* Offsets. */
    status = U_ZERO_ERROR;
    ucnv_reset(cnv);
    static const char text[] = "a\xc3\xa9\xff\xe2\x82\xac\xf0\x9f\x98\x80";
    UChar units[16];
    int32_t offsets[16];
    UChar *unitTarget = units;
    const char *byteSource = text;
    ucnv_toUnicode(cnv, &unitTarget, units + 16, &byteSource, text + 11, offsets, 1, &status);
    printf("offsets to UTF-16:");
    for (int32_t i = 0; i < unitTarget - units; i++)
        printf(" %04X@%d", units[i], offsets[i]);
    printf(" %s\n", u_errorName(status));
    UConverter *latin = ucnv_open("ISO-8859-1", &status);
    static const UChar latinText[] = { 'a', 0xe9, 0x20ac, 0xd83d, 0xde00, 'b' };
    char bytes[16];
    target = bytes;
    source = latinText;
    ucnv_fromUnicode(latin, &target, bytes + 16, &source, latinText + 6, offsets, 1, &status);
    printf("offsets from UTF-16:");
    for (int32_t i = 0; i < target - bytes; i++)
        printf(" %02X@%d", (unsigned char)bytes[i], offsets[i]);
    printf(" %s\n", u_errorName(status));
    ucnv_close(latin);

    /* Pending bytes. */
    status = U_ZERO_ERROR;
    unitTarget = units;
    byteSource = "\xe2\x82";
    ucnv_toUnicode(cnv, &unitTarget, units + 16, &byteSource, byteSource + 2, NULL, 0, &status);
    printf("pending: %d %s\n", ucnv_toUCountPending(cnv, &status), u_errorName(status));
    ucnv_resetToUnicode(cnv);
    printf("after reset: %d\n", ucnv_toUCountPending(cnv, &status));
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* One character, two converters, signatures */

static void others(void)
{
    static const struct { const char *charset; const char *bytes; int32_t length; } cases[] = {
        { "UTF-8", "a\xc3\xa9\xf0\x9f\x98\x80\xff" "b\xe2\x82", 11 },
        { "UTF-16LE", "a\x00\x3d\xd8\x00\xde\x00\xdc" "b", 9 },
        { "ISO-8859-2", "a\xa1\xff", 3 },
        { "US-ASCII", "a\x80" "b", 3 },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        UConverter *cnv = ucnv_open(cases[i].charset, &status);
        const char *source = cases[i].bytes;
        const char *end = source + cases[i].length;
        printf("next %s:", cases[i].charset);
        for (int k = 0; k < 12; k++) {
            status = U_ZERO_ERROR;
            UChar32 c = ucnv_getNextUChar(cnv, &source, end, &status);
            printf(" %X@%d", c, (int)(source - cases[i].bytes));
            if (U_FAILURE(status)) {
                printf("(%s)", u_errorName(status));
                if (status == U_INDEX_OUTOFBOUNDS_ERROR)
                    break;
            }
        }
        printf("\n");
        ucnv_close(cnv);
    }

    static const struct { const char *to; const char *from; const char *bytes; int32_t length; } conversions[] = {
        { "UTF-16BE", "UTF-8", "a\xc3\xa9\xf0\x9f\x98\x80", 7 },
        { "ISO-8859-1", "UTF-8", "a\xc3\xa9\xe2\x82\xac", 6 },
        { "UTF-8", "ISO-8859-1", "a\xe9\xff", 3 },
        { "UTF-8", "UTF-16LE", "a\x00\x3d\xd8\x00\xde", 6 },
        { "US-ASCII", "UTF-8", "abc\xff", 4 },
        { "UTF-8", "ISO-8859-5", "\xb0\xd0", 2 },
        { "UTF-16", "UTF-8", "ab", 2 },
        { "UTF-32", "UTF-8", "ab", 2 },
        { "UTF-8", "UTF-16", "\xff\xfe" "a\x00", 4 },
    };
    for (size_t i = 0; i < sizeof conversions / sizeof conversions[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        char out[64];
        int32_t n = ucnv_convert(conversions[i].to, conversions[i].from, out, 64, conversions[i].bytes, conversions[i].length, &status);
        printf("convert %s<-%s:", conversions[i].to, conversions[i].from);
        showBytes(out, n);
        printf(" (%d) %s", n, u_errorName(status));
        status = U_ZERO_ERROR;
        n = ucnv_convert(conversions[i].to, conversions[i].from, NULL, 0, conversions[i].bytes, conversions[i].length, &status);
        printf(" | preflight %d %s\n", n, u_errorName(status));
    }

    /* convertEx in pieces, with a small pivot and a small target. */
    UErrorCode status = U_ZERO_ERROR;
    UConverter *from = ucnv_open("UTF-8", &status);
    UConverter *to = ucnv_open("UTF-16LE", &status);
    static const char text[] = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z";
    UChar pivot[3];
    UChar *pivotSource = pivot;
    UChar *pivotTarget = pivot;
    char result[64];
    char *target = result;
    const char *source = text;
    for (int32_t at = 0; at < 11; at += 2) {
        const char *limit = text + (at + 2 < 11 ? at + 2 : 11);
        UBool flush = limit == text + 11;
        do {
            status = U_ZERO_ERROR;
            char *chunkLimit = target + 3 < result + 64 ? target + 3 : result + 64;
            ucnv_convertEx(to, from, &target, chunkLimit, &source, limit, pivot, &pivotSource, &pivotTarget, pivot + 3, at == 0, flush, &status);
        } while (status == U_BUFFER_OVERFLOW_ERROR);
        if (U_FAILURE(status))
            printf("convertEx at %d: %s\n", at, u_errorName(status));
    }
    printf("convertEx:");
    showBytes(result, (int32_t)(target - result));
    printf(" %s\n", u_errorName(status));
    ucnv_close(from);
    ucnv_close(to);

    static const struct { const char *bytes; int32_t length; } signatures[] = {
        { "\xef\xbb\xbf" "a", 4 }, { "\xfe\xff\x00" "a", 4 }, { "\xff\xfe" "a\x00", 4 }, { "\xff\xfe\x00\x00", 4 },
        { "\x00\x00\xfe\xff", 4 }, { "\x0e\xfe\xff", 3 }, { "\xfb\xee\x28", 3 }, { "+/v8", 4 }, { "\xdd\x73\x66\x73", 4 },
        { "abc", 3 }, { "\xef\xbb", 2 }, { "", 0 }, { "\xfe\xff", -1 },
    };
    for (size_t i = 0; i < sizeof signatures / sizeof signatures[0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t length = -5;
        const char *name = ucnv_detectUnicodeSignature(signatures[i].bytes, signatures[i].length, &length, &status);
        printf("signature %d:", (int)i);
        showBytes(signatures[i].bytes, signatures[i].length < 0 ? 2 : signatures[i].length);
        printf(" %s %d %s\n", name ? name : "none", length, u_errorName(status));
    }

    /* Clones convert as the original does. */
    status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("UTF-8", &status);
    ucnv_setFromUCallBack(cnv, UCNV_FROM_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_HEX, NULL, NULL, &status);
    UConverter *copy = ucnv_clone(cnv, &status);
    static const UChar lone[] = { 'x', 0xdc00, 'y' };
    char out[32];
    int32_t n = ucnv_fromUChars(copy, out, 32, lone, 3, &status);
    printf("clone:");
    showBytes(out, n);
    printf(" %s %s\n", ucnv_getName(copy, &status), u_errorName(status));
    ucnv_close(copy);
    ucnv_close(cnv);
}

/* ------------------------------------------------------------------ */
/* Errors */

static void errors(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UConverter *cnv = ucnv_open("UTF-8", &status);
    UChar units[4];
    UChar *target = units;
    const char *source = "ab";
    ucnv_toUnicode(cnv, &target, units + 4, &source, source - 1, NULL, 1, &status);
    printf("toUnicode limit before source: %s\n", u_errorName(status));
    status = U_ZERO_ERROR;
    ucnv_toUChars(cnv, units, -1, "ab", 2, &status);
    printf("toUChars capacity -1: %s\n", u_errorName(status));
    status = U_ILLEGAL_ARGUMENT_ERROR;
    int32_t n = ucnv_toUChars(cnv, units, 4, "ab", 2, &status);
    printf("toUChars failed status: %d %s\n", n, u_errorName(status));
    status = U_ZERO_ERROR;
    source = "";
    UChar32 c = ucnv_getNextUChar(cnv, &source, source, &status);
    printf("getNextUChar at end: %X %s\n", c, u_errorName(status));
    status = U_ZERO_ERROR;
    ucnv_setSubstChars(cnv, "12345", 5, &status);
    printf("setSubstChars 5: %s\n", u_errorName(status));
    ucnv_close(cnv);
    ucnv_close(NULL);
}

/* ------------------------------------------------------------------ */
/* Random texts in random pieces */

static uint32_t seed = 12345;

static uint32_t next(uint32_t bound)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) % bound;
}

static uint32_t hash;

static void mix(uint32_t value)
{
    hash = (hash ^ value) * 16777619u;
}

/* Bytes made of pieces of characters, whole and broken, decoded in pieces
   of 1 to 6 bytes into 1 to 5 units at a time (stopping on illegal input
   or not), and UTF-16 with unpaired surrogates encoded with the escapes
   into 1 to 8 bytes at a time: a hash of all they write and the error
   that stops them and where, ten conversions to a line. (How far each
   call reads before its output overflows is left out: ICU's converters
   differ there among themselves, UTF-16's reading a byte of the next unit
   after an unpaired lead surrogate that began in an earlier call.) */
static void randomTexts(void)
{
    static const char *pieces[] = {
        "a", "z", "\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "\xc3", "\xe2\x82", "\xf0\x9f", "\x80", "\xff",
        "\xed\xa0\x80", "\xc0\xaf", "\xf4\x90\x80\x80", "\x00", "\xfe", "\xd8\x3d", "\xde\x00", "\x3d\xd8",
    };
    static const char *decoders[] = { "UTF-8", "UTF-16LE", "UTF-16BE", "UTF-16", "UTF-32LE", "UTF-32", "US-ASCII", "ISO-8859-1", "ISO-8859-7" };
    static const char *encoders[] = { "UTF-8", "UTF-16LE", "UTF-16", "UTF-32BE", "US-ASCII", "ISO-8859-1", "ISO-8859-5", "KOI8-R" };
    static const UChar units16[] = { 'a', 'b', 0xe9, 0x416, 0x20ac, 0xd83d, 0xde00, 0xd800, 0xdc00, 0x3b1, 0xffff, 0 };
    static const char *escapes[] = { UCNV_ESCAPE_ICU, UCNV_ESCAPE_JAVA, UCNV_ESCAPE_C, UCNV_ESCAPE_XML_DEC, UCNV_ESCAPE_XML_HEX, UCNV_ESCAPE_UNICODE, UCNV_ESCAPE_CSS2 };
    for (int round = 0; round < 1500; round++) {
        hash = 2166136261u;
        UErrorCode status = U_ZERO_ERROR;
        if (round % 2 == 0) {
            char bytes[64];
            int32_t length = 0;
            for (uint32_t k = next(12); k > 0; k--) {
                const char *piece = pieces[next(sizeof pieces / sizeof pieces[0])];
                size_t n = piece[0] ? strlen(piece) : 1;
                memcpy(bytes + length, piece, n);
                length += (int32_t)n;
            }
            UConverter *cnv = ucnv_open(decoders[next(sizeof decoders / sizeof decoders[0])], &status);
            uint32_t mode = next(4);
            if (mode == 1)
                ucnv_setToUCallBack(cnv, UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, NULL, NULL, &status);
            else if (mode == 2)
                ucnv_setToUCallBack(cnv, UCNV_TO_U_CALLBACK_ESCAPE, UCNV_ESCAPE_XML_HEX, NULL, NULL, &status);
            else if (mode == 3)
                ucnv_setToUCallBack(cnv, UCNV_TO_U_CALLBACK_SKIP, NULL, NULL, NULL, &status);
            int32_t chunk = 1 + (int32_t)next(6);
            int32_t room = 1 + (int32_t)next(5);
            const char *source = bytes;
            const char *end = bytes + length;
            for (int guard = 0; guard < 200; guard++) {
                const char *limit = end - source > chunk ? source + chunk : end;
                UBool flush = limit == end;
                UChar out[8];
                UChar *target = out;
                UErrorCode err = U_ZERO_ERROR;
                ucnv_toUnicode(cnv, &target, out + room, &source, limit, NULL, flush, &err);
                for (UChar *u = out; u < target; u++)
                    mix(*u);
                if (U_FAILURE(err) && err != U_BUFFER_OVERFLOW_ERROR) {
                    mix((uint32_t)err);
                    mix((uint32_t)(source - bytes));
                    break;
                }
                if (flush && err == U_ZERO_ERROR)
                    break;
            }
            ucnv_close(cnv);
        } else {
            UChar text[24];
            int32_t length = (int32_t)next(16);
            for (int32_t i = 0; i < length; i++)
                text[i] = units16[next(11)];
            UConverter *cnv = ucnv_open(encoders[next(sizeof encoders / sizeof encoders[0])], &status);
            uint32_t mode = next(5);
            if (mode < 4)
                ucnv_setFromUCallBack(cnv, UCNV_FROM_U_CALLBACK_ESCAPE, escapes[next(7)], NULL, NULL, &status);
            else
                ucnv_setFromUCallBack(cnv, UCNV_FROM_U_CALLBACK_SUBSTITUTE, UCNV_SUB_STOP_ON_ILLEGAL, NULL, NULL, &status);
            int32_t chunk = 1 + (int32_t)next(4);
            int32_t room = 1 + (int32_t)next(8);
            const UChar *source = text;
            const UChar *end = text + length;
            for (int guard = 0; guard < 400; guard++) {
                const UChar *limit = end - source > chunk ? source + chunk : end;
                UBool flush = limit == end;
                char out[8];
                char *target = out;
                UErrorCode err = U_ZERO_ERROR;
                ucnv_fromUnicode(cnv, &target, out + room, &source, limit, NULL, flush, &err);
                for (char *b = out; b < target; b++)
                    mix((unsigned char)*b);
                if (U_FAILURE(err) && err != U_BUFFER_OVERFLOW_ERROR) {
                    mix((uint32_t)err);
                    mix((uint32_t)(source - text));
                    break;
                }
                if (flush && err == U_ZERO_ERROR)
                    break;
            }
            ucnv_close(cnv);
        }
        printf("%08x%c", hash, round % 10 == 9 ? '\n' : ' ');
    }
}

int main(void)
{
    names();
    decoding();
    encoding();
    substitution();
    ownCallbacks();
    streaming();
    others();
    errors();
    randomTexts();
    return 0;
}
