/* mlx-unicode: code points, and UTF-8 and UTF-16. */
#ifndef MLX_UNICODE_UTF_H
#define MLX_UNICODE_UTF_H

#include <unicode/umachine.h>

#define U_IS_UNICODE_NONCHAR(c) ((c) >= 0xfdd0 && ((c) <= 0xfdef || ((c) & 0xfffe) == 0xfffe) && (c) <= 0x10ffff)
#define U_IS_UNICODE_CHAR(c) ((uint32_t)(c) < 0xd800 || (0xdfff < (c) && (c) <= 0x10ffff && !U_IS_UNICODE_NONCHAR(c)))
#define U_IS_BMP(c) ((uint32_t)(c) <= 0xffff)
#define U_IS_SUPPLEMENTARY(c) ((uint32_t)((c) - 0x10000) <= 0xfffff)
#define U_IS_LEAD(c) (((c) & 0xfffffc00) == 0xd800)
#define U_IS_TRAIL(c) (((c) & 0xfffffc00) == 0xdc00)
#define U_IS_SURROGATE(c) (((c) & 0xfffff800) == 0xd800)
#define U_IS_SURROGATE_LEAD(c) (((c) & 0x400) == 0)
#define U_IS_SURROGATE_TRAIL(c) (((c) & 0x400) != 0)

#include <unicode/utf8.h>
#include <unicode/utf16.h>

#endif
