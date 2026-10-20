#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
/* Justification of some texts with a font (argv[1]) to targets around
 * their default advance. */
static const char *texts[] = {"ABCABC", "Hello justified world", "AAA", ""};
static const float factors[][2] = {{0.9f, 0.95f}, {1.05f, 1.1f}, {0.5f, 0.6f}, {1.5f, 2.0f}, {1.0f, 1.0f}, {0.99f, 1.01f}};

int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_font_t *font = hb_font_create(face);
  for (unsigned i = 0; i < sizeof texts / sizeof texts[0]; i++) {
    hb_buffer_t *b = hb_buffer_create();
    hb_buffer_add_utf8(b, texts[i], -1, 0, -1);
    hb_buffer_guess_segment_properties(b);
    hb_shape(font, b, NULL, 0);
    unsigned n;
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(b, &n);
    float base = 0;
    for (unsigned k = 0; k < n; k++) base += pos[k].x_advance;
    hb_buffer_destroy(b);
    for (unsigned f = 0; f < sizeof factors / sizeof factors[0]; f++) {
      for (int given = 0; given < 2; given++) {
        hb_font_t *sub = hb_font_create(face);
        b = hb_buffer_create();
        hb_buffer_add_utf8(b, texts[i], -1, 0, -1);
        hb_buffer_guess_segment_properties(b);
        float advance = given ? base : 0;
        hb_tag_t tag = 1;
        float value = -1;
        int r = hb_shape_justify(sub, b, NULL, 0, NULL, base * factors[f][0], base * factors[f][1], &advance, &tag, &value);
        hb_glyph_position_t *p = hb_buffer_get_glyph_positions(b, &n);
        float total = 0;
        for (unsigned k = 0; k < n; k++) total += p[k].x_advance;
        printf("%u/%u/%d: %d adv %.3f tag %08x value %.6f total %.1f len %u\n", i, f, given, r, advance, tag, value, total, n);
        hb_buffer_destroy(b);
        hb_font_destroy(sub);
      }
    }
  }
  return 0;
}
