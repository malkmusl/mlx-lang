/* mlx-unicode: strings of UTF-16 code units. A function that writes a
   string returns its whole length: when it does not fit in `capacity`
   it sets U_BUFFER_OVERFLOW_ERROR, when it fits exactly no NUL follows
   (U_STRING_NOT_TERMINATED_WARNING), else a NUL does. A length of -1
   means the string ends at its NUL. */
#ifndef MLX_UNICODE_USTRING_H
#define MLX_UNICODE_USTRING_H

#include <unicode/utypes.h>

#ifndef UBRK_TYPEDEF_UBREAK_ITERATOR
#define UBRK_TYPEDEF_UBREAK_ITERATOR
typedef struct UBreakIterator UBreakIterator;
#endif

#define U_COMPARE_CODE_POINT_ORDER 0x8000
#define U_COMPARE_IGNORE_CASE 0x10000
#define U_TITLECASE_NO_LOWERCASE 0x100
#define U_TITLECASE_NO_BREAK_ADJUSTMENT 0x200
#define U_TITLECASE_ADJUST_TO_CASED 0x400
#define U_TITLECASE_SENTENCES 0x800
#define U_TITLECASE_WHOLE_STRING 0x20
#define U_FOLD_CASE_DEFAULT 0
#define U_FOLD_CASE_EXCLUDE_SPECIAL_I 1

U_CDECL_BEGIN
U_CAPI int32_t u_strlen(const UChar *s);
U_CAPI int32_t u_countChar32(const UChar *s, int32_t length);
U_CAPI UChar *u_strcat(UChar *dst, const UChar *src);
U_CAPI UChar *u_strncat(UChar *dst, const UChar *src, int32_t n);
U_CAPI UChar *u_strchr(const UChar *s, UChar c);
U_CAPI UChar *u_strchr32(const UChar *s, UChar32 c);
U_CAPI UChar *u_strrchr(const UChar *s, UChar c);
U_CAPI UChar *u_memchr(const UChar *s, UChar c, int32_t count);
U_CAPI UChar *u_strstr(const UChar *s, const UChar *substring);
U_CAPI int32_t u_strcmp(const UChar *s1, const UChar *s2);
U_CAPI int32_t u_strncmp(const UChar *ucs1, const UChar *ucs2, int32_t n);
U_CAPI int32_t u_strCompare(const UChar *s1, int32_t length1, const UChar *s2, int32_t length2, UBool codePointOrder);
U_CAPI int32_t u_strcmpCodePointOrder(const UChar *s1, const UChar *s2);
U_CAPI int32_t u_strCaseCompare(const UChar *s1, int32_t length1, const UChar *s2, int32_t length2, uint32_t options, UErrorCode *pErrorCode);
U_CAPI int32_t u_strcasecmp(const UChar *s1, const UChar *s2, uint32_t options);
U_CAPI int32_t u_strncasecmp(const UChar *s1, const UChar *s2, int32_t n, uint32_t options);
U_CAPI int32_t u_memcasecmp(const UChar *s1, const UChar *s2, int32_t length, uint32_t options);
U_CAPI UChar *u_strcpy(UChar *dst, const UChar *src);
U_CAPI UChar *u_strncpy(UChar *dst, const UChar *src, int32_t n);
U_CAPI UChar *u_uastrcpy(UChar *dst, const char *src);
U_CAPI UChar *u_uastrncpy(UChar *dst, const char *src, int32_t n);
U_CAPI char *u_austrcpy(char *dst, const UChar *src);
U_CAPI char *u_austrncpy(char *dst, const UChar *src, int32_t n);
U_CAPI UChar *u_memcpy(UChar *dest, const UChar *src, int32_t count);
U_CAPI UChar *u_memmove(UChar *dest, const UChar *src, int32_t count);
U_CAPI UChar *u_memset(UChar *dest, UChar c, int32_t count);
U_CAPI int32_t u_memcmp(const UChar *buf1, const UChar *buf2, int32_t count);
U_CAPI int32_t u_memcmpCodePointOrder(const UChar *s1, const UChar *s2, int32_t count);
U_CAPI int32_t u_unescape(const char *src, UChar *dest, int32_t destCapacity);

U_CAPI int32_t u_strToUpper(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *pErrorCode);
U_CAPI int32_t u_strToLower(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *pErrorCode);
U_CAPI int32_t u_strToTitle(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, UBreakIterator *titleIter, const char *locale, UErrorCode *pErrorCode);
U_CAPI int32_t u_strFoldCase(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, uint32_t options, UErrorCode *pErrorCode);

U_CAPI char *u_strToUTF8(char *dest, int32_t destCapacity, int32_t *pDestLength, const UChar *src, int32_t srcLength, UErrorCode *pErrorCode);
U_CAPI UChar *u_strFromUTF8(UChar *dest, int32_t destCapacity, int32_t *pDestLength, const char *src, int32_t srcLength, UErrorCode *pErrorCode);
U_CAPI char *u_strToUTF8WithSub(char *dest, int32_t destCapacity, int32_t *pDestLength, const UChar *src, int32_t srcLength, UChar32 subchar, int32_t *pNumSubstitutions, UErrorCode *pErrorCode);
U_CAPI UChar *u_strFromUTF8WithSub(UChar *dest, int32_t destCapacity, int32_t *pDestLength, const char *src, int32_t srcLength, UChar32 subchar, int32_t *pNumSubstitutions, UErrorCode *pErrorCode);
U_CAPI UChar *u_strFromUTF8Lenient(UChar *dest, int32_t destCapacity, int32_t *pDestLength, const char *src, int32_t srcLength, UErrorCode *pErrorCode);
U_CAPI UChar32 *u_strToUTF32(UChar32 *dest, int32_t destCapacity, int32_t *pDestLength, const UChar *src, int32_t srcLength, UErrorCode *pErrorCode);
U_CAPI UChar *u_strFromUTF32(UChar *dest, int32_t destCapacity, int32_t *pDestLength, const UChar32 *src, int32_t srcLength, UErrorCode *pErrorCode);
U_CDECL_END

#endif
