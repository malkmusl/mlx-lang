/* Case mapping, folding and UTF-8 of strings, and their buffers. */
#include <stdio.h>
#include <string.h>
#include <unicode/ustring.h>
static void show(const char *what, const UChar *s, int32_t n, UErrorCode e) {
    char out[256]; int32_t len = 0; UErrorCode e2 = U_ZERO_ERROR;
    u_strToUTF8(out, sizeof out, &len, s, n, &e2);
    printf("%s: [%s] len=%d status=%s\n", what, out, n, u_errorName(e));
}
int main(void) {
    UChar src[64], dst[64]; int32_t n; UErrorCode e;
    const char *t = "Straße ΣΑΣ Istanbul ǅ ﬁ";
    e = U_ZERO_ERROR; u_strFromUTF8(src, 64, &n, t, -1, &e);
    int32_t sn = n;
    e = U_ZERO_ERROR; n = u_strToUpper(dst, 64, src, sn, "", &e); show("upper", dst, n, e);
    e = U_ZERO_ERROR; n = u_strToLower(dst, 64, src, sn, "", &e); show("lower", dst, n, e);
    e = U_ZERO_ERROR; n = u_strToLower(dst, 64, src, sn, "tr", &e); show("lower tr", dst, n, e);
    e = U_ZERO_ERROR; n = u_strToUpper(dst, 64, src, sn, "tr_TR", &e); show("upper tr", dst, n, e);
    e = U_ZERO_ERROR; n = u_strFoldCase(dst, 64, src, sn, U_FOLD_CASE_DEFAULT, &e); show("fold", dst, n, e);
    e = U_ZERO_ERROR; n = u_strToUpper(dst, 5, src, sn, "", &e); printf("overflow: n=%d %s\n", n, u_errorName(e));
    e = U_ZERO_ERROR; n = u_strToUpper(NULL, 0, src, sn, "", &e); printf("preflight: n=%d %s\n", n, u_errorName(e));
    UChar a[8], b[8]; e = U_ZERO_ERROR; u_strFromUTF8(a, 8, NULL, "STRASSE", -1, &e); u_strFromUTF8(b, 8, NULL, "straße", -1, &e);
    printf("casecmp=%d strcmp=%d len=%d count32=%d\n", u_strcasecmp(a, b, 0), u_strcmp(a, b) > 0, u_strlen(a), u_countChar32(src, sn));
    char bad[] = "a\xC3\x28z\xF0\x9F\x98\x80"; int32_t subs = 0;
    e = U_ZERO_ERROR; u_strFromUTF8(dst, 64, &n, bad, -1, &e); printf("bad utf8: %s\n", u_errorName(e));
    e = U_ZERO_ERROR; u_strFromUTF8WithSub(dst, 64, &n, bad, -1, 0xFFFD, &subs, &e); printf("withsub n=%d subs=%d %04x %04x %04x %04x %04x %04x\n", n, subs, dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
    return 0;
}
