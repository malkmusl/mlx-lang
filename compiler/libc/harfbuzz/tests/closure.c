#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Substitution closures of a font (argv[1]): per lookup, of all lookups,
 * of some lookups, and of shaping some texts. */
static void print_set(const char *name, hb_set_t *s) {
  printf(" %s%u[", name, hb_set_get_population(s));
  hb_codepoint_t first = HB_SET_VALUE_INVALID, last = HB_SET_VALUE_INVALID;
  unsigned n = 0;
  while (hb_set_next_range(s, &first, &last) && n < 60) { printf(" %u-%u", first, last); n++; }
  printf("]");
}

static const char *texts[] = {
  "Hello fi ffl office", "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9 (x)",
  "\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa5\x8d\xe0\xa4\xa6\xe0\xa5\x80", "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d [a]",
};

int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_font_t *font = hb_font_create(face);
  unsigned glyphs = hb_face_get_glyph_count(face);
  hb_set_t *start = hb_set_create();
  hb_set_t *unicodes = hb_set_create();
  hb_map_t *mapping = hb_map_create();
  hb_face_collect_nominal_glyph_mapping(face, mapping, unicodes);
  hb_codepoint_t u = HB_SET_VALUE_INVALID;
  unsigned k = 0;
  while (hb_set_next(unicodes, &u)) {
    if (u < 0x80 || (k++ % 3) == 0) hb_set_add(start, hb_map_get(mapping, u));
  }
  unsigned n = hb_ot_layout_table_get_lookup_count(face, HB_OT_TAG_GSUB);
  for (unsigned l = 0; l <= n && l < 500; l++) {
    hb_set_t *s = hb_set_copy(start);
    hb_ot_layout_lookup_substitute_closure(face, l, s);
    printf("L%u", l);
    print_set("", s);
    hb_set_clear(s);
    hb_set_add(s, l % (glyphs ? glyphs : 1));
    hb_set_add_range(s, l * 7 % (glyphs ? glyphs : 1), l * 7 % (glyphs ? glyphs : 1) + 20);
    hb_ot_layout_lookup_substitute_closure(face, l, s);
    print_set("x", s);
    printf("\n");
    hb_set_destroy(s);
  }
  hb_set_t *s = hb_set_copy(start);
  hb_ot_layout_lookups_substitute_closure(face, NULL, s);
  printf("all");
  print_set("", s);
  printf("\n");
  hb_set_t *lookups = hb_set_create();
  for (unsigned l = 0; l < n; l += 2) hb_set_add(lookups, l);
  hb_set_add(lookups, n + 3);
  hb_set_destroy(s);
  s = hb_set_copy(start);
  hb_ot_layout_lookups_substitute_closure(face, lookups, s);
  printf("even");
  print_set("", s);
  printf("\n");
  hb_set_destroy(s);
  hb_feature_t features[2];
  hb_feature_from_string("smcp", -1, &features[0]);
  hb_feature_from_string("-liga", -1, &features[1]);
  for (unsigned i = 0; i < sizeof texts / sizeof texts[0]; i++) {
    for (unsigned f = 0; f < 2; f++) {
      hb_buffer_t *b = hb_buffer_create();
      hb_buffer_add_utf8(b, texts[i], -1, 0, -1);
      hb_buffer_guess_segment_properties(b);
      s = hb_set_create();
      hb_ot_shape_glyphs_closure(font, b, f ? features : NULL, f ? 2 : 0, s);
      printf("text%u/%u", i, f);
      print_set("", s);
      printf("\n");
      hb_set_destroy(s);
      hb_buffer_destroy(b);
    }
  }
  return 0;
}
