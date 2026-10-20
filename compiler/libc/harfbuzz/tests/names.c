#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Names of a font (argv[1]): the entries listed, each name in UTF-8,
 * UTF-16 and UTF-32 with whole and short buffers, in several languages;
 * the style values of a font. */
static void show(hb_face_t *face, hb_ot_name_id_t id, hb_language_t lang) {
  char u8[300]; uint16_t u16[300]; uint32_t u32[300];
  unsigned n8 = sizeof u8, n16 = 300, n32 = 300;
  unsigned t8 = hb_ot_name_get_utf8(face, id, lang, &n8, u8);
  unsigned t16 = hb_ot_name_get_utf16(face, id, lang, &n16, u16);
  unsigned t32 = hb_ot_name_get_utf32(face, id, lang, &n32, u32);
  printf("  %u %s: %u %u [%s] %u %u", id, lang ? hb_language_to_string(lang) : "-", t8, n8, u8, t16, n16);
  unsigned h = 0;
  for (unsigned i = 0; i <= n16; i++) h = h * 31 + u16[i];
  printf(" %x %u %u", h, t32, n32);
  h = 0;
  for (unsigned i = 0; i <= n32; i++) h = h * 31 + u32[i];
  printf(" %x", h);
  for (unsigned size = 0; size < 6; size++) {
    char s8[8] = "xxxxxxx"; uint16_t s16[8] = {7, 7, 7, 7, 7, 7, 7, 7};
    unsigned a = size, b = size;
    unsigned r8 = hb_ot_name_get_utf8(face, id, lang, &a, s8);
    unsigned r16 = hb_ot_name_get_utf16(face, id, lang, &b, s16);
    printf(" |%u %u %u %02x%02x%02x%02x%02x%02x %u %u %x %x %x", size, r8, a, (unsigned char) s8[0], (unsigned char) s8[1], (unsigned char) s8[2], (unsigned char) s8[3], (unsigned char) s8[4], (unsigned char) s8[5], r16, b, s16[0], s16[1], s16[2]);
  }
  unsigned none = hb_ot_name_get_utf8(face, id, lang, NULL, NULL);
  printf(" ~%u\n", none);
}

int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  unsigned count = 0;
  const hb_ot_name_entry_t *entries = hb_ot_name_list_names(face, &count);
  printf("entries %u\n", count);
  for (unsigned i = 0; i < count; i++)
    printf(" %u %u %u %s\n", entries[i].name_id, entries[i].var.u16[0], entries[i].var.u16[1], entries[i].language ? hb_language_to_string(entries[i].language) : "-");
  const char *langs[] = {NULL, "en", "de", "en-us", "zh-tw", "fr-ca", "ja", "x"};
  for (unsigned i = 0; i < count; i++) {
    show(face, entries[i].name_id, entries[i].language);
    if (i < 40)
      for (unsigned k = 0; k < sizeof langs / sizeof *langs; k++)
        show(face, entries[i].name_id, langs[k] ? hb_language_from_string(langs[k], -1) : NULL);
  }
  show(face, 9999, NULL);
  hb_font_t *font = hb_font_create(face);
  hb_style_tag_t tags[] = {HB_STYLE_TAG_ITALIC, HB_STYLE_TAG_OPTICAL_SIZE, HB_STYLE_TAG_SLANT_ANGLE, HB_STYLE_TAG_SLANT_RATIO, HB_STYLE_TAG_WIDTH, HB_STYLE_TAG_WEIGHT, HB_TAG('a','b','c','d')};
  for (int pass = 0; pass < 3; pass++) {
    if (pass == 1) { hb_font_set_synthetic_slant(font, 0.3f); hb_font_set_ptem(font, 14.5f); }
    if (pass == 2) {
      hb_variation_t v[3];
      hb_variation_from_string("wght=650", -1, &v[0]);
      hb_variation_from_string("wdth=80", -1, &v[1]);
      hb_variation_from_string("opsz=20", -1, &v[2]);
      hb_font_set_variations(font, v, 3);
    }
    printf("style");
    for (unsigned i = 0; i < sizeof tags / sizeof *tags; i++) printf(" %.9g", hb_style_get_value(font, tags[i]));
    printf("\n");
  }
  unsigned design = 1, sub = 2, subname = 3, lo = 4, hi = 5;
  int ok = hb_ot_layout_get_size_params(face, &design, &sub, &subname, &lo, &hi);
  printf("size %d %u %u %u %u %u\n", ok, design, sub, subname, lo, hi);
  return 0;
}
