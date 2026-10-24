/* Decodes a JPEG through libjpeg's API and prints what an application
   sees: the header, every call's progress, warnings and errors (as
   format_message makes them), and a hash of the pixels. Built with gcc
   and the system's libjpeg-turbo, and with mlx-cc and mlx-libjpeg, the
   two outputs must be the same.

       decode MODE FILE [PIECE]

   MODE webkit: WebKit's JPEGImageDecoder: a suspending source given the
   file PIECE bytes more at each call (0: all at once), BGRA or CMYK
   output, buffered-image output of a progressive file with WebKit's
   loop of output passes, EXIF and ICC markers kept.
   The other modes read the whole file through a source that fills 4096
   bytes at a time (as Skia's does) and print each row's hash:
   rgba, rgb, bgr, gray, cmyk, ycc (no conversion), plain (no fancy
   upsampling), nosmooth, crop (columns from a third of the width, the
   first quarter of the rows skipped), raw (jpeg_read_raw_data), multi
   (rows asked eight at a time), scale (the dimensions at each scale). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>

struct errors {
  struct jpeg_error_mgr pub;
  jmp_buf jump;
};

static void error_exit(j_common_ptr cinfo) {
  char text[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, text);
  printf("error %d: %s\n", cinfo->err->msg_code, text);
  longjmp(((struct errors *)cinfo->err)->jump, 1);
}

static void emit_message(j_common_ptr cinfo, int level) {
  char text[JMSG_LENGTH_MAX];
  if (level >= 0) return;
  (*cinfo->err->format_message)(cinfo, text);
  printf("warning %d: %s\n", cinfo->err->msg_code, text);
}

static unsigned long long hash(const unsigned char *data, size_t length, unsigned long long h) {
  size_t i;
  for (i = 0; i < length; i++) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

static unsigned char *file;
static size_t fileLength;

/* WebKit's source: everything given so far, nothing more on fill. */
struct webkit_source {
  struct jpeg_source_mgr pub;
  size_t given;
  long skip;
};

static void init_source(j_decompress_ptr cinfo) { (void)cinfo; }
static void term_source(j_decompress_ptr cinfo) { (void)cinfo; printf("term_source\n"); }
static boolean fill_nothing(j_decompress_ptr cinfo) { (void)cinfo; return FALSE; }

/* WebKit's skipBytes: what is there now, the rest when it comes. */
static void skip_bytes(struct webkit_source *src, long count) {
  long now = count < (long)src->pub.bytes_in_buffer ? count : (long)src->pub.bytes_in_buffer;
  src->pub.bytes_in_buffer -= now;
  src->pub.next_input_byte += now;
  src->skip = count - now;
}

static void skip_webkit(j_decompress_ptr cinfo, long count) {
  skip_bytes((struct webkit_source *)cinfo->src, count);
}

/* WebKit's setData: the new bytes after the ones not used yet. */
static void give(struct webkit_source *src, size_t total) {
  size_t used = src->given - src->pub.bytes_in_buffer;
  src->pub.bytes_in_buffer += total - src->given;
  src->pub.next_input_byte = file + used;
  src->given = total;
  if (src->skip) skip_bytes(src, src->skip);
}

static unsigned orientation(j_decompress_ptr info) {
  jpeg_saved_marker_ptr marker;
  for (marker = info->marker_list; marker; marker = marker->next) {
    const JOCTET *d = marker->data;
    unsigned length = marker->data_length, offset, count, i;
    int big;
    if (marker->marker != JPEG_APP0 + 1 || length < 14 || memcmp(d, "Exif\0", 5)) continue;
    if (!((d[6] == 'I' && d[7] == 'I') || (d[6] == 'M' && d[7] == 'M'))) continue;
    big = d[6] == 'M';
#define R16(p) (big ? ((p)[0] << 8 | (p)[1]) : ((p)[1] << 8 | (p)[0]))
#define R32(p) (big ? ((unsigned)(p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3]) : ((unsigned)(p)[3] << 24 | (p)[2] << 16 | (p)[1] << 8 | (p)[0]))
    if (R16(d + 8) != 42) continue;
    offset = R32(d + 10);
    if (offset >= length - 6) continue;
    offset += 6;
    if (length - offset < 2) continue;
    count = R16(d + offset);
    for (i = 0; i < count && length - offset - 2 - i * 12 >= 12; i++) {
      const JOCTET *e = d + offset + 2 + i * 12;
      if (R16(e) == 0x112 && R16(e + 2) == 3 && R32(e + 4) == 1) return R16(e + 8);
    }
  }
  return 1;
}

struct webkit {
  struct jpeg_decompress_struct info;
  struct errors err;
  struct webkit_source src;
  int state;
  unsigned char *pixels;
  JSAMPARRAY samples;
  int calls;
};

static void header(j_decompress_ptr info) {
  int ci;
  printf("header %ux%u components %d space %d progressive %d multiple %d JFIF %d %d.%d density %d %dx%d Adobe %d %d\n",
         info->image_width, info->image_height, info->num_components, info->jpeg_color_space, info->progressive_mode,
         jpeg_has_multiple_scans(info), info->saw_JFIF_marker, info->JFIF_major_version, info->JFIF_minor_version,
         info->density_unit, info->X_density, info->Y_density, info->saw_Adobe_marker, info->Adobe_transform);
  for (ci = 0; ci < info->num_components; ci++) {
    jpeg_component_info *c = &info->comp_info[ci];
    printf("  component %d id %d sampling %dx%d quant %d blocks %ux%u\n", ci, c->component_id, c->h_samp_factor,
           c->v_samp_factor, c->quant_tbl_no, c->width_in_blocks, c->height_in_blocks);
  }
}

/* WebKit's outputScanlines: rows straight into the frame (BGRA) or
   through the sample row (CMYK, made RGB). */
static int output_rows(struct webkit *w) {
  j_decompress_ptr info = &w->info;
  unsigned width = info->output_width;
  while (info->output_scanline < info->output_height) {
    unsigned y = info->output_scanline, x;
    unsigned char *row = w->pixels + (size_t)y * width * 4;
    if (info->out_color_space == JCS_EXT_BGRA) {
      if (jpeg_read_scanlines(info, &row, 1) != 1) return 0;
    } else {
      if (jpeg_read_scanlines(info, w->samples, 1) != 1) return 0;
      for (x = 0; x < width; x++) {
        unsigned char *s = w->samples[0] + x * 4;
        unsigned k = s[3];
        row[x * 4] = s[2] * k / 255;
        row[x * 4 + 1] = s[1] * k / 255;
        row[x * 4 + 2] = s[0] * k / 255;
        row[x * 4 + 3] = 255;
      }
    }
  }
  return 1;
}

static void pass_done(struct webkit *w) {
  size_t size = (size_t)w->info.output_width * w->info.output_height * 4;
  printf("pass scan %d hash %016llx\n", w->info.output_scan_number, hash(w->pixels, size, 14695981039346656037ULL));
}

enum { HEADER, START, PROGRESSIVE, SEQUENTIAL, DONE };

/* JPEGImageReader::decode, one call: false when it must wait for data. */
static int webkit_decode(struct webkit *w) {
  j_decompress_ptr info = &w->info;
  w->calls++;
  if (setjmp(w->err.jump)) return -1;
  switch (w->state) {
  case HEADER: {
    JOCTET *icc = NULL;
    unsigned iccLength = 0;
    if (jpeg_read_header(info, TRUE) == JPEG_SUSPENDED) return 0;
    switch (info->jpeg_color_space) {
    case JCS_GRAYSCALE: case JCS_RGB: case JCS_YCbCr: info->out_color_space = JCS_EXT_BGRA; break;
    case JCS_CMYK: case JCS_YCCK: info->out_color_space = JCS_CMYK; break;
    default: printf("unsupported colour space\n"); return -1;
    }
    w->state = START;
    header(info);
    printf("orientation %u\n", orientation(info));
    if (jpeg_read_icc_profile(info, &icc, &iccLength)) {
      printf("icc %u bytes hash %016llx\n", iccLength, hash(icc, iccLength, 14695981039346656037ULL));
      free(icc);
    }
    info->buffered_image = jpeg_has_multiple_scans(info);
    jpeg_calc_output_dimensions(info);
    w->samples = (*info->mem->alloc_sarray)((j_common_ptr)info, JPOOL_IMAGE, info->output_width * 4, 1);
    w->pixels = calloc((size_t)info->output_width * info->output_height, 4);
  }
  /* fall through */
  case START:
    info->dct_method = JDCT_ISLOW;
    info->dither_mode = JDITHER_FS;
    info->do_fancy_upsampling = TRUE;
    info->enable_2pass_quant = FALSE;
    info->do_block_smoothing = TRUE;
    if (!jpeg_start_decompress(info)) return 0;
    printf("output %ux%u components %d\n", info->output_width, info->output_height, info->output_components);
    w->state = info->buffered_image ? PROGRESSIVE : SEQUENTIAL;
  /* fall through */
  case SEQUENTIAL:
    if (w->state == SEQUENTIAL) {
      if (!output_rows(w)) return 0;
      pass_done(w);
      w->state = DONE;
    }
  /* fall through */
  case PROGRESSIVE:
    if (w->state == PROGRESSIVE) {
      int status;
      do {
        status = jpeg_consume_input(info);
      } while (status != JPEG_SUSPENDED && status != JPEG_REACHED_EOI);
      for (;;) {
        if (!info->output_scanline) {
          int scan = info->input_scan_number;
          if (!info->output_scan_number && scan > 1 && status != JPEG_REACHED_EOI) --scan;
          if (!jpeg_start_output(info, scan)) return 0;
        }
        if (info->output_scanline == 0xffffff) info->output_scanline = 0;
        if (!output_rows(w)) {
          if (!info->output_scanline) info->output_scanline = 0xffffff;
          return 0;
        }
        if (info->output_scanline == info->output_height) {
          if (!jpeg_finish_output(info)) return 0;
          pass_done(w);
          if (jpeg_input_complete(info) && info->input_scan_number == info->output_scan_number) break;
          info->output_scanline = 0;
        }
      }
      w->state = DONE;
    }
  /* fall through */
  case DONE:
    return jpeg_finish_decompress(info) ? 1 : 0;
  }
  return 1;
}

static int run_webkit(size_t piece) {
  struct webkit w;
  size_t total = 0;
  int result = 0;
  memset(&w, 0, sizeof w);
  w.info.err = jpeg_std_error(&w.err.pub);
  w.err.pub.error_exit = error_exit;
  w.err.pub.emit_message = emit_message;
  jpeg_create_decompress(&w.info);
  w.info.src = &w.src.pub;
  w.src.pub.init_source = init_source;
  w.src.pub.fill_input_buffer = fill_nothing;
  w.src.pub.skip_input_data = skip_webkit;
  w.src.pub.resync_to_restart = jpeg_resync_to_restart;
  w.src.pub.term_source = term_source;
  jpeg_save_markers(&w.info, JPEG_APP0 + 1, 0xFFFF);
  jpeg_save_markers(&w.info, JPEG_APP0 + 2, 0xFFFF);
  while (total < fileLength) {
    total = piece ? (total + piece < fileLength ? total + piece : fileLength) : fileLength;
    give(&w.src, total);
    result = webkit_decode(&w);
    printf("call %d: %d scanline %u scans %d/%d\n", w.calls, result, w.info.output_scanline, w.info.input_scan_number, w.info.output_scan_number);
    if (result != 0) break;
  }
  if (result != 1) printf("not complete\n");
  if (w.pixels) {
    size_t size = (size_t)w.info.output_width * w.info.output_height * 4;
    printf("final hash %016llx\n", hash(w.pixels, size, 14695981039346656037ULL));
    free(w.pixels);
  }
  jpeg_destroy_decompress(&w.info);
  return 0;
}

/* Skia's kind of source: 4096 bytes at a time, FALSE at the end. */
struct chunk_source {
  struct jpeg_source_mgr pub;
  size_t at;
};

static boolean fill_chunk(j_decompress_ptr cinfo) {
  struct chunk_source *src = (struct chunk_source *)cinfo->src;
  size_t count = fileLength - src->at < 4096 ? fileLength - src->at : 4096;
  if (!count) return FALSE;
  src->pub.next_input_byte = file + src->at;
  src->pub.bytes_in_buffer = count;
  src->at += count;
  return TRUE;
}

static void skip_chunk(j_decompress_ptr cinfo, long count) {
  struct chunk_source *src = (struct chunk_source *)cinfo->src;
  if (count <= 0) return;
  if ((size_t)count <= src->pub.bytes_in_buffer) {
    src->pub.next_input_byte += count;
    src->pub.bytes_in_buffer -= count;
  } else {
    size_t rest = count - src->pub.bytes_in_buffer;
    src->pub.bytes_in_buffer = 0;
    src->at = src->at + rest < fileLength ? src->at + rest : fileLength;
  }
}

static int run_other(const char *mode) {
  struct jpeg_decompress_struct info;
  struct errors err;
  struct chunk_source src;
  unsigned char *row = NULL;
  unsigned rows = 0;
  unsigned long long all = 14695981039346656037ULL;
  memset(&src, 0, sizeof src);
  info.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  err.pub.emit_message = emit_message;
  if (setjmp(err.jump)) {
    free(row);
    jpeg_destroy_decompress(&info);
    return 0;
  }
  jpeg_create_decompress(&info);
  info.src = &src.pub;
  src.pub.init_source = init_source;
  src.pub.fill_input_buffer = fill_chunk;
  src.pub.skip_input_data = skip_chunk;
  src.pub.resync_to_restart = jpeg_resync_to_restart;
  src.pub.term_source = term_source;
  if (jpeg_read_header(&info, TRUE) != JPEG_HEADER_OK) { printf("no header\n"); jpeg_destroy_decompress(&info); return 0; }
  header(&info);
  if (!strcmp(mode, "scale")) {
    unsigned n;
    for (n = 1; n <= 16; n++) {
      info.scale_num = n;
      info.scale_denom = 8;
      jpeg_calc_output_dimensions(&info);
      printf("scale %u/8: %ux%u\n", n, info.output_width, info.output_height);
    }
    jpeg_destroy_decompress(&info);
    return 0;
  }
  if (!strcmp(mode, "rgba")) info.out_color_space = JCS_EXT_RGBA;
  else if (!strcmp(mode, "rgb")) info.out_color_space = JCS_RGB;
  else if (!strcmp(mode, "bgr")) info.out_color_space = JCS_EXT_BGR;
  else if (!strcmp(mode, "gray")) info.out_color_space = JCS_GRAYSCALE;
  else if (!strcmp(mode, "cmyk")) info.out_color_space = JCS_CMYK;
  else if (!strcmp(mode, "ycc")) info.out_color_space = info.jpeg_color_space;
  else if (!strcmp(mode, "plain")) { info.out_color_space = JCS_EXT_XRGB; info.do_fancy_upsampling = FALSE; }
  else if (!strcmp(mode, "nosmooth")) { info.out_color_space = JCS_EXT_ABGR; info.do_block_smoothing = FALSE; }
  else if (!strcmp(mode, "raw")) info.raw_data_out = TRUE;
  else info.out_color_space = JCS_EXT_BGRX;
  jpeg_start_decompress(&info);
  printf("output %ux%u components %d rec %d\n", info.output_width, info.output_height, info.output_components, info.rec_outbuf_height);
  if (!strcmp(mode, "raw")) {
    int ci;
    JSAMPARRAY planes[10];
    unsigned lines = info.max_v_samp_factor * DCTSIZE;
    unsigned char *memory;
    size_t widths[10], total = 0;
    for (ci = 0; ci < info.num_components; ci++) { widths[ci] = info.comp_info[ci].width_in_blocks * DCTSIZE; total += widths[ci] * info.comp_info[ci].v_samp_factor * DCTSIZE; }
    memory = calloc(total, 1);
    total = 0;
    for (ci = 0; ci < info.num_components; ci++) {
      int r, n = info.comp_info[ci].v_samp_factor * DCTSIZE;
      planes[ci] = malloc(sizeof(JSAMPROW) * n);
      for (r = 0; r < n; r++) { planes[ci][r] = memory + total; total += widths[ci]; }
    }
    while (info.output_scanline < info.output_height) {
      unsigned got = jpeg_read_raw_data(&info, planes, lines);
      if (!got) { printf("raw suspended\n"); break; }
      all = hash(memory, total, all);
      memset(memory, 0, total);
      rows += got;
    }
    for (ci = 0; ci < info.num_components; ci++) free(planes[ci]);
    free(memory);
  } else {
    unsigned stride = info.output_width * info.output_components, x = 0, width = info.output_width;
    if (!strcmp(mode, "crop")) {
      x = info.output_width / 3;
      width = info.output_width - x;
      jpeg_crop_scanline(&info, &x, &width);
      printf("crop %u %u output_width %u\n", x, width, info.output_width);
      stride = info.output_width * info.output_components;
      printf("skipped %u\n", jpeg_skip_scanlines(&info, info.output_height / 4));
    }
    row = malloc((size_t)stride * 8 + 16);
    while (info.output_scanline < info.output_height) {
      JSAMPROW pointers[8];
      unsigned got, i, want = !strcmp(mode, "multi") ? 8 : 1;
      for (i = 0; i < 8; i++) pointers[i] = row + (size_t)i * stride;
      got = jpeg_read_scanlines(&info, pointers, want);
      if (!got) { printf("suspended\n"); break; }
      all = hash(row, (size_t)stride * got, all);
      rows += got;
    }
  }
  printf("rows %u hash %016llx\n", rows, all);
  jpeg_finish_decompress(&info);
  free(row);
  jpeg_destroy_decompress(&info);
  return 0;
}

int main(int argc, char **argv) {
  FILE *f;
  long size;
  if (argc < 3) return 2;
  f = fopen(argv[2], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  file = malloc(size + 1);
  fileLength = fread(file, 1, size, f);
  fclose(f);
  if (!strcmp(argv[1], "webkit")) run_webkit(argc > 3 ? (size_t)atol(argv[3]) : 0);
  else run_other(argv[1]);
  free(file);
  return 0;
}
