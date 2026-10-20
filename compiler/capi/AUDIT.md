# The C API layers: what is logic, what is C shape, where each goes

The libraries of this tree are C APIs of other projects (GLib, libsoup,
ICU, HarfBuzz, FreeType, zlib, libpng, libjpeg, libwebp, SQLite, libxml2,
Little CMS, libgcrypt, libtasn1, PulseAudio's and PipeWire's client
libraries) written in Mlx, so that the C and C++ programs built here
(WebKit through mlx-cc, and third-party clients of MLX Audio and MLX
Capture) link them in place of the originals. Each is a `--plugin` shared
object whose headers mlx-cheaders makes of the Mlx modules of its `api/`.

The rule this audit applies: everything that is real functionality lives
in std (native Mlx modules under std/src), so that our own programs (the
browser Voyager, the engine Lattice) use it directly; a C-shaped layer is
tolerated as a thin shim for the C and C++ code we still compile, and
belongs under compiler/libc (our C library), not in a tree of its own.
GLib/GIO and libsoup as C APIs are being retired with the WPE port; their
reusable logic is saved into std before they go.

Line counts are of the Mlx sources (`wc -l`), the C tests apart.

| Library | `api/` | shim | C tests | Logic not yet in std | Plan |
|---|---:|---:|---:|---|---|
| glib | 6209 | 32805 | 29 | the resolver, content types by extension, IDNA host names, URI algorithms, a key-file reader, the GVDB reader, GMarkup, GVariant's serialization | keep as shim (WPE WTF/JSC, mlx-soup) until the WPE port is retired; save the generic pieces into std (below); then drop |
| soup | 1806 | 17993 | 18 | HTTP/1.1 client sessions (connections, redirects, requeues), Basic and Digest auth, cookies and the jar with its policies and text file, HSTS policies and their file, multipart bodies, the WebSocket handshake and permessage-deflate, request/response line parsing, ranges, content disposition, the logger, the server | save into std.http (sessions as `std.http.client`); keep as shim (WPE's NetworkProcess) until retired; drop the server, logger and cache with it |
| gcrypt | 523 | 6713 | 4 | signed multi-precision integers (scan, print, Knuth division, gcd, inverse), RSA key generation, the CRT private operation, OAEP and PKCS #1 encryption, PSS signing, S-expressions | save MPI and RSA's private half into std.crypto; keep as shim (WebCore's WebCrypto, PAL's digests) |
| tasn1 | 146 | 1693 | 1 | a DER coder driven by asn1Parser's definition arrays; a DER writer | keep as shim (WebCore PAL::TASN1); std.asn1 gains a writer |
| icu | 4339 | 907 | 43 | none: every function calls std.unicode's of the same name | keep as shim (WTF, JSC, WebCore) |
| xml2 | 1974 | 4428 | 6 | entity tables and the predefined entities, entity escaping, qualified names | keep as shim (WebCore's XML parser); the small generic pieces into std.xml |
| freetype | 3043 | 2031 | 10 | none: records over std.truetype | keep as shim (Skia, WebCore) |
| harfbuzz | 1750 | 2896 | 19 | none: exports of std.shaping, hb-ft over mlx-freetype | keep as shim (WebCore, Skia) |
| zlib | 334 | 1893 | 6 | gzip files (transparent reads, concatenated members, seeking, buffering) | save into std.flate (gzip files); keep as shim (ANGLE, WebCore) |
| png | 646 | 3204 | 2 | libpng's writer: the chunks of the info in libpng's order, the filter choice, libpng's zlib settings and IDAT sizes | save into std.png (a writer that makes libpng's bytes, under `std.png.saveFile` too); keep as shim (WebCore, Skia) |
| jpeg | 764 | 2016 | 2 | none: the error manager, the sources and the public fields over std.jpeg | keep as shim (WebCore, Skia); a JPEG encoder is missing in std and in the shim alike |
| webp | 777 | 1451 | 3 | none: layouts and the picture's importers over std.webp | keep as shim (WebCore, Skia) |
| lcms2 | 772 | 539 | 2 | none: handles over std.colorprofile | keep as shim (WebCore's decoders) |
| sqlite3 | 627 | 2409 | 7 | none: SQLite's printf is C's variable arguments over std.sql's formatting | keep as shim (WebCore's databases) |
| pulse | 0 | 10315 | 0 (values compared by tools/libpulse_values.py) | sample specs, channel maps, volumes (dB, text forms), format infos; the native protocol's client (contexts, streams, timing) | keep as shim (third-party PulseAudio clients of MLX Audio, the ALSA plugin); the value arithmetic and names are candidates for std.pulse |
| pipewire | 0 | 6963 | 0 (tools/libpipewire_values.py) | property dicts (parsing, JSON); the native protocol's client (core, proxies, client nodes) | keep as shim (third-party PipeWire clients of MLX Capture); the dict parsing is a candidate for std.pipewire |
| total | 23710 | 98256 | | | |

A "C test" is a program under `tests/` that mlx-cc builds against the
library and whose output `tools/check_LIB.sh` compares with the system
library's (`NAME.expected`); the checks keep passing whatever moves, since
a shim's behaviour must not change.

## What is C shape, in every library

The same things in each, and they stay in the shims:

- handles: objects as C memory (`calloc` or the C allocator, since an
  export's arena is gone when it returns), reached by address with
  `getWord`/`putWord` at fixed offsets; reference counts; the GObject
  type system, properties and signals (glib, soup);
- struct layouts C reads directly (`z_stream`, `png_struct` is opaque but
  `png_info` getters are not, `jpeg_decompress_struct`, `WebPDecoderConfig`,
  `FT_FaceRec`, `hb_glyph_info_t`, `pa_sample_spec`, `struct pw_buffer`,
  `xmlParserCtxt`, `sqlite3_value`), checked by `tests/layout.c` where the
  original's layout matters;
- error conventions: errno, `UErrorCode` by address, `GError**`,
  `gpg_error_t`, `asn1_strerror`, `setjmp`/`longjmp` through an error
  manager (jpeg, png), libxml2's error routing, SQLite's result codes;
- callbacks through C function pointers (sources, `fill_input_buffer`,
  `png_rw_ptr`, SAX handlers, `pa_*_cb_t`, `spa_hook`), C's variable
  arguments read from the registers (`gzprintf`, `sqlite3_mprintf`,
  `g_strdup_printf`, `xmlParserError`, `pa_proplist_setf`);
- C strings, NUL-ended arrays, the originals' names, versions, messages
  and quirks (what the checks compare).

## glib (compiler/capi/glib, 39014 lines)

GLib, GObject and GIO (2.80) as far as WTF's WPE sources, JavaScriptCore's
GLib API and mlx-soup reach: a real main loop, threads, memory, strings,
containers, errors, GVariant, GUri, options, the type system, GIO's
streams, sockets, files, monitors, TLS, resolver and proxies.

(a) Logic that is not in std, by module:

- `resolver.mlx` (481): names to addresses and back by the C library's
  getaddrinfo and getnameinfo, in a thread for the asynchronous calls. std
  has no resolver at all (no `std.net`): a DNS client (`/etc/hosts`,
  `/etc/resolv.conf`, A and AAAA queries over UDP) belongs in std for
  `std.http.client`; the shim keeps GIO's semantics on libc.
- `contenttype.mlx` (143): a table of the web's types by extension, with
  GIO's "uncertain" flag, and sniffing by signature. The signature part is
  std.http.sniff's; the extension table is generic → `std.http.mime`.
- `hostnames.mlx` (159): `g_hostname_to_ascii`'s Punycode. std.unicode.idna
  has IDNA whole (UTS #46, with Punycode); the shim's own copy is GLib's
  variant (no nameprep). Nothing to save; the shim could call
  std.unicode.idna's encoder once it exposes the label step.
- `uri.mlx` (1193): GUri's parsing, resolution against a base, escaping,
  query parameters, with GLib's flags and messages. RFC 3986 itself is
  generic; std has only std.xml.uri (reference resolution for namespace
  names). A `std.uri` (parse, resolve, normalize, percent-coding, query
  pairs) is needed by `std.http.client`; the shim keeps GUri's flag
  semantics.
- `tlsconnection.mlx` (1152), `tls.mlx` (1187): TLS client connections over
  a GIOStream on std.tls, certificates on std.x509. std.tls already does no
  I/O of its own and has blocking helpers over read/write functions; the
  certificate side is std.x509's. Nothing to save but a TCP transport to run
  it on (`std.net`); the GIOStream glue is GIO's shape.
- `markup.mlx` (824): GMarkup, GLib's XML subset. std.xml.sax parses XML
  whole; GMarkup's own rules (its errors, passthrough) are GLib's. Drop
  with the shim.
- `variant.mlx` (2468): GVariant's serialized form, builders, printing. The
  format is GLib's (std.dbus marshals D-Bus itself). Drop with the shim.
- `misc.mlx` (772): patterns (std.glob has them), timers, key files
  (std.conf reads settings files; GKeyFile's groups and escapes are GLib's),
  shell quoting (std.shell). Drop with the shim.
- `resource.mlx` (461): GVDB files, GLib's own format. Drop.
- `datetime.mlx` (347): the proleptic calendar (std.calendar, std.timezone).
  Drop. `checksum.mlx`, `random.mlx`, `strings.mlx`'s base64 and UTF-8:
  std's (std.md5, std.sha1, std.sha2, std.random, std.base64, std.unicode.utf).
- `options.mlx` (814): GOption's grammar and messages, GLib's. Drop.
- `streams.mlx` (2145), `socket.mlx`, `network.mlx`, `localfile.mlx`,
  `fileinput.mlx`, `monitors.mlx`, `cancellable.mlx`, `task.mlx`,
  `main.mlx`: GIO's object model over std.linux's descriptors, epoll,
  inotify and poll; the generic parts (an event loop with sources, file
  monitoring) exist in std.linux's event loop and std.spawn. Drop.
- `types.mlx`, `values.mlx`, `params.mlx`, `closures.mlx`, `signals.mlx`,
  `objects.mlx` (6776): the GObject type system. Not generic: stays in
  the shim as long as the shim lives.

(b) C shape: everything else: every GLib structure, the type system, GIO's
classes and their virtual tables, the messages and criticals.

Plan: keep as a shim at compiler/libc/glib (needed by: the WPE WebKit port's
WTF and JSC GLib sources, mlx-soup, libpulse's GLib main loop), to be
dropped when the WPE port is retired. Save into std first: a resolver
(`std.net`), the MIME-by-extension table (`std.http.mime`), URIs
(`std.uri`).

## soup (compiler/capi/soup, 19799 lines)

libsoup 3.4 over mlx-glib and std.http, what WebKit's NetworkProcess uses.

(a) Logic that is not in std:

- `session.mlx` (1671), `item.mlx`, `conn.mlx` (573), `io.mlx` (851),
  `bodystream.mlx` (661), `inflate.mlx`, `decoder.mlx`, `sniffer.mlx`: a
  client session: the queue of messages, connections per host (limits,
  reuse, idle timeouts), HTTP/1.1 message I/O (request headers and body
  written with Content-Length, chunked or 100-continue; the response's
  status line, headers and body read, framed by Content-Length, chunked
  coding or the connection's end), Content-Encoding undone (gzip, deflate
  by std.flate), sniffing, redirects (which statuses redirect, which
  change the method to GET, which keep it; the Location resolved against
  the message's URI; too many redirects), requeues for authentication,
  preconnecting. std.http has the pieces (headers, chunk sizes, dates,
  status phrases, sniffing) but no message I/O and no sessions. →
  `std.http.message` (request writing, response reading over any byte
  stream, bodies framed and decoded) and `std.http.client` (sessions:
  connections kept per host, redirects, auth, cookies, HSTS), on
  `std.net` and std.tls. The shim keeps its GIO state machines (its
  asynchronous model is GIO's), calling std for the rules.
- `auth.mlx` (797), `authmanager.mlx` (549): Basic (RFC 7617, the
  user:password made ISO-8859-1 when it can be) and Digest (RFC 2617: MD5
  and MD5-sess, qop=auth, the client nonce, nonce counts,
  Authentication-Info's nextnonce), the protection spaces of each, the
  manager's cache of auths by host and path and its retries. →
  `std.http.auth`.
- `cookie.mlx` (418), `cookiejar.mlx` (713), `tld.mlx`: cookies parsed
  from Set-Cookie with libsoup's rules (domains made to start with a dot,
  a domain needing a dot, the default path, Max-Age over Expires),
  written back as Set-Cookie and Cookie; the jar: cookies by domain, the
  ones that apply to a URI (secure, expiry, path, SameSite with top-level
  navigation and safe methods, HttpOnly), longest paths first then the
  oldest, the accept policies (always, never, no third party,
  grandfathered third party, by the public suffix's registrable domain),
  the refusals (public suffixes, secure cookies from http, `__Secure-`
  and `__Host-`, control characters, 4096 bytes), and the cookies.txt
  file (Mozilla's format, `#HttpOnly_`). std.http.cookie has the
  Set-Cookie syntax and the host and path matching only. →
  `std.http.cookie` (the whole cookie) and `std.http.cookiejar`.
- `hsts.mlx` (545): Strict-Transport-Security read strictly, policies
  (domain, max-age, expiry, subdomains), the enforcer's host and session
  policies, the super-domain walk, the rewrite of http:// to https://
  (port 80 to 443), the policies' file. → `std.http.hsts`.
- `multipart.mlx` (780): multipart bodies split at their boundaries as
  libsoup splits them and put together (form-data parts, generated
  boundaries), and the part stream over a filter stream's buffer. →
  `std.http.multipart` (whole bodies); the stream is GIO's shape.
- `websocket.mlx` (1530), `wsext.mlx` (489): the client's handshake
  request (key, version, origin, protocols, extensions offered), the
  server's check and response, the client's verification (Accept, the
  protocol chosen, the extensions accepted), the extensions' negotiation,
  permessage-deflate (RFC 7692: window bits, context takeover, the sync
  flush's 00 00 ff ff taken off and put back, the errors); the
  connection: frames queued and written, read and assembled, pings,
  close codes, the keepalive. std.http.websocket has keys, frame
  headers, masking and the UTF-8 check. → `std.http.websocket` (the
  handshake, a message assembler) and `std.http.websocket.deflate`.
- `headers.mlx` (1386): SoupMessageHeaders over std.http.headers, plus
  request lines, status lines and response headers parsed as libsoup
  parses them (HTTP/0.9 and 1.x versions, the Host header required, bad
  requests), Range and Content-Range, Content-Type's and
  Content-Disposition's parameters, the encodings (Content-Length,
  chunked, EOF) and expectations. → `std.http.message` (the lines) and
  `std.http.headers` (ranges, disposition).
- `misc.mlx` (468): dates, forms (std.http.date, std.http.form), the
  URI helpers (default ports, normalization, "host:port" for the Host
  header, data: URIs). → `std.uri`'s HTTP part.
- `server.mlx` (2181), `authdomain.mlx` (565), `pathmap.mlx`: an HTTP/1.1
  server on the GLib main context with handlers by path, auth domains
  (Basic, Digest), WebSocket upgrades. A server belongs in std one day
  (`std.http.server`); it is GIO-bound here and WebKit does not use it:
  drop with the shim.
- `logger.mlx` (557), `cache.mlx`, `metrics.mlx`: libsoup's log lines,
  an object without a cache, metrics. Drop.

(b) C shape: the GObject types (SoupMessage, SoupSession, the features,
the auths, the jars, the enforcer, the extensions), their properties and
signals, GBytes bodies, GIO streams, GUri, the asynchronous machinery on
GMainContext.

Plan: save into std.http in this order (the biggest reusable chunks
first): auth, cookies and the jar, HSTS, multipart, the WebSocket
handshake and deflate, redirects and message framing, then
`std.http.client` on `std.net`. Keep the shim at compiler/libc/soup
(needed by: the WPE port's NetworkProcess) with the GIO machinery only,
until it is retired with mlx-glib.

## gcrypt (compiler/capi/gcrypt, 7236 lines)

libgcrypt 1.10 over std.crypto, what WebCore's WebCrypto and PAL call.

(a) Logic not in std:

- `mpi.mlx` (1311): signed numbers of 64-bit limbs: scanning and printing
  in every format (STD, USG, PGP, SSH, HEX), addition, subtraction,
  multiplication, Knuth's division in both roundings, the modular
  operations, gcd, the inverse, bits and shifts. std.crypto.bignum is
  fixed-width Montgomery arithmetic for secret exponents (constant time);
  a general integer is missing. → `std.crypto.mpi`.
- `pk.mlx` (1734): RSA key generation (sieved random primes, Miller-Rabin,
  p < q, u = p^-1 mod q, d modulo the lcm), the private operation by the
  CRT, OAEP and PKCS #1 v1.5 encryption and decryption, PSS and PKCS #1
  v1.5 signing; ECDSA, EdDSA and ECDH are std.crypto's already. std.crypto
  has RSA verification only. → `std.crypto.rsa` (keys with their private
  halves, signing, encryption, generation).
- `ec.mlx` (1002): the curves under libgcrypt's names, contexts, point
  coding in SEC 1, EdDSA's and Montgomery's forms, affine results. The
  curve arithmetic is std.crypto's; the point coding of the Weierstrass
  curves is std.crypto.ecc's. The rest is libgcrypt's object model.
- `sexp.mlx` (1374): S-expressions in libgcrypt's tagged layout, read,
  built from C's variable arguments, printed, walked. libgcrypt's own
  format: stays.
- `md.mlx`, `cipher.mlx` (907): handles over std.crypto's hashes, HMAC,
  AES modes, GCM, the key wrap (std.aes). C shape.

(b) C shape: handles, gpg-error codes and texts, `gcry_control`, the
S-expressions, `gcry_buffer_t`.

Plan: save `std.crypto.mpi` and RSA's private operations into std.crypto;
keep the shim at compiler/libc/gcrypt (needed by: WebCore's
crypto/gcrypt and PAL's digests, through mlx-cc).

## tasn1 (compiler/capi/tasn1, 1839 lines)

libtasn1 4.19, the part PAL::TASN1 calls: definitions from asn1Parser's
arrays, DER decoded into a tree and read by names, structures written and
DER encoded.

(a) Logic not in std: a DER writer (lengths, tags explicit and implicit,
INTEGER made minimal, OBJECT IDENTIFIER from dotted text, BIT STRING with
its bit count, SET OF in DER's order). std.asn1 reads DER only. → a writer
in `std.asn1`. The definition-driven decoder is libtasn1's model (names
like `tbsCertificate.subject`, `?1`): WebCore's key formats (SPKI, PKCS #8,
ECPrivateKey, RSAPrivateKey) are better read and written directly in
`std.crypto` with std.asn1 when Voyager needs them.

(b) C shape: `asn1_node` trees, `ASN1_*` errors, `asn1_static_node` arrays.

Plan: keep the shim at compiler/libc/tasn1 (needed by: WebCore PAL); a DER
writer in std.asn1 when the key formats come.

## icu (compiler/capi/icu, 5246 lines)

ICU 74's C API over std.unicode: 907 lines of `export fn`s calling the
`pub fn` of the same name in std.unicode (an export cannot be called from
Mlx), the 4339 lines of `api/` being the declarations the headers are
made of. Nothing to move: the implementation is std.unicode's by design
(properties, casing, normalization, breaks, collation, IDNA, charsets,
bidi, search, converters, locales, calendars, the formatters).

Plan: keep the shim at compiler/libc/icu (needed by: WTF, JSC, WebCore
through mlx-cc; `./mlx build unicode` keeps its name).

## xml2 (compiler/capi/xml2, 6402 lines)

libxml2 2.9.14's push parser over std.xml.sax.

(a) Logic not in std: `tree.mlx` (740): documents with their subsets and
entity tables (the first declaration of a name kept), the predefined
entities, `xmlEncodeEntitiesReentrant` and `xmlEncodeSpecialChars`
(escaping for content and attributes), qualified names split, built and
validated (NCName, QName, Name, NMTOKEN by XML 1.0's classes). std.xml.sax
keeps entities for the parse but std has no escaping or name validation of
its own. → the escaping and the name validators into `std.xml.text`.

(b) C shape: `xmlParserCtxt` laid out as libxml2's with the std parser
after it, the SAX handler tables and their routing (`parser.mlx`,
`sax2.mlx`), the error routing and libxml2's stderr report (`errors.mlx`),
`xmlstring.h`, memory and dictionaries (`memory.mlx`), encoding handlers
over std.xml.encoding (`encoding.mlx`), the globals.

Plan: keep the shim at compiler/libc/xml2 (needed by: WebCore's
XMLDocumentParser); save the escaping and name validation into std.xml.

## freetype (compiler/capi/freetype, 5074 lines)

FreeType 2.13's records (FT_FaceRec, FT_SizeRec, FT_GlyphSlotRec,
FT_CharMapRec) pushed from std.truetype's objects after each call, the
library's modules and properties, the outline, bitmap, glyph and
fixed-point functions. Nothing to move: the engine is std.truetype.

Plan: keep the shim at compiler/libc/freetype (needed by: Skia's FreeType
font host, WebCore's FreeType code, mlx-harfbuzz's hb-ft).

## harfbuzz (compiler/capi/harfbuzz, 4646 lines)

HarfBuzz 8.3's C API: `exports.mlx` (2013, written by
tools/harfbuzz_exports.py) exports std.shaping's function of each name;
`ft.mlx` (850) is hb-ft over mlx-freetype's records. Nothing to move.

Plan: keep the shim at compiler/libc/harfbuzz (needed by: WebCore,
Skia's SkShaper).

## zlib (compiler/capi/zlib, 2227 lines)

zlib 1.3.1 over std.flate and std.crc.

(a) Logic not in std: `gzip.mlx` (1054): gzip files read through inflate
(the header looked for; files that are not gzip copied as they are;
concatenated members), written through deflate, with zlib's buffering,
positions, seeks (by reading or writing zeros forward), `gzgets`,
`gzungetc`, appending, the errors and their messages. std.flate has
`gunzip` of a whole buffer only. → `std.flate.gzip` (a reader and a writer
over a descriptor).

(b) C shape: `z_stream` and `gz_header` as C lays them out, `zalloc`/`zfree`,
the return codes, `gzprintf`'s variable arguments, `gzFile`'s visible
fields, the version and flag words.

Plan: save gzip files into std.flate; keep the shim at compiler/libc/zlib
(needed by: ANGLE, WebCore's CompressionStream, WOFF and the WebSocket
extension; also libpng's shim is over std.flate directly).

## png (compiler/capi/png, 3850 lines)

libpng 1.6.43 over std.png.

(a) Logic not in std: `writer.mlx` (870): the signature and the chunks of
the info in libpng's order (IHDR, gAMA, iCCP or sRGB, sBIT, cHRM, PLTE,
tRNS, bKGD, pHYs, tEXt and zTXt, the unknown chunks where they belong),
the filter chosen per row as libpng chooses it, libpng's zlib settings
(the window made small for small images and the stream's header told so)
and IDAT chunks of 8192 bytes. std.png.saveFile writes PNGs with a filter
choice of its own and one IDAT. → `std.png.write` making libpng's bytes,
`saveFile` on it.

(b) C shape: `png_struct` and `png_info` with their getters and setters,
the progressive callbacks and their order (`reader.mlx`, over std.png.read
and std.png.rows), `setjmp` errors, the user limits and options.

Plan: save the writer into std.png; keep the shim at compiler/libc/png
(needed by: WebCore's PNGImageDecoder, Skia's PNG codec and encoder).

## jpeg (compiler/capi/jpeg, 2780 lines)

libjpeg-turbo 2.1.5's libjpeg 8 API over std.jpeg (the decoder whole:
markers, Huffman decoding, the IDCT, upsampling, colour conversion).
`decompress.mlx` (806) is the global states, the data source, suspension,
buffered-image mode, cropping and skipping over std.jpeg; `records.mlx`,
`messages.mlx`, `sources.mlx`, `common.mlx` the structures, libjpeg's
message texts, the memory manager. `compress.mlx` (80) raises
JERR_NOT_COMPILED: neither std nor the shim has an encoder yet (Skia's
JPEG encoder would need one; that is new work, in std.jpeg first).

Plan: keep the shim at compiler/libc/jpeg (needed by: WebCore's
JPEGImageDecoder, Skia's JPEG codec).

## webp (compiler/capi/webp, 2228 lines)

libwebp 1.3.2 over std.webp (decoding, the incremental decoder, the
demuxer, the animation decoder, a lossless encoder, the muxer).
`encoding.mlx` (692) is libwebp's configuration checks, `WebPPicture`'s
memory, importers and views over std.webp.encode; the rest is layouts
and exports. Nothing to move.

Plan: keep the shim at compiler/libc/webp (needed by: WebCore's
WEBPImageDecoder, Skia's WebP encoder).

## lcms2 (compiler/capi/lcms2, 1311 lines)

Little CMS 2.14 over std.colorprofile: handles are std.colorprofile's
blocks. Nothing to move.

Plan: keep the shim at compiler/libc/lcms2 (needed by: WebCore's PNG and
JPEG decoders with USE_LCMS).

## sqlite3 (compiler/capi/sqlite3, 3036 lines)

SQLite 3.45's C API over std.sql: connections, statements, binds, columns,
application functions and collations, blob I/O. `printf.mlx` (230) is
SQLite's printf with `%q`, `%Q`, `%w` and `%z` over C's variable arguments;
std.sql's own `printf`/`format` SQL functions have the formatting. Nothing
to move.

Plan: keep the shim at compiler/libc/sqlite3 (needed by: WebCore's
SQLiteDatabase and the stores over it).

## pulse (compiler/capi/pulse, 10315 lines)

PulseAudio's client libraries (libpulse 16.1, libpulse-simple,
libpulse-mainloop-glib) and the ALSA pulse plugin, for programs built
against PulseAudio's own headers (no `api/`: the headers are the system's).

(a) Logic not in std: `sample.mlx`, `channels.mlx`, `volume.mlx`,
`format.mlx`, `proplist.mlx` (1974): sample specs (sizes, times, names),
channel maps (positions, names, the default maps, parsing and printing),
volumes (linear, dB, their text forms, scaling and averaging), format
infos (their JSON properties), property lists; `wire.mlx`, `core.mlx`,
`context.mlx`, `stream.mlx`, `introspect.mlx` (3467): the native
protocol's client on std.pulse's numbers and tagstructs (authentication,
streams with their buffer attributes, timing interpolated between the
server's replies, introspection, subscriptions). MLX Audio's server
(projects/desktop/audio) has its own copy of the value rules it needs; a
`std.pulse.values` would let both sides share them. A PulseAudio client in
std has no user of our own (our programs play through MLX Audio
directly): leave it.

(b) C shape: all of `pa_*`'s structures and callbacks, `c.mlx`'s C memory
and calls, the main loops (`mainloop.mlx`, `threaded.mlx`, `glib.mlx`),
`printf.mlx`, the ALSA ioplug and ctl_ext interfaces (`alsa.mlx`).

Plan: keep as shims at compiler/libc/pulse (needed by: third-party
PulseAudio clients of MLX Audio: Firefox, Chromium, SDL, mpv, GStreamer,
pavucontrol; ALSA programs through the plugin). The value arithmetic is a
candidate for `std.pulse` when the server is made to share it.

## pipewire (compiler/capi/pipewire, 6963 lines)

PipeWire's client library (libpipewire 1.0.5) for programs built against
PipeWire's own headers.

(a) Logic not in std: `properties.mlx` (372): `pw_properties` parsed from
`{ key = value }` strings and JSON and written back; `core.mlx`,
`proxy.mlx`, `context.mlx`, `objects.mlx`, `infos.mlx`, `stream.mlx`
(3722): the native protocol's client on std.pipewire's numbers and pods
(the core's handshake, the registry, proxies and their events, client
nodes with shared buffers and activation records). MLX Capture's server
is the other end of the same protocol with its own code; as with pulse, a
client in std has no user of our own. Leave it.

(b) C shape: `spa_hook` lists and `spa_interface` method tables
(`hooks.mlx`), the loops as SPA interfaces (`loop.mlx`, `thread_loop.mlx`),
the C memory of every object, the info structures (`infos.mlx`'s
layouts), `pw_log_level` as a data symbol, the stubs.

Plan: keep as a shim at compiler/libc/pipewire (needed by: third-party
PipeWire clients of MLX Capture: Firefox, Chromium, OBS, GStreamer's
pipewiresrc).

## The layout after the move

    compiler/libc/              mlxlibc (libmlxc.so) and, beside it, the shims:
    compiler/libc/LIB/api/      the C interface in Mlx (mlx-cheaders makes the headers)
    compiler/libc/LIB/*.mlx     the shim: C types <-> std calls
    compiler/libc/LIB/tests/    the C programs tools/check_LIB.sh compares with the system's library
    std/src/http/               auth, cookies, the jar, HSTS, multipart, the WebSocket handshake and deflate...
    std/src/crypto/             mpi, rsa with its private half
    std/src/flate/gzip.mlx      gzip files
    std/src/png/write.mlx       libpng's writer

The build targets keep their names (`./mlx build glib`, `soup`, `unicode`,
`zlib`, ...), as do the check scripts (`tools/check_LIB.sh`) and the
libraries' sonames.
