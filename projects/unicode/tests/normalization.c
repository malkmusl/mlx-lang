/* NormalizationTest.txt: every line's five columns against the four
   forms, as UAX #15 states the invariants. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/unorm2.h>
#include <unicode/utf16.h>

static int32_t parse(const char *field, UChar *out)
{
    int32_t n = 0;
    while (*field && *field != ';') {
        char *end;
        long c = strtol(field, &end, 16);
        if (end == field) { ++field; continue; }
        U16_APPEND_UNSAFE(out, n, c);
        field = end;
    }
    return n;
}

static int same(const UNormalizer2 *norm, const UChar *in, int32_t inLength, const UChar *expected, int32_t expectedLength)
{
    UChar out[256];
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = unorm2_normalize(norm, in, inLength, out, 256, &status);
    return U_SUCCESS(status) && length == expectedLength && !memcmp(out, expected, length * 2);
}

int main(int argc, char **argv)
{
    FILE *file = fopen(argc > 1 ? argv[1] : "projects/unicode/ucd/NormalizationTest.txt", "r");
    if (!file) { printf("no test file\n"); return 2; }
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2 *nfc = unorm2_getNFCInstance(&status), *nfd = unorm2_getNFDInstance(&status);
    const UNormalizer2 *nfkc = unorm2_getNFKCInstance(&status), *nfkd = unorm2_getNFKDInstance(&status);
    char line[4096];
    int lines = 0, failures = 0;
    while (fgets(line, sizeof line, file)) {
        if (line[0] == '#' || line[0] == '@' || line[0] == '\n') continue;
        UChar c[5][128];
        int32_t n[5];
        const char *field = line;
        for (int i = 0; i < 5; ++i) {
            n[i] = parse(field, c[i]);
            field = strchr(field, ';') + 1;
        }
        int ok = 1;
        for (int i = 0; i < 3; ++i) ok &= same(nfc, c[i], n[i], c[1], n[1]) && same(nfd, c[i], n[i], c[2], n[2]);
        for (int i = 3; i < 5; ++i) ok &= same(nfc, c[i], n[i], c[3], n[3]) && same(nfd, c[i], n[i], c[4], n[4]);
        for (int i = 0; i < 5; ++i) ok &= same(nfkc, c[i], n[i], c[3], n[3]) && same(nfkd, c[i], n[i], c[4], n[4]);
        for (int i = 0; i < 5; ++i) {
            UErrorCode s = U_ZERO_ERROR;
            ok &= unorm2_isNormalized(nfc, c[1], n[1], &s) && unorm2_isNormalized(nfd, c[2], n[2], &s);
        }
        ++lines;
        if (!ok && ++failures <= 5) printf("fails: %s", line);
    }
    printf("%d of %d lines pass\n", lines - failures, lines);
    return failures != 0;
}
