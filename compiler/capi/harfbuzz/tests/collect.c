#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Glyphs collected per lookup of GSUB and GPOS, glyph alternates and
 * optical bounds of a font (argv[1]). */
static void print_set(const char *name, hb_set_t *s) {
  printf(" %s%u[", name, hb_set_get_population(s));
  hb_codepoint_t first = HB_SET_VALUE_INVALID, last = HB_SET_VALUE_INVALID;
  unsigned n = 0;
  while (hb_set_next_range(s, &first, &last) && n < 40) { printf(" %u-%u", first, last); n++; }
  printf("]");
}

int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_font_t *font = hb_font_create(face);
  hb_tag_t tables[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};
  unsigned glyphs = hb_face_get_glyph_count(face);
  for (unsigned t = 0; t < 2; t++) {
    unsigned n = hb_ot_layout_table_get_lookup_count(face, tables[t]);
    for (unsigned l = 0; l <= n && l < 400; l++) {
      hb_set_t *b = hb_set_create(), *i = hb_set_create(), *a = hb_set_create(), *o = hb_set_create();
      hb_ot_layout_lookup_collect_glyphs(face, tables[t], l, b, i, a, o);
      printf("%c%u", t ? 'P' : 'S', l);
      print_set("b", b); print_set("i", i); print_set("a", a); print_set("o", o);
      hb_set_clear(i);
      hb_ot_layout_lookup_collect_glyphs(face, tables[t], l, NULL, i, NULL, NULL);
      print_set("i'", i);
      hb_set_clear(o);
      hb_ot_layout_lookup_collect_glyphs(face, tables[t], l, NULL, NULL, NULL, o);
      print_set("o'", o);
      printf("\n");
      if (t == 0) {
        for (unsigned g = 0; g < glyphs && g < 600; g++) {
          hb_codepoint_t alts[4];
          unsigned count = 4;
          unsigned total = hb_ot_layout_lookup_get_glyph_alternates(face, l, g, 1, &count, alts);
          unsigned count0 = 0;
          unsigned total0 = hb_ot_layout_lookup_get_glyph_alternates(face, l, g, 0, &count0, NULL);
          unsigned totalnull = hb_ot_layout_lookup_get_glyph_alternates(face, l, g, 0, NULL, NULL);
          if (!total && !total0 && !totalnull) continue;
          printf(" alt %u %u %u %u %u:", g, total, count, total0, totalnull);
          for (unsigned k = 0; k < count; k++) printf(" %u", alts[k]);
          printf("\n");
        }
      }
      /* (hb_ot_layout_lookup_get_optical_bound of HarfBuzz 8.3 crashes on
       * single positioning lookups: not compared.) */
      hb_set_destroy(b); hb_set_destroy(i); hb_set_destroy(a); hb_set_destroy(o);
    }
  }
  return 0;
}
