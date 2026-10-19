# mlx-libwebp

libwebp for the C and C++ programs built here, in Mlx, with the API of
libwebp 1.3.2: `<webp/decode.h>` (decoder ABI 0x0209), `<webp/demux.h>`
(demux ABI 0x0107), `<webp/mux_types.h>` and `<webp/types.h>`. WebKit's
WebP decoder (WebCore's WEBPImageDecoder) includes them as it would
libwebp's headers and links `libmlxwebp.so` in place of `libwebp.so.7`
and `libwebpdemux.so.2`.

The decoders are the standard library's: `std.webp` (std/src/webp.mlx and
std/src/webp/), which Mlx programs use directly, libwebp's decoder step
for step:

- `std.webp.vp8`: lossy pictures (VP8 key frames): the boolean decoder
  reading 56 bits at a time as on x86-64, the headers, segments and
  token partitions, the intra modes and coefficients, the reconstruction
  and the loop filter, rows handed on with the filter's delay. The full
  inverse transform is computed in 16-bit lanes as libwebp's SSE2 code
  computes it, which only matters for coefficients no encoder makes.
- `std.webp.vp8l`: lossless pictures (VP8L): the Huffman codes and their
  two-level tables, the meta Huffman image, backward references, the
  colour cache, the four transforms; and the alpha planes of lossy
  pictures that are coded losslessly.
- `std.webp.alpha`: the ALPH chunk: raw or lossless, unfiltered row by
  row (horizontal, vertical, gradient).
- `std.webp.io`: the output colour spaces (RGB, RGBA, BGR(A), ARGB,
  RGBA4444, RGB565, premultiplied or not, YUV(A)) with libwebp's fancy
  upsampler and YUV to RGB conversion, the alpha put in place.
- `std.webp.decode`: the container (RIFF, VP8X, ALPH and the other
  chunks), WebPGetFeatures, WebPDecode and the incremental decoder (data
  appended or remapped, a VP8 macroblock or 8 VP8L rows at a time,
  `last_y` as libwebp moves it).
- `std.webp.demux` and `std.webp.anim`: the demuxer (partial files too)
  and the animation decoder (frames blended and disposed onto the
  canvas).

This directory is the C layer: the headers (made by mlx-cheaders from
the Mlx modules of `api/`), the public structures laid out as libwebp's
(which `tests/layout.c` checks), and the exported functions over
std.webp. The memory the API hands out (WebPDecodeRGBA's pixels, the
decoder's own output buffers) is the C library's, freed by `WebPFree`
and `WebPFreeDecBuffer` as libwebp's is.

    ./mlx build webp        the library and its headers
    tools/check_webp.sh     builds it, decodes the test files with it
                            and with the system's libwebp and compares

## What it does

- `WebPGetInfo`, `WebPGetFeatures`, `WebPDecodeRGBA` and its kin (ARGB,
  BGRA, RGB, BGR, YUV), into memory of the library's or the caller's
  (`WebPDecode*Into`).
- `WebPDecode` with a `WebPDecoderConfig`: every colour space, the plain
  upsampler, cropping, flipping, no loop filter, output into memory of
  the caller's (slow memory too: decoded apart and copied, as libwebp
  does for premultiplied alpha).
- The incremental decoder: `WebPINewDecoder`, `WebPINewRGB`,
  `WebPINewYUV(A)`, `WebPIDecode`, `WebPIAppend`, `WebPIUpdate`,
  `WebPIDecGetRGB`, `WebPIDecGetYUVA`, `WebPIDecodedArea`.
- The demuxer: `WebPDemux`, `WebPDemuxPartial`, `WebPDemuxGetI`, frames
  and chunks (ICCP, EXIF, XMP) with their iterators.
- The animation decoder: `WebPAnimDecoderNew` (RGBA, BGRA, rgbA, bgrA),
  `WebPAnimDecoderGetNext`, `GetInfo`, `Reset`, `GetDemuxer`.

## What it does not do yet

- Scaling (`use_scaling`): WebPDecode and WebPIDecode answer
  `VP8_STATUS_INVALID_PARAM` (libwebp's rescaler is not there).
- Dithering (`dithering_strength`, `alpha_dithering_strength`): the
  options are taken and nothing is dithered; the default is none.
- Threads (`use_threads`): decoding is single-threaded, which gives the
  same pixels.
