#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Fonts with the OpenType functions (each argument a font file[:index]):
 * extents, metrics, the characters and their glyphs, advances, origins,
 * glyph extents and names, scaled, emboldened and slanted. */
static void font_test(const char *path, unsigned index) {
  hb_blob_t *blob = hb_blob_create_from_file(path);
  hb_face_t *face = hb_face_create(blob, index);
  hb_font_t *font = hb_font_create(face);
  printf("== %s %u upem=%u glyphs=%u\n", path, index, hb_face_get_upem(face), hb_face_get_glyph_count(face));
  hb_font_extents_t fe;
  int r = hb_font_get_h_extents(font, &fe);
  printf("hext %d %d %d %d\n", r, fe.ascender, fe.descender, fe.line_gap);
  r = hb_font_get_v_extents(font, &fe);
  printf("vext %d %d %d %d\n", r, fe.ascender, fe.descender, fe.line_gap);
  static const hb_ot_metrics_tag_t tags[] = {HB_OT_METRICS_TAG_HORIZONTAL_ASCENDER, HB_OT_METRICS_TAG_HORIZONTAL_CLIPPING_ASCENT, HB_OT_METRICS_TAG_X_HEIGHT, HB_OT_METRICS_TAG_CAP_HEIGHT, HB_OT_METRICS_TAG_UNDERLINE_OFFSET, HB_OT_METRICS_TAG_STRIKEOUT_SIZE, HB_OT_METRICS_TAG_SUPERSCRIPT_EM_Y_OFFSET, HB_OT_METRICS_TAG_HORIZONTAL_CARET_RISE};
  for (unsigned i = 0; i < sizeof tags / sizeof tags[0]; i++) {
    hb_position_t p = 0;
    int ok = hb_ot_metrics_get_position(font, tags[i], &p);
    hb_position_t q = 0;
    hb_ot_metrics_get_position_with_fallback(font, tags[i], &q);
    printf("metric %c%c%c%c %d %d %d\n", HB_UNTAG(tags[i]), ok, p, q);
  }
  hb_set_t *unicodes = hb_set_create();
  hb_face_collect_unicodes(face, unicodes);
  printf("unicodes %u min %x max %x\n", hb_set_get_population(unicodes), hb_set_get_min(unicodes), hb_set_get_max(unicodes));
  hb_set_t *vs = hb_set_create();
  hb_face_collect_variation_selectors(face, vs);
  printf("vs %u\n", hb_set_get_population(vs));
  unsigned long sum = 0;
  hb_codepoint_t u = HB_SET_VALUE_INVALID;
  int n = 0;
  while (hb_set_next(unicodes, &u)) {
    hb_codepoint_t g;
    if (!hb_font_get_nominal_glyph(font, u, &g)) { printf("missing %x\n", u); continue; }
    sum = sum * 31 + g;
    if ((n++ % 97) == 0) printf("u %x -> %u\n", u, g);
  }
  printf("cmap sum %lx\n", sum);
  unsigned count = hb_face_get_glyph_count(face);
  if (count > 3000) count = 3000;
  unsigned long asum = 0, esum = 0, nsum = 0;
  for (hb_codepoint_t g = 0; g < count + 2; g++) {
    hb_position_t a = hb_font_get_glyph_h_advance(font, g);
    hb_position_t va = hb_font_get_glyph_v_advance(font, g);
    hb_position_t ox, oy;
    hb_font_get_glyph_v_origin(font, g, &ox, &oy);
    hb_glyph_extents_t e = {0};
    int ok = hb_font_get_glyph_extents(font, g, &e);
    char name[64] = "";
    int nok = hb_font_get_glyph_name(font, g, name, sizeof name);
    hb_codepoint_t back = 12345;
    int bok = nok ? hb_font_get_glyph_from_name(font, name, -1, &back) : -1;
    asum = asum * 31 + a * 7 + va * 3 + ox * 5 + oy;
    esum = esum * 31 + ok + e.x_bearing * 3 + e.y_bearing * 5 + e.width * 7 + e.height;
    for (char *p = name; *p; p++) nsum = nsum * 31 + *p;
    nsum = nsum * 3 + bok + back;
    if (g % 211 == 0 || g >= count) printf("g %u adv %d %d org %d %d ext %d %d %d %d %d name %d %s %d %u\n", g, a, va, ox, oy, ok, e.x_bearing, e.y_bearing, e.width, e.height, nok, name, bok, back);
  }
  printf("sums %lx %lx %lx\n", asum, esum, nsum);
  hb_font_set_scale(font, 2048 * 3, -1000);
  hb_font_set_synthetic_bold(font, 0.02, 0.03, 0);
  hb_font_set_synthetic_slant(font, 0.2);
  for (hb_codepoint_t g = 0; g < 40 && g < count; g += 3) {
    hb_glyph_extents_t e = {0};
    int ok = hb_font_get_glyph_extents(font, g, &e);
    hb_position_t x, y;
    hb_font_get_glyph_v_origin(font, g, &x, &y);
    printf("scaled %u %d %d ext %d %d %d %d %d org %d %d\n", g, hb_font_get_glyph_h_advance(font, g), hb_font_get_glyph_v_advance(font, g), ok, e.x_bearing, e.y_bearing, e.width, e.height, x, y);
  }
  r = hb_font_get_h_extents(font, &fe);
  printf("hext %d %d %d %d\n", r, fe.ascender, fe.descender, fe.line_gap);
  hb_set_destroy(unicodes);
  hb_set_destroy(vs);
  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    char *colon = strchr(argv[i], ':');
    unsigned index = 0;
    if (colon) { *colon = 0; index = atoi(colon + 1); }
    font_test(argv[i], index);
  }
  return 0;
}
