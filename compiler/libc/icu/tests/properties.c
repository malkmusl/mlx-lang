/* Properties of code points against what the UCD says of them. */
#include <stdio.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
int main(void) {
    UErrorCode e = U_ZERO_ERROR;
    printf("type A=%d a=%d 0=%d space=%d U+4E00=%d U+1F600=%d U+E000=%d U+0378=%d\n", u_charType('A'), u_charType('a'), u_charType('0'), u_charType(' '), u_charType(0x4E00), u_charType(0x1F600), u_charType(0xE000), u_charType(0x378));
    printf("dir A=%d alef=%d 1=%d arabic1=%d\n", u_charDirection('A'), u_charDirection(0x5D0), u_charDirection('1'), u_charDirection(0x661));
    printf("lower A=%x upper a=%x title dz=%x fold A=%x upper sharp s=%x lower Omega=%x\n", u_tolower('A'), u_toupper('a'), u_totitle(0x1C6), u_foldCase('A', 0), u_toupper(0xDF), u_tolower(0x3A9));
    printf("emoji pres 1F600=%d 'A'=%d, ws=%d %d, DI 200B=%d, ea 4E00=%d lb=%d\n", u_hasBinaryProperty(0x1F600, UCHAR_EMOJI_PRESENTATION), u_hasBinaryProperty('A', UCHAR_EMOJI_PRESENTATION), u_isUWhiteSpace(' '), u_isUWhiteSpace('x'), u_hasBinaryProperty(0x200B, UCHAR_DEFAULT_IGNORABLE_CODE_POINT), u_getIntPropertyValue(0x4E00, UCHAR_EAST_ASIAN_WIDTH), u_getIntPropertyValue(0x4E00, UCHAR_LINE_BREAK));
    printf("script A=%d (%s) alef=%s han=%s, enum Latn=%d Latin=%d hiragana=%d\n", uscript_getScript('A', &e), uscript_getName(uscript_getScript('A', &e)), uscript_getShortName(uscript_getScript(0x5D0, &e)), uscript_getName(uscript_getScript(0x4E00, &e)), u_getPropertyValueEnum(UCHAR_SCRIPT, "Latn"), u_getPropertyValueEnum(UCHAR_SCRIPT, "latin"), USCRIPT_HIRAGANA == uscript_getScript(0x3042, &e));
    printf("scx 0x3001 hira=%d han=%d latn=%d; ccc 0301=%d lccc 1E08=%d tccc 1E08=%d, block 0041=%d ->%d\n", uscript_hasScript(0x3001, USCRIPT_HIRAGANA), uscript_hasScript(0x3001, USCRIPT_HAN), uscript_hasScript(0x3001, USCRIPT_LATIN), u_getCombiningClass(0x301), u_getIntPropertyValue(0x1E08, UCHAR_LEAD_CANONICAL_COMBINING_CLASS), u_getIntPropertyValue(0x1E08, UCHAR_TRAIL_CANONICAL_COMBINING_CLASS), ublock_getCode('A'), UBLOCK_BASIC_LATIN);
    /* ICU's other names of scripts (WebKit's LocaleToScriptMapping names
       USCRIPT_MANDAEAN). */
    printf("aliases mandaean=%d old turkic=%d sindhi=%d meroitic=%d pollard=%d\n", USCRIPT_MANDAEAN, USCRIPT_OLD_TURKIC, USCRIPT_SINDHI, USCRIPT_MEROITIC, USCRIPT_PHONETIC_POLLARD);
    printf("num 0x00BD=%g 0x0663=%g digit=%d, isprint %d %d, alpha %d, error %s\n", u_getNumericValue(0xBD), u_getNumericValue(0x663), u_charDigitValue(0x669), u_isprint('a'), u_isprint(7), u_isalpha(0x4E00), u_errorName(U_BUFFER_OVERFLOW_ERROR));
    printf("mirror ( -> %c, gcb 0D=%d wb A=%d sb .=%d vo 4E00=%d\n", (char)u_charMirror('('), u_getIntPropertyValue('\r', UCHAR_GRAPHEME_CLUSTER_BREAK), u_getIntPropertyValue('A', UCHAR_WORD_BREAK), u_getIntPropertyValue('.', UCHAR_SENTENCE_BREAK), u_getIntPropertyValue(0x4E00, UCHAR_VERTICAL_ORIENTATION));
    /* ICU numbers the blocks in the order they came to it, not Blocks.txt's. */
    printf("blocks 0370=%d 2700=%d 1D360=%d 1FA70=%d 11F00=%d 2EBF0=%d, %d %d\n", ublock_getCode(0x370), ublock_getCode(0x2700), ublock_getCode(0x1D360), ublock_getCode(0x1FA70), ublock_getCode(0x11F00), ublock_getCode(0x2EBF0), UBLOCK_DINGBATS, UBLOCK_SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A);
    return 0;
}
