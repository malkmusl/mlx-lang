/* The headers made of the API's Mlx modules, in C and C++ (this file is
   compiled as both): every header together, and the macros as C code
   uses them (indexes of other integer types, `char` strings), doing what
   tests/api.mlx checks of the Mlx they were made from. */
#include <stdio.h>
#include <string.h>
#include <unicode/umachine.h>
#include <unicode/utypes.h>
#include <unicode/uversion.h>
#include <unicode/uconfig.h>
#include <unicode/utf.h>
#include <unicode/utf8.h>
#include <unicode/utf16.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>
#include <unicode/parseerr.h>
#include <unicode/uloc.h>
#include <unicode/uenum.h>
#include <unicode/uiter.h>
#include <unicode/ubrk.h>
#include <unicode/unorm2.h>
#include <unicode/uidna.h>
#include <unicode/uset.h>
#include <unicode/ucol.h>
#include <unicode/ucal.h>

int main(void)
{
    const char text[] = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\xff" "b";
    const UChar32 expected[] = { 0x61, 0xe9, 0x20ac, 0x1f600, -1, 0x62 };
    size_t i = 0, n = 0;
    while (i < 12) {
        UChar32 c;
        U8_NEXT(text, i, 12, c);
        if (c != expected[n++]) return 1;
    }
    int32_t back = 12;
    UChar32 c;
    U8_PREV(text, 0, back, c);
    if (c != 0x62 || back != 11) return 3;
    UChar units[8];
    int32_t length = 0;
    UBool failed = 0;
    U16_APPEND(units, length, 8, 0x61, failed);
    U16_APPEND(units, length, 8, 0x1f600, failed);
    if (length != 3 || failed) return 5;
    unsigned j = 0;
    U16_NEXT(units, j, length, c);
    U16_NEXT(units, j, length, c);
    if (c != 0x1f600 || j != 3) return 7;
    if (!U16_IS_LEAD(units[1]) || U16_IS_LEAD(units[2]) || U16_LENGTH(0x1f600) != 2) return 8;
    if (!U_SUCCESS(U_ZERO_ERROR) || !U_FAILURE(U_BUFFER_OVERFLOW_ERROR)) return 9;
    if (U_GET_GC_MASK('A') != U_GC_LU_MASK || !(U_GC_L_MASK & U_MASK(U_LOWERCASE_LETTER))) return 10;
    UText ut = UTEXT_INITIALIZER;
    if (ut.magic != UTEXT_MAGIC || ut.sizeOfStruct != (int32_t)sizeof(UText)) return 11;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    if (info.size != (int16_t)sizeof(UIDNAInfo)) return 12;
    char out[16];
    int32_t written = 0;
    U8_APPEND_UNSAFE(out, written, 0x20ac);
    if (written != 3 || memcmp(out, "\xe2\x82\xac", 3)) return 13;
    printf("macros: ok (%s, Unicode %s)\n", U_ICU_VERSION, U_UNICODE_VERSION);
    return 0;
}
