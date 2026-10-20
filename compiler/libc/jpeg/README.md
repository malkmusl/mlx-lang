# mlx-libjpeg

libjpeg for the C and C++ programs built here, in Mlx, with the libjpeg 8
API of libjpeg-turbo 2.1.5 (its extensions too: the extended RGB colour
spaces, `jpeg_skip_scanlines`, `jpeg_crop_scanline`, the memory source).
WebKit's JPEG decoder (WebCore's JPEGImageDecoder) and Skia's JPEG codec
include `<jpeglib.h>` as they would libjpeg-turbo's header and link
`libmlxjpeg.so` in place of `libjpeg.so.8`.

The decoder is the standard library's: `std.jpeg` (std/src/jpeg.mlx and
std/src/jpeg/), which Mlx programs use directly. `std.jpeg.read` is the
input half of libjpeg-turbo's decompressor step for step (jdmarker.c,
jdinput.c, jdhuff.c, jdphuff.c): the markers and their checks, with the
same warnings and errors, suspension at libjpeg's commit points, the
sequential and progressive Huffman decoding (with libjpeg-turbo's fast
path where it takes it, for the bytes it reads ahead), restart markers
and resynchronization, damaged data. `std.jpeg.output` is the output
half: the accurate integer IDCT as libjpeg-turbo computes it on x86-64,
block smoothing of progressive passes, fancy and plain upsampling, the
colour conversions, and the moment each row can come out.

This directory is the C layer: the public structures (laid out as
libjpeg-turbo's, which `tests/layout.c` checks), the global states and
their checks, the data source (its bytes taken as they come, the
application's `fill_input_buffer` called when the decoder wants more),
the memory manager with its pools, the error manager (`jpeg_std_error`'s
functions, libjpeg's message texts and formatting), the public fields
kept as libjpeg keeps them.

    ./mlx build jpeg        the library and its headers
    tools/check_jpeg.sh     builds it, decodes the test images with it
                            and with the system's libjpeg-turbo and
                            compares, compiles Skia's JPEG sources

## Errors: setjmp and longjmp

libjpeg reports an error by calling the error manager's `error_exit`,
which an application replaces with a function that longjmps (WebKit's
and Skia's do). The decoder itself only marks itself failed; the C layer
then sets `msg_code` and `msg_parm` and calls `error_exit`. The Mlx
frames between are left behind: the library is built with
`--stack-arena`, so their arenas are on the stack the longjmp gives back,
and nothing is held in them: whatever is allocated hangs off the object
(`cinfo->mem` is the state's memory manager), which
`jpeg_destroy_decompress` frees. Warnings go through `emit_message` in
the order the decoder met them.

## What it does

- Decompression: `jpeg_read_header` (tables-only datastreams too),
  `jpeg_start_decompress`, `jpeg_read_scanlines` (as many rows per call
  as libjpeg gives), `jpeg_finish_decompress`, `jpeg_consume_input`,
  suspending sources (WebKit's: `fill_input_buffer` answering FALSE),
  `jpeg_mem_src` and `jpeg_stdio_src` (an end of image put in when the
  data runs out, with libjpeg's warning).
- Buffered-image mode: `jpeg_start_output`, `jpeg_finish_output`,
  `jpeg_input_complete`, `jpeg_has_multiple_scans`; block smoothing of
  passes whose scans are not all there yet.
- Output: grey, RGB and the extended RGB spaces (RGBX, BGR, BGRX, XBGR,
  XRGB and their alpha forms), CMYK (from Adobe's YCCK too), YCbCr as it
  is; fancy upsampling or not; `jpeg_read_raw_data`,
  `jpeg_crop_scanline`, `jpeg_skip_scanlines`;
  `jpeg_calc_output_dimensions` at every scale.
- Markers: `jpeg_save_markers` (`marker_list`), `jpeg_read_icc_profile`.

## How it differs from libjpeg

- Output is decoded at scale 1 only: other scales give their dimensions,
  and `jpeg_start_decompress` refuses them (JERR_BAD_DCTSIZE).
- No arithmetic-coded files (JERR_NOT_COMPILED), no 12-bit data, no
  colour quantization (`quantize_colors`), no RGB565 output, no
  transcoding (`jpeg_read_coefficients`), no marker processors of the
  application's (`jpeg_set_marker_processor`).
- The DCT method asked for is not looked at: always the accurate integer
  one (WebKit and Skia ask for it).
- The compressor (Skia's JPEG encoder) is not written yet: creating and
  destroying compressors works, the functions that compress raise
  JERR_NOT_COMPILED through the error manager.
- Trace messages (levels 1 and up) are not made.
- The decoder keeps each component's coefficients and samples for the
  whole image, where libjpeg keeps an iMCU row of a sequential file.
