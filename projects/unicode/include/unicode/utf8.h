/* mlx-unicode: UTF-8 by macros. An ill-formed sequence reads as `sub`
   (U_SENTINEL, or U+FFFD for the _OR_FFFD forms) and moves past its
   longest well-formed beginning (its "maximal subpart"), as Unicode
   recommends. */
#ifndef MLX_UNICODE_UTF8_H
#define MLX_UNICODE_UTF8_H

#include <unicode/umachine.h>
#ifndef MLX_UNICODE_UTF_H
#include <unicode/utf.h>
#endif

#define U8_IS_SINGLE(c) (((c) & 0x80) == 0)
#define U8_IS_LEAD(c) ((uint8_t)((c) - 0xc2) <= 0x32)
#define U8_IS_TRAIL(c) ((int8_t)(c) < -0x40)
#define U8_LENGTH(c) \
    ((uint32_t)(c) <= 0x7f ? 1 : \
        ((uint32_t)(c) <= 0x7ff ? 2 : \
            ((uint32_t)(c) <= 0xd7ff ? 3 : \
                ((uint32_t)(c) <= 0xdfff || (uint32_t)(c) > 0x10ffff ? 0 : \
                    ((uint32_t)(c) <= 0xffff ? 3 : 4)))))
#define U8_MAX_LENGTH 4

/* The trail bytes a lead byte announces (0 for one that is no lead). */
#define U8_COUNT_TRAIL_BYTES(leadByte) \
    (U8_IS_LEAD(leadByte) ? ((uint8_t)(leadByte) >= 0xe0) + ((uint8_t)(leadByte) >= 0xf0) + 1 : 0)
#define U8_COUNT_TRAIL_BYTES_UNSAFE(leadByte) \
    (((uint8_t)(leadByte) >= 0xc2) + ((uint8_t)(leadByte) >= 0xe0) + ((uint8_t)(leadByte) >= 0xf0))
#define U8_MASK_LEAD_BYTE(leadByte, countTrailBytes) ((leadByte) &= (1 << (6 - (countTrailBytes))) - 1)

/* Which first trail bytes (by their top three bits) a three-byte lead
   (by its low four) allows, and which (by their top four) allow a
   four-byte lead (by its low three). */
#define U8_LEAD3_T1_BITS "\x20\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x10\x30\x30"
#define U8_IS_VALID_LEAD3_AND_T1(lead, t1) (U8_LEAD3_T1_BITS[(lead) & 0xf] & (1 << ((uint8_t)(t1) >> 5)))
#define U8_LEAD4_T1_BITS "\x00\x00\x00\x00\x00\x00\x00\x00\x1E\x0F\x0F\x0F\x00\x00\x00\x00"
#define U8_IS_VALID_LEAD4_AND_T1(lead, t1) (U8_LEAD4_T1_BITS[(uint8_t)(t1) >> 4] & (1 << ((lead) & 7)))

/* The code point at `p`, `available` bytes there (negative: as many as
   are well-formed); `*used` the bytes it took. */
static inline UChar32 __mlx_u8_decode(const uint8_t *p, int64_t available, int32_t *used, UChar32 sub)
{
    UChar32 c = p[0];
    uint8_t low = 0x80, high = 0xbf;
    int32_t need, k;
    *used = 1;
    if (c < 0x80)
        return c;
    if (c >= 0xc2 && c <= 0xdf) {
        need = 1;
        c &= 0x1f;
    } else if (c >= 0xe0 && c <= 0xef) {
        need = 2;
        c &= 0x0f;
        if (c == 0)
            low = 0xa0;
        else if (c == 0xd)
            high = 0x9f;
    } else if (c >= 0xf0 && c <= 0xf4) {
        need = 3;
        c &= 0x07;
        if (c == 0)
            low = 0x90;
        else if (c == 4)
            high = 0x8f;
    } else {
        return sub;
    }
    for (k = 1; k <= need; ++k) {
        uint8_t t;
        if (available >= 0 && k >= available)
            return sub;
        t = p[k];
        if (t < low || t > high)
            return sub;
        low = 0x80;
        high = 0xbf;
        c = (c << 6) | (t & 0x3f);
        *used = k + 1;
    }
    return c;
}

/* The code point that ends at s + i (s + start the earliest it may
   begin); `*i` moves to its start. */
static inline UChar32 __mlx_u8_previous(const uint8_t *s, int64_t start, int64_t *i, UChar32 sub)
{
    int64_t end = *i;
    int64_t lead = end - 1;
    UChar32 c = s[lead];
    if (c < 0x80) {
        *i = lead;
        return c;
    }
    if (U8_IS_TRAIL(c)) {
        int32_t trails = 0;
        while (trails < 3 && lead > start && U8_IS_TRAIL(s[lead])) {
            --lead;
            ++trails;
        }
        if (!U8_IS_TRAIL(s[lead])) {
            int32_t used;
            UChar32 decoded = __mlx_u8_decode(s + lead, end - lead, &used, -1);
            if (decoded >= 0 && lead + used == end) {
                *i = lead;
                return decoded;
            }
        }
    }
    *i = end - 1;
    return sub;
}

#define U8_INTERNAL_NEXT_OR_SUB(s, i, length, c, sub) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __used; \
    (c) = __mlx_u8_decode((const uint8_t *)(s) + (i), (int64_t)(length) < 0 ? -1 : (int64_t)(length) - (int64_t)(i), &__used, (sub)); \
    (i) += __used; \
} UPRV_BLOCK_MACRO_END

#define U8_NEXT(s, i, length, c) U8_INTERNAL_NEXT_OR_SUB(s, i, length, c, U_SENTINEL)
#define U8_NEXT_OR_FFFD(s, i, length, c) U8_INTERNAL_NEXT_OR_SUB(s, i, length, c, 0xfffd)

#define U8_NEXT_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __used; \
    (c) = __mlx_u8_decode((const uint8_t *)(s) + (i), 4, &__used, 0xfffd); \
    (i) += __used; \
} UPRV_BLOCK_MACRO_END

#define U8_GET(s, start, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    U8_SET_CP_START(s, start, __at); \
    { int32_t __used; (c) = __mlx_u8_decode((const uint8_t *)(s) + __at, (int64_t)(length) < 0 ? -1 : (int64_t)(length) - __at, &__used, U_SENTINEL); \
      if (__at + __used <= (int64_t)(i)) (c) = U_SENTINEL; } \
} UPRV_BLOCK_MACRO_END

#define U8_GET_OR_FFFD(s, start, i, length, c) UPRV_BLOCK_MACRO_BEGIN { \
    U8_GET(s, start, i, length, c); \
    if ((c) < 0) (c) = 0xfffd; \
} UPRV_BLOCK_MACRO_END

#define U8_GET_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    while (U8_IS_TRAIL((s)[__at])) --__at; \
    { int32_t __used; (c) = __mlx_u8_decode((const uint8_t *)(s) + __at, 4, &__used, 0xfffd); } \
} UPRV_BLOCK_MACRO_END

#define U8_APPEND_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    uint32_t __code = (c); \
    if (__code <= 0x7f) { \
        (s)[(i)++] = (uint8_t)__code; \
    } else { \
        if (__code <= 0x7ff) { \
            (s)[(i)++] = (uint8_t)((__code >> 6) | 0xc0); \
        } else { \
            if (__code <= 0xffff) { \
                (s)[(i)++] = (uint8_t)((__code >> 12) | 0xe0); \
            } else { \
                (s)[(i)++] = (uint8_t)((__code >> 18) | 0xf0); \
                (s)[(i)++] = (uint8_t)(((__code >> 12) & 0x3f) | 0x80); \
            } \
            (s)[(i)++] = (uint8_t)(((__code >> 6) & 0x3f) | 0x80); \
        } \
        (s)[(i)++] = (uint8_t)((__code & 0x3f) | 0x80); \
    } \
} UPRV_BLOCK_MACRO_END

#define U8_APPEND(s, i, capacity, c, isError) UPRV_BLOCK_MACRO_BEGIN { \
    uint32_t __code = (c); \
    if (__code <= 0x7f) { \
        (s)[(i)++] = (uint8_t)__code; \
    } else if (__code <= 0x7ff && (i) + 1 < (capacity)) { \
        (s)[(i)++] = (uint8_t)((__code >> 6) | 0xc0); \
        (s)[(i)++] = (uint8_t)((__code & 0x3f) | 0x80); \
    } else if ((__code <= 0xd7ff || (0xe000 <= __code && __code <= 0xffff)) && (i) + 2 < (capacity)) { \
        (s)[(i)++] = (uint8_t)((__code >> 12) | 0xe0); \
        (s)[(i)++] = (uint8_t)(((__code >> 6) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)((__code & 0x3f) | 0x80); \
    } else if (0xffff < __code && __code <= 0x10ffff && (i) + 3 < (capacity)) { \
        (s)[(i)++] = (uint8_t)((__code >> 18) | 0xf0); \
        (s)[(i)++] = (uint8_t)(((__code >> 12) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)(((__code >> 6) & 0x3f) | 0x80); \
        (s)[(i)++] = (uint8_t)((__code & 0x3f) | 0x80); \
    } else { \
        (isError) = true; \
    } \
} UPRV_BLOCK_MACRO_END

#define U8_FWD_1_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    (i) += 1 + U8_COUNT_TRAIL_BYTES_UNSAFE((s)[i]); \
} UPRV_BLOCK_MACRO_END

#define U8_FWD_1(s, i, length) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __used; \
    (void)__mlx_u8_decode((const uint8_t *)(s) + (i), (int64_t)(length) < 0 ? -1 : (int64_t)(length) - (int64_t)(i), &__used, 0); \
    (i) += __used; \
} UPRV_BLOCK_MACRO_END

#define U8_FWD_N(s, i, length, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0 && ((i) < (length) || ((length) < 0 && (s)[i] != 0))) { U8_FWD_1(s, i, length); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U8_FWD_N_UNSAFE(s, i, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0) { U8_FWD_1_UNSAFE(s, i); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U8_SET_CP_START_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    while (U8_IS_TRAIL((s)[i])) --(i); \
} UPRV_BLOCK_MACRO_END

#define U8_SET_CP_START(s, start, i) UPRV_BLOCK_MACRO_BEGIN { \
    if (U8_IS_TRAIL((s)[(i)])) { \
        int64_t __end = (int64_t)(i) + 1; \
        while (__end - (int64_t)(i) < 4 && U8_IS_TRAIL((s)[__end])) ++__end; \
        { int64_t __at = __end; \
          (void)__mlx_u8_previous((const uint8_t *)(s), (start), &__at, -1); \
          if (__at <= (int64_t)(i)) (i) = __at; } \
    } \
} UPRV_BLOCK_MACRO_END

#define U8_PREV(s, start, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    (c) = __mlx_u8_previous((const uint8_t *)(s), (start), &__at, U_SENTINEL); \
    (i) = __at; \
} UPRV_BLOCK_MACRO_END

#define U8_PREV_OR_FFFD(s, start, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    (c) = __mlx_u8_previous((const uint8_t *)(s), (start), &__at, 0xfffd); \
    (i) = __at; \
} UPRV_BLOCK_MACRO_END

#define U8_PREV_UNSAFE(s, i, c) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    (c) = __mlx_u8_previous((const uint8_t *)(s), 0, &__at, 0xfffd); \
    (i) = __at; \
} UPRV_BLOCK_MACRO_END

#define U8_BACK_1(s, start, i) UPRV_BLOCK_MACRO_BEGIN { \
    int64_t __at = (i); \
    (void)__mlx_u8_previous((const uint8_t *)(s), (start), &__at, 0); \
    (i) = __at; \
} UPRV_BLOCK_MACRO_END

#define U8_BACK_1_UNSAFE(s, i) UPRV_BLOCK_MACRO_BEGIN { \
    while (U8_IS_TRAIL((s)[--(i)])) {} \
} UPRV_BLOCK_MACRO_END

#define U8_BACK_N(s, start, i, n) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __count = (n); \
    while (__count > 0 && (i) > (start)) { U8_BACK_1(s, start, i); --__count; } \
} UPRV_BLOCK_MACRO_END

#define U8_SET_CP_LIMIT(s, start, i, length) UPRV_BLOCK_MACRO_BEGIN { \
    if ((start) < (i) && ((i) < (length) || (length) < 0)) { \
        int64_t __back = (i); \
        U8_BACK_1(s, start, __back); \
        U8_FWD_1(s, __back, length); \
        (i) = __back; \
    } \
} UPRV_BLOCK_MACRO_END

#endif
