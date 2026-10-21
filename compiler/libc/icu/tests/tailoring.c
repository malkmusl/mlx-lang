/* Collation by locale (CLDR's tailorings), by a program's own rules, and
   the keywords and attributes that go with them: each case printed as
   its words sorted ("<" between words that differ, "=" between words
   equal), the attributes and lists the collators give. tailoring.expected
   is what this prints with ICU 74 too (built with the system's compiler
   and ICU, CLDR 44's data: where CLDR 48 changed what these cases see,
   they would differ). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ucol.h>
#include <unicode/uenum.h>
#include <unicode/ustring.h>

static UCollator *current;

static int compare(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    UErrorCode status = U_ZERO_ERROR;
    int r = ucol_strcollUTF8(current, x, -1, y, -1, &status);
    return r ? r : strcmp(x, y);
}

/* The words of `list` (space-separated UTF-8) sorted by `coll`. */
static void sorted(const char *name, UCollator *coll, const char *list)
{
    char copy[4096];
    const char *words[256];
    int count = 0;
    strncpy(copy, list, sizeof copy - 1);
    copy[sizeof copy - 1] = 0;
    for (char *w = strtok(copy, " "); w && count < 256; w = strtok(NULL, " ")) words[count++] = w;
    current = coll;
    qsort(words, count, sizeof *words, compare);
    printf("%s:", name);
    for (int i = 0; i < count; ++i) {
        if (i) {
            UErrorCode status = U_ZERO_ERROR;
            printf(" %s", ucol_strcollUTF8(coll, words[i - 1], -1, words[i], -1, &status) == 0 ? "=" : "<");
        }
        printf(" %s", words[i]);
    }
    printf("\n");
}

static const char *LATIN = "a A á ä Ä å æ ae af aa b c C č ch cz Ch CH d đ ð e é ê ě f g h i ı İ j k l ll lz ł m n ñ ň o ó ö ø œ oe p q r ř s š ß ss t þ th u ü ue ů v w x y ý z ž Z";

static void byLocale(const char *locale, const char *words)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open(locale, &status);
    if (U_FAILURE(status)) {
        printf("%s: %s\n", locale, u_errorName(status));
        return;
    }
    sorted(locale, coll, words);
    ucol_close(coll);
}

static void withAttribute(const char *locale, UColAttribute attribute, UColAttributeValue value, const char *words)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open(locale, &status);
    ucol_setAttribute(coll, attribute, value, &status);
    char name[128];
    snprintf(name, sizeof name, "%s [%d=%d]", locale, attribute, value);
    sorted(name, coll, words);
    ucol_close(coll);
}

static void byRules(const char *rules, const char *words)
{
    UChar units[1024];
    int32_t length = 0;
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(units, 1024, &length, rules, -1, &status);
    UParseError error;
    UCollator *coll = ucol_openRules(units, length, UCOL_DEFAULT, UCOL_DEFAULT, &error, &status);
    char name[256];
    snprintf(name, sizeof name, "rules \"%s\"", rules);
    if (U_FAILURE(status)) {
        printf("%s: %s\n", name, u_errorName(status));
        return;
    }
    sorted(name, coll, words);
    int32_t back = 0;
    ucol_getRules(coll, &back);
    if (back != length) printf("  ucol_getRules gives %d units of %d\n", back, length);
    ucol_close(coll);
}

static void attributes(const char *locale)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open(locale, &status);
    printf("%s attributes:", locale);
    for (int a = 0; a < UCOL_ATTRIBUTE_COUNT; ++a) printf(" %d", ucol_getAttribute(coll, (UColAttribute)a, &status));
    int32_t codes[16];
    int32_t n = ucol_getReorderCodes(coll, codes, 16, &status);
    printf("; reorder");
    for (int i = 0; i < n; ++i) printf(" %d", codes[i]);
    printf("; max variable %d\n", ucol_getMaxVariable(coll));
    ucol_close(coll);
}

static void keywordValues(const char *locale)
{
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *values = ucol_getKeywordValuesForLocale("collation", locale, 0, &status);
    printf("collations of %s:", locale);
    const char *value;
    while ((value = uenum_next(values, NULL, &status))) printf(" %s", value);
    printf("\n");
    uenum_close(values);
}

/* Whether the sort keys order the words as ucol_strcoll does. */
static void sortKeys(const char *locale, UColAttribute attribute, UColAttributeValue value, const char *list)
{
    char copy[4096];
    const char *words[256];
    int count = 0;
    strncpy(copy, list, sizeof copy - 1);
    copy[sizeof copy - 1] = 0;
    for (char *w = strtok(copy, " "); w && count < 256; w = strtok(NULL, " ")) words[count++] = w;
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open(locale, &status);
    if (attribute != UCOL_ATTRIBUTE_COUNT) ucol_setAttribute(coll, attribute, value, &status);
    static uint8_t keys[256][512];
    static int32_t lengths[256];
    for (int i = 0; i < count; ++i) {
        UChar units[64];
        int32_t length = 0;
        u_strFromUTF8(units, 64, &length, words[i], -1, &status);
        lengths[i] = ucol_getSortKey(coll, units, length, keys[i], 512);
    }
    int pairs = 0, agree = 0;
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < count; ++j) {
            int byKey = strcmp((const char *)keys[i], (const char *)keys[j]);
            int byCompare = ucol_strcollUTF8(coll, words[i], -1, words[j], -1, &status);
            byKey = byKey < 0 ? -1 : byKey > 0 ? 1 : 0;
            ++pairs;
            if (byKey == byCompare) ++agree;
        }
    printf("sort keys of %s [%d=%d]: %d of %d pairs as ucol_strcoll\n", locale, attribute, value, agree, pairs);
    ucol_close(coll);
}

int main(void)
{
    /* Locales and their types. */
    const char *locales[] = {
        "root", "en", "sv", "sv@collation=traditional", "de", "de@collation=phonebook", "de_AT@collation=phonebook",
        "de-u-co-phonebk", "es", "es@collation=traditional", "es-u-co-trad", "da", "nb", "nn", "no", "fi", "fi@collation=traditional",
        "is", "fo", "et", "lv", "lt", "pl", "cs", "sk", "sl", "hr", "bs", "sr_Latn", "hu", "ro", "tr", "az", "vi",
        "fr", "fr_CA", "en_US_POSIX", "ca", "cy", "mt", "kl", "se", "smn", "haw", "lb", "sq", "wo", "yo", "ha", "ig",
        "eo", "fil", "gl", "hsb", "dsb", "lkt", "to", "tk", "uz", "kk", "ky", "mn", 0 };
    for (int i = 0; locales[i]; ++i) byLocale(locales[i], LATIN);
    const char *scripts[][2] = {
        { "ru", "я ж е ё ê a z α 1 й и і ї ь ъ" },
        { "uk", "г ґ є е і ї и й ь ъ ё a" },
        { "be", "е ё ў у и і й ь a" },
        { "bg", "ь ъ ю я й и a" },
        { "mk", "ѓ ѕ ј љ њ ќ џ г з к л н ч a" },
        { "sr", "ђ ј љ њ ћ џ д з к л н ч a" },
        { "el", "α ά β ω ώ a я 1" },
        { "ka", "ა ბ ჱ ჲ ჳ ჴ ჵ a" },
        { "hy", "ա բ և ու օ ֆ a" },
        { "he", "א ב בּ ג ת ײ װ a" },
        { "ar", "ا أ إ آ ب ة ت ث ي ى ه a" },
        { "fa", "ا آ ب پ ت ک گ ی ي ه ة a" },
        { "ur", "ا آ ب پ ٹ ے ی ہ ھ a" },
        { "hi", "क ख ग क़ ख़ ग़ अ आ ः ं ँ a" },
        { "bn", "ক খ গ ক্ষ অ আ ং ঃ ঁ a" },
        { "ta", "க ங ச க்ஷ ஶ அ ஆ a" },
        { "th", "ก ข ฃ ค เก แก ไก ๆ ฯ a" },
        { "lo", "ກ ຂ ຄ ເກ ແກ ໆ a" },
        { "km", "ក ខ គ ក់ ៈ a" },
        { "my", "က ခ ဂ ကျ ကြ ကွ a" },
        { "ja", "あ ア ｱ ぁ ァ か が カ ガ ー あー アー ゝ ヽ 亜 唖 漢 字 a" },
        { "ko", "가 각 간 ㄱ ㄲ 伽 佳 家 可 漢 a" },
        { "zh", "阿 啊 爱 安 不 中 国 人 大 小 ā á ǎ à a ㄅ ㄆ" },
        { "zh@collation=stroke", "一 乙 二 十 丁 厂 七 人 中 国 大 小 阿" },
        { "zh@collation=zhuyin", "ㄅ ㄆ ㄇ 八 巴 爸 中 国 阿" },
        { "zh@collation=unihan", "一 丁 七 丨 中 丶 丸 人 大 小 阿" },
        { "zh_Hant", "一 乙 二 十 丁 厂 七 人 中 國 大 小 阿" },
        { "zh_TW", "一 乙 二 十 丁 厂 七 人 中 國 大 小 阿" },
        { "yue", "一 乙 二 十 丁 厂 七 人 中 國 大 小 阿" },
        { "ja@collation=unihan", "一 丁 七 丨 中 丶 丸 人 大 小 阿 あ" },
        { "si", "අ ආ ක ඛ ං ඃ a" },
        { "bo", "ཀ ཁ ག ང ཀར ཀརྐ ། ༎ a" },
        { "am", "ሀ ሁ ለ ሉ a" },
        { "chr", "Ꭰ Ꭱ Ꭶ Ꭷ a" },
        { "en@collation=emoji", "😀 😃 👍 ❤ € $ a 1" },
        { "en@collation=eor", "a ä ǟ þ th b ʙ ƀ" },
        { "de@collation=search", "a ä ae b" },
        { "da@collation=search", "a å aa æ ä" },
        { "ko@collation=search", "가 각 ㄱ ㄲ 갂" },
        { 0, 0 } };
    for (int i = 0; scripts[i][0]; ++i) byLocale(scripts[i][0], scripts[i][1]);

    /* Attributes over the tailorings' own. */
    withAttribute("sv", UCOL_STRENGTH, UCOL_PRIMARY, LATIN);
    withAttribute("da", UCOL_CASE_FIRST, UCOL_OFF, "a A b B å Å aa Aa AA");
    withAttribute("de@collation=phonebook", UCOL_NUMERIC_COLLATION, UCOL_ON, "a10 a2 ä1 ae3 af");
    withAttribute("cs", UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, "ch c-h c h-ch ch- h");
    withAttribute("ja", UCOL_STRENGTH, UCOL_QUATERNARY, "あ ア ｱ か カ ゝ ヽ");
    withAttribute("fr_CA", UCOL_FRENCH_COLLATION, UCOL_OFF, "cote côte coté côté");
    byLocale("fr_CA", "cote côte coté côté");
    byLocale("fr", "cote côte coté côté");

    /* Keywords of the locale ID. */
    byLocale("en-u-kn-true", "a10 a2 a1");
    byLocale("en-u-ks-level1", "a A á b");
    byLocale("en-u-kf-upper", "a A b B");
    byLocale("en-u-ka-shifted", "a-b ab a+b a_b");
    byLocale("en-u-kr-grek-latn", "a α я 1");
    byLocale("en@colReorder=Cyrl-Grek", "a α я 1");
    byLocale("en-u-kr-digit-latn", "a α 1 $");
    byLocale("de-u-co-phonebk-kn", "ä10 ä2 ae1");
    byLocale("en@colStrength=secondary;colCaseFirst=upper", "a A á Á");

    /* A program's own rules. */
    const char *WORDS = "a A b B c C ch d e x y z æ ae ä å x|y";
    byRules("&a<b<<c<<<C", WORDS);
    byRules("&[before 1]a<x<y", WORDS);
    byRules("&[before 2]a<<x&[before 3]b<<<y", WORDS);
    byRules("&c<ch<<<Ch<<<CH", "c ch Ch CH cz d h");
    byRules("&ae<<æ<<<Æ", "a ae æ Æ af b");
    byRules("&a<<<x|y", "xy ay y a xa");
    byRules("&t<<<þ/h", "t th þ ti tz u");
    byRules("&z<*αβγ", "a z α β γ ω");
    byRules("&z<*α-γ", "a z α β γ ω");
    byRules("[reorder Grek Latn]&b<a", "a b c α ω я");
    byRules("[caseFirst upper][strength 2]&a<b", "a A b B ab Ab");
    byRules("[alternate shifted][backwards 2]&a=b", "a b a-b ab côte coté");
    byRules("&'\\u0020'<'-'<'&'", "a - & \\");
    byRules("&a<'''<'b'", "a ' b c");
    byRules("&[last regular]<x<y", "a z x y 漢");
    byRules("&[first regular]<x", "x a ^ ( 1");
    byRules("[import de-u-co-phonebk]&z<ä", "a ä ae z");
    byRules("[suppressContractions [Ии]]", "и й ӣ к");
    byRules("&a<b<", "a b");
    byRules("&a<<<<b", "a b");
    byRules("a<b", "a b");
    byRules("&a<[bogus]", "a b");

    /* Attributes and reordering the rules set. */
    const char *withSettings[] = { "root", "da", "fr_CA", "th", "ru", "ja", "zh", "zh_Hant", "ko", "el", "ar", "he", "hi", "mt", "si", 0 };
    for (int i = 0; withSettings[i]; ++i) attributes(withSettings[i]);

    /* Sort keys. */
    sortKeys("sv", UCOL_ATTRIBUTE_COUNT, UCOL_DEFAULT, LATIN);
    sortKeys("de@collation=phonebook", UCOL_ATTRIBUTE_COUNT, UCOL_DEFAULT, LATIN);
    sortKeys("fr_CA", UCOL_ATTRIBUTE_COUNT, UCOL_DEFAULT, "cote côte coté côté a à â b");
    sortKeys("da", UCOL_CASE_LEVEL, UCOL_ON, LATIN);
    sortKeys("en", UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, "a-b ab a+b a_b ab- -ab a b");
    sortKeys("ja", UCOL_STRENGTH, UCOL_QUATERNARY, "あ ア ｱ か カ ゝ ヽ あー アー 漢");
    sortKeys("zh", UCOL_STRENGTH, UCOL_IDENTICAL, "阿 啊 爱 安 不 中 国 a A ā");
    sortKeys("ru", UCOL_NUMERIC_COLLATION, UCOL_ON, "я ж 1 10 2 a α a10 a9");

    /* The types of a locale. */
    const char *types[] = { "root", "de", "de_AT", "es", "sv", "zh", "zh_Hant", "zh_TW", "ja", "ko", "si", "ln", "bn", "ar", "nb", 0 };
    for (int i = 0; types[i]; ++i) keywordValues(types[i]);
    return 0;
}
