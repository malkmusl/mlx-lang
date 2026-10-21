#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Glyph names of a font (argv[1]) with the OpenType font functions: each
 * glyph's name (whole and cut short), the glyph found by that name, and a
 * few names that are no glyph's. */
int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_font_t *font = hb_font_create(face);
  unsigned n = hb_face_get_glyph_count(face);
  unsigned limit = argc > 2 ? atoi(argv[2]) : 3000;
  if (n > limit) n = limit;
  for (unsigned g = 0; g < n + 2; g++) {
    char name[64] = "unset", shortname[4] = "xyz";
    int ok = hb_font_get_glyph_name(font, g, name, sizeof name);
    int ok2 = hb_font_get_glyph_name(font, g, shortname, sizeof shortname);
    int ok3 = hb_font_get_glyph_name(font, g, NULL, 0);
    hb_codepoint_t back = 12345;
    int found = ok ? hb_font_get_glyph_from_name(font, name, -1, &back) : -1;
    hb_codepoint_t back2 = 12345;
    int found2 = ok && strlen(name) > 1 ? hb_font_get_glyph_from_name(font, name, strlen(name) - 1, &back2) : -1;
    printf("g%u %d %s %d %s %d %d %u %d %u\n", g, ok, ok ? name : "-", ok2, ok2 ? shortname : "-", ok3, found, back, found2, back2);
  }
  const char *probes[] = {"", "a", "A", ".notdef", "space", "zzzzzz", "uni0041", "glyph1", "gid1", ".null", "a.sc", "f_f"};
  for (unsigned i = 0; i < sizeof probes / sizeof *probes; i++) {
    hb_codepoint_t g = 777;
    int ok = hb_font_get_glyph_from_name(font, probes[i], -1, &g);
    printf("probe %s %d %u\n", probes[i], ok, g);
  }
  return 0;
}
