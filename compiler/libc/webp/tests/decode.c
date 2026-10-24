/* Drives libwebp's decoding API over a file and prints what it gives
   (sizes, statuses, rows done, a hash of every pixel), so that the
   system's libwebp and mlx-libwebp can be compared:

       decode MODE file.webp

   MODE: simple (WebPGetInfo, WebPGetFeatures, WebPDecodeRGBA and kin),
   into (into the caller's memory), config (WebPDecode in every colour
   space, with and without fancy upsampling, cropped, flipped, without
   the loop filter, into slow memory), idec (the incremental decoders
   with data cut short, appended in pieces, remapped, into RGB and YUVA),
   demux (the demuxer on the file and on parts of it: features, frames,
   chunks), anim (the animation decoder in each colour mode), all. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webp/decode.h>
#include <webp/demux.h>

static uint8_t* data;
static size_t size;

static unsigned long long hashBytes(unsigned long long h, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ULL;
  return h;
}

static unsigned long long hashRows(const uint8_t* p, int stride, int rowBytes, int rows) {
  unsigned long long h = 14695981039346656037ULL;
  if (p == NULL) return 0;
  for (int y = 0; y < rows; ++y) h = hashBytes(h, p + (long)y * stride, rowBytes);
  return h;
}

static int bpp(WEBP_CSP_MODE m) {
  static const int k[] = { 3, 4, 3, 4, 4, 2, 2, 4, 4, 4, 2, 1, 1 };
  return k[m];
}

static void simple(void) {
  int w = -1, h = -1;
  int ok = WebPGetInfo(data, size, &w, &h);
  printf("info %d %d %d\n", ok, w, h);
  WebPBitstreamFeatures f;
  memset(&f, 0x55, sizeof f);
  VP8StatusCode s = WebPGetFeatures(data, size, &f);
  printf("features %d", s);
  if (s == VP8_STATUS_OK) printf(" %d %d %d %d %d", f.width, f.height, f.has_alpha, f.has_animation, f.format);
  printf("\n");
  for (int k = 0; k < 6; ++k) {
    static const char* names[] = { "RGBA", "ARGB", "BGRA", "RGB", "BGR", "YUV" };
    w = -1; h = -1;
    uint8_t* out = NULL;
    uint8_t *u = NULL, *v = NULL;
    int stride = 0, uvStride = 0;
    switch (k) {
      case 0: out = WebPDecodeRGBA(data, size, &w, &h); break;
      case 1: out = WebPDecodeARGB(data, size, &w, &h); break;
      case 2: out = WebPDecodeBGRA(data, size, &w, &h); break;
      case 3: out = WebPDecodeRGB(data, size, &w, &h); break;
      case 4: out = WebPDecodeBGR(data, size, &w, &h); break;
      case 5: out = WebPDecodeYUV(data, size, &w, &h, &u, &v, &stride, &uvStride); break;
    }
    printf("decode %s %d %d %d", names[k], out != NULL, w, h);
    if (out != NULL) {
      if (k < 5) {
        printf(" %llu", hashRows(out, w * (k < 3 ? 4 : 3), w * (k < 3 ? 4 : 3), h));
      } else {
        printf(" %d %d %llu %llu %llu %d %d", stride, uvStride, hashRows(out, stride, w, h), hashRows(u, uvStride, (w + 1) / 2, (h + 1) / 2),
               hashRows(v, uvStride, (w + 1) / 2, (h + 1) / 2), (int)(u - out), (int)(v - out));
      }
      WebPFree(out);
    }
    printf("\n");
  }
}

static void into(void) {
  int w = 0, h = 0;
  if (!WebPGetInfo(data, size, &w, &h)) { printf("into: no info\n"); return; }
  int stride = w * 4 + 12;
  size_t bytes = (size_t)stride * h;
  uint8_t* buf = malloc(bytes + 64);
  for (int k = 0; k < 5; ++k) {
    memset(buf, 0xa5, bytes + 64);
    uint8_t* r = NULL;
    switch (k) {
      case 0: r = WebPDecodeRGBAInto(data, size, buf, bytes, stride); break;
      case 1: r = WebPDecodeARGBInto(data, size, buf, bytes, stride); break;
      case 2: r = WebPDecodeBGRAInto(data, size, buf, bytes, stride); break;
      case 3: r = WebPDecodeRGBInto(data, size, buf, bytes, stride); break;
      case 4: r = WebPDecodeBGRInto(data, size, buf, bytes - 1 - stride, stride); break;
    }
    printf("into %d %d %llu\n", k, r == buf, hashRows(buf, stride, stride, h));
  }
  uint8_t* y = malloc((size_t)w * h + 16);
  int uvw = (w + 1) / 2, uvh = (h + 1) / 2;
  uint8_t* u = malloc((size_t)uvw * uvh + 16);
  uint8_t* v = malloc((size_t)uvw * uvh + 16);
  uint8_t* r = WebPDecodeYUVInto(data, size, y, (size_t)w * h, w, u, (size_t)uvw * uvh, uvw, v, (size_t)uvw * uvh, uvw);
  printf("into yuv %d", r == y);
  if (r) printf(" %llu %llu %llu", hashRows(y, w, w, h), hashRows(u, uvw, uvw, uvh), hashRows(v, uvw, uvw, uvh));
  printf("\n");
  free(y); free(u); free(v); free(buf);
}

static void printBuffer(const char* label, VP8StatusCode s, const WebPDecBuffer* o) {
  printf("%s %d", label, s);
  if (s == VP8_STATUS_OK) {
    printf(" %d %d %d", o->colorspace, o->width, o->height);
    if (WebPIsRGBMode(o->colorspace)) {
      const int rowBytes = o->width * bpp(o->colorspace);
      /* (A flipped buffer starts at its last row with a negative stride.) */
      printf(" %d %llu", o->u.RGBA.stride, hashRows(o->u.RGBA.rgba, o->u.RGBA.stride, rowBytes, o->height));
    } else {
      const WebPYUVABuffer* b = &o->u.YUVA;
      printf(" %d %d %llu %llu %llu", b->y_stride, b->u_stride, hashRows(b->y, b->y_stride, o->width, o->height),
             hashRows(b->u, b->u_stride, (o->width + 1) / 2, (o->height + 1) / 2), hashRows(b->v, b->v_stride, (o->width + 1) / 2, (o->height + 1) / 2));
      if (b->a) printf(" %llu", hashRows(b->a, b->a_stride, o->width, o->height));
    }
  }
  printf("\n");
}

static void config(void) {
  for (int mode = 0; mode < MODE_LAST; ++mode) {
    for (int variant = 0; variant < 6; ++variant) {
      WebPDecoderConfig c;
      if (!WebPInitDecoderConfig(&c)) { printf("init failed\n"); return; }
      VP8StatusCode s = WebPGetFeatures(data, size, &c.input);
      if (s != VP8_STATUS_OK) { printf("config features %d\n", s); return; }
      c.output.colorspace = mode;
      uint8_t* external = NULL;
      switch (variant) {
        case 0: break;
        case 1: c.options.no_fancy_upsampling = 1; break;
        case 2: c.options.use_cropping = 1; c.options.crop_left = c.input.width / 3 | 1; c.options.crop_top = c.input.height / 4 | 1;
                c.options.crop_width = c.input.width - c.options.crop_left - (c.input.width > 4); c.options.crop_height = c.input.height - c.options.crop_top - (c.input.height > 6);
                if (c.options.crop_width <= 0) c.options.crop_width = 1;
                if (c.options.crop_height <= 0) c.options.crop_height = 1;
                break;
        case 3: c.options.flip = 1; break;
        case 4: c.options.bypass_filtering = 1; c.options.no_fancy_upsampling = mode & 1; break;
        case 5:
          if (!WebPIsRGBMode(mode)) continue;
          c.output.is_external_memory = 2;
          c.output.u.RGBA.stride = c.input.width * bpp(mode) + 4;
          c.output.u.RGBA.size = (size_t)c.output.u.RGBA.stride * c.input.height;
          external = calloc(1, c.output.u.RGBA.size);
          c.output.u.RGBA.rgba = external;
          break;
      }
      s = WebPDecode(data, size, &c);
      char label[64];
      snprintf(label, sizeof label, "config %d %d", mode, variant);
      printBuffer(label, s, &c.output);
      WebPFreeDecBuffer(&c.output);
      free(external);
    }
  }
}

static void idecSteps(const char* label, int mode, int append, size_t step, int crop) {
  WebPDecoderConfig c;
  WebPInitDecoderConfig(&c);
  c.output.colorspace = mode;
  if (crop) {
    WebPGetFeatures(data, size, &c.input);
    c.options.use_cropping = 1;
    c.options.crop_left = 1;
    c.options.crop_top = c.input.height > 2 ? 2 : 0;
    c.options.crop_width = c.input.width > 1 ? c.input.width - 1 : 1;
    c.options.crop_height = c.input.height - c.options.crop_top;
  }
  WebPIDecoder* idec = crop ? WebPIDecode(NULL, 0, &c) : WebPINewDecoder(&c.output);
  if (!idec) { printf("%s new failed\n", label); return; }
  size_t done = 0;
  uint8_t* copy = malloc(size);
  while (done < size) {
    size_t n = step;
    if (done + n > size) n = size - done;
    VP8StatusCode s;
    if (append) {
      s = WebPIAppend(idec, data + done, n);
    } else {
      memcpy(copy + done, data + done, n);
      s = WebPIUpdate(idec, copy, done + n);
    }
    done += n;
    int lastY = -1, w = -1, h = -1, stride = -1, left = -1, top = -1, aw = -1, ah = -1;
    const WebPDecBuffer* area = WebPIDecodedArea(idec, &left, &top, &aw, &ah);
    printf("%s %zu %d area %d %d %d %d %d", label, done, s, area != NULL, left, top, aw, ah);
    if (WebPIsRGBMode(mode)) {
      uint8_t* rgb = WebPIDecGetRGB(idec, &lastY, &w, &h, &stride);
      printf(" rgb %d", rgb != NULL);
      if (rgb) printf(" %d %d %d %d %llu", lastY, w, h, stride, hashRows(rgb, stride, w * bpp(mode), lastY));
    } else {
      uint8_t *u = NULL, *v = NULL, *a = NULL;
      int uvStride = -1, aStride = -1;
      uint8_t* y = WebPIDecGetYUVA(idec, &lastY, &u, &v, &a, &w, &h, &stride, &uvStride, &aStride);
      printf(" yuva %d", y != NULL);
      if (y) printf(" %d %d %d %d %d %d %llu %llu %llu", lastY, w, h, stride, uvStride, a != NULL, hashRows(y, stride, w, lastY), hashRows(u, uvStride, (w + 1) / 2, lastY / 2), hashRows(a, aStride, w, lastY));
    }
    printf("\n");
    if (s != VP8_STATUS_SUSPENDED) break;
  }
  WebPIDelete(idec);
  WebPFreeDecBuffer(&c.output);
  free(copy);
}

static void idec(void) {
  size_t step = size / 7 + 1;
  idecSteps("idec-update-rgba", MODE_RGBA, 0, step, 0);
  idecSteps("idec-append-bgrA", MODE_bgrA, 1, size / 11 + 1, 0);
  idecSteps("idec-append-rgb565", MODE_RGB_565, 1, size / 5 + 1, 0);
  idecSteps("idec-append-yuva", MODE_YUVA, 1, step, 0);
  idecSteps("idec-crop-argb", MODE_ARGB, 1, step, 1);
  idecSteps("idec-small-rgba", MODE_RGBA, 1, 3, 0);
  /* WebPINewRGB into the caller's memory, and with data checked first. */
  int w = 0, h = 0;
  if (WebPGetInfo(data, size, &w, &h)) {
    int stride = w * 3 + 5;
    uint8_t* mem = calloc(1, (size_t)stride * h);
    WebPIDecoder* d = WebPINewRGB(MODE_BGR, mem, (size_t)stride * h, stride);
    VP8StatusCode s = WebPIAppend(d, data, size);
    printf("inewrgb %d %llu\n", s, hashRows(mem, stride, w * 3, h));
    WebPIDelete(d);
    free(mem);
  }
  WebPDecoderConfig c;
  WebPInitDecoderConfig(&c);
  c.output.colorspace = MODE_rgbA;
  WebPIDecoder* d = WebPIDecode(data, size, &c);
  printf("idecode %d", d != NULL);
  if (d) {
    VP8StatusCode s = WebPIUpdate(d, data, size);
    printf(" %d", s);
    printBuffer("", s, &c.output);
    WebPIDelete(d);
    WebPFreeDecBuffer(&c.output);
  } else {
    printf("\n");
  }
}

static void demuxAt(size_t n) {
  if (n > size) return;
  WebPData wd = { data, n };
  WebPDemuxState state = (WebPDemuxState)7;
  WebPDemuxer* dm = WebPDemuxPartial(&wd, &state);
  printf("demux %zu %d %d", n, dm != NULL, state);
  if (!dm) { printf("\n"); return; }
  printf(" flags %u canvas %u %u loop %u bg %08x frames %u\n", WebPDemuxGetI(dm, WEBP_FF_FORMAT_FLAGS), WebPDemuxGetI(dm, WEBP_FF_CANVAS_WIDTH),
         WebPDemuxGetI(dm, WEBP_FF_CANVAS_HEIGHT), WebPDemuxGetI(dm, WEBP_FF_LOOP_COUNT), WebPDemuxGetI(dm, WEBP_FF_BACKGROUND_COLOR), WebPDemuxGetI(dm, WEBP_FF_FRAME_COUNT));
  WebPIterator it;
  if (WebPDemuxGetFrame(dm, 0, &it)) {
    printf("  last %d of %d\n", it.frame_num, it.num_frames);
    WebPDemuxReleaseIterator(&it);
  }
  if (WebPDemuxGetFrame(dm, 1, &it)) {
    do {
      printf("  frame %d %d %d %d %d %d %d %d %d %d %ld %zu\n", it.frame_num, it.num_frames, it.x_offset, it.y_offset, it.width, it.height, it.duration,
             it.dispose_method, it.blend_method, it.has_alpha * 10 + it.complete, (long)(it.fragment.bytes - data), it.fragment.size);
    } while (WebPDemuxNextFrame(&it));
    printf("  prev %d", WebPDemuxPrevFrame(&it));
    printf(" %d\n", it.frame_num);
    WebPDemuxReleaseIterator(&it);
  }
  static const char* tags[] = { "ICCP", "EXIF", "XMP ", "ANIM", "VP8 " };
  for (int t = 0; t < 5; ++t) {
    WebPChunkIterator ci;
    if (WebPDemuxGetChunk(dm, tags[t], 1, &ci)) {
      do {
        printf("  chunk %s %d of %d %ld %zu\n", tags[t], ci.chunk_num, ci.num_chunks, (long)(ci.chunk.bytes - data), ci.chunk.size);
      } while (WebPDemuxNextChunk(&ci));
      WebPDemuxReleaseChunkIterator(&ci);
    }
  }
  WebPDemuxDelete(dm);
}

static void demux(void) {
  demuxAt(size);
  for (int k = 1; k < 8; ++k) demuxAt(size * k / 8);
  demuxAt(12);
  demuxAt(20);
  demuxAt(30);
  WebPData wd = { data, size };
  WebPDemuxer* dm = WebPDemux(&wd);
  printf("demux whole %d\n", dm != NULL);
  WebPDemuxDelete(dm);
  wd.size = size - 1;
  dm = WebPDemux(&wd);
  printf("demux cut %d\n", dm != NULL);
  WebPDemuxDelete(dm);
}

static void anim(void) {
  static const WEBP_CSP_MODE modes[] = { MODE_RGBA, MODE_BGRA, MODE_rgbA, MODE_bgrA, MODE_RGB };
  for (int m = 0; m < 5; ++m) {
    WebPAnimDecoderOptions o;
    WebPAnimDecoderOptionsInit(&o);
    o.color_mode = modes[m];
    WebPData wd = { data, size };
    WebPAnimDecoder* dec = WebPAnimDecoderNew(&wd, &o);
    printf("anim %d %d", modes[m], dec != NULL);
    if (!dec) { printf("\n"); continue; }
    WebPAnimInfo info;
    WebPAnimDecoderGetInfo(dec, &info);
    printf(" %u %u %u %08x %u\n", info.canvas_width, info.canvas_height, info.loop_count, info.bgcolor, info.frame_count);
    for (int round = 0; round < 2; ++round) {
      while (WebPAnimDecoderHasMoreFrames(dec)) {
        uint8_t* buf = NULL;
        int timestamp = -1;
        int ok = WebPAnimDecoderGetNext(dec, &buf, &timestamp);
        printf("  next %d %d %llu\n", ok, timestamp, ok ? hashRows(buf, info.canvas_width * 4, info.canvas_width * 4, info.canvas_height) : 0);
        if (!ok) break;
      }
      WebPAnimDecoderReset(dec);
    }
    printf("  demuxer %d\n", WebPAnimDecoderGetDemuxer(dec) != NULL);
    WebPAnimDecoderDelete(dec);
  }
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  FILE* f = fopen(argv[2], "rb");
  if (!f) return 3;
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  data = malloc(size + 1);
  if (fread(data, 1, size, f) != size) return 4;
  fclose(f);
  const char* m = argv[1];
  int all = !strcmp(m, "all");
  if (all || !strcmp(m, "simple")) simple();
  if (all || !strcmp(m, "into")) into();
  if (all || !strcmp(m, "config")) config();
  if (all || !strcmp(m, "idec")) idec();
  if (all || !strcmp(m, "demux")) demux();
  if (all || !strcmp(m, "anim")) anim();
  printf("version %x %x\n", WebPGetDecoderVersion(), WebPGetDemuxVersion());
  free(data);
  return 0;
}
