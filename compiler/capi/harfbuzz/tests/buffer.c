#include <hb.h>
#include <stdio.h>
#include <string.h>

/* Buffers (text in, the segment properties guessed, serialized), features
 * and variations from and to strings, languages, scripts and the default
 * Unicode functions. */
int main(void) {
  const char *texts[] = {"Hello", "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85 abc", "a\xcc\x81\xe2\x80\x8d!", "\xff\xfe" "abc\xe0\x80", NULL};
  for (int t = 0; texts[t]; t++) {
    hb_buffer_t *b = hb_buffer_create();
    hb_buffer_add_utf8(b, texts[t], -1, 1, -1);
    hb_buffer_guess_segment_properties(b);
    char out[1000];
    unsigned consumed;
    hb_buffer_serialize(b, 0, -1, out, sizeof out, &consumed, NULL, HB_BUFFER_SERIALIZE_FORMAT_TEXT, 0);
    printf("%s dir=%s script=%x lang=%s len=%u\n", out, hb_direction_to_string(hb_buffer_get_direction(b)), hb_buffer_get_script(b), hb_language_to_string(hb_buffer_get_language(b)), hb_buffer_get_length(b));
    hb_buffer_destroy(b);
  }
  const char *feats[] = {"kern", "-liga", "aalt=2", "+smcp[3:5]", "dlig[2]", "'ss01' on", "c2sc off", "frac[:3]=0", "x", "kern=", NULL};
  for (int i = 0; feats[i]; i++) {
    hb_feature_t f; char s[128];
    int ok = hb_feature_from_string(feats[i], -1, &f);
    hb_feature_to_string(&f, s, sizeof s);
    printf("feat %s -> %d %08x %u %u %u '%s'\n", feats[i], ok, f.tag, f.value, f.start, f.end, s);
  }
  const char *vars[] = {"wght=400", "wdth 75.5", "opsz=12e1", "slnt=-12.25", "x=1e-5", "ital=0.123456789", NULL};
  for (int i = 0; vars[i]; i++) {
    hb_variation_t v; char s[128];
    int ok = hb_variation_from_string(vars[i], -1, &v);
    hb_variation_to_string(&v, s, sizeof s);
    printf("var %s -> %d %08x %.9g '%s'\n", vars[i], ok, v.tag, v.value, s);
  }
  printf("%s %s %d\n", hb_language_to_string(hb_language_from_string("EN_us", -1)), hb_language_to_string(hb_language_from_string("zh-Hant-TW", 5)), hb_language_from_string("en-US", -1) == hb_language_from_string("en_us", -1));
  printf("%x %x %x\n", hb_script_from_string("arab", -1), hb_script_from_string("Qaai", -1), hb_script_from_string("123", -1));
  unsigned cats[] = {0x41, 0x61, 0x300, 0x627, 0x20, 0x200d, 0x1f600, 0x5d0, 0x0e33, 0x10ffff, 0xe000};
  hb_unicode_funcs_t *u = hb_unicode_funcs_get_default();
  for (int i = 0; i < 11; i++) {
    hb_codepoint_t a, b2, ab;
    int d = hb_unicode_decompose(u, cats[i], &a, &b2);
    printf("U+%04X gc=%d cc=%d mir=%x sc=%x dec=%d %x %x\n", cats[i], hb_unicode_general_category(u, cats[i]), hb_unicode_combining_class(u, cats[i]), hb_unicode_mirroring(u, cats[i]), hb_unicode_script(u, cats[i]), d, a, b2);
  }
  hb_codepoint_t ab;
  printf("compose %d", hb_unicode_compose(u, 0x41, 0x301, &ab)); printf(" %x\n", ab);
  printf("compose %d", hb_unicode_compose(u, 0x1100, 0x1161, &ab)); printf(" %x\n", ab);
  return 0;
}
