/* ubidi.h against ICU: texts made at random of characters of every
   bidi class (brackets, marks, controls, isolates, digits of both kinds,
   paragraph separators, a supplementary character) in every paragraph
   direction, and for each what ICU reports: the paragraph levels, the
   direction, the levels, the runs in visual and in logical order, the
   maps, the text written in visual order with ubidi_writeReordered's
   options, and a line of it (ubidi_setLine). The library follows ICU
   where UAX #9 leaves the choice open and in how ICU resolves a
   paragraph after the first, but not where ICU 74 fails the UCD's
   tests (BidiCharacterTest.txt): a mark right after a bracket is left
   out of the texts here for that. The first cases are printed whole,
   the rest summed into a hash; bidirandom.expected is what ICU 74 prints
   (the program built with gcc against the system's ICU). */
#include <stdio.h>
#include <string.h>
#include <unicode/ubidi.h>

static const UChar palette[] = {
    'a', 'b', 'Z', 0x05D0, 0x05D1, 0x0627, 0x0628, '1', '2', 0x0661, 0x0662, 0x06F1,
    '+', '-', '$', '%', ',', '.', ':', '/', ' ', ' ', '\t', '!', '"', '(', ')', '[', ']',
    0x2329, 0x3009, 0x0300, 0x064B, 0x00AD, 0x200D, 0x200E, 0x200F, 0x061C,
    0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069,
    '\n', 0x2029, 0x00A0, 0x0020,
};

static int isBracket(UChar c) { return c == '(' || c == ')' || c == '[' || c == ']' || c == 0x2329 || c == 0x3009; }

static unsigned long long state = 88172645463325252ULL;
static unsigned next(unsigned limit)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return (unsigned)(state % limit);
}

static unsigned long long hash = 1469598103934665603ULL;
static int whole = 1;
static char line[65536];
static size_t used;

static void say(const char *format, int value)
{
    char piece[64];
    int n = snprintf(piece, sizeof piece, format, value);
    for (int i = 0; i < n; i++) hash = (hash ^ (unsigned char)piece[i]) * 1099511628211ULL;
    if (whole && used + n < sizeof line) {
        memcpy(line + used, piece, n);
        used += n;
    }
}

static void describe(UBiDi *bidi, int32_t length)
{
    UErrorCode status = U_ZERO_ERROR;
    say(" dir %d", ubidi_getDirection(bidi));
    say(" para %d", ubidi_getParaLevel(bidi));
    const UBiDiLevel *levels = ubidi_getLevels(bidi, &status);
    say(" levels", 0);
    for (int32_t i = 0; levels && i < length; i++) say(" %d", levels[i]);
    int32_t runs = ubidi_countRuns(bidi, &status);
    say(" runs %d:", runs);
    for (int32_t r = 0; r < runs; r++) {
        int32_t start, n;
        UBiDiDirection d = ubidi_getVisualRun(bidi, r, &start, &n);
        say(" %d", start);
        say("+%d", n);
        say("/%d", d);
    }
    say(" logical", 0);
    for (int32_t i = 0; i < length;) {
        int32_t limit = i + 1;
        UBiDiLevel level = 0;
        ubidi_getLogicalRun(bidi, i, &limit, &level);
        say(" %d", limit);
        say("@%d", level);
        if (limit <= i) break;
        i = limit;
    }
    int32_t map[512];
    ubidi_getVisualMap(bidi, map, &status);
    say(" visual", 0);
    for (int32_t i = 0; i < length; i++) say(" %d", map[i]);
    ubidi_getLogicalMap(bidi, map, &status);
    say(" logicalmap", 0);
    for (int32_t i = 0; i < length; i++) say(" %d", map[i]);
    if (length > 0) {
        say(" index %d", ubidi_getVisualIndex(bidi, length / 2, &status));
        say(" %d", ubidi_getLogicalIndex(bidi, length / 3, &status));
    }
    static const uint16_t options[] = { 0, UBIDI_DO_MIRRORING, UBIDI_DO_MIRRORING | UBIDI_REMOVE_BIDI_CONTROLS, UBIDI_KEEP_BASE_COMBINING | UBIDI_DO_MIRRORING, UBIDI_OUTPUT_REVERSE | UBIDI_DO_MIRRORING };
    for (int o = 0; o < 5; o++) {
        UChar out[512];
        UErrorCode e = U_ZERO_ERROR;
        int32_t n = ubidi_writeReordered(bidi, out, 512, options[o], &e);
        say(" write%d:", o);
        for (int32_t i = 0; i < n; i++) say(" %x", out[i]);
    }
    say(" status %d", status);
}

int main(int argc, char **argv)
{
    static const UBiDiLevel levels[] = { 0, 1, UBIDI_DEFAULT_LTR, UBIDI_DEFAULT_RTL, 2 };
    for (int test = 0; test < 6000; test++) {
        /* (With an argument, every case is printed whole.) */
        whole = test < 300 || argc > 1;
        used = 0;
        UChar text[256];
        int32_t length = 0;
        int32_t size = 1 + next(test < 3000 ? 12 : 40);
        for (int32_t i = 0; i < size; i++) {
            if (next(25) == 0) {
                /* A supplementary character (Adlam, R; or a Latin
                   mathematical letter, L). */
                if (next(2)) { text[length++] = 0xD83A; text[length++] = 0xDD00; }
                else { text[length++] = 0xD835; text[length++] = 0xDC00; }
            } else {
                UChar c = palette[next(sizeof palette / sizeof palette[0])];
                /* (No mark right after a bracket: ICU 74 does not give it
                   the bracket's direction, as N0 says; BidiCharacterTest.txt
                   has the cases, which ICU 74 fails and the library
                   passes.) */
                while ((c == 0x0300 || c == 0x064B) && length > 0 && isBracket(text[length - 1]))
                    c = palette[next(sizeof palette / sizeof palette[0])];
                text[length++] = c;
            }
        }
        UBiDiLevel paraLevel = levels[next(5)];
        UErrorCode status = U_ZERO_ERROR;
        UBiDi *bidi = ubidi_open();
        ubidi_setPara(bidi, text, length, paraLevel, NULL, &status);
        say("%d:", test);
        say(" base %d", ubidi_getBaseDirection(text, length));
        say(" paragraphs %d", ubidi_countParagraphs(bidi));
        describe(bidi, length);
        /* A line within the first paragraph. */
        int32_t paraStart = 0, paraLimit = 0;
        ubidi_getParagraphByIndex(bidi, 0, &paraStart, &paraLimit, NULL, &status);
        if (paraLimit - paraStart >= 2) {
            int32_t start = paraStart + next(paraLimit - paraStart - 1);
            int32_t limit = start + 1 + next(paraLimit - start - 1);
            UBiDi *line = ubidi_open();
            UErrorCode e = U_ZERO_ERROR;
            ubidi_setLine(bidi, start, limit, line, &e);
            say(" line %d", start);
            say("-%d", limit);
            say(" e %d", e);
            if (U_SUCCESS(e)) describe(line, limit - start);
            ubidi_close(line);
        }
        ubidi_close(bidi);
        if (whole) printf("%.*s\n", (int)used, line);
    }
    (void)argv;
    printf("hash %llx\n", hash);
    return 0;
}
