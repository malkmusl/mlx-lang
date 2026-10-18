#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Outlines and extents with the OpenType font functions: each glyph's
 * path, extents, advances and origins, plain, scaled with negative scales,
 * slanted, emboldened (in place and not), and at variation coordinates
 * when the font has axes. Arguments: font file, glyph limit, then optional
 * variations ("wght=700,wdth=80"). */
static void move_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float x, float y, void *u) { printf(" M%.9g,%.9g", x, y); }
static void line_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float x, float y, void *u) { printf(" L%.9g,%.9g", x, y); }
static void quad_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float cx, float cy, float x, float y, void *u) { printf(" Q%.9g,%.9g,%.9g,%.9g", cx, cy, x, y); }
static void cubic_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float a, float b, float c, float e, float x, float y, void *u) { printf(" C%.9g,%.9g,%.9g,%.9g,%.9g,%.9g", a, b, c, e, x, y); }
static void close_path(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, void *u) { printf(" Z"); }

static void dump(const char *what, hb_font_t *font, unsigned limit, hb_draw_funcs_t *df) {
  printf("== %s\n", what);
  unsigned n = hb_face_get_glyph_count(hb_font_get_face(font));
  if (n > limit) n = limit;
  for (unsigned g = 0; g < n; g++) {
    hb_glyph_extents_t e = {0};
    int ok = hb_font_get_glyph_extents(font, g, &e);
    hb_position_t vx = 0, vy = 0;
    hb_font_get_glyph_v_origin(font, g, &vx, &vy);
    printf("g%u %d %d,%d,%d,%d adv %d,%d vo %d,%d", g, ok, e.x_bearing, e.y_bearing, e.width, e.height,
           hb_font_get_glyph_h_advance(font, g), hb_font_get_glyph_v_advance(font, g), vx, vy);
    hb_font_draw_glyph(font, g, df, NULL);
    printf("\n");
  }
}

int main(int argc, char **argv) {
  hb_draw_funcs_t *df = hb_draw_funcs_create();
  hb_draw_funcs_set_move_to_func(df, move_to, NULL, NULL);
  hb_draw_funcs_set_line_to_func(df, line_to, NULL, NULL);
  hb_draw_funcs_set_quadratic_to_func(df, quad_to, NULL, NULL);
  hb_draw_funcs_set_cubic_to_func(df, cubic_to, NULL, NULL);
  hb_draw_funcs_set_close_path_func(df, close_path, NULL, NULL);
  hb_blob_t *blob = hb_blob_create_from_file(argv[1]);
  hb_face_t *face = hb_face_create(blob, 0);
  unsigned limit = argc > 2 ? atoi(argv[2]) : 200;
  hb_font_t *font = hb_font_create(face);
  dump("plain", font, limit, df);
  hb_font_set_scale(font, -1500, 2500);
  dump("scaled", font, limit, df);
  hb_font_set_synthetic_slant(font, 0.25f);
  dump("slanted", font, limit, df);
  hb_font_set_synthetic_slant(font, 0.f);
  hb_font_set_scale(font, 1000, 1000);
  hb_font_set_synthetic_bold(font, 0.02f, 0.03f, 1);
  dump("bold in place", font, limit, df);
  hb_font_set_synthetic_bold(font, 0.04f, 0.01f, 0);
  dump("bold", font, limit, df);
  hb_font_set_scale(font, -1000, -800);
  dump("bold negative", font, limit, df);
  hb_font_destroy(font);
  if (argc > 3) {
    font = hb_font_create(face);
    hb_variation_t vars[8];
    unsigned nv = 0;
    char *s = strdup(argv[3]);
    for (char *t = strtok(s, ","); t && nv < 8; t = strtok(NULL, ","))
      if (hb_variation_from_string(t, -1, &vars[nv])) nv++;
    hb_font_set_variations(font, vars, nv);
    dump("varied", font, limit, df);
    hb_font_set_synthetic_bold(font, 0.02f, 0.02f, 0);
    dump("varied bold", font, limit, df);
    hb_font_destroy(font);
  }
  return 0;
}
