#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Shapes the lines of text files with a font: shape FONT[:INDEX] [--ppem=N]
 * [--lines=N] FILE...  A line may start with "features;" (comma-separated,
 * hb_feature_from_string), "dir=ltr|rtl|ttb;", "script=Xxxx;" and
 * "lang=xx;". Prints each line and its glyphs serialized with their flags;
 * --lines shapes only the first N lines of each file. */
int main(int argc, char **argv) {
  char path[512];
  strncpy(path, argv[1], sizeof path - 1);
  path[sizeof path - 1] = 0;
  unsigned index = 0;
  char *colon = strchr(path, ':');
  if (colon) { *colon = 0; index = atoi(colon + 1); }
  hb_blob_t *blob = hb_blob_create_from_file(path);
  hb_face_t *face = hb_face_create(blob, index);
  hb_font_t *font = hb_font_create(face);
  unsigned max_lines = 1u << 30;
  for (int a = 2; a < argc; a++) {
    if (!strncmp(argv[a], "--ppem=", 7)) { int ppem = atoi(argv[a] + 7); hb_font_set_ppem(font, ppem, ppem); continue; }
    if (!strncmp(argv[a], "--lines=", 8)) { max_lines = atoi(argv[a] + 8); continue; }
    FILE *in = fopen(argv[a], "r");
    if (!in) { printf("cannot open %s\n", argv[a]); continue; }
    char line[4096];
    unsigned lines = 0;
    while (lines < max_lines && fgets(line, sizeof line, in)) {
      size_t n = strlen(line);
      while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
      if (!n || line[0] == '#') continue;
      lines++;
      char *text = line;
      hb_feature_t features[16];
      unsigned nf = 0;
      hb_direction_t dir = HB_DIRECTION_INVALID;
      hb_script_t script = HB_SCRIPT_INVALID;
      const char *lang = NULL;
      while (1) {
        char *semi = strchr(text, ';');
        if (!semi || semi == text) break;
        *semi = 0;
        if (!strncmp(text, "dir=", 4)) dir = hb_direction_from_string(text + 4, -1);
        else if (!strncmp(text, "script=", 7)) script = hb_script_from_string(text + 7, -1);
        else if (!strncmp(text, "lang=", 5)) lang = text + 5;
        else {
          char *f = text;
          while (f && *f && nf < 16) {
            char *comma = strchr(f, ',');
            if (comma) *comma = 0;
            if (hb_feature_from_string(f, -1, &features[nf])) nf++;
            f = comma ? comma + 1 : NULL;
          }
        }
        text = semi + 1;
      }
      hb_buffer_t *buf = hb_buffer_create();
      hb_buffer_add_utf8(buf, text, -1, 0, -1);
      if (dir) hb_buffer_set_direction(buf, dir);
      if (script) hb_buffer_set_script(buf, script);
      if (lang) hb_buffer_set_language(buf, hb_language_from_string(lang, -1));
      hb_buffer_guess_segment_properties(buf);
      hb_shape(font, buf, features, nf);
      unsigned len = hb_buffer_get_length(buf), start = 0;
      static char out[65536];
      printf("%s => ", text);
      while (start < len) {
        unsigned consumed = 0;
        start += hb_buffer_serialize(buf, start, len, out, sizeof out, &consumed, font, HB_BUFFER_SERIALIZE_FORMAT_TEXT, HB_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS);
        fputs(out, stdout);
        if (!consumed) break;
      }
      printf("\n");
      hb_buffer_destroy(buf);
    }
    fclose(in);
  }
  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
  return 0;
}
