#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Color fonts: palettes, layers, paint graphs, SVG and PNG glyphs, glyph
 * extents and a trace of hb_font_paint_glyph for each glyph (plain,
 * scaled and slanted with a ppem, at variations). Arguments: font file,
 * glyph limit, optional variations. */
static unsigned hash(hb_blob_t *b) {
  unsigned n; const char *d = hb_blob_get_data(b, &n); unsigned h = 5381;
  for (unsigned i = 0; i < n; i++) h = h * 33 + (unsigned char) d[i];
  return h ^ n;
}
static void line(hb_color_line_t *cl) {
  unsigned total = hb_color_line_get_color_stops(cl, 0, NULL, NULL);
  printf(" stops%u ext%d [", total, hb_color_line_get_extend(cl));
  hb_color_stop_t st[4];
  for (unsigned start = 0; start < total; start += 4) {
    unsigned n = 4;
    hb_color_line_get_color_stops(cl, start, &n, st);
    for (unsigned i = 0; i < n; i++) printf(" %.9g:%d:%08x", st[i].offset, st[i].is_foreground, st[i].color);
  }
  printf(" ]");
}
static void push_transform(hb_paint_funcs_t *f, void *d, float xx, float yx, float xy, float yy, float dx, float dy, void *u) { printf(" T(%.9g %.9g %.9g %.9g %.9g %.9g)", xx, yx, xy, yy, dx, dy); }
static void pop_transform(hb_paint_funcs_t *f, void *d, void *u) { printf(" t"); }
static hb_bool_t color_glyph(hb_paint_funcs_t *f, void *d, hb_codepoint_t g, hb_font_t *font, void *u) { printf(" CG%u", g); return g % 7 == 3; }
static void push_clip_glyph(hb_paint_funcs_t *f, void *d, hb_codepoint_t g, hb_font_t *font, void *u) { printf(" CLIPG%u", g); }
static void push_clip_rect(hb_paint_funcs_t *f, void *d, float a, float b, float c, float e, void *u) { printf(" CLIPR(%.9g %.9g %.9g %.9g)", a, b, c, e); }
static void pop_clip(hb_paint_funcs_t *f, void *d, void *u) { printf(" c"); }
static void color(hb_paint_funcs_t *f, void *d, hb_bool_t fg, hb_color_t col, void *u) { printf(" COLOR(%d %08x)", fg, col); }
static hb_bool_t image(hb_paint_funcs_t *f, void *d, hb_blob_t *b, unsigned w, unsigned h, hb_tag_t fmt, float slant, hb_glyph_extents_t *e, void *u) {
  printf(" IMG(%u %u %c%c%c%c %.9g %u", w, h, HB_UNTAG(fmt), slant, hash(b));
  if (e) printf(" %d,%d,%d,%d", e->x_bearing, e->y_bearing, e->width, e->height);
  printf(")");
  return w % 2 == 0;
}
static void linear(hb_paint_funcs_t *f, void *d, hb_color_line_t *cl, float a, float b, float c, float e, float g, float h, void *u) { printf(" LIN(%.9g %.9g %.9g %.9g %.9g %.9g", a, b, c, e, g, h); line(cl); printf(")"); }
static void radial(hb_paint_funcs_t *f, void *d, hb_color_line_t *cl, float a, float b, float c, float e, float g, float h, void *u) { printf(" RAD(%.9g %.9g %.9g %.9g %.9g %.9g", a, b, c, e, g, h); line(cl); printf(")"); }
static void sweep(hb_paint_funcs_t *f, void *d, hb_color_line_t *cl, float a, float b, float c, float e, void *u) { printf(" SWP(%.9g %.9g %.9g %.9g", a, b, c, e); line(cl); printf(")"); }
static void push_group(hb_paint_funcs_t *f, void *d, void *u) { printf(" G"); }
static void pop_group(hb_paint_funcs_t *f, void *d, hb_paint_composite_mode_t m, void *u) { printf(" g%d", m); }
static hb_bool_t custom(hb_paint_funcs_t *f, void *d, unsigned idx, hb_color_t *c, void *u) {
  if (idx % 5 == 1) { *c = 0x11223344u * (idx + 1); return 1; }
  return 0;
}

static void dump(const char *what, hb_font_t *font, hb_face_t *face, unsigned limit, hb_paint_funcs_t *pf) {
  printf("== %s\n", what);
  unsigned n = hb_face_get_glyph_count(face);
  if (n > limit) n = limit;
  for (unsigned g = 0; g < n; g++) {
    hb_glyph_extents_t e = {0};
    int ok = hb_font_get_glyph_extents(font, g, &e);
    hb_blob_t *png = hb_ot_color_glyph_reference_png(font, g);
    printf("g%u %d %d,%d,%d,%d png%u:%u", g, ok, e.x_bearing, e.y_bearing, e.width, e.height, hb_blob_get_length(png), hash(png));
    hb_blob_destroy(png);
    hb_font_paint_glyph(font, g, pf, NULL, g % 3, 0x10203040u);
    printf("\n");
  }
}

int main(int argc, char **argv) {
  hb_blob_t *blob = hb_blob_create_from_file(argv[1]);
  hb_face_t *face = hb_face_create(blob, 0);
  unsigned limit = argc > 2 ? atoi(argv[2]) : 300;
  unsigned np = hb_ot_color_palette_get_count(face);
  printf("palettes %d %u layers %d paint %d svg %d png %d\n", hb_ot_color_has_palettes(face), np,
         hb_ot_color_has_layers(face), hb_ot_color_has_paint(face), hb_ot_color_has_svg(face), hb_ot_color_has_png(face));
  for (unsigned p = 0; p <= np && p < 20; p++) {
    hb_color_t colors[300];
    unsigned count = 300;
    unsigned total = hb_ot_color_palette_get_colors(face, p, 0, &count, colors);
    printf("palette %u flags %d name %u total %u count %u:", p, hb_ot_color_palette_get_flags(face, p), hb_ot_color_palette_get_name_id(face, p), total, count);
    for (unsigned i = 0; i < count; i++) printf(" %08x", colors[i]);
    count = 3;
    total = hb_ot_color_palette_get_colors(face, p, 2, &count, colors);
    printf(" | %u %u", total, count);
    printf("\n");
  }
  for (unsigned i = 0; i < 20; i++) printf(" %u", hb_ot_color_palette_color_get_name_id(face, i));
  printf("\n");
  unsigned n = hb_face_get_glyph_count(face);
  if (n > limit) n = limit;
  for (unsigned g = 0; g < n + 2; g++) {
    hb_ot_color_layer_t layers[8];
    unsigned count = 8;
    unsigned total = hb_ot_color_glyph_get_layers(face, g, 0, &count, layers);
    unsigned count2 = 2;
    unsigned total2 = hb_ot_color_glyph_get_layers(face, g, 1, &count2, layers + 4);
    hb_blob_t *svg = hb_ot_color_glyph_reference_svg(face, g);
    printf("g%u layers %u %u", g, total, count);
    for (unsigned i = 0; i < count; i++) printf(" %u/%u", layers[i].glyph, layers[i].color_index);
    printf(" | %u %u paint %d svg %u:%u\n", total2, count2, hb_ot_color_glyph_has_paint(face, g), hb_blob_get_length(svg), hash(svg));
    hb_blob_destroy(svg);
  }
  hb_paint_funcs_t *pf = hb_paint_funcs_create();
  hb_paint_funcs_set_push_transform_func(pf, push_transform, NULL, NULL);
  hb_paint_funcs_set_pop_transform_func(pf, pop_transform, NULL, NULL);
  hb_paint_funcs_set_color_glyph_func(pf, color_glyph, NULL, NULL);
  hb_paint_funcs_set_push_clip_glyph_func(pf, push_clip_glyph, NULL, NULL);
  hb_paint_funcs_set_push_clip_rectangle_func(pf, push_clip_rect, NULL, NULL);
  hb_paint_funcs_set_pop_clip_func(pf, pop_clip, NULL, NULL);
  hb_paint_funcs_set_color_func(pf, color, NULL, NULL);
  hb_paint_funcs_set_image_func(pf, image, NULL, NULL);
  hb_paint_funcs_set_linear_gradient_func(pf, linear, NULL, NULL);
  hb_paint_funcs_set_radial_gradient_func(pf, radial, NULL, NULL);
  hb_paint_funcs_set_sweep_gradient_func(pf, sweep, NULL, NULL);
  hb_paint_funcs_set_push_group_func(pf, push_group, NULL, NULL);
  hb_paint_funcs_set_pop_group_func(pf, pop_group, NULL, NULL);
  hb_paint_funcs_set_custom_palette_color_func(pf, custom, NULL, NULL);
  hb_font_t *font = hb_font_create(face);
  dump("plain", font, face, limit, pf);
  hb_font_set_scale(font, 2000, -1500);
  hb_font_set_ppem(font, 40, 30);
  hb_font_set_synthetic_slant(font, 0.2f);
  dump("scaled", font, face, limit, pf);
  hb_font_set_ppem(font, 200, 0);
  hb_font_set_synthetic_slant(font, 0.f);
  hb_font_set_scale(font, 0, 0);
  dump("ppem 200, scale 0", font, face, limit, pf);
  hb_font_destroy(font);
  if (argc > 3) {
    font = hb_font_create(face);
    hb_variation_t v[8];
    unsigned nv = 0;
    char *s = argv[3];
    while (*s && nv < 8) {
      char *e = strchr(s, ',');
      int len = e ? e - s : (int) strlen(s);
      if (hb_variation_from_string(s, len, &v[nv])) nv++;
      s += len + (e ? 1 : 0);
    }
    hb_font_set_variations(font, v, nv);
    dump("varied", font, face, limit, pf);
    hb_font_destroy(font);
  }
  return 0;
}
