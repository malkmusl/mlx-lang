# mlx-libpng

libpng for the C and C++ programs built here, in Mlx, with libpng's C
API (1.6.43). WebKit's PNG decoder (WebCore's PNGImageDecoder, which
also reads APNG through libpng's unknown chunk function) and Skia's PNG
codec and encoder include `<png.h>` as they would libpng's header and
link `libmlxpng16.so` in place of `libpng16.so`.

The decoder is the standard library's: `std.png` (std/src/png.mlx and
std/src/png/), which Mlx programs use directly. `std.png.read` is
libpng's progressive reader step for step (pngpread.c, pngrutil.c): the
file in pieces of any size, the same checks of every chunk with the same
warnings and errors, the CRC handling, the colour space (gAMA, sRGB,
cHRM, iCCP and how they invalidate each other), the treatment of damaged
image data. `std.png.rows` has the filters, Adam7's geometry, the
combining of interlaced rows and libpng's row transformations and gamma
tables (pngrtran.c, png.c), with the same results byte for byte.

This directory is the C layer: the structures behind `png_structp` and
`png_infop`, the callbacks (info, row, end, unknown chunks), the
order in which libpng calls them (the row numbers and passes of its
interlace handling, the rows without data), the transformation setters
and their interplay (png_read_update_info), setjmp-based errors, the
info getters and setters, and the writer.

    ./mlx build png        the library and its headers
    tools/check_png.sh     builds it, decodes and encodes the test images
                           with it and with the system's libpng and
                           compares, compiles Skia's PNG sources

## Errors: setjmp and longjmp

`png_jmpbuf(png_ptr)` is libpng's macro, `png_set_longjmp_fn` with the C
library's `longjmp`: the application's `setjmp` fills a buffer the
structure holds. An error calls the application's error function (which
usually longjmps itself, as WebKit's and Skia's do), then libpng's
default: the message on stderr and a call of the stored `longjmp` with
that buffer. The Mlx frames between are left behind: the library is
built with `--stack-arena`, so their arenas are on the stack the
longjmp gives back, and nothing is held in them: whatever is allocated
hangs off the structure, which `png_destroy_read_struct` frees.

## What it does

- Progressive reading: `png_process_data` with any pieces (a byte at a
  time too), `png_process_data_pause` (WebKit's size-only decoding),
  the info, row and end functions, `png_progressive_combine_row` with
  libpng's 'blocky' display of the passes, `png_set_sig_bytes`.
- Transformations: expand (palette, low-depth grey, tRNS to alpha),
  expand_16, strip_16, scale_16, strip_alpha, gray_to_rgb, gamma
  (`png_set_gamma`, with libpng's 8-bit, 16-bit and 16-to-8 tables and
  the sBIT shift), invert_mono, invert_alpha, packing, bgr, packswap,
  filler and add_alpha, swap_alpha, swap, interlace handling.
- Unknown chunks: `png_set_keep_unknown_chunks`, the user chunk function
  (with libpng's 'forcing save' warning and 'unhandled critical chunk'
  error), `png_handle_as_unknown`.
- The info: IHDR, PLTE, tRNS, sBIT, bKGD, gAMA (double and fixed),
  cHRM, sRGB, iCCP (the profile checked as libpng checks it), pHYs;
  `png_set_crc_action`, `png_set_option` (the maximum inflate window),
  user limits, benign errors.
- Writing (Skia's encoder): IHDR, gAMA, iCCP or sRGB, sBIT, cHRM, PLTE,
  tRNS, bKGD, pHYs, tEXt and zTXt, unknown chunks where they belong;
  libpng's filter choice, zlib settings (the window made small for small
  images and the stream's header told so) and 8192-byte IDATs, deflated
  by std.flate as zlib does: the files are libpng's byte for byte.

## How it differs from libpng

- No sequential reader (`png_read_info`, `png_read_row`, `png_read_png`)
  and no simplified API (`png_image_*`): WebKit and Skia use neither.
- Text, time, eXIf, hIST, oFFs, pCAL, sCAL and sPLT chunks are checked
  as libpng checks them, with its warnings, but not kept: there are no
  getters for them, nor for unknown chunks kept in the info.
- The writer writes no interlaced images (an interlaced IHDR is written
  without interlacing, with a warning).
- ICC profiles that libpng recognises as sRGB's are not marked sRGB.
