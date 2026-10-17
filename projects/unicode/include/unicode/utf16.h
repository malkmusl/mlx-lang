/* mlx-unicode: UTF-16 by macros (code units in, code points out). */
#ifndef MLX_UNICODE_UTF16_H
#define MLX_UNICODE_UTF16_H

#include <unicode/umachine.h>
#ifndef MLX_UNICODE_UTF_H
#include <unicode/utf.h>
#endif

#define U16_IS_SINGLE(c) !U_IS_SURROGATE(c)
#define U16_IS_LEAD(c) (((c) & 0xfffffc00) == 0xd800)
#define U16_IS_TRAIL(c) (((c) & 0xfffffc00) == 0xdc00)
#define U16_IS_SURROGATE(c) U_IS_SURROGATE(c)
#define U16_IS_SURROGATE_LEAD(c) (((c) & 0x400) == 0)
#define U16_IS_SURROGATE_TRAIL(c) (((c) & 0x400) != 0)

/* lead << 10 + trail - this: the code point. */
#define U16_SURROGATE_OFFSET ((0xd800 << 10UL) + 0xdc00 - 0x10000)
#define U16_GET_SUPPLEMENTARY(lead, trail) (((UChar32)(lead) << 10UL) + (UChar32)(trail) - U16_SURROGATE_OFFSET)
#define U16_LEAD(supplementary) (UChar)(((supplementary) >> 10) + 0xd7c0)
#define U16_TRAIL(supplementary) (UChar)(((supplementary) & 0x3ff) | 0xdc00)
#define U16_LENGTH(c) ((uint32_t)(c) <= 0xffff ? 1 : 2)
#define U16_MAX_LENGTH 2

#define U16_GET_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[i]; \
    if (U16_IS_SURROGATE(c)) { \
        if (U16_IS_SURROGATE_LEAD(c)) (c) = U16_GET_SUPPLEMENTARY((c), (s)[(i) + 1]); \
        else (c) = U16_GET_SUPPLEMENTARY((s)[(i) - 1], (c)); \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_GET(s, start, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[i]; \
    if (U16_IS_SURROGATE(c)) { \
        uint16_t __pair; \
        if (U16_IS_SURROGATE_LEAD(c)) { \
            if ((i) + 1 != (length) && U16_IS_TRAIL(__pair = (s)[(i) + 1])) (c) = U16_GET_SUPPLEMENTARY((c), __pair); \
        } else { \
            if ((i) > (start) && U16_IS_LEAD(__pair = (s)[(i) - 1])) (c) = U16_GET_SUPPLEMENTARY(__pair, (c)); \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_GET_OR_FFFD(s, start, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[i]; \
    if (U16_IS_SURROGATE(c)) { \
        uint16_t __pair; \
        if (U16_IS_SURROGATE_LEAD(c)) { \
            if ((i) + 1 != (length) && U16_IS_TRAIL(__pair = (s)[(i) + 1])) (c) = U16_GET_SUPPLEMENTARY((c), __pair); \
            else (c) = 0xfffd; \
        } else { \
            if ((i) > (start) && U16_IS_LEAD(__pair = (s)[(i) - 1])) (c) = U16_GET_SUPPLEMENTARY(__pair, (c)); \
            else (c) = 0xfffd; \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_NEXT_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[(i)++]; \
    if (U16_IS_LEAD(c)) (c) = U16_GET_SUPPLEMENTARY((c), (s)[(i)++]); \
} UPRV_BLOCK_MACRO_END

#define U16_NEXT(s, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[(i)++]; \
    if (U16_IS_LEAD(c)) { \
        uint16_t __trail; \
        if ((i) != (length) && U16_IS_TRAIL(__trail = (s)[(i)])) { \
            ++(i); \
            (c) = U16_GET_SUPPLEMENTARY((c), __trail); \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_NEXT_OR_FFFD(s, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[(i)++]; \
    if (U16_IS_SURROGATE(c)) { \
        uint16_t __trail; \
        if (U16_IS_SURROGATE_LEAD(c) && (i) != (length) && U16_IS_TRAIL(__trail = (s)[(i)])) { \
            ++(i); \
            (c) = U16_GET_SUPPLEMENTARY((c), __trail); \
        } else { \
            (c) = 0xfffd; \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_APPEND_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    if ((uint32_t)(c) <= 0xffff) { \
        (s)[(i)++] = (uint16_t)(c); \
    } else { \
        (s)[(i)++] = (uint16_t)(((c) >> 10) + 0xd7c0); \
        (s)[(i)++] = (uint16_t)(((c) & 0x3ff) | 0xdc00); \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_APPEND(s, i, capacity, c, isError) UPRV_BLOCK_MACRO_BEGIN { \
    if ((uint32_t)(c) <= 0xffff) { \
        (s)[(i)++] = (uint16_t)(c); \
    } else if ((uint32_t)(c) <= 0x10ffff && (i) + 1 < (capacity)) { \
        (s)[(i)++] = (uint16_t)(((c) >> 10) + 0xd7c0); \
        (s)[(i)++] = (uint16_t)(((c) & 0x3ff) | 0xdc00); \
    } else { \
        (isError) = true; \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_FWD_1_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_LEAD((s)[(i)++])) ++(i); \
} UPRV_BLOCK_MACRO_END

#define U16_FWD_1(s, i, length) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_LEAD((s)[(i)++]) && (i) != (length) && U16_IS_TRAIL((s)[i])) ++(i); \
} UPRV_BLOCK_MACRO_END

#define U16_FWD_N_UNSAFE(s, i, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0) { U16_FWD_1_UNSAFE(s, i); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U16_FWD_N(s, i, length, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0 && ((i) < (length) || ((length) < 0 && (s)[i] != 0))) { U16_FWD_1(s, i, length); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U16_SET_CP_START_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_TRAIL((s)[i])) --(i); \
} UPRV_BLOCK_MACRO_END

#define U16_SET_CP_START(s, start, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_TRAIL((s)[i]) && (i) > (start) && U16_IS_LEAD((s)[(i) - 1])) --(i); \
} UPRV_BLOCK_MACRO_END

#define U16_PREV_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[--(i)]; \
    if (U16_IS_TRAIL(c)) (c) = U16_GET_SUPPLEMENTARY((s)[--(i)], (c)); \
} UPRV_BLOCK_MACRO_END

#define U16_PREV(s, start, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[--(i)]; \
    if (U16_IS_TRAIL(c)) { \
        uint16_t __lead; \
        if ((i) > (start) && U16_IS_LEAD(__lead = (s)[(i) - 1])) { \
            --(i); \
            (c) = U16_GET_SUPPLEMENTARY(__lead, (c)); \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_PREV_OR_FFFD(s, start, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    (c) = (s)[--(i)]; \
    if (U16_IS_SURROGATE(c)) { \
        uint16_t __lead; \
        if (U16_IS_SURROGATE_TRAIL(c) && (i) > (start) && U16_IS_LEAD(__lead = (s)[(i) - 1])) { \
            --(i); \
            (c) = U16_GET_SUPPLEMENTARY(__lead, (c)); \
        } else { \
            (c) = 0xfffd; \
        } \
    } \
} UPRV_BLOCK_MACRO_END

#define U16_BACK_1_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_TRAIL((s)[--(i)])) --(i); \
} UPRV_BLOCK_MACRO_END

#define U16_BACK_1(s, start, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_TRAIL((s)[--(i)]) && (i) > (start) && U16_IS_LEAD((s)[(i) - 1])) --(i); \
} UPRV_BLOCK_MACRO_END

#define U16_BACK_N_UNSAFE(s, i, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0) { U16_BACK_1_UNSAFE(s, i); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U16_BACK_N(s, start, i, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0 && (i) > (start)) { U16_BACK_1(s, start, i); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U16_SET_CP_LIMIT_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U16_IS_LEAD((s)[(i) - 1])) ++(i); \
} UPRV_BLOCK_MACRO_END

#define U16_SET_CP_LIMIT(s, start, i, length) UPRV_BLOCK_MACRO_BEGIN { \
    if ((start) < (i) && ((i) < (length) || (length) < 0) && U16_IS_LEAD((s)[(i) - 1]) && U16_IS_TRAIL((s)[i])) ++(i); \
} UPRV_BLOCK_MACRO_END

#endif
