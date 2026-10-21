#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <string.h>

/* OpenType tags of scripts and languages, both ways. */
static void show(hb_script_t script, const char *lang) {
  hb_tag_t st[3], lt[3];
  unsigned sc = 3, lc = 3;
  hb_language_t l = lang ? hb_language_from_string(lang, -1) : HB_LANGUAGE_INVALID;
  hb_ot_tags_from_script_and_language(script, l, &sc, st, &lc, lt);
  printf("%c%c%c%c %s ->", HB_UNTAG(script), lang ? lang : "(null)");
  for (unsigned i = 0; i < sc; i++) printf(" s:%c%c%c%c", HB_UNTAG(st[i]));
  for (unsigned i = 0; i < lc; i++) printf(" l:%c%c%c%c", HB_UNTAG(lt[i]));
  printf("\n");
}

int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  const char *langs[] = {"en", "en-US", "ar", "fa", "ur", "hi", "mr", "ne", "sa", "ta", "zh", "zh-Hant", "zh-HK", "zh-TW", "zh-Hant-HK", "zh-CN", "ja", "ko", "th", "he", "yi", "de-CH", "sr-Latn", "el-polyton", "art-lojban", "x-hbsc-12345678", "en-x-hbot-41424344", "und-fonipa", "cmn-Hant", "yue", "nan-Hant-TW", "mn-Mong", "az-Arab", "ku", "ms", "fil", "tlh", "abc", "xyz-foo", "a", "", "sgn-be-fr", "no", "nb", "nn", "pt-BR", "ro-MD", "kok", "ber", "x-foo", "de-x-hbsc-latn-hbot-deu", "qaa", "en-x-hbotabcd", "hy-arevmda", NULL};
  hb_script_t scripts[] = {HB_SCRIPT_LATIN, HB_SCRIPT_ARABIC, HB_SCRIPT_DEVANAGARI, HB_SCRIPT_MYANMAR, HB_SCRIPT_HIRAGANA, HB_SCRIPT_KATAKANA, HB_SCRIPT_LAO, HB_SCRIPT_NKO, HB_SCRIPT_MATH, HB_SCRIPT_INVALID, HB_SCRIPT_UNKNOWN, HB_SCRIPT_TAMIL, HB_SCRIPT_HAN, HB_SCRIPT_YI, HB_SCRIPT_VAI};
  for (unsigned s = 0; s < sizeof scripts / sizeof scripts[0]; s++) show(scripts[s], NULL);
  for (int i = 0; langs[i]; i++) show(HB_SCRIPT_LATIN, langs[i]);
  hb_tag_t tags[] = {HB_TAG('l','a','t','n'), HB_TAG('d','e','v','2'), HB_TAG('d','e','v','3'), HB_TAG('k','a','n','a'), HB_TAG('n','k','o',' '), HB_TAG('D','F','L','T'), HB_TAG('m','a','t','h'), HB_TAG('m','y','m','2'), HB_TAG('y','i',' ',' '), HB_TAG('x','y','z','w')};
  for (unsigned i = 0; i < sizeof tags / sizeof tags[0]; i++) {
    hb_script_t s = hb_ot_tag_to_script(tags[i]);
    printf("tag %c%c%c%c -> %c%c%c%c\n", HB_UNTAG(tags[i]), HB_UNTAG(s));
  }
  hb_tag_t ltags[] = {HB_TAG('E','N','G',' '), HB_TAG('A','R','A',' '), HB_TAG('Z','H','H',' '), HB_TAG('Z','H','S',' '), HB_TAG('D','E','U',' '), HB_TAG('A','B','C',' '), HB_TAG('a','b','c','d'), HB_TAG('d','f','l','t'), HB_TAG('I','P','P','H'), HB_TAG('N','O','R',' ')};
  for (unsigned i = 0; i < sizeof ltags / sizeof ltags[0]; i++) {
    hb_language_t l = hb_ot_tag_to_language(ltags[i]);
    printf("lang %c%c%c%c -> %s\n", HB_UNTAG(ltags[i]), l ? hb_language_to_string(l) : "(null)");
    /* (HarfBuzz 8.3 crashes in hb_ot_tags_to_script_and_language on a
     * language tag of no language.) */
    if (!l) continue;
    hb_script_t s; hb_language_t l2;
    hb_ot_tags_to_script_and_language(HB_TAG('a','r','a','b'), ltags[i], &s, &l2);
    printf("  -> %c%c%c%c %s\n", HB_UNTAG(s), l2 ? hb_language_to_string(l2) : "(null)");
    hb_ot_tags_to_script_and_language(HB_TAG('d','e','v','2'), ltags[i], &s, &l2);
    printf("  -> %c%c%c%c %s\n", HB_UNTAG(s), l2 ? hb_language_to_string(l2) : "(null)");
  }
  hb_tag_t a, b;
  hb_ot_tags_from_script(HB_SCRIPT_BENGALI, &a, &b);
  printf("%c%c%c%c %c%c%c%c %c%c%c%c\n", HB_UNTAG(a), HB_UNTAG(b), HB_UNTAG(hb_ot_tag_from_language(hb_language_from_string("fr-CA", -1))));
  return 0;
}
