#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Baselines and font extents (BASE), meta entries, feature names and
 * characters, and the size feature of a font (argv[1]). */
static unsigned hash(hb_blob_t *b) {
  unsigned n; const char *d = hb_blob_get_data(b, &n); unsigned h = 5381;
  for (unsigned i = 0; i < n; i++) h = h * 33 + (unsigned char) d[i];
  return h ^ n;
}

int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_font_t *font = hb_font_create(face);
  hb_ot_layout_baseline_tag_t baselines[] = {HB_OT_LAYOUT_BASELINE_TAG_ROMAN, HB_OT_LAYOUT_BASELINE_TAG_HANGING,
    HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT, HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
    HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_CENTRAL, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
    HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL,
    HB_OT_LAYOUT_BASELINE_TAG_MATH, HB_TAG('x','y','z','w')};
  hb_script_t scripts[] = {HB_SCRIPT_LATIN, HB_SCRIPT_DEVANAGARI, HB_SCRIPT_HAN, HB_SCRIPT_ARABIC, HB_SCRIPT_TIBETAN, HB_SCRIPT_INVALID};
  const char *langs[] = {"en", "ja", "hi", "zh-tw", NULL};
  hb_direction_t dirs[] = {HB_DIRECTION_LTR, HB_DIRECTION_TTB};
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) { hb_font_set_scale(font, 3000, -1700); hb_font_set_ppem(font, 17, 23); }
    for (unsigned d = 0; d < 2; d++)
      for (unsigned s = 0; s < sizeof scripts / sizeof *scripts; s++)
        for (unsigned l = 0; l < sizeof langs / sizeof *langs; l++) {
          hb_language_t lang = langs[l] ? hb_language_from_string(langs[l], -1) : NULL;
          hb_font_extents_t e = {0};
          int ok = hb_ot_layout_get_font_extents2(font, dirs[d], scripts[s], lang, &e);
          printf("ext %u %u %u %d %d %d %d |", d, s, l, ok, e.ascender, e.descender, e.line_gap);
          for (unsigned b = 0; b < sizeof baselines / sizeof *baselines; b++) {
            hb_position_t c = 12345, c2 = 54321;
            int okb = hb_ot_layout_get_baseline2(font, baselines[b], dirs[d], scripts[s], lang, &c);
            hb_ot_layout_get_baseline_with_fallback2(font, baselines[b], dirs[d], scripts[s], lang, &c2);
            printf(" %d:%d:%d", okb, okb ? c : 0, c2);
          }
          printf("\n");
        }
  }
  hb_font_extents_t e = {0};
  printf("raw %d", hb_ot_layout_get_font_extents(font, HB_DIRECTION_LTR, HB_TAG('l','a','t','n'), HB_TAG('E','N','G',' '), &e));
  printf(" %d %d\n", e.ascender, e.descender);
  for (unsigned s = 0; s < 200; s += 7) printf("%x ", hb_ot_layout_get_horizontal_baseline_tag_for_script(hb_script_from_iso15924_tag(HB_TAG('L','a','t','n') + s)));
  printf("%x %x %x\n", hb_ot_layout_get_horizontal_baseline_tag_for_script(HB_SCRIPT_DEVANAGARI), hb_ot_layout_get_horizontal_baseline_tag_for_script(HB_SCRIPT_HAN), hb_ot_layout_get_horizontal_baseline_tag_for_script(HB_SCRIPT_NANDINAGARI));
  hb_ot_meta_tag_t tags[10];
  unsigned count = 10;
  unsigned total = hb_ot_meta_get_entry_tags(face, 0, &count, tags);
  printf("meta %u %u:", total, count);
  for (unsigned i = 0; i < count; i++) {
    hb_blob_t *b = hb_ot_meta_reference_entry(face, tags[i]);
    printf(" %c%c%c%c=%u", HB_UNTAG(tags[i]), hash(b));
    hb_blob_destroy(b);
  }
  hb_blob_t *b = hb_ot_meta_reference_entry(face, HB_OT_META_TAG_DESIGN_LANGUAGES);
  printf(" dlng %u\n", hash(b));
  hb_tag_t tables[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};
  for (unsigned t = 0; t < 2; t++) {
    unsigned n = hb_ot_layout_table_get_feature_tags(face, tables[t], 0, NULL, NULL);
    for (unsigned i = 0; i <= n; i++) {
      hb_ot_name_id_t label = 1, tip = 2, sample = 3, first = 4;
      unsigned np = 5;
      int ok = hb_ot_layout_feature_get_name_ids(face, tables[t], i, &label, &tip, &sample, &np, &first);
      hb_codepoint_t chars[6];
      unsigned cc = 6;
      unsigned tc = hb_ot_layout_feature_get_characters(face, tables[t], i, 1, &cc, chars);
      if (!ok && !tc && i < n - 3 && i > 2) continue;
      printf("feat %u %u %d %u %u %u %u %u | %u %u", t, i, ok, label, tip, sample, np, first, tc, cc);
      for (unsigned k = 0; k < cc; k++) printf(" %x", chars[k]);
      printf("\n");
    }
  }
  unsigned design = 1, sub = 2, subname = 3, lo = 4, hi = 5;
  int ok = hb_ot_layout_get_size_params(face, &design, &sub, &subname, &lo, &hi);
  printf("size %d %u %u %u %u %u\n", ok, design, sub, subname, lo, hi);
  return 0;
}
