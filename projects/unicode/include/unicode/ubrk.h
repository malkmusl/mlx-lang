/* mlx-unicode: break iterators: grapheme clusters (UAX #29), words
   (UAX #29), lines (UAX #14) and sentences (UAX #29). */
#ifndef MLX_UNICODE_UBRK_H
#define MLX_UNICODE_UBRK_H

#include <unicode/utypes.h>
#include <unicode/utext.h>
#include <unicode/parseerr.h>

#ifndef UBRK_TYPEDEF_UBREAK_ITERATOR
#define UBRK_TYPEDEF_UBREAK_ITERATOR
typedef struct UBreakIterator UBreakIterator;
#endif

typedef enum UBreakIteratorType {
    UBRK_CHARACTER = 0,
    UBRK_WORD = 1,
    UBRK_LINE = 2,
    UBRK_SENTENCE = 3,
    UBRK_TITLE = 4,
    UBRK_COUNT = 5
} UBreakIteratorType;

#define UBRK_DONE ((int32_t)-1)

typedef enum UWordBreak {
    UBRK_WORD_NONE = 0,
    UBRK_WORD_NONE_LIMIT = 100,
    UBRK_WORD_NUMBER = 100,
    UBRK_WORD_NUMBER_LIMIT = 200,
    UBRK_WORD_LETTER = 200,
    UBRK_WORD_LETTER_LIMIT = 300,
    UBRK_WORD_KANA = 300,
    UBRK_WORD_KANA_LIMIT = 400,
    UBRK_WORD_IDEO = 400,
    UBRK_WORD_IDEO_LIMIT = 500
} UWordBreak;

typedef enum ULineBreakTag {
    UBRK_LINE_SOFT = 0,
    UBRK_LINE_SOFT_LIMIT = 100,
    UBRK_LINE_HARD = 100,
    UBRK_LINE_HARD_LIMIT = 200
} ULineBreakTag;

typedef enum USentenceBreakTag {
    UBRK_SENTENCE_TERM = 0,
    UBRK_SENTENCE_TERM_LIMIT = 100,
    UBRK_SENTENCE_SEP = 100,
    UBRK_SENTENCE_SEP_LIMIT = 200
} USentenceBreakTag;

typedef enum ULocDataLocaleType {
    ULOC_ACTUAL_LOCALE = 0,
    ULOC_VALID_LOCALE = 1,
    ULOC_REQUESTED_LOCALE = 2,
    ULOC_DATA_LOCALE_TYPE_LIMIT = 3
} ULocDataLocaleType;

U_CDECL_BEGIN
U_CAPI UBreakIterator *ubrk_open(UBreakIteratorType type, const char *locale, const UChar *text, int32_t textLength, UErrorCode *status);
U_CAPI UBreakIterator *ubrk_openRules(const UChar *rules, int32_t rulesLength, const UChar *text, int32_t textLength, UParseError *parseErr, UErrorCode *status);
U_CAPI UBreakIterator *ubrk_clone(const UBreakIterator *bi, UErrorCode *status);
U_CAPI UBreakIterator *ubrk_safeClone(const UBreakIterator *bi, void *stackBuffer, int32_t *pBufferSize, UErrorCode *status);
U_CAPI void ubrk_close(UBreakIterator *bi);
U_CAPI void ubrk_setText(UBreakIterator *bi, const UChar *text, int32_t textLength, UErrorCode *status);
U_CAPI void ubrk_setUText(UBreakIterator *bi, UText *text, UErrorCode *status);
U_CAPI int32_t ubrk_current(const UBreakIterator *bi);
U_CAPI int32_t ubrk_next(UBreakIterator *bi);
U_CAPI int32_t ubrk_previous(UBreakIterator *bi);
U_CAPI int32_t ubrk_first(UBreakIterator *bi);
U_CAPI int32_t ubrk_last(UBreakIterator *bi);
U_CAPI int32_t ubrk_preceding(UBreakIterator *bi, int32_t offset);
U_CAPI int32_t ubrk_following(UBreakIterator *bi, int32_t offset);
U_CAPI UBool ubrk_isBoundary(UBreakIterator *bi, int32_t offset);
U_CAPI int32_t ubrk_getRuleStatus(UBreakIterator *bi);
U_CAPI int32_t ubrk_getRuleStatusVec(UBreakIterator *bi, int32_t *fillInVec, int32_t capacity, UErrorCode *status);
U_CAPI const char *ubrk_getAvailable(int32_t index);
U_CAPI int32_t ubrk_countAvailable(void);
U_CAPI const char *ubrk_getLocaleByType(const UBreakIterator *bi, ULocDataLocaleType type, UErrorCode *status);
U_CAPI void ubrk_refreshUText(UBreakIterator *bi, UText *text, UErrorCode *status);
U_CDECL_END

#ifdef __cplusplus
/* (ICU's C++ smart pointer, for code that names it.) */
#include <memory>
namespace icu {
struct LocalUBreakIteratorPointerDeleter { void operator()(UBreakIterator *p) const { ubrk_close(p); } };
}
#endif

#endif
