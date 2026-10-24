/* Sets of code points (uset.h) from patterns, ranges and other sets, as
   WebCore's SmartReplace.cpp builds them; titlecasing (u_strToTitle) as
   RenderText's `text-transform: capitalize` asks for it. */
#include <stdio.h>
#include <string.h>
#include <unicode/uset.h>
#include <unicode/ustring.h>

/* (The sizes of sets of properties depend on the Unicode version: those
   of literal sets only are printed.) */
static USet *pattern(const char *text, int sizes) {
    UChar buffer[128];
    int32_t length = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(buffer, 128, &length, text, -1, &status);
    USet *set = uset_openPattern(buffer, length, &status);
    printf("%s: %s", text, u_errorName(status));
    if (set && sizes) printf(" size=%d ranges=%d", uset_size(set), uset_getRangeCount(set));
    printf("\n");
    return set;
}

static void members(USet *set, const UChar32 *points, int count) {
    for (int i = 0; i < count; i++) printf("%c", uset_contains(set, points[i]) ? '1' : '0');
    printf("\n");
}

static void title(const char *text, const char *locale) {
    UChar src[64], dst[64];
    int32_t n = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(src, 64, &n, text, -1, &status);
    int32_t length = u_strToTitle(dst, 64, src, n, NULL, locale, &status);
    char out[128];
    UErrorCode status2 = U_ZERO_ERROR;
    u_strToUTF8(out, sizeof out, NULL, dst, length, &status2);
    printf("title [%s] %s: [%s] %d %s\n", text, locale, out, length, U_FAILURE(status) ? u_errorName(status) : "ok");
}

int main(void) {
    static const UChar32 points[] = { ' ', '\n', 0x0B, 0x85, 0xA0, 0x2028, 'a', '(', ')', '.', '-', '!', 0x3000, 0x4E00, 0x1100, 0x20000, 0x00BF, 0x2E3A, '$', '+' };
    const int count = sizeof points / sizeof *points;
    USet *smart = pattern("[[:WSpace:] [\\u000A\\u000B\\u000C\\u000D\\u0085]]", 1);
    members(smart, points, count);
    uset_addRange(smart, 0x1100, 0x1100 + 256);
    uset_addRange(smart, 0x3200, 0x3200 + 29392);
    uset_addRange(smart, 0x20000, 0x20000 + 0xA6D7);
    const char *pre = "([\"'#$/-`{";
    for (const char *c = pre; *c; c++) uset_add(smart, *c);
    members(smart, points, count);
    USet *punctuation = pattern("[:P:]", 0);
    members(punctuation, points, count);
    uset_addAll(smart, punctuation);
    uset_close(punctuation);
    members(smart, points, count);
    uset_close(smart);

    USet *set = pattern("[a-z\\u00E0-\\u00E5 \\x41 \\U0001F600]", 1);
    members(set, points, count);
    uset_close(set);
    set = pattern("[^\\p{L}\\p{N}]", 0);
    members(set, points, count);
    uset_close(set);
    set = pattern("\\p{Script=Han}", 0);
    members(set, points, count);
    uset_close(set);
    set = pattern("[[:^Zs:] [x]", 0);
    if (set) uset_close(set);
    set = pattern("[a-", 0);
    if (set) uset_close(set);
    set = pattern("[:Nonsense:]", 0);
    if (set) uset_close(set);

    title("hello", "");
    title("hELLO wORLD", "");
    title("ijssel", "nl");
    title("ijssel", "en");
    title("(quoted) words", "");
    title("ǆungla", "");
    title("straße", "");
    title("123abc", "");
    return 0;
}
