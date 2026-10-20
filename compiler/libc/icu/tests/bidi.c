/* The UCD's tests of the Bidirectional Algorithm (UAX #9) through ubidi.h:
   every line of BidiTest.txt (classes, as a character of each class, in
   each paragraph direction its bitset names) and of BidiCharacterTest.txt
   (code points, with their paragraph direction): the paragraph level, the
   levels (but those the test leaves out, x) and the visual order (L2,
   without the characters X9 removes).

   As ICU, the library reports text with one direction (ubidi_getDirection
   not UBIDI_MIXED) with all its levels the paragraph's: the order is the
   same, and the levels are taken as right then, as ICU's own conformance
   test takes them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

/* A character of each class, in UCharDirection's order. */
static const UChar32 representative[] = {
    0x0061, 0x05D0, 0x0030, 0x002B, 0x0024, 0x0660, 0x002C, 0x2029, 0x0009, 0x0020, 0x0021,
    0x202A, 0x202D, 0x0627, 0x202B, 0x202E, 0x202C, 0x0300, 0x00AD, 0x2068, 0x2066, 0x2067, 0x2069,
};
static const char *names[] = {
    "L", "R", "EN", "ES", "ET", "AN", "CS", "B", "S", "WS", "ON",
    "LRE", "LRO", "AL", "RLE", "RLO", "PDF", "NSM", "BN", "FSI", "LRI", "RLI", "PDI",
};

/* The text's units and, for each code point, its first unit. */
typedef struct {
    UChar units[512];
    int32_t unitCount;
    int32_t firstUnit[256];
    int removed[256];
    int count;
} Text;

static void add(Text *text, UChar32 c, int removed)
{
    text->firstUnit[text->count] = text->unitCount;
    text->removed[text->count] = removed;
    text->count++;
    U16_APPEND_UNSAFE(text->units, text->unitCount, c);
}

static int isRemovedClass(int c) { return c == 11 || c == 12 || c == 14 || c == 15 || c == 16 || c == 18; }

/* Whether the library's levels and order for the text in a paragraph of
   `paraLevel` are those expected (`levels`: -1 for x; `order`: the code
   points' indices in visual order, the removed ones left out). */
static int check(const Text *text, UBiDiLevel paraLevel, int expectedParaLevel, const int *levels, const int *order, int orderCount)
{
    UErrorCode status = U_ZERO_ERROR;
    UBiDi *bidi = ubidi_open();
    ubidi_setPara(bidi, text->units, text->unitCount, paraLevel, NULL, &status);
    const UBiDiLevel *actual = ubidi_getLevels(bidi, &status);
    int ok = U_SUCCESS(status);
    if (ok && expectedParaLevel >= 0 && ubidi_getParaLevel(bidi) != expectedParaLevel) ok = 0;
    UBiDiDirection direction = ubidi_getDirection(bidi);
    for (int i = 0; ok && i < text->count; i++) {
        if (levels[i] < 0) continue;
        int32_t last = (i + 1 < text->count ? text->firstUnit[i + 1] : text->unitCount) - 1;
        if (actual[last] != levels[i] && !(direction != UBIDI_MIXED && actual[last] == ubidi_getParaLevel(bidi))) ok = 0;
    }
    if (ok) {
        int32_t visual[512];
        ubidi_getVisualMap(bidi, visual, &status);
        int n = 0;
        int seen[256] = { 0 };
        for (int32_t v = 0; ok && v < text->unitCount; v++) {
            int32_t unit = visual[v];
            int cp = 0;
            while (cp + 1 < text->count && text->firstUnit[cp + 1] <= unit) cp++;
            if (text->removed[cp] || seen[cp]) continue;
            seen[cp] = 1;
            if (n >= orderCount || order[n] != cp) ok = 0;
            n++;
        }
        if (n != orderCount) ok = 0;
    }
    ubidi_close(bidi);
    return ok;
}

static int parseLevels(const char *field, int *levels)
{
    int n = 0;
    while (*field && *field != ';' && *field != '\n') {
        if (*field == 'x') { levels[n++] = -1; field++; continue; }
        if (*field >= '0' && *field <= '9') { levels[n++] = (int)strtol(field, (char **)&field, 10); continue; }
        field++;
    }
    return n;
}

static int bidiTest(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) { printf("no %s\n", path); return 0; }
    char line[4096];
    int levels[256], order[256], orderCount = 0, cases = 0, failures = 0;
    while (fgets(line, sizeof line, file)) {
        if (!strncmp(line, "@Levels:", 8)) { parseLevels(line + 8, levels); continue; }
        if (!strncmp(line, "@Reorder:", 9)) { orderCount = parseLevels(line + 9, order); continue; }
        if (line[0] == '#' || line[0] == '@' || line[0] == '\n') continue;
        Text text = { .unitCount = 0, .count = 0 };
        char *at = line;
        while (*at && *at != ';') {
            while (*at == ' ' || *at == '\t') at++;
            if (*at == ';') break;
            int c;
            for (c = 0; c < 23; c++) {
                size_t length = strlen(names[c]);
                if (!strncmp(at, names[c], length) && (at[length] == ' ' || at[length] == '\t' || at[length] == ';')) break;
            }
            add(&text, representative[c], isRemovedClass(c));
            at += strlen(names[c]);
        }
        int bits = (int)strtol(at + 1, NULL, 16);
        static const UBiDiLevel directions[] = { UBIDI_DEFAULT_LTR, 0, 1 };
        for (int d = 0; d < 3; d++) {
            if (!(bits & (1 << d))) continue;
            cases++;
            if (!check(&text, directions[d], -1, levels, order, orderCount)) {
                if (failures < 10) printf("BidiTest.txt: %s", line);
                failures++;
            }
        }
    }
    fclose(file);
    printf("BidiTest.txt: %d of %d\n", cases - failures, cases);
    return failures == 0;
}

static int characterTest(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) { printf("no %s\n", path); return 0; }
    char line[8192];
    int cases = 0, failures = 0;
    while (fgets(line, sizeof line, file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *fields[5];
        fields[0] = line;
        for (int f = 1; f < 5; f++) {
            fields[f] = strchr(fields[f - 1], ';');
            if (!fields[f]) break;
            *fields[f]++ = 0;
        }
        Text text = { .unitCount = 0, .count = 0 };
        char *at = fields[0];
        while (*at) {
            char *end;
            long c = strtol(at, &end, 16);
            if (end == at) { at++; continue; }
            int u = 0;
            /* (Which characters X9 removes: the test's x levels say.) */
            add(&text, (UChar32)c, u);
            at = end;
        }
        int levels[256], order[256];
        parseLevels(fields[3], levels);
        for (int i = 0; i < text.count; i++) text.removed[i] = levels[i] < 0;
        int orderCount = parseLevels(fields[4], order);
        int direction = atoi(fields[1]);
        UBiDiLevel paraLevel = direction == 2 ? UBIDI_DEFAULT_LTR : (UBiDiLevel)direction;
        cases++;
        if (!check(&text, paraLevel, atoi(fields[2]), levels, order, orderCount)) {
            if (failures < 10) printf("BidiCharacterTest.txt: %s;%s;%s;%s;%s", fields[0], fields[1], fields[2], fields[3], fields[4]);
            failures++;
        }
    }
    fclose(file);
    printf("BidiCharacterTest.txt: %d of %d\n", cases - failures, cases);
    return failures == 0;
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "std/registry/unicode/ucd";
    char path[1024];
    snprintf(path, sizeof path, "%s/BidiTest.txt", directory);
    int ok = bidiTest(path);
    snprintf(path, sizeof path, "%s/BidiCharacterTest.txt", directory);
    ok &= characterTest(path);
    return ok ? 0 : 1;
}
