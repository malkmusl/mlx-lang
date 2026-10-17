/* mlx-unicode: internationalized domain names (UTS #46, with Punycode,
   RFC 3492). */
#ifndef MLX_UNICODE_UIDNA_H
#define MLX_UNICODE_UIDNA_H

#include <unicode/utypes.h>

enum {
    UIDNA_DEFAULT = 0,
    UIDNA_ALLOW_UNASSIGNED = 1,
    UIDNA_USE_STD3_RULES = 2,
    UIDNA_CHECK_BIDI = 4,
    UIDNA_CHECK_CONTEXTJ = 8,
    UIDNA_NONTRANSITIONAL_TO_ASCII = 0x10,
    UIDNA_NONTRANSITIONAL_TO_UNICODE = 0x20,
    UIDNA_CHECK_CONTEXTO = 0x40
};

struct UIDNA;
typedef struct UIDNA UIDNA;

typedef struct UIDNAInfo {
    int16_t size;
    UBool isTransitionalDifferent;
    UBool reservedB3;
    uint32_t errors;
    int32_t reservedI2;
    int32_t reservedI3;
} UIDNAInfo;

#define UIDNA_INFO_INITIALIZER { (int16_t)sizeof(UIDNAInfo), false, false, 0, 0, 0 }

enum {
    UIDNA_ERROR_EMPTY_LABEL = 1,
    UIDNA_ERROR_LABEL_TOO_LONG = 2,
    UIDNA_ERROR_DOMAIN_NAME_TOO_LONG = 4,
    UIDNA_ERROR_LEADING_HYPHEN = 8,
    UIDNA_ERROR_TRAILING_HYPHEN = 0x10,
    UIDNA_ERROR_HYPHEN_3_4 = 0x20,
    UIDNA_ERROR_LEADING_COMBINING_MARK = 0x40,
    UIDNA_ERROR_DISALLOWED = 0x80,
    UIDNA_ERROR_PUNYCODE = 0x100,
    UIDNA_ERROR_LABEL_HAS_DOT = 0x200,
    UIDNA_ERROR_INVALID_ACE_LABEL = 0x400,
    UIDNA_ERROR_BIDI = 0x800,
    UIDNA_ERROR_CONTEXTJ = 0x1000,
    UIDNA_ERROR_CONTEXTO_PUNCTUATION = 0x2000,
    UIDNA_ERROR_CONTEXTO_DIGITS = 0x4000
};

U_CDECL_BEGIN
U_CAPI UIDNA *uidna_openUTS46(uint32_t options, UErrorCode *pErrorCode);
U_CAPI void uidna_close(UIDNA *idna);
U_CAPI int32_t uidna_labelToASCII(const UIDNA *idna, const UChar *label, int32_t length, UChar *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CAPI int32_t uidna_labelToUnicode(const UIDNA *idna, const UChar *label, int32_t length, UChar *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CAPI int32_t uidna_nameToASCII(const UIDNA *idna, const UChar *name, int32_t length, UChar *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CAPI int32_t uidna_nameToUnicode(const UIDNA *idna, const UChar *name, int32_t length, UChar *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CAPI int32_t uidna_nameToASCII_UTF8(const UIDNA *idna, const char *name, int32_t length, char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CAPI int32_t uidna_nameToUnicodeUTF8(const UIDNA *idna, const char *name, int32_t length, char *dest, int32_t capacity, UIDNAInfo *pInfo, UErrorCode *pErrorCode);
U_CDECL_END

#endif
