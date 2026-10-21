#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
/* The characters of a font (argv[1]) and their nominal glyphs. */
int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_set_t *u = hb_set_create(), *u2 = hb_set_create();
  hb_map_t *map = hb_map_create();
  hb_face_collect_unicodes(face, u);
  hb_face_collect_nominal_glyph_mapping(face, map, u2);
  printf("unicodes %u mapped %u map %u\n", hb_set_get_population(u), hb_set_get_population(u2), hb_map_get_population(map));
  hb_codepoint_t first = HB_SET_VALUE_INVALID, last = HB_SET_VALUE_INVALID;
  while (hb_set_next_range(u, &first, &last)) printf(" %x-%x", first, last);
  printf("\n");
  first = last = HB_SET_VALUE_INVALID;
  while (hb_set_next_range(u2, &first, &last)) printf(" %x-%x", first, last);
  printf("\n");
  hb_codepoint_t c = HB_SET_VALUE_INVALID;
  unsigned long sum = 0;
  while (hb_set_next(u2, &c)) sum = sum * 31 + c * 7 + hb_map_get(map, c);
  printf("sum %lu\n", sum);
  return 0;
}
