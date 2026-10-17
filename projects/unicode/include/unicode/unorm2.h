/* mlx-unicode: normalization (UAX #15): NFC, NFD, NFKC, NFKD and NFKC
   with case folding. */
#ifndef MLX_UNICODE_UNORM2_H
#define MLX_UNICODE_UNORM2_H

#include <unicode/utypes.h>

typedef enum UNormalization2Mode {
    UNORM2_COMPOSE,
    UNORM2_DECOMPOSE,
    UNORM2_FCD,
    UNORM2_COMPOSE_CONTIGUOUS
} UNormalization2Mode;

typedef enum UNormalizationCheckResult {
    UNORM_NO,
    UNORM_YES,
    UNORM_MAYBE
} UNormalizationCheckResult;

struct UNormalizer2;
typedef struct UNormalizer2 UNormalizer2;

U_CDECL_BEGIN
U_CAPI const UNormalizer2 *unorm2_getNFCInstance(UErrorCode *pErrorCode);
U_CAPI const UNormalizer2 *unorm2_getNFDInstance(UErrorCode *pErrorCode);
U_CAPI const UNormalizer2 *unorm2_getNFKCInstance(UErrorCode *pErrorCode);
U_CAPI const UNormalizer2 *unorm2_getNFKDInstance(UErrorCode *pErrorCode);
U_CAPI const UNormalizer2 *unorm2_getNFKCCasefoldInstance(UErrorCode *pErrorCode);
U_CAPI const UNormalizer2 *unorm2_getInstance(const char *packageName, const char *name, UNormalization2Mode mode, UErrorCode *pErrorCode);
U_CAPI void unorm2_close(UNormalizer2 *norm2);
U_CAPI int32_t unorm2_normalize(const UNormalizer2 *norm2, const UChar *src, int32_t length, UChar *dest, int32_t capacity, UErrorCode *pErrorCode);
U_CAPI int32_t unorm2_normalizeSecondAndAppend(const UNormalizer2 *norm2, UChar *first, int32_t firstLength, int32_t firstCapacity, const UChar *second, int32_t secondLength, UErrorCode *pErrorCode);
U_CAPI int32_t unorm2_append(const UNormalizer2 *norm2, UChar *first, int32_t firstLength, int32_t firstCapacity, const UChar *second, int32_t secondLength, UErrorCode *pErrorCode);
U_CAPI int32_t unorm2_getDecomposition(const UNormalizer2 *norm2, UChar32 c, UChar *decomposition, int32_t capacity, UErrorCode *pErrorCode);
U_CAPI int32_t unorm2_getRawDecomposition(const UNormalizer2 *norm2, UChar32 c, UChar *decomposition, int32_t capacity, UErrorCode *pErrorCode);
U_CAPI UChar32 unorm2_composePair(const UNormalizer2 *norm2, UChar32 a, UChar32 b);
U_CAPI uint8_t unorm2_getCombiningClass(const UNormalizer2 *norm2, UChar32 c);
U_CAPI UBool unorm2_isNormalized(const UNormalizer2 *norm2, const UChar *s, int32_t length, UErrorCode *pErrorCode);
U_CAPI UNormalizationCheckResult unorm2_quickCheck(const UNormalizer2 *norm2, const UChar *s, int32_t length, UErrorCode *pErrorCode);
U_CAPI int32_t unorm2_spanQuickCheckYes(const UNormalizer2 *norm2, const UChar *s, int32_t length, UErrorCode *pErrorCode);
U_CAPI UBool unorm2_hasBoundaryBefore(const UNormalizer2 *norm2, UChar32 c);
U_CAPI UBool unorm2_hasBoundaryAfter(const UNormalizer2 *norm2, UChar32 c);
U_CAPI UBool unorm2_isInert(const UNormalizer2 *norm2, UChar32 c);
U_CDECL_END

#endif
