/* mlx-unicode: the basic types of the ICU C API (projects/unicode).
   The library is this repository's own, in Mlx; these headers give C
   and C++ programs written for ICU's API what they name. */
#ifndef MLX_UNICODE_UMACHINE_H
#define MLX_UNICODE_UMACHINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#define U_CDECL_BEGIN extern "C" {
#define U_CDECL_END }
#define U_CFUNC extern "C"
#else
#define U_CDECL_BEGIN
#define U_CDECL_END
#define U_CFUNC extern
#endif

#define U_EXPORT __attribute__((visibility("default")))
#define U_IMPORT
#define U_EXPORT2
#define U_CALLCONV
#define U_CALLCONV_FPTR
#define U_CAPI U_CFUNC U_EXPORT
#define U_STABLE U_CAPI
#define U_DRAFT U_CAPI
#define U_DEPRECATED U_CAPI
#define U_OBSOLETE U_CAPI
#define U_INTERNAL U_CAPI
#define U_COMMON_API
#define U_I18N_API
#define U_INLINE inline
#define U_NOEXCEPT noexcept
#define U_FALLTHROUGH __attribute__((fallthrough))
#define U_ATTRIBUTE_DEPRECATED
#define U_DISABLE_RENAMING 1
#define U_HAVE_RVALUE_REFERENCES 1
#define U_IS_BIG_ENDIAN 0
#define U_CHARSET_FAMILY 0
#define U_ASCII_FAMILY 0
#define U_SIZEOF_UCHAR 2
#define U_SIZEOF_WCHAR_T 4
#define U_HAVE_WCHAR_H 1
#define U_PLATFORM_IS_LINUX_BASED 1
#define U_PF_LINUX 4000
#define U_PLATFORM U_PF_LINUX

#define UPRV_BLOCK_MACRO_BEGIN do
#define UPRV_BLOCK_MACRO_END while (0)

#ifdef __cplusplus
#define U_NAMESPACE_BEGIN namespace icu {
#define U_NAMESPACE_END }
#define U_NAMESPACE_USE using namespace icu;
#define U_NAMESPACE_QUALIFIER icu::
#define U_ICU_NAMESPACE icu
namespace icu {}
#endif

typedef int8_t UBool;
#ifndef U_DEFINE_FALSE_AND_TRUE
#define U_DEFINE_FALSE_AND_TRUE 0
#endif

#ifdef __cplusplus
typedef char16_t UChar;
#else
typedef uint16_t UChar;
#endif
typedef int32_t UChar32;
typedef uint8_t UVersionInfo[4];
typedef double UDate;

#define U_SENTINEL (-1)
#define UCHAR_MIN_VALUE 0
#define UCHAR_MAX_VALUE 0x10ffff
#define U_MAX_VERSION_LENGTH 4
#define U_MAX_VERSION_STRING_LENGTH 20
#define U_MILLIS_PER_SECOND (1000)
#define U_MILLIS_PER_MINUTE (60000)
#define U_MILLIS_PER_HOUR (3600000)
#define U_MILLIS_PER_DAY (86400000)

#define U_INT8_MIN INT8_MIN
#define U_INT8_MAX INT8_MAX
#define U_INT16_MIN INT16_MIN
#define U_INT16_MAX INT16_MAX
#define U_INT32_MIN INT32_MIN
#define U_INT32_MAX INT32_MAX
#define U_INT64_MIN INT64_MIN
#define U_INT64_MAX INT64_MAX
#define U_UINT8_MAX UINT8_MAX
#define U_UINT16_MAX UINT16_MAX
#define U_UINT32_MAX UINT32_MAX
#define U_UINT64_MAX UINT64_MAX

#define U_STRING_DECL(var, cs, length) static const UChar var[(length) + 1] = u##cs
#define U_STRING_INIT(var, cs, length)

#endif
