// The WebP corpus of tools/check_webp.sh, made by the system's libwebp
// encoder and muxer: a raw RGBA picture (sources.py's) encoded with many
// settings, so the decoders meet every feature: lossy with each filter,
// segment and partition setting, alpha raw and lossless with each filter
// and quantized, lossless at each effort and quality, animations, and
// files with ICCP, EXIF and XMP chunks.
//
//   corpus in.rgba width height outprefix [opaque]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webp/encode.h>
#include <webp/mux.h>

static int width, height;
static uint8_t* rgba;

static void writeOut(const char* prefix, const char* name, const uint8_t* data, size_t size) {
  char path[512];
  snprintf(path, sizeof path, "%s-%s.webp", prefix, name);
  FILE* f = fopen(path, "wb");
  fwrite(data, 1, size, f);
  fclose(f);
}

static void encode(const char* prefix, const char* name, WebPConfig* config) {
  WebPPicture pic;
  WebPMemoryWriter writer;
  WebPPictureInit(&pic);
  pic.width = width;
  pic.height = height;
  pic.use_argb = config->lossless ? 1 : 0;
  if (!WebPPictureImportRGBA(&pic, rgba, width * 4)) { fprintf(stderr, "import\n"); exit(1); }
  WebPMemoryWriterInit(&writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &writer;
  if (!WebPValidateConfig(config)) { fprintf(stderr, "config %s\n", name); exit(1); }
  if (!WebPEncode(config, &pic)) { fprintf(stderr, "encode %s: %d\n", name, pic.error_code); exit(1); }
  writeOut(prefix, name, writer.mem, writer.size);
  WebPMemoryWriterClear(&writer);
  WebPPictureFree(&pic);
}

// The file PREFIX-NAME.webp again as PREFIX-NAME-meta.webp, with
// metadata chunks (a VP8X header, chunks before and after the image).
static void meta(const char* prefix, const char* name) {
  char path[512];
  snprintf(path, sizeof path, "%s-%s.webp", prefix, name);
  FILE* f = fopen(path, "rb");
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t* d = malloc(n);
  if (fread(d, 1, n, f) != (size_t)n) exit(1);
  fclose(f);
  WebPData in = { d, (size_t)n };
  WebPMux* mux = WebPMuxCreate(&in, 1);
  static const uint8_t icc[33] = "fake icc profile, 33 bytes long!";
  static const uint8_t exif[18] = "Exif\0\0MM\0*\0\0\0\b\0\0\0";
  static const uint8_t xmp[11] = "<x:xmpmeta>";
  WebPData c1 = { icc, sizeof icc }, c2 = { exif, sizeof exif }, c3 = { xmp, sizeof xmp };
  WebPData out;
  if (!mux || WebPMuxSetChunk(mux, "ICCP", &c1, 1) != WEBP_MUX_OK || WebPMuxSetChunk(mux, "EXIF", &c2, 1) != WEBP_MUX_OK ||
      WebPMuxSetChunk(mux, "XMP ", &c3, 1) != WEBP_MUX_OK || WebPMuxAssemble(mux, &out) != WEBP_MUX_OK) {
    fprintf(stderr, "meta %s\n", name);
    exit(1);
  }
  char metaName[64];
  snprintf(metaName, sizeof metaName, "%s-meta", name);
  writeOut(prefix, metaName, out.bytes, out.size);
  WebPDataClear(&out);
  WebPMuxDelete(mux);
  free(d);
}

static void lossy(WebPConfig* c, float quality) {
  WebPConfigInit(c);
  c->quality = quality;
}

static void animation(const char* prefix, const char* name, int lossless, int mixed, int minimize) {
  WebPAnimEncoderOptions options;
  WebPAnimEncoderOptionsInit(&options);
  options.allow_mixed = mixed;
  options.minimize_size = minimize;
  options.anim_params.loop_count = 3;
  options.anim_params.bgcolor = 0x80ff8040;
  WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, &options);
  WebPConfig config;
  WebPConfigInit(&config);
  config.lossless = lossless;
  config.quality = 60;
  uint8_t* frame = malloc((size_t)width * height * 4);
  int timestamp = 0;
  for (int k = 0; k < 4; ++k) {
    // Each frame: the picture with a moving square changed (so frames
    // differ in a sub-rectangle) and, in the third, transparency.
    memcpy(frame, rgba, (size_t)width * height * 4);
    int x0 = (k * width) / 5, y0 = (k * height) / 6;
    for (int y = y0; y < y0 + height / 3 && y < height; ++y) {
      for (int x = x0; x < x0 + width / 3 && x < width; ++x) {
        uint8_t* p = frame + ((size_t)y * width + x) * 4;
        p[0] = (uint8_t)(40 * k + x); p[1] = (uint8_t)(255 - 30 * k); p[2] = (uint8_t)y;
        if (k == 2) p[3] = (uint8_t)(x * 4);
      }
    }
    WebPPicture pic;
    WebPPictureInit(&pic);
    pic.width = width;
    pic.height = height;
    pic.use_argb = 1;
    WebPPictureImportRGBA(&pic, frame, width * 4);
    if (!WebPAnimEncoderAdd(enc, &pic, timestamp, &config)) { fprintf(stderr, "anim add\n"); exit(1); }
    WebPPictureFree(&pic);
    timestamp += 70 + 10 * k;
  }
  WebPAnimEncoderAdd(enc, NULL, timestamp, NULL);
  WebPData data;
  WebPDataInit(&data);
  if (!WebPAnimEncoderAssemble(enc, &data)) { fprintf(stderr, "anim assemble\n"); exit(1); }
  writeOut(prefix, name, data.bytes, data.size);
  WebPDataClear(&data);
  WebPAnimEncoderDelete(enc);
  free(frame);
}

int main(int argc, char** argv) {
  if (argc < 5) return 2;
  FILE* f = fopen(argv[1], "rb");
  width = atoi(argv[2]);
  height = atoi(argv[3]);
  rgba = malloc((size_t)width * height * 4);
  if (fread(rgba, 1, (size_t)width * height * 4, f) != (size_t)width * height * 4) return 3;
  fclose(f);
  const char* p = argv[4];
  int all = argc < 6;
  WebPConfig c;
  lossy(&c, 75); encode(p, "q75", &c);
  lossy(&c, 5); encode(p, "q5", &c);
  lossy(&c, 100); encode(p, "q100", &c);
  lossy(&c, 60); c.filter_strength = 0; encode(p, "nofilter", &c);
  lossy(&c, 60); c.filter_type = 0; c.filter_strength = 60; encode(p, "simple", &c);
  lossy(&c, 40); c.filter_sharpness = 7; c.filter_strength = 100; encode(p, "sharp7", &c);
  lossy(&c, 70); c.segments = 1; c.partitions = 3; encode(p, "seg1part8", &c);
  lossy(&c, 50); c.partitions = 2; c.method = 6; c.sns_strength = 100; encode(p, "part4m6", &c);
  lossy(&c, 80); c.method = 0; encode(p, "m0", &c);
  lossy(&c, 30); c.use_sharp_yuv = 1; c.autofilter = 1; encode(p, "sharpyuv", &c);
  if (all) {
    lossy(&c, 75); c.alpha_compression = 0; encode(p, "alpharaw", &c);
    lossy(&c, 75); c.alpha_filtering = 0; encode(p, "alphanf", &c);
    lossy(&c, 75); c.alpha_filtering = 1; encode(p, "alphaf1", &c);
    lossy(&c, 75); c.alpha_filtering = 2; c.alpha_quality = 60; encode(p, "alphaq60", &c);
    lossy(&c, 75); c.alpha_quality = 0; encode(p, "alphaq0", &c);
    lossy(&c, 75); c.exact = 1; c.alpha_compression = 0; c.alpha_filtering = 2; encode(p, "alpharawf", &c);
  }
  for (int m = 0; m <= 6; m += 3) {
    char name[32];
    WebPConfigInit(&c); c.lossless = 1; c.method = m; c.quality = m == 0 ? 0 : 75;
    snprintf(name, sizeof name, "ll-m%d", m); encode(p, name, &c);
  }
  WebPConfigInit(&c); c.lossless = 1; c.method = 4; c.quality = 100; c.exact = 1; encode(p, "ll-exact", &c);
  WebPConfigInit(&c); c.lossless = 1; c.near_lossless = 40; encode(p, "ll-near", &c);
  WebPConfigLosslessPreset(&c, 9); encode(p, "ll-z9", &c);
  WebPConfigLosslessPreset(&c, 0); encode(p, "ll-z0", &c);
  if (all) {
    animation(p, "anim-lossy", 0, 0, 0);
    animation(p, "anim-lossless", 1, 0, 1);
    animation(p, "anim-mixed", 0, 1, 1);
    meta(p, "alphaf1");
    meta(p, "anim-mixed");
  }
  meta(p, "ll-m3");
  return 0;
}
