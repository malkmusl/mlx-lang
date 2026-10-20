/* IdnaTestV2.txt (UTS #46 16.0): every line's toUnicode, toAsciiN and
   toAsciiT with UseSTD3ASCIIRules, CheckBidi, CheckJoiners, CONTEXTO,
   CheckHyphens and VerifyDnsLength, as ICU tests itself: a line expecting
   errors must find some; a line expecting none must find none and give
   the string. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unicode/uidna.h>
#include <unicode/ustring.h>

static int hexValue(const char *s, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
    }
    return v;
}

static void put(UChar *out, int *length, int c)
{
    if (c < 0x10000)
        out[(*length)++] = (UChar)c;
    else {
        out[(*length)++] = (UChar)((c >> 10) + 0xd7c0);
        out[(*length)++] = (UChar)((c & 0x3ff) | 0xdc00);
    }
}

/* A field: UTF-8 with \uXXXX and \x{X...} escapes, "" for empty; -1 for
   a blank field. */
static int field(const char *text, int n, UChar *out)
{
    while (n > 0 && (*text == ' ' || *text == '\t')) { text++; n--; }
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
    if (n == 0)
        return -1;
    if (n == 2 && text[0] == '"' && text[1] == '"')
        return 0;
    int length = 0;
    for (int i = 0; i < n;) {
        unsigned char b = (unsigned char)text[i];
        if (b == '\\' && i + 1 < n && text[i + 1] == 'u') {
            put(out, &length, hexValue(text + i + 2, 4));
            i += 6;
        } else if (b == '\\' && i + 1 < n && text[i + 1] == 'x') {
            int j = i + 3;
            while (text[j] != '}') j++;
            put(out, &length, hexValue(text + i + 3, j - (i + 3)));
            i = j + 1;
        } else if (b < 0x80) {
            out[length++] = b;
            i++;
        } else {
            int c, extra;
            if (b >= 0xf0) { c = b & 7; extra = 3; }
            else if (b >= 0xe0) { c = b & 15; extra = 2; }
            else { c = b & 31; extra = 1; }
            for (int k = 1; k <= extra; k++)
                c = (c << 6) | (text[i + k] & 63);
            put(out, &length, c);
            i += extra + 1;
        }
    }
    return length;
}

/* A status: -1 blank, 0 none ("[]"), 1 errors, 2 only A4_2. */
static int status(const char *text, int n)
{
    while (n > 0 && (*text == ' ' || *text == '\t')) { text++; n--; }
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) n--;
    if (n == 0)
        return -1;
    if (n == 6 && memcmp(text, "[A4_2]", 6) == 0)
        return 2;
    return n == 2 ? 0 : 1;
}

static int checked, failed;

static void check(const char *what, int lineNumber, UIDNA *idna, int toASCII, const UChar *source, int sourceLength,
                  const UChar *expected, int expectedLength, int expectErrors)
{
    UChar result[1024];
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UErrorCode error = U_ZERO_ERROR;
    int length = (toASCII ? uidna_nameToASCII : uidna_nameToUnicode)(idna, source, sourceLength, result, 1024, &info, &error);
    checked++;
    int hasErrors = info.errors != 0;
    /* As ICU does: an empty root label (a name ending in one dot) is no
       error in ToASCII, which A4_2 alone may be about. */
    if (expectErrors == 2) {
        expectErrors = 1;
        if (toASCII && !hasErrors && length > 0 && result[length - 1] == '.') {
            int doubled = 0;
            for (int i = 1; i < length; i++)
                if (result[i] == '.' && result[i - 1] == '.')
                    doubled = 1;
            if (!doubled)
                expectErrors = 0;
        }
    }
    int ok = U_SUCCESS(error) && hasErrors == expectErrors;
    if (ok && !expectErrors)
        ok = length == expectedLength && memcmp(result, expected, length * sizeof(UChar)) == 0;
    if (!ok) {
        failed++;
        if (failed <= 30) {
            printf("line %d %s: expected %s, errors %04x %s; got", lineNumber, what, expectErrors ? "errors" : "none", info.errors, u_errorName(error));
            for (int i = 0; i < length && i < 24; i++)
                printf(" %04X", result[i]);
            printf(" / expected");
            for (int i = 0; i < expectedLength && i < 24; i++)
                printf(" %04X", expected[i]);
            printf("\n");
        }
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "std/registry/unicode/idna/IdnaTestV2.txt";
    FILE *file = fopen(path, "r");
    if (!file) {
        printf("cannot read %s\n", path);
        return 1;
    }
    UErrorCode error = U_ZERO_ERROR;
    UIDNA *trans = uidna_openUTS46(UIDNA_USE_STD3_RULES | UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_CHECK_CONTEXTO, &error);
    UIDNA *nontrans = uidna_openUTS46(UIDNA_USE_STD3_RULES | UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_CHECK_CONTEXTO | UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE, &error);
    static char line[65536];
    int lineNumber = 0, lines = 0;
    while (fgets(line, sizeof line, file)) {
        lineNumber++;
        char *hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        const char *fields[7];
        int lengths[7], count = 0;
        const char *start = line;
        for (char *s = line;; s++) {
            if (*s == ';' || *s == 0 || *s == '\n') {
                if (count < 7) {
                    fields[count] = start;
                    lengths[count] = (int)(s - start);
                    count++;
                }
                if (*s != ';')
                    break;
                start = s + 1;
            }
        }
        if (count < 7)
            continue;
        static UChar source[1024], unicode[1024], asciiN[1024], asciiT[1024];
        int sourceLength = field(fields[0], lengths[0], source);
        if (sourceLength < 0)
            sourceLength = 0;
        int unicodeLength = field(fields[1], lengths[1], unicode);
        if (unicodeLength < 0) {
            memcpy(unicode, source, sourceLength * sizeof(UChar));
            unicodeLength = sourceLength;
        }
        int unicodeStatus = status(fields[2], lengths[2]);
        if (unicodeStatus < 0)
            unicodeStatus = 0;
        int asciiNLength = field(fields[3], lengths[3], asciiN);
        if (asciiNLength < 0) {
            memcpy(asciiN, unicode, unicodeLength * sizeof(UChar));
            asciiNLength = unicodeLength;
        }
        int asciiNStatus = status(fields[4], lengths[4]);
        if (asciiNStatus < 0)
            asciiNStatus = unicodeStatus;
        int asciiTLength = field(fields[5], lengths[5], asciiT);
        if (asciiTLength < 0) {
            memcpy(asciiT, asciiN, asciiNLength * sizeof(UChar));
            asciiTLength = asciiNLength;
        }
        int asciiTStatus = status(fields[6], lengths[6]);
        if (asciiTStatus < 0)
            asciiTStatus = asciiNStatus;
        lines++;
        check("toUnicode", lineNumber, nontrans, 0, source, sourceLength, unicode, unicodeLength, unicodeStatus);
        check("toAsciiN", lineNumber, nontrans, 1, source, sourceLength, asciiN, asciiNLength, asciiNStatus);
        check("toAsciiT", lineNumber, trans, 1, source, sourceLength, asciiT, asciiTLength, asciiTStatus);
    }
    fclose(file);
    /* The UTF-8 forms. */
    {
        char ascii[256], unicode[256];
        UIDNAInfo info = UIDNA_INFO_INITIALIZER;
        UErrorCode e = U_ZERO_ERROR;
        int n = uidna_nameToASCII_UTF8(nontrans, "B\xc3\xbc" "cher.Example", -1, ascii, 256, &info, &e);
        checked++;
        if (U_FAILURE(e) || info.errors || n != 21 || strcmp(ascii, "xn--bcher-kva.example") != 0) {
            failed++;
            printf("nameToASCII_UTF8: %s %04x %s\n", ascii, info.errors, u_errorName(e));
        }
        UIDNAInfo info2 = UIDNA_INFO_INITIALIZER;
        n = uidna_nameToUnicodeUTF8(nontrans, ascii, n, unicode, 256, &info2, &e);
        checked++;
        if (U_FAILURE(e) || info2.errors || strcmp(unicode, "b\xc3\xbc" "cher.example") != 0) {
            failed++;
            printf("nameToUnicodeUTF8: %s %04x %s\n", unicode, info2.errors, u_errorName(e));
        }
    }
    uidna_close(trans);
    uidna_close(nontrans);
    printf("IdnaTestV2.txt: %d of %d conversions right (%d lines, and UTF-8)\n", checked - failed, checked, lines);
    return failed ? 1 : 0;
}
