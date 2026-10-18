# WebCore for WPE: what it needs, how far mlx-cc gets

WebCore is the next library of WPE WebKit after JavaScriptCore. This
page says what it (and WebKit and WPEPlatform above it) needs from
libraries outside WebKit, which of them this repository has in Mlx
already, which can be configured away, and how far mlx-cc compiles
WebCore's sources so far. It is the first phase: the inventory and the
compiler's pass; the libraries missing are written after it.

    ./mlx build webkit-wpe-webcore    WebCore's unified sources and the others for WPE (libWebCore.a)

The target is not part of `./mlx build webkit` yet: most of WebCore's
sources do not compile (below). It compiles what the reference build
compiles for WPE: the unified sources of WebCore's source lists
(`Sources.txt`, `SourcesWPE.txt`, `platform/Sources*.txt` of GLib,
Soup, Skia, GCrypt, Adwaita), the generated sources (webcore.mlx), and
the sources the build compiles apart (`lists/webcore-sources.txt`),
each C++ unit with `WebCorePrefix.h` first (`-include`, the reference
build's precompiled header), with the reference build's defines
(`WEBCORE_DEFINES`), its include directories (`lists/webcore-includes.txt`)
and the forwarding headers of `<WebCore/...>`
(`lists/webcore-headers.txt`, made under the port's generated
`WebCore/PrivateHeaders`).

## The configuration

The reference build is WPE 2.54.1's CMake build with C_LOOP, Skia, the
headless WPEPlatform, no video, web audio, WebRTC, media stream,
GStreamer, WebGPU, GPU process, spell checking, speech synthesis, AVIF,
JPEG XL, WOFF2, libdrm, GBM, Wayland, DRM platform, ATK, journald,
bubblewrap, sysprof, libbacktrace, documentation or introspection; with
WebGL (ANGLE), XSLT, Little CMS and PDF.js. `cmakeconfig.h` is
`config.WPE` (config.mlx).

What `OptionsWPE.cmake` requires whatever the options: GLib (with GIO),
HarfBuzz with its ICU part, ICU, libjpeg, libepoxy, libgcrypt, libsoup
3, libtasn1, xkbcommon, libxml2, libpng, SQLite, libwebp (with demux),
zlib and FreeType; Skia adds Fontconfig. What options take away:
libxslt (`ENABLE_XSLT=OFF`), Little CMS (`USE_LCMS=OFF`), and WebGL's
EGL and GL through libepoxy for WebGL itself (`ENABLE_WEBGL=OFF`; the
compositor's EGL stays unless Skia renders on the CPU).

## The libraries

The functions are those each library exports (`nm -D` of the system's
build) that WebCore (with PAL) or WebKit (with WPEPlatform) name: the
identifiers of the files the reference build compiles for WPE (the
members of its unified sources included) and of their headers. Skia and
ANGLE (ThirdParty, built by this repository already) are counted apart
where they call a library themselves. The units are those of the
reference build's WebCore units that include one of the library's
headers (`g++ -M` with the reference build's flags, 644 units).

| Library | Needed for | Functions: WebCore / WebKit (/ Skia) | Units including it | Existing Mlx code | Plan, priority |
|---|---|---|---|---|---|
| GLib, GObject, GIO | WTF's run loop and threads (done); GIO streams, files, D-Bus, network monitor, proxies, resolver, TLS types (libsoup's headers name GTlsCertificate) in WebCore's soup and GLib platform code; WebKit's whole GObject API, IPC, WPEPlatform | 173 / 502 (571 together) | 644 (all: WTF's headers) | mlx-glib (compiler/capi/glib): 397 of the 571; missing 50 of WebCore's (GDBus, GResolver, GProxyResolver, GNetworkMonitor, GTls*, memory streams) and 124 more of WebKit's | 1: extend mlx-glib, WebCore's 50 first; the GIO TLS types are needed to compile any unit (libsoup's headers) |
| libsoup 3 | HTTP: every network load (NetworkProcess), cookies, HSTS, authentication, proxies, the public suffix list (WebCore's soup platform code, WebKit's NetworkProcess/soup) | 82 / 75 (143) | 643 (ResourceRequest/Response headers, in the prefix) | none (no HTTP in std) | 1: mlx-soup, a C API layer over a new std HTTP client: HTTP/1.1 first, cookies, HSTS; TLS through GIO's TLS (a TLS implementation is the largest part) |
| ICU | text: breaking, collation, normalization, bidi, encodings, search, IDNA | 65 / 6 (66) | 643 | mlx-unicode (compiler/capi/icu): 49 of the 66; missing `ubidi_*` (6: bidi of text runs), `usearch_*` (10: find in page, TextIterator), `ucnv_close` and the headers `ubidi.h`, `usearch.h`, `ucnv.h`, `ucnv_cb.h`, `ucnv_err.h` | 1: add the bidi algorithm (UAX #9) and string search to std.unicode, and the headers; the converters exist already |
| HarfBuzz (+hb-icu) | text shaping (FontCascade, Skia's) | 46 / 0 (/ 57) | 643 | mlx-harfbuzz (compiler/capi/harfbuzz, std.shaping): all 46 and Skia's 57 declared | done (to be checked at run time) |
| FreeType | font files, glyphs (Skia's font host) | 0 / 0 (/ 52) | 0 | mlx-freetype (compiler/capi/freetype, std.truetype): Skia's 52 declared | done for compiling |
| Fontconfig | finding fonts: Skia's font manager, WebCore's FontCache (FreeType platform) | 21 / 0 (/ 53) | 2 | none | 2: mlx-fontconfig, matching over the fonts directories (fonts.conf's essentials) |
| libxml2 | XML documents (XHTML, SVG as XML) parsed by XMLDocumentParserLibxml2; XSLT's input | 59 / 4 (59) | 10 | std.xml: a pull tokenizer, a base | 2: mlx-libxml2: the SAX2 push parser WebCore drives (xmlCreatePushParserCtxt, xmlParseChunk, the callbacks), entities, encodings through ICU; its headers need `<iconv.h>`, which mlxlibc lacks |
| libxslt, libexslt | XSLT processing (XSLTProcessor, `<?xml-stylesheet?>`) | 26 / 0 | 5 | none | 3: `ENABLE_XSLT=OFF` |
| SQLite | storage: IndexedDB's SQLite back end, local storage, the icon and resource load statistics databases | 55 / 19 (60) | 13 | none | 2: the largest of the "small" libraries; a SQL engine of the subset WebKit's statements use, or the stores on another back end |
| libpng | PNG decoding (ImageDecoder) and Skia's PNG codec | 36 / 0 (/ 47) | 3 | std.png: a decoder and an encoder | 2: mlx-libpng over std.png |
| libjpeg | JPEG decoding (ImageDecoder), Skia's JPEG codec and encoder | 15 / 0 (/ 25) | 2 | std.jpeg: an encoder only | 2: a baseline and progressive decoder in std.jpeg, then mlx-libjpeg |
| libwebp (+demux, mux) | WebP decoding (ImageDecoder), Skia's codec | 7 / 0 (/ 12) | 2 | none | 3: VP8 and VP8L decoding in std, then the C API |
| Little CMS 2 | colour management of images (ICC profiles) | 7 / 0 | 4 | none | 3: `USE_LCMS=OFF` |
| libepoxy (GL, EGL) | WebGL (through ANGLE), the compositor's EGL images and contexts | 118 / 51 (129), and ANGLE's own entry points | 39 (+ EGL's 6) | none (std.gpu is Vulkan) | 3: CPU rendering first (Skia's raster back end); then an EGL/GLES layer, or `ENABLE_WEBGL=OFF` |
| libgcrypt (+gpg-error) | WebCrypto (AES modes, RSA, EC, HMAC, HKDF, PBKDF2, digests) and PAL's CryptoDigest | 71 / 0 | 8 | std: sha1, sha2, md5, blake2, AES-128 single blocks, crc | 2: the WebCrypto algorithms in std, then mlx-gcrypt (digests first: PAL needs them everywhere) |
| libtasn1 | WebCrypto key import and export (DER of SPKI, PKCS#8) | 8 / 0 | 1 | none | 3: a small DER reader and writer |
| zlib | compression streams (CompressionStream), WebSocket deflate, Skia's PNG | 9 / 1 (9 / 8) | 7 | mlx-zlib (compiler/capi/zlib, std.flate): all declared | done |
| xkbcommon | keyboard: WPEPlatform's keymaps and key events | 0 / 22 | 0 | std.xkb | 3: a C API layer over std.xkb when WPEPlatform is built |
| gettext (`libintl.h`) | GLib's `gi18n-lib.h` (the `_()` of WebKit's GLib code) | | | mlxlibc has no `libintl.h` | 1: mlxlibc's `libintl.h`, gettext returning its message id (tiny) |
| expat, libpsl, brotli, libatomic | expat: Skia on Android only; libpsl, brotli: libsoup's own; libatomic: GCC's atomics | 0 | 0 | | not needed by WebKit directly |

So the order: the headers and small layers every unit needs (GIO's TLS
types, `libintl.h`, `iconv.h`, ICU's bidi, search and converter
headers), then libsoup and its HTTP and TLS, the image decoders,
libxml2, SQLite, Fontconfig, WebCrypto; XSLT and Little CMS are
configured away.

## The compiler's pass

mlx-cc compiles WebCorePrefix.h alone (about 180,000 lines preprocessed
and 3.3 GB of mlx-cc's 8 GB arena) since this pass. It needed these
fixes, each with a program in compiler/cc/tests/cpp:

| mlx-cc | Test |
|---|---|
| up to 1024 include directories (WebCore has 330) | |
| `<source_location>`, `__builtin_LINE()` and the others where the call is | sourcelocation |
| member templates of an instance specialized, conversions to a template-id | memberspecialization |
| C++17 aggregates with base classes | aggregatebases |
| argument-dependent lookup joining the function templates of two namespaces, and through a pack's elements | adltemplates, adlpacks |
| class template argument deduction from a braced list (initializer_list guides first), for static data members, and `X x = { ... }` | ctadlist, staticctad |
| a variable template specialized by its qualified name | qualifiedvariablespecialization |
| abbreviated function templates named by operators, and friends | abbreviatedoperators |
| a trailing requires-clause seeing the parameters; float members of constant objects in constraints | trailingrequires, requiresmember |
| template parameters of a class type (C++20), of a function type named by a typedef | classvalueparams, functiontypeparam |
| `auto` results with no value returned | autovoid |
| using-declarations from several bases as one overload set | usingoverloads |
| member initializers' braces for an aggregate member | memberbraces |
| opaque enum declarations in their own scope | opaqueenums |
| joining overload chains without cutting any (WebCore lost BoundaryPoint's operator==) | joinedoverloads |
| `#include` passing over directories of the header's name (`css/typedom/numeric`) | includedirectory |
| a member function joining only member functions' chain, not a nested class's friend's | nestedfriends |
| an alias template's pack parameter given no arguments (`sizeof...`, `Ps...`) | aliaspacks |
| members' constraints of instances for classes not defined yet kept, not weighed | requiresincomplete |
| `T::member` for a value parameter; constrained templates chosen over unconstrained | valuequalified |
| no bodies for functions named only in unevaluated operands | unevaluatedbodies |
| function templates defined under a namespace's qualified name | nsqualtemplate |
| an explicit specialization's result class named, not made; a function template's in its signature too | specdecl, membertemplateresult |
| a lambda in a lambda naming the static locals further out | nestedlambdastatic |
| `T[N]` in a pattern not needing T whole (std::span of a class being defined) | arrayofincomplete |
| `auto` template parameters deduced from a class's value | autoclassparam |
| an alias template named in a pattern expanded once per place (25 million of a unit's 38 million tokens were such copies) | |

and in the libraries: mlxlibcpp's `std::ranges` algorithms WebCore
calls (rangesalgorithms), a `std::optional` usable in constant
expressions (optionalconstexpr) and `std::pair`'s `<=>`
(pairthreeway); mlx-unicode's block numbers as ICU's
(`UBLOCK_*`, which WebCore's CharacterProperties checks).

## How many compile

The target itself stops at the first header of a library this
repository does not have (libsoup's, which WebCore's resource
request and response headers include, in every unit). To see past them
the survey compiled with the system's headers of the missing libraries
after all others (and the system's GLib headers, for GIO's TLS types),
with ICU's missing C headers and a stand-in `libintl.h`: that is, the
category (a) failures taken out where a header alone does it.

WebCore for WPE is 518 unified sources (5,533 files) and 107
sources compiled apart. Each unit costs mlx-cc about 40 seconds and 3.3
GB for WebCorePrefix.h alone, so the survey compiled a sample: the first
file of every tenth unified source, 52 files from all of WebCore's
directories (bindings, Modules, accessibility, animation, css, dom,
editing, html, inspector, layout, loader, page, platform, rendering,
style, svg, ...), one at a time, each alone.

| | Files |
|---|---|
| compile | 28 of 52 |
| (a) a header of a missing library | 1: libxml2's `<iconv.h>` (mlxlibc has none) |
| (b) mlx-cc's memory: the 8 GB arena or the 4 million names | 8: units of style values, layout, CSS parsing and the inspector, heavy with WTF's Variant (mpark) and the style types' templates |
| (b) mlx-cc errors | 15, below |

| mlx-cc error | Files | Where |
|---|---|---|
| `expected ';'` reading `typename Converter<IDL>::ReturnType` of ConversionResult made too early (other paths than the two fixed) | 3 | JSDOMConvertResult.h:159 |
| `expected ';'` in StylePrimitiveNumeric.h:126 | 3 | style values |
| an explicit specialization after an instance made too early (`MarkableTraits<GlyphOverflow>`: "the instance exists already") | 2 | FontCascadeFonts.h:92 |
| `expected ','` | 2 | SVGPropertyOwnerRegistry.h:72 |
| WTF::switchOn's visitor taking `T&&` of a moved Variant: the exhaustiveness check, `invoke` | 2 | Variant.h:1415, 1340 |
| mpark::variant's converting constructor (`"none"_s` for a Variant with String) | 1 | CSSPerspective.cpp |
| ElementChildIterator's constructor | 1 | LinkIconCollector.cpp |
| `expected a function template` | 1 | StyleValueTypes.h:513 |

Also seen: `std::is_invocable` of a function taking `int&&` holds for
an `int&` argument in mlx-cc (it does not in C++), which the visitors
above may meet.

## What is next

- The headers every unit needs before any library is written: GIO's
  TLS types in mlx-glib, `libintl.h` and `iconv.h` in mlxlibc, ICU's
  `ubidi.h`, `usearch.h` and `ucnv*.h` in mlx-unicode. With them
  `./mlx build webkit-wpe-webcore` gets as far as the survey's
  stand-ins do.
- mlx-cc's memory: a heavy unit (style values, layout, the bindings of
  big interfaces) fills the 8 GB arena or the 4 million names with
  instances of WTF's Variant (mpark) and of the style types' templates.
  Each instance keeps its tokens and its mangled names; sharing them (the
  way an alias's expansion is shared now) or dropping a failed
  candidate's is the next step there.
- The remaining compiler errors of the survey, most common first.
- Then the libraries in the order above, libsoup's HTTP and TLS being the
  largest.
