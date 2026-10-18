/* A small hb-shape (the options HarfBuzz's shaping tests use) in batch
 * mode: each line of standard input is a font file and hb-shape's
 * arguments, separated by tabs; each output line is the glyphs as hb-shape
 * prints them. */
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *encode_unicodes(const char *s, int *length) {
  char *out = malloc(strlen(s) * 4 + 8);
  char *o = out;
  const char *p = s;
  while (*p) {
    while (*p && strchr(" ,;\t", *p)) p++;
    if (!*p) break;
    if ((p[0] == 'U' || p[0] == 'u') && p[1] == '+') p += 2;
    char *end;
    unsigned long u = strtoul(p, &end, 16);
    if (end == p) break;
    p = end;
    if (u < 0x80) *o++ = u;
    else if (u < 0x800) { *o++ = 0xC0 | (u >> 6); *o++ = 0x80 | (u & 0x3F); }
    else if (u < 0x10000) { *o++ = 0xE0 | (u >> 12); *o++ = 0x80 | ((u >> 6) & 0x3F); *o++ = 0x80 | (u & 0x3F); }
    else { *o++ = 0xF0 | (u >> 18); *o++ = 0x80 | ((u >> 12) & 0x3F); *o++ = 0x80 | ((u >> 6) & 0x3F); *o++ = 0x80 | (u & 0x3F); }
  }
  *o = 0;
  *length = o - out;
  return out;
}

/* Values separated by commas or spaces. */
static unsigned parse_list(const char *s, int features, hb_feature_t *fs, hb_variation_t *vs, unsigned max) {
  unsigned n = 0;
  const char *p = s;
  while (p && *p && n < max) {
    const char *end = strpbrk(p, ", ");
    int len = end ? (int) (end - p) : -1;
    if (features ? hb_feature_from_string(p, len, &fs[n]) : hb_variation_from_string(p, len, &vs[n])) n++;
    p = end ? end + 1 : NULL;
  }
  return n;
}

static void run(int argc, char **argv) {
  const char *fontfile = argv[0];
  const char *funcs = "ot", *shaper = NULL, *unicodes = "", *before = NULL, *after = NULL;
  const char *direction = NULL, *language = NULL, *script = NULL;
  int names = 1, positions = 1, clusters = 1, advances = 1, extents = 0, flagsout = 0;
  int bot = 0, eot = 0, remove = 0, preserve = 0, unsafe = 0, verify = 0;
  int cluster_level = 0, face_index = 0;
  double size_x = -1, size_y = -1, ptem = 0;
  hb_feature_t features[64];
  hb_variation_t variations[64];
  unsigned num_features = 0, num_variations = 0;
  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    char *v = strchr(a, '=');
    char name[64];
    if (v) {
      snprintf(name, sizeof name, "%.*s", (int) (v - a), a);
      v++;
      /* (A value in double quotes, as a shell would pass it.) */
      size_t n = strlen(v);
      if (n >= 2 && v[0] == '"' && v[n - 1] == '"') { v[n - 1] = 0; v++; }
    }
    else { snprintf(name, sizeof name, "%s", a); }
#define VALUE() (v ? v : (i + 1 < argc ? argv[++i] : ""))
    if (!strcmp(name, "--font-funcs")) funcs = VALUE();
    else if (!strcmp(name, "--shaper") || !strcmp(name, "--shapers")) shaper = VALUE();
    else if (!strcmp(name, "--unicodes")) unicodes = VALUE();
    else if (!strcmp(name, "--unicodes-before")) before = VALUE();
    else if (!strcmp(name, "--unicodes-after")) after = VALUE();
    else if (!strcmp(name, "--direction")) direction = VALUE();
    else if (!strcmp(name, "--language")) language = VALUE();
    else if (!strcmp(name, "--script")) script = VALUE();
    else if (!strcmp(name, "--features")) num_features = parse_list(VALUE(), 1, features, NULL, 64);
    else if (!strcmp(name, "--variations")) num_variations = parse_list(VALUE(), 0, NULL, variations, 64);
    else if (!strcmp(name, "--font-size")) { const char *s = VALUE(); if (sscanf(s, "%lf %lf", &size_x, &size_y) == 1) size_y = size_x; }
    else if (!strcmp(name, "--font-ptem")) ptem = atof(VALUE());
    else if (!strcmp(name, "--cluster-level")) cluster_level = atoi(VALUE());
    else if (!strcmp(name, "--face-index")) face_index = atoi(VALUE());
    else if (!strcmp(name, "--no-glyph-names")) names = 0;
    else if (!strcmp(name, "--no-positions")) positions = 0;
    else if (!strcmp(name, "--no-clusters")) clusters = 0;
    else if (!strcmp(name, "--no-advances")) advances = 0;
    else if (!strcmp(name, "--ned")) { clusters = 0; advances = 0; }
    else if (!strcmp(name, "--show-extents")) extents = 1;
    else if (!strcmp(name, "--show-flags")) flagsout = 1;
    else if (!strcmp(name, "--bot")) bot = 1;
    else if (!strcmp(name, "--eot")) eot = 1;
    else if (!strcmp(name, "--remove-default-ignorables")) remove = 1;
    else if (!strcmp(name, "--preserve-default-ignorables")) preserve = 1;
    else if (!strcmp(name, "--unsafe-to-concat")) unsafe = 1;
    else if (!strcmp(name, "--verify")) verify = 1;
    else if (!strcmp(name, "--single-par")) ;
    else { printf("unknown option %s\n", a); return; }
  }
  hb_blob_t *blob = hb_blob_create_from_file_or_fail(fontfile);
  if (!blob) { printf("no font\n"); return; }
  hb_face_t *face = hb_face_create(blob, face_index);
  hb_font_t *font = hb_font_create(face);
  unsigned upem = hb_face_get_upem(face);
  int sx = size_x < 0 ? (int) upem : (int) scalbn(size_x, 0);
  int sy = size_y < 0 ? (int) upem : (int) scalbn(size_y, 0);
  hb_font_set_ppem(font, 0, 0);
  hb_font_set_ptem(font, ptem);
  hb_font_set_scale(font, sx, sy);
  hb_font_set_variations(font, variations, num_variations);
  if (!strcmp(funcs, "ft")) hb_ft_font_set_funcs(font);
  else hb_ot_font_set_funcs(font);

  hb_buffer_t *b = hb_buffer_create();
  int len;
  char *text = encode_unicodes(unicodes, &len);
  if (before) { int n; char *t = encode_unicodes(before, &n); hb_buffer_add_utf8(b, t, n, n, 0); free(t); }
  hb_buffer_add_utf8(b, text, len, 0, len);
  if (after) { int n; char *t = encode_unicodes(after, &n); hb_buffer_add_utf8(b, t, n, 0, 0); free(t); }
  free(text);
  unsigned n;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos(b, &n);
  for (unsigned i = 0; i < n; i++) info[i].cluster = i;
  hb_buffer_set_direction(b, hb_direction_from_string(direction, -1));
  hb_buffer_set_script(b, hb_script_from_string(script, -1));
  hb_buffer_set_language(b, hb_language_from_string(language, -1));
  hb_buffer_set_flags(b, (hb_buffer_flags_t) (HB_BUFFER_FLAG_DEFAULT | (bot ? HB_BUFFER_FLAG_BOT : 0) | (eot ? HB_BUFFER_FLAG_EOT : 0) |
                                             (verify ? HB_BUFFER_FLAG_VERIFY : 0) | (unsafe ? HB_BUFFER_FLAG_PRODUCE_UNSAFE_TO_CONCAT : 0) |
                                             (preserve ? HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES : 0) |
                                             (remove ? HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES : 0)));
  hb_buffer_set_cluster_level(b, (hb_buffer_cluster_level_t) cluster_level);
  hb_buffer_guess_segment_properties(b);
  const char *shapers[2] = {shaper, NULL};
  if (!hb_shape_full(font, b, features, num_features, shaper ? shapers : NULL)) {
    printf("error: all shapers failed\n");
  } else {
    unsigned flags = 0;
    if (!names) flags |= HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES;
    if (!clusters) flags |= HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS;
    if (!positions) flags |= HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS;
    if (!advances) flags |= HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES;
    if (extents) flags |= HB_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS;
    if (flagsout) flags |= HB_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS;
    unsigned count = hb_buffer_get_length(b), start = 0;
    while (start < count) {
      char buf[32768];
      unsigned consumed;
      start += hb_buffer_serialize(b, start, count, buf, sizeof buf, &consumed, font, HB_BUFFER_SERIALIZE_FORMAT_TEXT, (hb_buffer_serialize_flags_t) flags);
      if (!consumed) break;
      fputs(buf, stdout);
    }
    putchar('\n');
  }
  hb_buffer_destroy(b);
  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
}

int main(void) {
  static char line[1 << 20];
  while (fgets(line, sizeof line, stdin)) {
    line[strcspn(line, "\n")] = 0;
    char *argv[256];
    int argc = 0;
    for (char *p = strtok(line, "\t"); p && argc < 256; p = strtok(NULL, "\t")) argv[argc++] = p;
    if (argc) run(argc, argv);
    else puts("");
    fflush(stdout);
  }
  return 0;
}
