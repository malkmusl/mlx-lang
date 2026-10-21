#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The MATH table of each font argument: constants, per-glyph values,
 * variants and assemblies, at two scales and with ppem set; --glyphs=N
 * before fonts looks at their first N glyphs only (3000 otherwise). */
static void dump(hb_font_t *font, hb_face_t *face, unsigned glyphs) {
  printf("has_data %d\n", hb_ot_math_has_data(face));
  for (int c = 0; c <= 57; c++) printf("c%d=%d ", c, hb_ot_math_get_constant(font, (hb_ot_math_constant_t) c));
  printf("\noverlap h %d v %d\n", hb_ot_math_get_min_connector_overlap(font, HB_DIRECTION_LTR),
         hb_ot_math_get_min_connector_overlap(font, HB_DIRECTION_TTB));
  for (unsigned g = 0; g < glyphs; g++) {
    hb_position_t ic = hb_ot_math_get_glyph_italics_correction(font, g);
    hb_position_t ta = hb_ot_math_get_glyph_top_accent_attachment(font, g);
    int ext = hb_ot_math_is_glyph_extended_shape(face, g);
    printf("g%u ic %d ta %d ext %d", g, ic, ta, ext);
    for (int k = 0; k < 5; k++) {
      printf(" k%d:%d,%d,%d", k, hb_ot_math_get_glyph_kerning(font, g, (hb_ot_math_kern_t) k, -500),
             hb_ot_math_get_glyph_kerning(font, g, (hb_ot_math_kern_t) k, 0),
             hb_ot_math_get_glyph_kerning(font, g, (hb_ot_math_kern_t) k, 700));
      hb_ot_math_kern_entry_t entries[8];
      unsigned n = 8;
      unsigned total = hb_ot_math_get_glyph_kernings(font, g, (hb_ot_math_kern_t) k, 0, &n, entries);
      printf(" [%u/%u", n, total);
      for (unsigned i = 0; i < n; i++) printf(" %d:%d", entries[i].max_correction_height, entries[i].kern_value);
      printf("]");
    }
    hb_direction_t dirs[2] = {HB_DIRECTION_LTR, HB_DIRECTION_BTT};
    for (int d = 0; d < 2; d++) {
      hb_ot_math_glyph_variant_t v[16];
      unsigned n = 16;
      unsigned total = hb_ot_math_get_glyph_variants(font, g, dirs[d], 1, &n, v);
      printf(" v%d %u/%u", d, n, total);
      for (unsigned i = 0; i < n; i++) printf(" %u:%d", v[i].glyph, v[i].advance);
      hb_ot_math_glyph_part_t parts[16];
      hb_position_t italic = -1;
      n = 16;
      total = hb_ot_math_get_glyph_assembly(font, g, dirs[d], 0, &n, parts, &italic);
      printf(" a%d %u/%u i%d", d, n, total, italic);
      for (unsigned i = 0; i < n; i++) printf(" %u:%d:%d:%d:%u", parts[i].glyph, parts[i].start_connector_length, parts[i].end_connector_length, parts[i].full_advance, parts[i].flags);
    }
    printf("\n");
  }
}

int main(int argc, char **argv) {
  unsigned limit = 3000;
  for (int a = 1; a < argc; a++) {
    if (!strncmp(argv[a], "--glyphs=", 9)) { limit = atoi(argv[a] + 9); continue; }
    printf("== %s\n", argv[a]);
    hb_blob_t *blob = hb_blob_create_from_file(argv[a]);
    hb_face_t *face = hb_face_create(blob, 0);
    hb_font_t *font = hb_font_create(face);
    unsigned glyphs = hb_face_get_glyph_count(face);
    if (glyphs > limit) glyphs = limit;
    dump(font, face, glyphs);
    hb_font_set_scale(font, 2345, -1234);
    hb_font_set_ppem(font, 13, 17);
    dump(font, face, glyphs);
  }
  return 0;
}
