/* mlx-unicode: versions. The API level is ICU 74's (what WebKit and the
   programs built here check for); the data is Unicode 16.0. */
#ifndef MLX_UNICODE_UVERSION_H
#define MLX_UNICODE_UVERSION_H

#include <unicode/umachine.h>
#include <unicode/uvernum.h>

#define U_COPYRIGHT_STRING ""

U_CDECL_BEGIN
U_CAPI void u_versionFromString(UVersionInfo versionArray, const char *versionString);
U_CAPI void u_versionFromUString(UVersionInfo versionArray, const UChar *versionString);
U_CAPI void u_versionToString(const UVersionInfo versionArray, char *versionString);
U_CAPI void u_getVersion(UVersionInfo versionArray);
U_CDECL_END

#endif
