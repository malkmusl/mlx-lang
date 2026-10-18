#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
/* The language systems of a font (argv[1]) with their features and the
 * lookups of each feature. */
int main(int argc, char **argv) {
  hb_face_t *face = hb_face_create(hb_blob_create_from_file(argv[1]), 0);
  hb_tag_t tables[] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};
  for (unsigned t = 0; t < 2; t++) {
    hb_tag_t scripts[32];
    unsigned sc = 32;
    hb_ot_layout_table_get_script_tags(face, tables[t], 0, &sc, scripts);
    for (unsigned s = 0; s < sc; s++) {
      unsigned lc = hb_ot_layout_script_get_language_tags(face, tables[t], s, 0, NULL, NULL);
      for (unsigned l = 0; l <= lc; l++) {
        unsigned li = l == lc ? HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX : l;
        unsigned req = 0;
        hb_tag_t reqtag = 0;
        hb_ot_layout_language_get_required_feature(face, tables[t], s, li, &req, &reqtag);
        unsigned fi[64], fc = 64;
        hb_ot_layout_language_get_feature_indexes(face, tables[t], s, li, 0, &fc, fi);
        printf("%u %c%c%c%c %u req %u:", t, HB_UNTAG(scripts[s]), li, req);
        for (unsigned k = 0; k < fc; k++) {
          hb_tag_t tag = 0;
          unsigned one = 1;
          hb_ot_layout_table_get_feature_tags(face, tables[t], fi[k], &one, &tag);
          unsigned lk[32], lkc = 32;
          hb_ot_layout_feature_get_lookups(face, tables[t], fi[k], 0, &lkc, lk);
          printf(" %u:%c%c%c%c(", fi[k], HB_UNTAG(tag));
          for (unsigned j = 0; j < lkc; j++) printf("%s%u", j ? "," : "", lk[j]);
          printf(")");
        }
        printf("\n");
      }
    }
  }
  return 0;
}
