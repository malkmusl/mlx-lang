/* Drives libwebp's encoding and muxing APIs as Skia's WebP encoder does
   (and the rest of them) and prints what does not depend on whose
   encoder made the bytes: configurations, statuses, whether each file
   decodes to the exact pixels given, what the demuxer and the muxer find
   in the files. Built against the system's libwebp and against
   mlx-libwebp, it prints the same (lossless with `exact`, where the
   system's encoder keeps every pixel too).

       encode [WIDTH HEIGHT] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>

static int width = 37, height = 23;

static void picture(uint8_t* rgba, int k) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint8_t* p = rgba + 4 * (y * width + x);
      p[0] = (uint8_t)(x * 7 + k * 30);
      p[1] = (uint8_t)(y * 11 + ((x / 5) & 1) * 60);
      p[2] = (uint8_t)((x + y) * 3 + k);
      p[3] = (uint8_t)(x < 4 ? 255 : (y * 13 + x) & 0xff);
      if ((x / 6 + y / 4 + k) % 3 == 0) memcpy(p, "\x20\x40\x60\xff", 4);
    }
  }
}

/* Whether DATA decodes (RGBA) to exactly RGBA. */
static int same(const uint8_t* data, size_t size, const uint8_t* rgba) {
  int w = 0, h = 0;
  uint8_t* out = WebPDecodeRGBA(data, size, &w, &h);
  int ok = out != NULL && w == width && h == height && !memcmp(out, rgba, (size_t)w * h * 4);
  WebPFree(out);
  return ok;
}

static void printConfig(const char* label, int ok, const WebPConfig* c) {
  printf("%s %d: %d %.1f %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", label, ok, c->lossless, c->quality, c->method, c->image_hint,
         c->segments, c->sns_strength, c->filter_strength, c->filter_sharpness, c->filter_type, c->alpha_compression, c->alpha_filtering,
         c->alpha_quality, c->pass, c->preprocessing, c->partitions, c->near_lossless, c->exact, c->qmax);
}

int main(int argc, char** argv) {
  if (argc > 2) {
    width = atoi(argv[1]);
    height = atoi(argv[2]);
  }
  setvbuf(stdout, NULL, _IONBF, 0);
  uint8_t* rgba = malloc((size_t)width * height * 4);
  uint8_t* other = malloc((size_t)width * height * 4);
  picture(rgba, 0);
  printf("versions %x %x\n", WebPGetEncoderVersion(), WebPGetMuxVersion());

  /* Configurations. */
  WebPConfig c;
  for (int preset = 0; preset <= WEBP_PRESET_TEXT; ++preset) {
    char label[32];
    snprintf(label, sizeof label, "preset %d", preset);
    printConfig(label, WebPConfigPreset(&c, (WebPPreset)preset, 42.5f), &c);
  }
  for (int level = -1; level <= 10; ++level) {
    WebPConfigInit(&c);
    char label[32];
    snprintf(label, sizeof label, "lossless %d", level);
    printConfig(label, WebPConfigLosslessPreset(&c, level), &c);
  }
  WebPConfigInit(&c);
  c.method = 7;
  printf("validate method 7: %d\n", WebPValidateConfig(&c));
  c.method = 4;
  c.qmin = 50;
  c.qmax = 40;
  printf("validate qmin > qmax: %d\n", WebPValidateConfig(&c));

  /* WebPEncode as Skia drives it: lossless (exact), RGBA and RGBX and
     BGRA imported, written to a memory writer. */
  for (int k = 0; k < 4; ++k) {
    WebPConfig config;
    WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, 100);
    config.lossless = 1;
    config.method = 0;
    config.exact = 1;
    WebPPicture pic;
    WebPPictureInit(&pic);
    pic.width = width;
    pic.height = height;
    pic.use_argb = 1;
    int imported = 0;
    uint8_t* source = rgba;
    if (k == 0) imported = WebPPictureImportRGBA(&pic, rgba, width * 4);
    if (k == 1) {
      for (int i = 0; i < width * height; ++i) { other[4 * i] = rgba[4 * i]; other[4 * i + 1] = rgba[4 * i + 1]; other[4 * i + 2] = rgba[4 * i + 2]; other[4 * i + 3] = 0x5a; }
      imported = WebPPictureImportRGBX(&pic, other, width * 4);
      for (int i = 0; i < width * height; ++i) other[4 * i + 3] = 255;
      source = other;
    }
    if (k == 2) {
      for (int i = 0; i < width * height; ++i) { other[4 * i] = rgba[4 * i + 2]; other[4 * i + 1] = rgba[4 * i + 1]; other[4 * i + 2] = rgba[4 * i]; other[4 * i + 3] = rgba[4 * i + 3]; }
      imported = WebPPictureImportBGRA(&pic, other, width * 4);
      source = rgba;
    }
    if (k == 3) {
      pic.width = 0;
      imported = WebPPictureImportRGBA(&pic, rgba, width * 4);
    }
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &writer;
    int encoded = imported && WebPEncode(&config, &pic);
    int exact = encoded && same(writer.mem, writer.size, source);
    int transparent = imported ? WebPPictureHasTransparency(&pic) : -1;
    printf("encode %d: imported %d encoded %d error %d exact %d transparent %d\n", k, imported, encoded, pic.error_code, exact, transparent);
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&pic);
  }
  {
    WebPPicture pic;
    WebPPictureInit(&pic);
    pic.width = width;
    pic.height = height;
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &writer;
    WebPConfigInit(&c);
    int r = WebPEncode(&c, &pic);
    printf("encode nothing: %d error %d\n", r, pic.error_code);
    r = WebPEncode(NULL, &pic);
    printf("encode no config: %d error %d\n", r, pic.error_code);
    c.quality = 200;
    r = WebPEncode(&c, &pic);
    printf("encode bad config: %d error %d\n", r, pic.error_code);
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&pic);
  }

  /* The one-call encoders. */
  /* (Opaque: without `exact`, libwebp's encoder may change the colour of
     transparent pixels.) */
  uint8_t* out = NULL;
  for (int i = 0; i < width * height; ++i) { memcpy(other + 4 * i, rgba + 4 * i, 3); other[4 * i + 3] = 255; }
  size_t n = WebPEncodeLosslessRGBA(other, width, height, width * 4, &out);
  int exact = n > 0 && same(out, n, other);
  printf("simple lossless rgba: %d %d\n", n > 0, exact);
  WebPFree(out);
  for (int i = 0; i < width * height; ++i) { other[3 * i] = rgba[4 * i + 2]; other[3 * i + 1] = rgba[4 * i + 1]; other[3 * i + 2] = rgba[4 * i]; }
  n = WebPEncodeLosslessBGR(other, width, height, width * 3, &out);
  {
    uint8_t* opaque = malloc((size_t)width * height * 4);
    for (int i = 0; i < width * height; ++i) { memcpy(opaque + 4 * i, rgba + 4 * i, 3); opaque[4 * i + 3] = 255; }
    exact = n > 0 && same(out, n, opaque);
    printf("simple lossless bgr: %d %d\n", n > 0, exact);
    free(opaque);
  }
  WebPFree(out);
  n = WebPEncodeRGBA(rgba, width, height, width * 4, 80, &out);
  {
    int w = 0, h = 0;
    int info = WebPGetInfo(out, n, &w, &h);
    printf("simple rgba: %d %d %d %d\n", n > 0, info, w, h);
  }

  /* The muxer, as Skia adds an ICC profile, and the demuxer on the file. */
  WebPMux* mux = WebPMuxNew();
  WebPData image = { out, n };
  static const uint8_t icc[] = "an ICC profile";
  WebPData iccChunk = { icc, sizeof icc };
  int set = WebPMuxSetImage(mux, &image, 0);
  printf("mux image %d", set);
  set = WebPMuxSetChunk(mux, "ICCP", &iccChunk, 0);
  printf(" icc %d\n", set);
  set = WebPMuxSetChunk(mux, "VP8 ", &iccChunk, 0);
  printf("mux vp8 tag %d\n", set);
  int count = -1;
  WebPMuxNumChunks(mux, WEBP_CHUNK_ICCP, &count);
  printf("mux iccp chunks %d", count);
  WebPMuxNumChunks(mux, WEBP_CHUNK_IMAGE, &count);
  printf(" images %d", count);
  WebPMuxNumChunks(mux, WEBP_CHUNK_ANMF, &count);
  printf(" frames %d\n", count);
  WebPData got;
  int status = WebPMuxGetChunk(mux, "ICCP", &got);
  printf("mux get icc %d", status);
  printf(" %zu %d\n", got.size, got.size == sizeof icc && !memcmp(got.bytes, icc, sizeof icc));
  status = WebPMuxGetChunk(mux, "EXIF", &got);
  printf("mux get exif %d\n", status);
  WebPData assembled;
  status = WebPMuxAssemble(mux, &assembled);
  printf("mux assemble %d\n", status);
  WebPMuxDelete(mux);
  WebPFree(out);
  {
    WebPDemuxer* d = WebPDemux(&assembled);
    WebPChunkIterator ci;
    printf("demux %d flags %u", d != NULL, d ? WebPDemuxGetI(d, WEBP_FF_FORMAT_FLAGS) & ~ALPHA_FLAG : 0);
    printf(" canvas %u %u", d ? WebPDemuxGetI(d, WEBP_FF_CANVAS_WIDTH) : 0, d ? WebPDemuxGetI(d, WEBP_FF_CANVAS_HEIGHT) : 0);
    int found = d && WebPDemuxGetChunk(d, "ICCP", 1, &ci);
    printf(" icc %d %d\n", found, found && ci.chunk.size == sizeof icc && !memcmp(ci.chunk.bytes, icc, sizeof icc));
    WebPDemuxDelete(d);
    int w = 0, h = 0;
    uint8_t* back = WebPDecodeRGBA(assembled.bytes, assembled.size, &w, &h);
    printf("muxed decodes %d %d %d\n", back != NULL, w, h);
    WebPFree(back);
    WebPMux* again = WebPMuxCreate(&assembled, 1);
    uint32_t flags = 0;
    printf("mux create %d", again != NULL);
    if (again) {
      status = WebPMuxGetFeatures(again, &flags);
      printf(" features %d %u", status, flags & ~ALPHA_FLAG);
      status = WebPMuxGetChunk(again, "ICCP", &got);
      printf(" icc %d", status);
      WebPMuxFrameInfo info;
      status = WebPMuxGetFrame(again, 1, &info);
      printf(" frame %d", status);
      printf(" id %d", info.id);
      int fw = 0, fh = 0;
      status = WebPGetInfo(info.bitstream.bytes, info.bitstream.size, &fw, &fh);
      printf(" frame decodes %d %d %d", status, fw, fh);
      WebPDataClear(&info.bitstream);
      status = WebPMuxDeleteChunk(again, "ICCP");
      printf(" delete icc %d", status);
      status = WebPMuxDeleteChunk(again, "ICCP");
      printf(" %d", status);
      WebPData plain;
      status = WebPMuxAssemble(again, &plain);
      printf(" assemble %d", status);
      WebPDemuxer* d2 = WebPDemux(&plain);
      printf(" plain flags %u", d2 ? WebPDemuxGetI(d2, WEBP_FF_FORMAT_FLAGS) & ~ALPHA_FLAG : 99);
      WebPDemuxDelete(d2);
      WebPDataClear(&plain);
      WebPMuxDelete(again);
    }
    printf("\n");
    WebPDataClear(&assembled);
  }

  /* An animation as Skia encodes one: three frames of the canvas's size,
     the end marked by a NULL frame; decoded by the animation decoder. */
  {
    WebPAnimEncoderOptions options;
    WebPAnimEncoderOptionsInit(&options);
    printf("anim options loop %d bg %08x kmin %d kmax %d\n", options.anim_params.loop_count, options.anim_params.bgcolor, options.kmin, options.kmax);
    WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, NULL);
    uint8_t* frames[3];
    int timestamp = 0;
    int durations[3] = { 40, 70, 100 };
    for (int k = 0; k < 3; ++k) {
      frames[k] = malloc((size_t)width * height * 4);
      picture(frames[k], k * 3);
      for (int i = 0; i < width * height; ++i) frames[k][4 * i + 3] = 255;
      WebPConfig config;
      WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, 100);
      config.lossless = 1;
      config.exact = 1;
      WebPPicture pic;
      WebPPictureInit(&pic);
      pic.width = width;
      pic.height = height;
      pic.use_argb = 1;
      WebPPictureImportRGBA(&pic, frames[k], width * 4);
      int added = WebPAnimEncoderAdd(enc, &pic, timestamp, &config);
      printf("anim add %d: %d\n", k, added);
      timestamp += durations[k];
      WebPPictureFree(&pic);
    }
    int added = WebPAnimEncoderAdd(enc, NULL, timestamp, NULL);
    printf("anim add end: %d\n", added);
    WebPData anim;
    added = WebPAnimEncoderAssemble(enc, &anim);
    printf("anim assemble: %d\n", added);
    WebPAnimEncoderDelete(enc);
    WebPAnimDecoderOptions decOptions;
    WebPAnimDecoderOptionsInit(&decOptions);
    WebPAnimDecoder* dec = WebPAnimDecoderNew(&anim, &decOptions);
    WebPAnimInfo info;
    if (dec && WebPAnimDecoderGetInfo(dec, &info)) {
      printf("anim info %u %u loop %u frames %u\n", info.canvas_width, info.canvas_height, info.loop_count, info.frame_count);
      int k = 0;
      while (WebPAnimDecoderHasMoreFrames(dec)) {
        uint8_t* buf;
        int ts;
        if (!WebPAnimDecoderGetNext(dec, &buf, &ts)) break;
        int exactFrame = k < 3 && !memcmp(buf, frames[k], (size_t)width * height * 4);
        printf("anim frame %d at %d exact %d\n", k, ts, exactFrame);
        ++k;
      }
    } else {
      printf("anim does not decode\n");
    }
    WebPAnimDecoderDelete(dec);
    WebPDataClear(&anim);
    /* One frame: a still picture. */
    enc = WebPAnimEncoderNew(width, height, &options);
    WebPPicture pic;
    WebPPictureInit(&pic);
    pic.width = width;
    pic.height = height;
    pic.use_argb = 1;
    WebPPictureImportRGBA(&pic, frames[1], width * 4);
    WebPConfigInit(&c);
    c.lossless = 1;
    c.exact = 1;
    WebPAnimEncoderAdd(enc, &pic, 0, &c);
    WebPPictureFree(&pic);
    WebPData still;
    added = WebPAnimEncoderAssemble(enc, &still);
    printf("one frame: %d", added);
    WebPBitstreamFeatures f;
    added = WebPGetFeatures(still.bytes, still.size, &f);
    int exactStill = same(still.bytes, still.size, frames[1]);
    printf(" features %d animated %d exact %d\n", added, f.has_animation, exactStill);
    WebPDataClear(&still);
    WebPAnimEncoderDelete(enc);
    for (int k = 0; k < 3; ++k) free(frames[k]);
  }
  free(rgba);
  free(other);
  return 0;
}
