/* mlx-unicode: UText, text a provider hands out in chunks of UTF-16
   (the layout ICU defines: programs fill these structures themselves). */
#ifndef MLX_UNICODE_UTEXT_H
#define MLX_UNICODE_UTEXT_H

#include <unicode/utypes.h>

struct UText;
typedef struct UText UText;

typedef UText *UTextClone(UText *dest, const UText *src, UBool deep, UErrorCode *status);
typedef int64_t UTextNativeLength(UText *ut);
typedef UBool UTextAccess(UText *ut, int64_t nativeIndex, UBool forward);
typedef int32_t UTextExtract(UText *ut, int64_t nativeStart, int64_t nativeLimit, UChar *dest, int32_t destCapacity, UErrorCode *status);
typedef int32_t UTextReplace(UText *ut, int64_t nativeStart, int64_t nativeLimit, const UChar *replacementText, int32_t replacmentLength, UErrorCode *status);
typedef void UTextCopy(UText *ut, int64_t nativeStart, int64_t nativeLimit, int64_t nativeDest, UBool move, UErrorCode *status);
typedef int64_t UTextMapOffsetToNative(const UText *ut);
typedef int32_t UTextMapNativeIndexToUTF16(const UText *ut, int64_t nativeIndex);
typedef void UTextClose(UText *ut);

struct UTextFuncs {
    int32_t tableSize;
    int32_t reserved1, reserved2, reserved3;
    UTextClone *clone;
    UTextNativeLength *nativeLength;
    UTextAccess *access;
    UTextExtract *extract;
    UTextReplace *replace;
    UTextCopy *copy;
    UTextMapOffsetToNative *mapOffsetToNative;
    UTextMapNativeIndexToUTF16 *mapNativeIndexToUTF16;
    UTextClose *close;
    UTextClose *spare1;
    UTextClose *spare2;
    UTextClose *spare3;
};
typedef struct UTextFuncs UTextFuncs;

struct UText {
    uint32_t magic;
    int32_t flags;
    int32_t providerProperties;
    int32_t sizeOfStruct;
    int64_t chunkNativeLimit;
    int32_t extraSize;
    int32_t nativeIndexingLimit;
    int64_t chunkNativeStart;
    int32_t chunkOffset;
    int32_t chunkLength;
    const UChar *chunkContents;
    const UTextFuncs *pFuncs;
    void *pExtra;
    const void *context;
    const void *p;
    const void *q;
    const void *r;
    void *privP;
    int64_t a;
    int64_t b;
    int64_t c;
    int64_t privA;
    int64_t privB;
    int64_t privC;
};

enum {
    UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE = 1,
    UTEXT_PROVIDER_STABLE_CHUNKS = 2,
    UTEXT_PROVIDER_WRITABLE = 3,
    UTEXT_PROVIDER_HAS_META_DATA = 4,
    UTEXT_PROVIDER_OWNS_TEXT = 5
};

enum { UTEXT_MAGIC = 0x345ad82c };

#define UTEXT_INITIALIZER { UTEXT_MAGIC, 0, 0, sizeof(UText), 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0 }

#define U_TEXT_SENTINEL (-1)

U_CDECL_BEGIN
U_CAPI UText *utext_close(UText *ut);
U_CAPI UText *utext_setup(UText *ut, int32_t extraSpace, UErrorCode *status);
U_CAPI UText *utext_openUChars(UText *ut, const UChar *s, int64_t length, UErrorCode *status);
U_CAPI UText *utext_openUTF8(UText *ut, const char *s, int64_t length, UErrorCode *status);
U_CAPI UText *utext_clone(UText *dest, const UText *src, UBool deep, UBool readOnly, UErrorCode *status);
U_CAPI int64_t utext_nativeLength(UText *ut);
U_CAPI UChar32 utext_char32At(UText *ut, int64_t nativeIndex);
U_CAPI UChar32 utext_current32(UText *ut);
U_CAPI UChar32 utext_next32(UText *ut);
U_CAPI UChar32 utext_previous32(UText *ut);
U_CAPI UChar32 utext_next32From(UText *ut, int64_t nativeIndex);
U_CAPI UChar32 utext_previous32From(UText *ut, int64_t nativeIndex);
U_CAPI int64_t utext_getNativeIndex(const UText *ut);
U_CAPI void utext_setNativeIndex(UText *ut, int64_t nativeIndex);
U_CAPI int32_t utext_extract(UText *ut, int64_t nativeStart, int64_t nativeLimit, UChar *dest, int32_t destCapacity, UErrorCode *status);
U_CDECL_END

#endif
