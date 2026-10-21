#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <hb-icu.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* hb-ft and hb-icu: fonts over FreeType faces (advances, extents, names,
 * origins, contour points, outlines, shaping), cached faces, FreeType
 * functions set on a HarfBuzz font, and ICU script codes. */
static void move_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float x, float y, void *u) { printf(" M%g,%g", x, y); }
static void line_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float x, float y, void *u) { printf(" L%g,%g", x, y); }
static void quad_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float cx, float cy, float x, float y, void *u) { printf(" Q%g,%g,%g,%g", cx, cy, x, y); }
static void cubic_to(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, float a, float b, float c, float e, float x, float y, void *u) { printf(" C%g,%g,%g,%g,%g,%g", a, b, c, e, x, y); }
static void close_path(hb_draw_funcs_t *d, void *data, hb_draw_state_t *st, void *u) { printf(" Z"); }

static void dump(hb_font_t *font, const char *text, unsigned glyphs, int outlines) {
  hb_draw_funcs_t *df = hb_draw_funcs_create();
  hb_draw_funcs_set_move_to_func(df, move_to, NULL, NULL);
  hb_draw_funcs_set_line_to_func(df, line_to, NULL, NULL);
  hb_draw_funcs_set_quadratic_to_func(df, quad_to, NULL, NULL);
  hb_draw_funcs_set_cubic_to_func(df, cubic_to, NULL, NULL);
  hb_draw_funcs_set_close_path_func(df, close_path, NULL, NULL);
  int xs, ys;
  hb_font_get_scale(font, &xs, &ys);
  printf("scale %d %d load %d\n", xs, ys, hb_ft_font_get_load_flags(font));
  hb_font_extents_t fe;
  hb_font_get_h_extents(font, &fe);
  printf("h extents %d %d %d\n", fe.ascender, fe.descender, fe.line_gap);
  for (unsigned g = 0; g < glyphs; g++) {
    hb_glyph_extents_t e = {0};
    int ok = hb_font_get_glyph_extents(font, g, &e);
    char name[64] = "";
    int nok = hb_font_get_glyph_name(font, g, name, sizeof name);
    hb_codepoint_t back = 0;
    int bok = nok ? hb_font_get_glyph_from_name(font, name, -1, &back) : 0;
    hb_position_t vx = 0, vy = 0;
    hb_font_get_glyph_v_origin(font, g, &vx, &vy);
    hb_position_t px = 0, py = 0;
    int pok = hb_font_get_glyph_contour_point(font, g, 1, &px, &py);
    printf("g%u adv %d,%d ext %d %d,%d,%d,%d name %d %s %d %u vorig %d,%d pt %d %d,%d", g,
           hb_font_get_glyph_h_advance(font, g), hb_font_get_glyph_v_advance(font, g),
           ok, e.x_bearing, e.y_bearing, e.width, e.height, nok, name, bok, back, vx, vy, pok, px, py);
    if (outlines && g % 7 == 0) hb_font_draw_glyph(font, g, df, NULL);
    printf("\n");
  }
  hb_buffer_t *buf = hb_buffer_create();
  hb_buffer_add_utf8(buf, text, -1, 0, -1);
  hb_buffer_guess_segment_properties(buf);
  hb_shape(font, buf, NULL, 0);
  char out[8192];
  hb_buffer_serialize(buf, 0, hb_buffer_get_length(buf), out, sizeof out, NULL, font, HB_BUFFER_SERIALIZE_FORMAT_TEXT, HB_BUFFER_SERIALIZE_FLAG_DEFAULT);
  printf("shape %s\n", out);
  hb_buffer_destroy(buf);
  hb_draw_funcs_destroy(df);
}

int main(int argc, char **argv) {
  FT_Library lib;
  FT_Init_FreeType(&lib);
  const char *text = "Hello, AVATAR fi ffl \xc3\xa9t\xc3\xa9 \xd0\x9f\xd1\x80\xd0\xb8";
  for (int a = 1; a < argc; a++) {
    FT_Face ft;
    if (FT_New_Face(lib, argv[a], 0, &ft)) { printf("cannot open %s\n", argv[a]); continue; }
    printf("== %s\n", argv[a]);
    FT_Set_Char_Size(ft, 0, 16 * 64, 72, 72);
    hb_font_t *font = hb_ft_font_create_referenced(ft);
    printf("face index %u upem %u\n", hb_face_get_index(hb_font_get_face(font)), hb_face_get_upem(hb_font_get_face(font)));
    printf("get_face same %d\n", hb_ft_font_get_face(font) == ft);
    dump(font, text, 120, 1);
    FT_Set_Char_Size(ft, 0, 40 * 64, 72, 72);
    hb_ft_font_changed(font);
    hb_ft_font_set_load_flags(font, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
    dump(font, text, 40, 0);
    hb_font_set_scale(font, -2000, 3000);
    printf("hb changed %d\n", hb_ft_hb_font_changed(font));
    printf("hb changed again %d\n", hb_ft_hb_font_changed(font));
    dump(font, text, 40, 1);
    hb_font_destroy(font);

    hb_face_t *c1 = hb_ft_face_create_cached(ft);
    hb_face_t *c2 = hb_ft_face_create_cached(ft);
    printf("cached same %d glyphs %u\n", c1 == c2, hb_face_get_glyph_count(c1));
    hb_face_destroy(c1);
    hb_face_destroy(c2);

    hb_face_t *rf = hb_ft_face_create_referenced(ft);
    hb_blob_t *head = hb_face_reference_table(rf, HB_TAG('h','e','a','d'));
    printf("referenced face upem %u head %u\n", hb_face_get_upem(rf), hb_blob_get_length(head));
    hb_blob_destroy(head);
    hb_face_destroy(rf);
    FT_Done_Face(ft);

    hb_blob_t *blob = hb_blob_create_from_file(argv[a]);
    hb_face_t *face = hb_face_create(blob, 0);
    hb_font_t *f2 = hb_font_create(face);
    hb_font_set_scale(f2, 1500, 1500);
    hb_ft_font_set_funcs(f2);
    printf("set_funcs face %d\n", hb_ft_font_get_face(f2) != NULL);
    dump(f2, text, 30, 1);
    hb_font_destroy(f2);
    hb_face_destroy(face);
    hb_blob_destroy(blob);
  }
  /* (Up to the scripts of the system ICU, Unicode 15.1.) */
  for (int s = -1; s < 200; s++) {
    hb_script_t script = hb_icu_script_to_script((UScriptCode) s);
    printf("%d:%c%c%c%c:%d ", s, HB_UNTAG(script), (int) hb_icu_script_from_script(script));
  }
  printf("\nfrom invalid %d unknown %d\n", (int) hb_icu_script_from_script(HB_SCRIPT_INVALID), (int) hb_icu_script_from_script(HB_TAG('Q','a','a','x')));
  hb_unicode_funcs_t *icu = hb_icu_get_unicode_funcs();
  printf("icu funcs parent is empty %d same %d\n", hb_unicode_funcs_get_parent(icu) == hb_unicode_funcs_get_empty(), icu == hb_icu_get_unicode_funcs());
  return 0;
}
