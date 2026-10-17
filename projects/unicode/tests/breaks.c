/* The UCD's break tests (GraphemeBreakTest.txt, WordBreakTest.txt,
   SentenceBreakTest.txt, LineBreakTest.txt): each line's boundaries
   against a break iterator's. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ubrk.h>
#include <unicode/utf16.h>

static int run(const char *path, UBreakIteratorType type, const char *locale, int verbose)
{
    FILE *file = fopen(path, "r");
    if (!file) { printf("cannot read %s\n", path); return 1; }
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *it = ubrk_open(type, locale, NULL, 0, &status);
    static char line[65536];
    int lines = 0, failures = 0;
    while (fgets(line, sizeof line, file)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        UChar text[1024];
        int32_t length = 0, expected[1024], count = 0;
        const char *at = line;
        int any = 0;
        while (*at) {
            if (!strncmp(at, "\xc3\xb7", 2)) { expected[count++] = length; at += 2; any = 1; continue; }
            if (!strncmp(at, "\xc3\x97", 2)) { at += 2; continue; }
            if (*at == ' ' || *at == '\t' || *at == '\n' || *at == '\r') { ++at; continue; }
            char *end;
            long c = strtol(at, &end, 16);
            if (end == at) { ++at; continue; }
            U16_APPEND_UNSAFE(text, length, c);
            at = end;
        }
        if (!any) continue;
        /* (The start of the text is a boundary to an iterator however a
           test file marks it.) */
        if (count == 0 || expected[0] != 0) {
            memmove(expected + 1, expected, count * sizeof expected[0]);
            expected[0] = 0;
            ++count;
        }
        ++lines;
        status = U_ZERO_ERROR;
        ubrk_setText(it, text, length, &status);
        int32_t got[1024], n = 0;
        for (int32_t b = ubrk_first(it); b != UBRK_DONE && n < 1024; b = ubrk_next(it)) got[n++] = b;
        int ok = n == count;
        for (int i = 0; ok && i < n; ++i) ok = got[i] == expected[i];
        if (!ok) {
            ++failures;
            if (failures <= verbose) {
                printf("  fails: %s\n    expected", line);
                for (int i = 0; i < count; ++i) printf(" %d", expected[i]);
                printf("\n    got     ");
                for (int i = 0; i < n; ++i) printf(" %d", got[i]);
                printf("\n");
            }
        }
    }
    ubrk_close(it);
    printf("%s: %d of %d pass\n", strrchr(path, '/') + 1, lines - failures, lines);
    return failures != 0;
}

int main(int argc, char **argv)
{
    int verbose = argc > 1 ? atoi(argv[1]) : 3;
    int failed = 0;
    failed |= run("projects/unicode/ucd/auxiliary/GraphemeBreakTest.txt", UBRK_CHARACTER, "", verbose);
    failed |= run("projects/unicode/ucd/auxiliary/WordBreakTest.txt", UBRK_WORD, "", verbose);
    failed |= run("projects/unicode/ucd/auxiliary/SentenceBreakTest.txt", UBRK_SENTENCE, "", verbose);
    failed |= run("projects/unicode/ucd/auxiliary/LineBreakTest.txt", UBRK_LINE, "", verbose);
    return failed;
}
