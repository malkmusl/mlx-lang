/* mlx-unicode: scripts (Scripts.txt and ScriptExtensions.txt, and the
   ISO 15924 codes no code point has). */
#ifndef MLX_UNICODE_USCRIPT_H
#define MLX_UNICODE_USCRIPT_H

#include <unicode/utypes.h>
#include <unicode/ugenerated.h>

typedef enum UScriptUsage {
    USCRIPT_USAGE_NOT_ENCODED,
    USCRIPT_USAGE_UNKNOWN,
    USCRIPT_USAGE_EXCLUDED,
    USCRIPT_USAGE_LIMITED_USE,
    USCRIPT_USAGE_ASPIRATIONAL,
    USCRIPT_USAGE_RECOMMENDED
} UScriptUsage;

U_CDECL_BEGIN
U_CAPI int32_t uscript_getCode(const char *nameOrAbbrOrLocale, UScriptCode *fillIn, int32_t capacity, UErrorCode *err);
U_CAPI const char *uscript_getName(UScriptCode scriptCode);
U_CAPI const char *uscript_getShortName(UScriptCode scriptCode);
U_CAPI UScriptCode uscript_getScript(UChar32 codepoint, UErrorCode *err);
U_CAPI UBool uscript_hasScript(UChar32 c, UScriptCode sc);
U_CAPI int32_t uscript_getScriptExtensions(UChar32 c, UScriptCode *scripts, int32_t capacity, UErrorCode *errorCode);
U_CAPI UScriptUsage uscript_getUsage(UScriptCode script);
U_CAPI UBool uscript_isRightToLeft(UScriptCode script);
U_CAPI UBool uscript_breaksBetweenLetters(UScriptCode script);
U_CAPI UBool uscript_isCased(UScriptCode script);
U_CDECL_END

#endif
