/* mlx-unicode: what the library does (everything is on; it has no
   conversion tables of legacy charsets beyond those it names, see
   ucnv.h). */
#ifndef MLX_UNICODE_UCONFIG_H
#define MLX_UNICODE_UCONFIG_H

#include <unicode/platform.h>

#define UCONFIG_ONLY_COLLATION 0
#define UCONFIG_NO_BREAK_ITERATION 0
#define UCONFIG_NO_IDNA 0
#define UCONFIG_NO_FORMATTING 0
#define UCONFIG_NO_TRANSLITERATION 1
#define UCONFIG_NO_REGULAR_EXPRESSIONS 1
#define UCONFIG_NO_SERVICE 1
#define UCONFIG_NO_FILE_IO 1
#define UCONFIG_NO_LEGACY_CONVERSION 0
#define UCONFIG_NO_CONVERSION 0
#define UCONFIG_NO_NORMALIZATION 0
#define UCONFIG_NO_COLLATION 0
#define UCONFIG_HAVE_PARSEALLINPUT 1
#define UCONFIG_FORMAT_FASTPATHS_49 1
#define UCONFIG_NO_FILTERED_BREAK_ITERATION 1
#define UCONFIG_USE_LOCAL 0

#endif
