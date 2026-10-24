/* usearch.h: string search by collation, as WebKit's find in page uses it
   (one searcher of "<locale>@collation=search", its collator's strength
   set to secondary or tertiary, the pattern's base letters wildcards when
   case-insensitive, then pattern, text, offset and next) and the rest of
   the API: every match forwards and backwards at each strength and
   element comparison, with overlap, from offsets, first/last,
   following/preceding, the matched text, an ignorable pattern, the
   errors. The texts mix case, accents (precomposed and combining),
   expansions (ß, æ, ﬁ), ignorables, Hangul, Arabic and Hebrew; random
   ones are made of the same pieces. search.expected is what ICU 74 prints
   (the program built with gcc against the system's ICU). */
#include <stdio.h>
#include <string.h>
#include <unicode/ucol.h>
#include <unicode/usearch.h>
#include <unicode/ustring.h>

static void toUnits(const char *utf8, UChar *out, int32_t capacity, int32_t *length)
{
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(out, capacity, length, utf8, -1, &status);
}

static void showUnits(const UChar *units, int32_t length)
{
    char utf8[512];
    int32_t n = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strToUTF8(utf8, sizeof utf8, &n, units, length, &status);
    printf("%.*s", (int)n, utf8);
}

/* Every match forwards, then (unless `forwardOnly`) backwards. */
static void matches(UStringSearch *search, int forwardOnly)
{
    UErrorCode status = U_ZERO_ERROR;
    printf(" >");
    for (int32_t at = usearch_first(search, &status); at != USEARCH_DONE; at = usearch_next(search, &status))
        printf(" %d+%d", at, usearch_getMatchedLength(search));
    printf(" <");
    for (int32_t at = forwardOnly ? USEARCH_DONE : usearch_last(search, &status); at != USEARCH_DONE; at = usearch_previous(search, &status))
        printf(" %d+%d", at, usearch_getMatchedLength(search));
    /* (Errors only: ICU warns of default data where the library does not.) */
    printf(" s%d", U_FAILURE(status) ? status : 0);
}

static const char *strengthNames[] = { "primary", "secondary", "tertiary" };
static const UColAttributeValue strengths[] = { UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_IDENTICAL };

static void searchEach(const char *locale, const char *pattern, const char *text)
{
    UChar p[128], t[512];
    int32_t pn, tn;
    toUnits(pattern, p, 128, &pn);
    toUnits(text, t, 512, &tn);
    UErrorCode status = U_ZERO_ERROR;
    UStringSearch *search = usearch_open(p, pn, t, tn, locale, NULL, &status);
    if (U_FAILURE(status)) {
        printf("%s open: %d\n", locale, status);
        return;
    }
    /* (ICU warns of the default collation for some locales.) */
    status = U_ZERO_ERROR;
    UCollator *collator = usearch_getCollator(search);
    for (int s = 0; s < 4; s++) {
        for (int comparison = USEARCH_STANDARD_ELEMENT_COMPARISON; comparison <= USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD; comparison++) {
            ucol_setStrength(collator, strengths[s]);
            usearch_reset(search);
            usearch_setAttribute(search, USEARCH_ELEMENT_COMPARISON, (USearchAttributeValue)comparison, &status);
            printf("%s [%s] in [%s] %s/%d:", locale, pattern, text, s < 3 ? strengthNames[s] : "identical", comparison);
            matches(search, 0);
            printf("\n");
        }
    }
    usearch_close(search);
}

/* WebKit's way: one searcher, its pattern and text set for each find. */
static void webkitFind(UStringSearch *search, const char *pattern, const char *text, int caseInsensitive, int offset)
{
    UChar p[128], t[512];
    int32_t pn, tn;
    toUnits(pattern, p, 128, &pn);
    toUnits(text, t, 512, &tn);
    UErrorCode status = U_ZERO_ERROR;
    UCollator *collator = usearch_getCollator(search);
    UColAttributeValue strength = caseInsensitive ? UCOL_SECONDARY : UCOL_TERTIARY;
    if (ucol_getStrength(collator) != strength) {
        ucol_setStrength(collator, strength);
        usearch_reset(search);
    }
    usearch_setAttribute(search, USEARCH_ELEMENT_COMPARISON, caseInsensitive ? USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD : USEARCH_STANDARD_ELEMENT_COMPARISON, &status);
    usearch_setPattern(search, p, pn, &status);
    usearch_setText(search, t, tn, &status);
    usearch_setOffset(search, offset, &status);
    printf("find [%s] in [%s] %s from %d:", pattern, text, caseInsensitive ? "any case" : "exact", offset);
    for (int32_t at = usearch_next(search, &status); at != USEARCH_DONE; at = usearch_next(search, &status)) {
        UChar matched[128];
        UErrorCode e = U_ZERO_ERROR;
        int32_t n = usearch_getMatchedText(search, matched, 128, &e);
        printf(" %d+%d[", at, usearch_getMatchedLength(search));
        showUnits(matched, n);
        printf("]");
    }
    printf(" offset %d s%d\n", usearch_getOffset(search), U_FAILURE(status) ? status : 0);
}

static unsigned long long state = 2463534242ULL;
static unsigned next(unsigned limit)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return (unsigned)(state % limit);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *texts[][2] = {
        { "a", "A a á à Á ā" },
        { "resume", "Resume résumé RESUMÉ resumes" },
        { "e", "e\xcc\x81 é E\xcc\x81 ë e" },
        { "é", "e\xcc\x81 é E e" },
        { "strasse", "Straße STRASSE strasse Strase" },
        { "ss", "ßs ß sß" },
        { "ae", "Æ æ ae AE a" },
        { "fi", "ﬁne fine ﬂ" },
        { "ab", "a\xe2\x80\x8b" "b ab a-b a b" },
        { "a b", "a b a  b a\xc2\xa0" "b ab" },
        { "co-op", "coop co-op co op CO-OP" },
        { "한", "한국 한 한" },
        { "كتب", "كَتَبَ كتب كتاب" },
        { "שלום", "שָׁלוֹם שלום" },
        { "aa", "aaaa" },
        { "x", "y" },
        { "Ö", "o ö Ö oe" },
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        searchEach("en@collation=search", texts[i][0], texts[i][1]);
    }
    searchEach("de@collation=search", "o", "ö Ö o");
    searchEach("sv@collation=search", "o", "ö Ö o");
    searchEach("da", "aa", "å aa Aa");
    searchEach("", "a", "à la carte");

    /* Overlap, offsets, the matched text, an ignorable pattern. */
    UChar p[16], t[64];
    int32_t pn, tn;
    toUnits("aa", p, 16, &pn);
    toUnits("aaaa", t, 64, &tn);
    UErrorCode status = U_ZERO_ERROR;
    UStringSearch *search = usearch_open(p, pn, t, tn, "en", NULL, &status);
    usearch_setAttribute(search, USEARCH_OVERLAP, USEARCH_ON, &status);
    printf("overlap %d:", usearch_getAttribute(search, USEARCH_OVERLAP));
    matches(search, 0);
    printf("\n");
    usearch_setAttribute(search, USEARCH_OVERLAP, USEARCH_OFF, &status);
    printf("following(1) %d preceding(3) %d offset %d\n", usearch_following(search, 1, &status), usearch_preceding(search, 3, &status), usearch_getOffset(search));
    usearch_setOffset(search, 2, &status);
    printf("next from 2: %d, start %d\n", usearch_next(search, &status), usearch_getMatchedStart(search));
    usearch_setOffset(search, 9, &status);
    printf("offset 9: s%d\n", status);
    status = U_ZERO_ERROR;
    toUnits("\xe2\x80\x8b", p, 16, &pn);
    usearch_setPattern(search, p, pn, &status);
    printf("ignorable pattern:");
    for (int32_t at = usearch_first(search, &status); at != USEARCH_DONE; at = usearch_next(search, &status)) printf(" %d+%d", at, usearch_getMatchedLength(search));
    printf(" s%d\n", U_FAILURE(status) ? status : 0);
    status = U_ZERO_ERROR;
    usearch_setPattern(search, p, 0, &status);
    printf("empty pattern s%d\n", status);
    status = U_ZERO_ERROR;
    usearch_setText(search, t, 0, &status);
    printf("empty text s%d\n", status);
    status = U_ZERO_ERROR;
    printf("open with empty text %p s", (void *)usearch_open(p, 1, t, 0, "en", NULL, &status));
    printf("%d\n", status);
    usearch_close(search);

    /* WebKit's searcher, made as WebCore's ICUSearcher makes it. */
    UChar newline = '\n';
    status = U_ZERO_ERROR;
    UStringSearch *webkit = usearch_open(&newline, 1, &newline, 1, "en_US@collation=search", NULL, &status);
    printf("webkit searcher s%d\n", U_FAILURE(status) ? status : 0);
    webkitFind(webkit, "resume", "Resume résumé RESUMÉ resumes", 1, 0);
    webkitFind(webkit, "resume", "Resume résumé RESUMÉ resumes", 0, 0);
    webkitFind(webkit, "résumé", "Resume résumé RESUMÉ resumes", 1, 0);
    webkitFind(webkit, "Strasse", "Straße strasse", 1, 0);
    webkitFind(webkit, "the", "In the theory, THE thesis", 1, 4);
    webkitFind(webkit, "naïve", "naive naïve NAÏVE", 1, 0);
    webkitFind(webkit, "ﬁ", "fine ﬁne", 1, 0);
    webkitFind(webkit, "cafe", "café cafe\xcc\x81 CAFÉ", 1, 0);

    /* Random texts of the same pieces. */
    static const char *pieces[] = { "a", "A", "á", "a\xcc\x81", "b", "e", "é", "E", " ", "-", "ß", "ss", "æ", "ae", "\xe2\x80\x8b", "o", "ö", "ﬁ", "f", "i", "ب", "\xcc\x88" };
    static const char *locales[] = { "en@collation=search", "de", "sv", "" };
    for (int round = 0; round < 1500; round++) {
        char pattern[64] = "", text[256] = "";
        int pieceCount = 1 + next(3);
        for (int k = 0; k < pieceCount; k++) {
            const char *piece = pieces[next(sizeof pieces / sizeof pieces[0])];
            /* (Not a combining mark first: ICU 74 crashes on some of those
               patterns, which it means to refuse.) */
            while (k == 0 && !strcmp(piece, "\xcc\x88")) piece = pieces[next(sizeof pieces / sizeof pieces[0])];
            strcat(pattern, piece);
        }
        int textPieces = 1 + next(12);
        for (int k = 0; k < textPieces; k++) strcat(text, pieces[next(sizeof pieces / sizeof pieces[0])]);
        UChar rp[64], rt[256];
        int32_t rpn, rtn;
        toUnits(pattern, rp, 64, &rpn);
        toUnits(text, rt, 256, &rtn);
        status = U_ZERO_ERROR;
        const char *locale = locales[next(4)];
        UStringSearch *random = usearch_open(rp, rpn, rt, rtn, locale, NULL, &status);
        status = U_ZERO_ERROR;
        UColAttributeValue strength = strengths[next(3)];
        int comparison = USEARCH_STANDARD_ELEMENT_COMPARISON + (int)next(3);
        ucol_setStrength(usearch_getCollator(random), strength);
        usearch_reset(random);
        usearch_setAttribute(random, USEARCH_ELEMENT_COMPARISON, (USearchAttributeValue)comparison, &status);
        printf("%d %s [%s] in [%s] %d/%d:", round, locale, pattern, text, strength, comparison);
        /* (Forwards only: ICU 74's backward search, which WebKit does not
           use, leaves out a first match that is an expansion and takes
           ignorables after a match into it, in some of these.) */
        matches(random, 1);
        printf("\n");
        usearch_close(random);
    }
    {
        /* unorm2.h comes with usearch.h (ICU's ucol.h includes unorm.h,
           which includes it): WebKit's ICUSearcher normalizes so. */
        UErrorCode status = U_ZERO_ERROR;
        const UNormalizer2 *nfc = unorm2_getNFCInstance(&status);
        UChar decomposed[2] = { 0x65, 0x301 }, composed[4];
        int32_t length = unorm2_normalize(nfc, decomposed, 2, composed, 4, &status);
        printf("nfc %d %x %s\n", (int)length, composed[0], u_errorName(status));
    }
    return 0;
}
