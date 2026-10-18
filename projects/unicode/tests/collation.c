/* The Unicode Collation Algorithm's tests (uca/CollationTest_*_SHORT.txt):
   their lines are in order, so each compares at most equal to the next
   (with the identical level as the last word: equal strings only). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ucol.h>
#include <unicode/utf16.h>

static int32_t parse(const char *line, UChar *out)
{
    int32_t n = 0;
    const char *at = line;
    while (*at && *at != ';' && *at != '#') {
        char *end;
        long c = strtol(at, &end, 16);
        if (end == at) { ++at; continue; }
        U16_APPEND_UNSAFE(out, n, c);
        at = end;
    }
    return n;
}

static int run(const char *path, UColAttributeValue alternate, int verbose)
{
    FILE *file = fopen(path, "r");
    if (!file) { printf("cannot read %s\n", path); return 1; }
    UErrorCode status = U_ZERO_ERROR;
    /* The root as DUCET has it (`ducet`): its variable elements and its
       Han order (CLDR's root, the default, has only spaces and
       punctuation variable and Han in radical-stroke order). */
    UCollator *coll = ucol_open("und-u-co-ducet", &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, alternate, &status);
    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_IDENTICAL, &status);
    char line[1024];
    UChar previous[64], current[64];
    int32_t previousLength = -1;
    int lines = 0, failures = 0;
    while (fgets(line, sizeof line, file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '@') continue;
        int32_t length = parse(line, current);
        ++lines;
        if (previousLength >= 0 && ucol_strcoll(coll, previous, previousLength, current, length) > 0) {
            if (++failures <= verbose) printf("  out of order before: %s", line);
        }
        memcpy(previous, current, length * sizeof(UChar));
        previousLength = length;
    }
    ucol_close(coll);
    printf("%s: %d of %d in order\n", strrchr(path, '/') + 1, lines - failures, lines);
    return failures != 0;
}

int main(int argc, char **argv)
{
    int verbose = argc > 1 ? atoi(argv[1]) : 3;
    int failed = run("projects/unicode/uca/CollationTest_NON_IGNORABLE_SHORT.txt", UCOL_NON_IGNORABLE, verbose);
    failed |= run("projects/unicode/uca/CollationTest_SHIFTED_SHORT.txt", UCOL_SHIFTED, verbose);
    return failed;
}
