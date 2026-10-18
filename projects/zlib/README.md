# mlx-zlib

zlib for the C and C++ programs built here, in Mlx, with zlib's C API
(1.3.1). WebKit's sources that compress or decompress (ANGLE's program
cache, WebCore's CompressionStream and WOFF fonts, the WebSocket
extension) include `<zlib.h>` as they would zlib's header and link
`libmlxz.so` in place of `libz.so`. The library is this repository's
own, in Mlx; its compressor, decompressor and gzip files follow zlib's
algorithms step for step (those of Jean-loup Gailly's and Mark Adler's
deflate.c, trees.c, inflate.c, inftrees.c, inffast.c and gz*.c, 1.3.1),
and are checked against what zlib prints and writes for the same
programs.

The interface is written once, in Mlx: the modules of `api/` declare the
types, constants, functions and macros C sees (`deflateInit`,
`inflateInit2`, the `Z_` constants, `z_stream`, and zconf.h's `FAR`,
`OF`, `z_const`...). `mlx-cheaders` (projects/cc) makes the C headers of
them into `mlx-out/include/mlx-zlib/`; there are no headers written by
hand.

    ./mlx build zlib           the library and its headers
    tools/check_zlib.sh        builds it, runs the tests, the round trips with the system's zlib, ANGLE's zlib sources

## What it does

- **deflate** writes raw deflate data (RFC 1951), zlib streams (RFC 1950)
  and gzip members (RFC 1952), with windows of 2^8 to 2^15 bytes, memory
  levels 1 to 9, every level (0 stored, 1 to 3 the fast matcher, 4 to 9
  lazy matching) and strategy (`Z_FILTERED`, `Z_HUFFMAN_ONLY`, `Z_RLE`,
  `Z_FIXED`), every flush (`Z_NO_FLUSH`, `Z_PARTIAL_FLUSH`,
  `Z_SYNC_FLUSH`, `Z_FULL_FLUSH`, `Z_FINISH`, `Z_BLOCK`), input and
  output in pieces of any size, dictionaries, gzip headers
  (`deflateSetHeader`), `deflateParams` on the way, `deflatePrime`,
  `deflateCopy`, `deflateBound`, `deflatePending`, `deflateTune`.
  It is zlib's algorithm step for step (the hash chains, the lazy
  evaluation and the configuration table of each level, the heap and the
  depth tie-break of the Huffman trees, the bit length limits, the block
  type chosen by the counted sizes, the stored blocks of level 0 copied
  straight through, the 16-bit bit buffer), so its output is byte for
  byte what zlib 1.3 writes: for every level, strategy, window, memory
  level, flush and piece size the tests try, and for all 238 streams of
  the round trips.
- **inflate** reads all of those, automatically detecting zlib or gzip
  (windowBits + 32), concatenated gzip members one at a time, with zlib's
  decoding tables (a root table and sub-tables), its fast loop, the
  window made only when needed, `Z_BLOCK` and `Z_TREES` stops,
  dictionaries, `inflateGetHeader`, `inflateSync`, `inflateMark`,
  `inflateCopy`, `inflatePrime`, `inflateValidate`, and `inflateBack`.
  Damaged data gives zlib's return codes and messages ("incorrect header
  check", "invalid distance too far back", "incorrect data check"...),
  and the same `data_type`, totals and marks along the way.
- `compress`, `compress2`, `compressBound`, `uncompress`, `uncompress2`;
  `adler32`, `crc32` (eight bytes at a time), their `_z` forms and
  combinations; `zError`, `zlibVersion`, `zlibCompileFlags`,
  `get_crc_table`.
- The gzip files: `gzopen`, `gzdopen`, `gzread`, `gzwrite`, `gzprintf`
  (C's variable arguments, formatted by the C library's `vsnprintf`),
  `gzputs`, `gzgets`, `gzputc`, `gzgetc`, `gzungetc`, `gzfread`,
  `gzfwrite`, `gzflush`, `gzseek`, `gztell`, `gzoffset`, `gzrewind`,
  `gzeof`, `gzdirect`, `gzbuffer`, `gzsetparams`, `gzerror`,
  `gzclearerr`, `gzclose`; files that are not gzip read as they are,
  appending, transparent writing, and the errors with their messages
  ("PATH: unexpected end of file").

## Parts

| File | What |
|---|---|
| `api/zconf.mlx`, `api/zlib.mlx` | The interface in Mlx: `zconf.h` (the basic types, `MAX_WBITS`, the declaration macros, empty) and `zlib.h` |
| `zlib.mlx` | The library's root (`--plugin --stack-arena`) |
| `common.mlx` | `z_stream` and `gz_header` as C lays them out, the return codes and their messages, the stream's allocator (zalloc and zfree, or malloc and free) |
| `checksum.mlx` | Adler-32, CRC-32 and their combinations |
| `trees.mlx` | The compressor's state and its Huffman side: the trees, the block types, the bits written |
| `deflate.mlx` | The compressor: the window, the hash chains, the matchers of each level and strategy, the flushes, headers and trailers |
| `inflate.mlx` | The decompressor: its state machine, the decoding tables, the fast loop, the window |
| `utility.mlx` | compress, uncompress, inflateBack, the version and error strings |
| `gzip.mlx`, `varargs.mlx` | The gzip files; C's variable arguments for gzprintf |
| `tests/` | C programs built by mlx-cc against the library, each with the output the same program prints built by gcc against the system's zlib (`NAME.expected`, the exit status last); `corpus.h`, the data they compress; `interop.c`, the round trips |

## How it differs from zlib

- `ZLIB_VERSION` is "1.3.1"; the output is that of zlib 1.3.1 and 1.3,
  which write the same streams.
- `inflateBack` works through inflate: the output goes through the
  caller's window to `out` as zlib's does, but inflate keeps its own copy
  of the history (another window's worth of memory).
- `gzgetc` is a function (zlib's header makes it a macro reading the
  bytes ahead itself; those are in the state, as in zlib, for programs
  that look).
- The library has no `deflate_copyright` or `inflate_copyright` strings
  and no 32-bit-only or Windows parts (`gzopen_w`, `_LARGEFILE64_SOURCE`
  variants beyond `gzopen64` and the other `64` functions, which are the
  same functions here).

## mlx-cheaders

zlib's headers needed macros of plain C tokens: a constant of the type
`c_tokens` is a macro of its string's text (`pub const FAR: c_tokens =
""` is `#define FAR`), and a macro's parameter of that type is used as it
is (`pub inline fn OF(args: c_tokens) -> c_tokens { return args }` is
`#define OF(args) args`). The glib and unicode headers come out as
before.
