# PlatformMlx: WebKit's port to Mlx

Voyager, our browser, first runs on WebKit, the reference engine. Our
own engine, Lattice, comes later: WebKit translated to Mlx by the C++ to
Mlx translator, then reshaped. This page is the design of the WebKit port
that comes first, **PlatformMlx**: what WebKit 2.54.1's sources
(`WEBKIT_SOURCE`, by default /home/user/csrc/wpe/wpewebkit-2.54.1) are
built as, what the port supplies, in what order, and what of this
repository's older plan (the WPE port on C library emulations) it retires.

## The principle: not a fork

WebKit's sources stay as they are. No WebKit file is patched, copied in
modified form or kept on a branch: WebKit is the bootstrap of Voyager,
and its daughter Lattice takes over from it, so all work done here must
be work on *our* side of WebKit's platform abstractions. The port is made
of these alone:

- **The build description** (projects/webkit/build.mlx and its modules):
  which of WebKit's files are compiled, in which source lists, with which
  defines and include directories, and the generators' outputs
  (generators/, already WebKit's to the byte).
- **The configuration**: the port's `cmakeconfig.h` lines and feature
  defines (config.mlx), the `USE_*`, `ENABLE_*` and `HAVE_*` choices a
  CMake port makes in its `Options<PORT>.cmake`.
- **The port's own sources** (projects/webkit/port/, to come): C++ files
  that implement what WebKit leaves to a platform (a `ResourceHandle`, a
  `FontCache`'s platform half, `GraphicsLayer::create`, image decoders
  of WebKit's decoder classes, the embedder's clients), and the headers
  WebKit expects a port directory to have (`ResourceRequest.h`,
  `ResourceResponse.h`, `CertificateInfo.h` of `platform/network/<port>`).
- **Mlx**: the standard library (std.http, std.tls, std.png, std.unicode
  and the rest), reached from the port's C++ through `export` functions
  with the C ABI that the port's headers declare `extern "C"`; and the
  Mlx side of the port (projects/webkit/port/*.mlx, one plugin, to come)
  where std needs a C-callable face shaped for WebKit.

Four ways to choose without patching, in this order of preference:

1. **Configuration**: a `USE_*`/`ENABLE_*` off where WebKit supports it
   (video, WebRTC, XSLT), on where WebKit's generic code is enough.
2. **Source selection**: compiling WebKit's generic files and the
   platform files of another port that fit (Skia's graphics, the BMP, GIF
   and ICO decoders, the Adwaita theme, the PlayStation port's
   accessibility stubs, `platform/unix`), and not compiling the ones that
   do not (the soup network code, `FontCacheSkia.cpp`).
3. **Same-name replacement**: a port file defining the functions or the
   class WebKit's generic code names, where WebKit's own definition is a
   file we do not compile (`PNGImageDecoder.h`/`.cpp` of our own over
   std.png, found first on the include path; `FontCache`'s platform
   functions in `FontCacheMlx.cpp`). The replaced class's header stays
   WebKit's whenever other WebKit files include it.
4. **An open point**, written below, where an abstraction cannot be met
   that way. Nothing is patched to get past it.

## Where it stands

- **M1, done**: the JSCOnly port's jsc (`./mlx build webkit-jsc-shell`,
  mlx-out/webkit/bin/jsc) runs on mlxlibc alone. Its interpreter is
  mlxlibc's loader (`mlx-out/libc/libmlxc.so.1`, `Port.mlxlibc` in
  build.mlx), `libc.so.6` is that loader, `libm.so.6` mlxlibc's beside
  it, mlxlibcpp its C++ library, mlx-unicode its ICU; a running jsc maps
  nothing of glibc. What it took (tools/check_jsc_mlxlibc.sh,
  tools/check_regex.sh):
  - `regcomp`/`regexec` (JavaScriptCore's configuration files,
    runtime/ConfigFile.cpp): the last C library functions jsc took from
    glibc. mlxlibc has POSIX regular expressions now
    (compiler/libc/regex.mlx), and mlx-cc's `<regex.h>` glibc's layout.
  - `pthread_getattr_np` of the main thread: mlxlibc described a stack
    at address 0, so WTF's `StackBounds` gave JavaScriptCore a limit that
    every script overflowed before its first statement. It describes the
    kernel's stack now, as glibc does.
  - `ldexpl` was already there (the x87 page of the loader); threads,
    TLS, `dlopen`, signals, time, `mmap`/`madvise`, `sysconf`,
    `getauxval` and the rest were enough as they were.
  - The loader looks for libraries in its own directory (after
    `LD_LIBRARY_PATH`, before the system's), where `libm.so.6` is.

  The test scripts (projects/webkit/tests/jsc: Intl as node prints it,
  the core language, number formatting, a stress of the runtime, sorting,
  typed arrays) print what the glibc-linked jsc printed, byte for byte,
  also with `--collectContinuously=true`, `--useConcurrentGC=false` and
  `--useSamplingProfiler=true` (the collector's thread suspending the
  main thread with signals). One known problem is the same on both C
  libraries and not mlxlibc's: `$262.agent` (several threads running
  scripts) crashes jsc.
- WebCore: about 35 of 52 sampled files compile with WPE's flags
  (WEBCORE.md); the WPE targets (`webkit-wpe-*`) build WTF and
  JavaScriptCore on mlx-glib.
- **M2, under way**: the port is described and builds. `config.MLX`
  (JSCOnly's configuration with WebCore's choices above), the source
  selection (webcore.mlx's `mlx()`: Sources.txt, the Skia and Adwaita
  lists, lists/webcore-mlx-sources.txt of other ports' files that fit,
  port/SourcesMlx.txt, without lists/webcore-mlx-skipped.txt), what
  WebCore's build generates for those feature defines
  (`webkit-mlx-webcore-derived`), and the targets: `webkit-mlx-pal`
  (libPAL.a, built), `webkit-mlx-webcore` (libWebCore.a),
  `webkit-mlx-port` (libmlxwebport.so, built) and
  `webkit-mlx-webcore-test`, the program of tests/webcore-mlx.cpp
  linked with all of them on mlxlibc (tools/check_webkit_mlx_port.sh).
  The port's sources are in projects/webkit/port (below: what each
  replaces); those that only stand in for later milestones say so with
  a `// STUB (Mn: ...)` line, which the check lists. What remains is
  mlx-cc's: the units it does not compile yet are listed in WEBCORE.md,
  each with its first error.

## The shape of the port

### The base: JSCOnly, grown to WebCore

WebKit's ports are `Options<PORT>.cmake` plus `Platform<PORT>.cmake`
files. The tarball has JSCOnly, WPE (and GTK's, Cocoa's, Haiku's pieces);
WinCairo's and PlayStation's CMake files are gone, though some of
PlayStation's platform sources are still there.

PlatformMlx builds as **JSCOnly**: `BUILDING_JSCONLY__`, for which
WTF's PlatformLegacy.h defines no `PLATFORM()` at all ("JSCOnly does not
provide PLATFORM() macro"), and bmalloc's BPlatform.h its own case. So no
`PLATFORM(GTK)`, `PLATFORM(WPE)` or `PLATFORM(COCOA)` branch of WebKit is
taken, and every choice of the port is a `USE_*`, `ENABLE_*` or `HAVE_*`
of its configuration. JSCOnly's CMake build stops at JavaScriptCore;
PlatformMlx goes on to PAL and WebCore with source lists of its own
(lists/webcore-mlx*.txt, made from `Sources.txt` and the backend lists of
`platform/Sources*.txt` that fit, as `PlatformHaiku.cmake` does: the one
port in the tarball that builds WebCore without GLib).

| | Choice | Why |
|---|---|---|
| Event loop | `USE_GENERIC_EVENT_LOOP` (RunLoopGeneric.cpp, WorkQueueGeneric.cpp, MainThreadGeneric.cpp; JSCOnly's lists/wtf.txt) | No GLib; the generic loop is WTF's own code, timers included |
| Graphics | `USE_SKIA`, CPU raster; no `USE_CAIRO` | Skia compiles here already (`webkit-skia`); WebCore's only living 2D back end besides Cairo (GTK/WPE moved to Skia); CPU first, no EGL |
| Compositing | `USE_TEXTURE_MAPPER` and `USE_COORDINATED_GRAPHICS` off; accelerated compositing off in the Page's settings | TextureMapper is GL; a view painted whole into a buffer needs no layers at first |
| Network | neither `USE_SOUP` nor `USE_CURL` | A load is the embedder's (`LoaderStrategy`), over std.http and std.tls |
| Images | WebCore's `ScalableImageDecoder`, with our PNG, JPEG and WebP decoders; WebKit's BMP, GIF, ICO as they are | WebCore's decoders call libpng, libjpeg, libwebp; ours call std.png, std.jpeg, std.webp |
| Colour | `USE_LCMS` off, colour management in our decoders (std.colorprofile) | Little CMS is only the decoders' |
| Fonts | Skia's typefaces and our font manager over std.fontconfig | below |
| Text | `USE_ICU` through mlx-unicode | below |
| Theme | `USE_THEME_ADWAITA` (RenderThemeAdwaita, ScrollbarThemeAdwaita: GraphicsContext drawing; their GLib resources under `USE(GLIB)`) | WebKit's own non-native theme |
| Crypto | PAL's digests ours (std.sha1, std.sha2: `port/pal/CryptoDigestMlx.cpp`); WebCrypto, which 2.54 always compiles (there is no `ENABLE_WEB_CRYPTO` any more), registers the digests only and refuses the rest until M7 (`port/crypto/CryptoMlx.cpp`), then `crypto/mlx` ours over std.crypto, std.asn1, std.x509 | no libgcrypt, no libtasn1 |
| Off | `ENABLE_VIDEO`, `ENABLE_WEB_AUDIO`, `ENABLE_MEDIA_STREAM`, `ENABLE_WEB_RTC`, `ENABLE_ENCRYPTED_MEDIA`, `ENABLE_WEBGL`, `ENABLE_WEBXR`, `ENABLE_GAMEPAD`, `ENABLE_SPEECH_SYNTHESIS`, `ENABLE_SPELLCHECK`, `ENABLE_XSLT`, `ENABLE_DRAG_SUPPORT` (until M5), `ENABLE_REMOTE_INSPECTOR`, `ENABLE_NOTIFICATIONS`, `ENABLE_GPU_PROCESS`, `USE_GSTREAMER`, `USE_ATSPI`, `USE_LIBWPE`, `USE_GBM`, `USE_LIBDRM`, `USE_AVIF`, `USE_JPEGXL`, `USE_WOFF2` | media, devices and accessibility come after the page itself |
| JavaScript | `ENABLE_C_LOOP` (no JIT), as now | the JIT's code needs offlineasm's x86 back end and mlx-cc's inline assembly; later |

### WTF, bmalloc, JavaScriptCore

As M1 has them: JSCOnly's sources (lists/wtf.txt: `generic/`,
`posix/`, `unix/`, `linux/` memory status), on mlxlibc. `FileSystemPOSIX`,
`ThreadingPOSIX` (signals for thread suspension: they work on mlxlibc),
`MemoryPressureHandlerUnix`, `LanguageUnix`, `TimeZone.cpp` with mlx-unicode
and std.timezone, `TextBreakIteratorInternalICUUnix`.

**The main loop.** WebCore and the embedder run on the main thread's
`RunLoop` (RunLoopGeneric: a condition variable, a tree of timers). The
port's I/O (the Wayland socket, network sockets, file watchers) is waited
for by one thread of the port's Mlx side on std.linux's epoll, timerfd
and eventfd, which hands each ready source to the main thread with
`RunLoop::main().dispatch()`. That is the pattern WebKit's own socket
code uses for generic loops (`RunLoop::setWakeUpCallback` with
`RunLoop::cycle()`, RemoteInspectorSocketEndpoint.cpp), turned around:
the generic loop stays WebKit's, unchanged. A main loop on epoll itself
(a `RunLoop` of our own with the generic loop's members) can replace it
if the extra hop shows in the frame times.

### ICU: mlx-unicode is WebKit's ICU

WTF, JavaScriptCore and WebCore call ICU's C API in hundreds of places
(`ubrk_*` for every line, word and grapheme break, `ucol_*`, `unorm2_*`,
`ubidi_*`, `ucnv_*` for TextCodecICU, `usearch_*`, `uidna_*`, the
`u_*` character properties, Intl's formatters). Wrapping them means
patching all of them; the API they call is already Mlx: mlx-unicode
(compiler/libc/icu) is ICU's C API implemented over std.unicode and
std.timezone, its Intl output identical to node's. So **mlx-unicode is
the port's ICU**, kept as it is: it is an export surface of std.unicode
in ICU's names, not an emulation of a C library underneath. What WebCore
asks of it beyond JavaScriptCore (`u_strToTitle`, the `uset_*` of
SmartReplace) is added there. It goes away with WebKit, when the
translator turns those calls into std.unicode calls in Lattice.

### Graphics: Skia on the CPU, a page painted whole

WebCore paints through `GraphicsContext`, here `GraphicsContextSkia`
(platform/graphics/skia, compiled as WebKit has it), into a Skia raster
`SkSurface`. The embedder paints the `LocalFrameView` into a surface
made over the pixels of a wl_shm buffer of our compositor's surface
(std/src/wayland's client), then damages and commits it.

With accelerated compositing off in the Page's settings, the
`RenderLayerCompositor` makes no `GraphicsLayer`s and the view paints
through the layer tree in software, as WebKitLegacy's non-composited
ports did. WebCore still links a `GraphicsLayer::create` (each port has
one: GraphicsLayerTextureMapper.cpp, GraphicsLayerCoordinated.cpp); the
port's is `GraphicsLayerMlx.cpp`, a layer that composites nothing. Later,
for scrolling and animation off the main thread, layers composited by
Skia on the CPU: 2.54's `SkiaCompositingLayer` does this for coordinated
graphics, which is WebKit2's; for a single process our own
`GraphicsLayer` over Skia (M8).

Encoding images (`toDataURL`, `toBlob`): `ImageUtilitiesSkia.cpp` is not
compiled (it calls Skia's encoders, which call libpng, libjpeg and libwebp,
and includes GLContext.h); `ImageUtilitiesMlx.cpp` defines
`platformEncodeData` over std.png, std.jpeg and std.webp (a stub until
M4). Skia is to be built for the port without its codecs and encoders
(lists/skia-mlx.txt, M4), so that Skia needs no image library at all; M2
links webkit-skia's libSkia.a as it is, whose codecs the port's sources
do not reach.

### Images

`ImageDecoder::create` (graphics/ImageDecoder.cpp) ends in
`ScalableImageDecoder::create`, which picks a decoder by the data's
signature among `BMPImageDecoder`, `GIFImageDecoder`, `ICOImageDecoder`,
`JPEGImageDecoder`, `PNGImageDecoder`, `WEBPImageDecoder`, including
each by its name. BMP, GIF and ICO are WebKit's own C++ readers:
compiled as they are. PNG, JPEG and WebP are the port's own classes of
the same names (port/image-decoders/PNGImageDecoder.h and .cpp, and the
others), found first on the include path, subclasses of
`ScalableImageDecoder` as WebKit's are, decoding incrementally with
std.png, std.jpeg and std.webp and their ICC profiles with
std.colorprofile; WebKit's png/, jpeg/ and webp/ directories are not
compiled. ImageBackingStoreSkia.cpp is compiled as it is.

### Fonts and text

`FontCacheSkia.cpp` and `SkiaSystemFallbackFontCache.cpp` call
Fontconfig directly (`FcConfig`, `FcPattern`); `FontPlatformDataSkia`,
`FontSkia`, `ComplexTextControllerSkia` and `SkiaHarfBuzzFont` call
HarfBuzz; Skia's typefaces (`SkFontHost_FreeType`) call FreeType.

- **Matching** (which font for a family, a style, a character): the
  port's `FontCacheMlx.cpp` defines what FontCacheSkia.cpp defines of
  `FontCache` (`systemFallbackForCharacterCluster`,
  `createFontPlatformData`, `lastResortFallbackFont`, the family
  lists), over std.fontconfig (Fontconfig 2.15's matching, in Mlx), and
  an `SkFontMgr` of the port's that opens the files std.fontconfig
  names. `FontCache.h`'s Skia members stay as they are.
- **Glyphs** (outlines, metrics, hinting, rasterizing): Skia's FreeType
  typefaces on mlx-freetype at first (it is compiled that way already);
  then an `SkTypeface` and `SkScalerContext` of the port's over
  std.truetype, and mlx-freetype goes.
- **Shaping**: ComplexTextControllerSkia.cpp on mlx-harfbuzz
  (std.shaping) at first; then the port's
  `ComplexTextController::collectComplexTextRunsForCharacters` over
  std.shaping (open point: `SkiaHarfBuzzFont.h` is included by
  FontPlatformData's Skia half, so `hb_font_t` stays in a header WebKit
  shares; the replacement must keep it compiling, or keep mlx-harfbuzz).
- **System fonts** (`SystemFontDatabase`): the port's
  `SystemFontDatabaseMlx.cpp`, as PlayStation's
  SystemFontDatabasePlayStation.cpp is for its port.
- Text breaking, bidi, case and normalization: ICU's API, mlx-unicode.

### Network

`ResourceHandle.cpp` is a stub for soup and curl (`ASSERT_NOT_REACHED`
under `USE(SOUP) || USE(CURL)`): those ports load in WebKit2's network
process (NetworkDataTaskSoup, NetworkDataTaskCurl), not in WebCore. With
neither, PlatformMlx defines the platform half itself:

- `platform/network/mlx` (the port's): `ResourceRequest.h`,
  `ResourceResponse.h`, `ResourceError.h`, `CertificateInfo.h`,
  `AuthenticationChallenge.h`, `CredentialStorage`'s and
  `NetworkStorageSession`'s platform halves, as the soup directory has
  them for soup, made of the generic `*Base` classes.
  `ResourceHandleMlx.cpp` defines `ResourceHandle`'s platform functions.
- Loading is the embedder's `LoaderStrategy`
  (`platformStrategies()->loaderStrategy()`: `loadResource`,
  `loadResourceSynchronously`, `setDefersLoading`, `remove`,
  `pageLoadCompleted`, ...), as WebKit2's WebLoaderStrategy and
  WebKitLegacy's WebResourceLoadScheduler are theirs: it makes the
  `SubresourceLoader`, runs the request on the Mlx side with std.http and
  std.tls (HTTP/1.1, then HTTP/2), cookies
  with std.http's cookie jar and public suffix list, and gives the
  response and the data back through `ResourceLoader::didReceiveResponse`,
  `didReceiveData`, `didFinishLoading` on the main thread. `data:` URLs
  are WebCore's (DataURLDecoder), `file:` the port's (std.fs), first.
- Not soup, not curl: platform/network/soup is not compiled, nor
  `platform/soup`, `platform/network/glib`.

### Storage

WebCore's `SQLiteDatabase` and `SQLiteStatement` are written on
SQLite's C API (`sqlite3*` members in SQLiteDatabase.h, which every
storage file includes). **mlx-sqlite3** (compiler/libc/sqlite3, over
std.sql) stays the port's SQLite: a minimal ABI of 59 functions WebKit
calls, implemented in Mlx; replacing the class means replacing a header
WebKit shares. IndexedDB, local storage and the cache come at M7.

### XML

`XMLDocumentParser.h` includes `<libxml/tree.h>` and keeps an
`xmlParserCtxtPtr`: the XML parser of WebCore is libxml2's SAX2 client,
in a header other WebCore files include. **mlx-xml2** (std.xml's SAX
behind libxml2's API) stays for the WebKit stage; XSLT is off.

### Compression, WOFF, crypto

`Modules/compression` (ZStream), WebSocketDeflater and
WOFFFileFormat.cpp call zlib's streams: mlx-zlib (std.flate) stays while
those files are WebKit's. PAL's `CryptoDigest` is the port's
(`CryptoDigestMlx.cpp` over std.sha1, std.sha2, std.md5, in place of
`pal/crypto/gcrypt`). WebCrypto (M7): `crypto/mlx/*` of the port, the
`CryptoAlgorithm*::platform*` functions over std.crypto, std.aes, std.asn1
and std.x509, as crypto/gcrypt and crypto/openssl are for their ports.

### Everything else a port supplies

| What | PlatformMlx |
|---|---|
| MIME types | `MIMETypeRegistryMlx.cpp` (a table, std.xdg's shared-mime-info later), not xdgmime |
| Screen, user agent | `PlatformScreenMlx.cpp`, `UserAgentMlx.cpp` (as PlayStation's PlatformScreenPlayStation.cpp, UserAgentPlayStation.cpp) |
| Input events | `PlatformKeyboardEvent`, `PlatformMouseEvent`, `PlatformWheelEvent` made by the embedder from Wayland's events (std.xkb for key symbols); `PlatformKeyboardEventMlx.cpp` for the key names and codes |
| Clipboard | `Pasteboard` through the embedder's `PasteboardStrategy` over the Wayland data device (M5); until then `PasteboardMlx.cpp`, a clipboard of the process's own (plain text, HTML, URLs, custom data); drag and drop off until then |
| Cursors | WebCore's generic `Cursor` (no platform cursor) until the embedder sets the compositor's shapes (M5) |
| Accessibility | off: `accessibility/playstation/*.cpp` (the stubs PlatformHaiku.cmake compiles too) |
| Logging, shared memory | `platform/unix/LoggingUnix.cpp`, `SharedMemoryUnix.cpp` (memfd on mlxlibc) |
| Editing | the port's `EditorMlx.cpp` (`Editor`'s pasteboard functions over the port's Pasteboard: plain text, and the selection as text and HTML); EditorGLib.cpp's `WebContentReader` is GTK's and WPE's |
| Localized strings | `LocalizedStringsMlx.cpp` (English first) |
| Memory pressure | WTF's Unix handler; the compositor's memory notices later |

### One process first

**Recommendation: the first runnable milestones embed WebCore in
Voyager's process, as WebKitLegacy did (single process); WebKit2's
processes come later, if at all before Lattice.**

- WebKit2 (Source/WebKit) is a port of its own again: process launching,
  IPC connections and shared memory, a UI process API (WPE's and GTK's
  are GObject), a drawing area and a compositor (DrawingAreaCoordinated
  Graphics and its accelerated surface: GL), the network process whose
  data tasks exist only for soup and curl (we would write
  NetworkDataTaskMlx, NetworkSessionMlx, the cache), and a WebProcess
  that must reach all of WebCore's platform code too. All of that before
  the first pixel.
- A single process needs WebCore's client interfaces only: a `Page`
  made with a `PageConfiguration` of the port's clients
  (`ChromeClient`, `LocalFrameLoaderClient`, `EditorClient`,
  `ProgressTrackerClient`, `ContextMenuClient`, the platform strategies;
  `loader/EmptyClients.cpp` is WebKit's own set of empty ones to start
  from), a `LocalFrameView` to lay out and paint, and the loader strategy
  above. WebKitLegacy is not in the tarball, so this embedding is the
  port's own (projects/webkit/port/embed, a small C API Voyager calls).
- The C++ to Mlx translator will translate WebCore whichever way it is
  embedded; a single process gives it less of WebKit to translate.
- What is given up: process isolation of web content (a crash or an
  exploit takes Voyager), parallel tabs. Acceptable for the bootstrap;
  Voyager can run one WebCore process per tab of its own (std.spawn,
  Wayland subsurfaces) before WebKit2 is ever needed.

### The Mlx side

The port's C++ reaches std through `export` functions with the C ABI.
Most belong in the port's Mlx plugin (projects/webkit/port/mlx,
`libmlxwebport.so`), which imports std.http, std.png and the others and
exports what the port's C++ needs in its shapes (a decoder object with
`append`/`frame` functions, an HTTP request with callbacks): std's own
APIs are Mlx slices and structs, not C. Rules of exports apply: an
exported function cannot be called from Mlx (a `pub fn` and an `export`
beside it), its 64 KB stack arena lives until it returns (state on the
heap, no slices of locals that escape), and callbacks into C++ are C
function pointers with a context pointer, called on the main thread
(through `RunLoop::dispatch`) or on the I/O thread when the C++ side says
it is thread-safe.

## Milestones

| | Milestone | Done when | Check |
|---|---|---|---|
| M1 | JavaScriptCore on mlxlibc, no glibc | jsc's scripts print what the glibc-linked jsc prints, nothing of glibc mapped | tools/check_jsc_mlxlibc.sh (done) |
| M2 | WebCore compiles and links for PlatformMlx | `./mlx build webkit-webcore` makes libPAL.a and libWebCore.a with the configuration above; a program linking WebCore, JavaScriptCore, WTF, bmalloc, Skia and the port's sources on mlxlibc links with no undefined symbol | a link check: `nm -u` against mlxlibc, mlxlibcpp, mlx-unicode and the port plugin is empty |
| M3 | A page loaded and laid out headless | `mlx-out/webkit/bin/webcore-dump FILE.html` makes a Page with the port's clients, loads a `file:` or `data:` URL, lays it out at 800x600 and prints `externalRepresentation()` (rendering/RenderTreeAsText.cpp: DumpRenderTree's render tree) | render trees of LayoutTests pages that use the Ahem font (metrics independent of the system's fonts) compared with LayoutTests' `-expected.txt` of WebKit's repository at 2.54.1's revision (`platform/wpe` and `platform/glib` baselines first), and with a WPE build's WebKitTestRunner where one can be built |
| M4 | Painting into a buffer | the same program paints the view into a Skia raster surface and writes a PNG (std.png); images decoded by the port's decoders | pixel tests: reference PNGs of simple pages (Ahem text, boxes, borders, gradients, images) within a small tolerance |
| M5 | A Voyager window | Voyager shows a page on our compositor (wl_shm buffers of std/src/wayland's client), scrolls, follows links, takes keys and the mouse (focus, text fields, selection), copies and pastes | a scripted session (key and pointer events through the compositor's test interface) and a screenshot compared |
| M6 | The network | http and https pages load (std.http, std.tls with std.x509's chain checks), redirects, cookies, caching headers, forms, `fetch` and XHR, WebSockets | a local std.http server's pages; the TLS checks of std.tls's tests; then real sites by hand |
| M7 | Storage and the rest of a browser's basics | local storage, IndexedDB (mlx-sqlite3), the HTTP cache on disk, WebCrypto (`crypto/mlx`), downloads, file uploads (std.fs) | WebKit's LayoutTests of those areas that run in a single process |
| M8 and on | Beyond the bootstrap | composited layers on the CPU (scrolling, animations), the JIT, media (std.audio, std.matroska), WebGL (ANGLE on std.gpu's Vulkan), accessibility (AT-SPI over std.dbus), processes per tab | |

M2 waits mostly on mlx-cc's C++ (WEBCORE.md: 35 of 52 sampled files
compile); the port's sources for M2 can be written beside it, since each
compiles alone.

## What is retired, and when

The C API layers of compiler/libc and the WPE targets were made for the
WPE plan. They go when PlatformMlx does what they did, not before:

| | Made for | Retired at | Because |
|---|---|---|---|
| `webkit-wpe-*` targets, `config.WPE`, lists/`*-wpe*`, lists/jsc-glib*, tests/wtf-wpe.cpp, tests/jsc-glib.c, tests/wpe.js, tools/check_wpe_wtf.sh, tools/check_wpe_jsc.sh | WTF, JavaScriptCore and WebCore of WPE | M2 | WTF and JavaScriptCore run on mlxlibc (M1), WebCore builds for PlatformMlx (M2). The generators stay (PlatformMlx's derived sources are made by them; tools/check_webcore_generators.sh compares with the Mlx port's feature defines) |
| compiler/libc/glib (mlx-glib) | WTF's GLib loop, WebCore's GIO, the GObject API | M2, with the WPE targets | No GLib anywhere in PlatformMlx (libpulse-mainloop-glib binds the system's GLib, not mlx-glib) |
| compiler/libc/soup (mlx-soup) | WebCore's and NetworkProcess's soup code | M2 (WebCore no longer compiles soup code); its HTTP is std.http's, which M6 uses directly | |
| compiler/libc/png, jpeg, webp (libpng, libjpeg, libwebp) | WebCore's decoders, Skia's codecs | M4 | the port's decoders and encoders call std.png, std.jpeg, std.webp; Skia is built without codecs |
| compiler/libc/lcms2 | WebCore's decoders (USE_LCMS) | M4 | colour management in the port's decoders (std.colorprofile) |
| compiler/libc/gcrypt, compiler/libc/tasn1 | WebCrypto, PAL's digests | M2 for PAL's digests (CryptoDigestMlx), M7 for WebCrypto (`crypto/mlx`) | std.crypto, std.asn1, std.x509 |
| compiler/libc/freetype | Skia's typefaces | after M5, with the port's `SkTypeface` over std.truetype | |
| compiler/libc/harfbuzz | WebCore's Skia text code, hb-icu | after M5, if the shaping replacement meets the open point below | |
| Fontconfig (no C layer was made; std.fontconfig is Mlx) | FontCacheSkia | never needed: `FontCacheMlx.cpp` calls std.fontconfig from M3 | |
| compiler/libc/sqlite3 | WebCore's SQLite wrapper | stays for the WebKit stage | SQLiteDatabase.h is shared by all storage code |
| compiler/libc/xml2 | XMLDocumentParserLibxml2 | stays for the WebKit stage | XMLDocumentParser.h includes libxml2 |
| compiler/libc/zlib | ZStream, WebSocket deflate, WOFF, ANGLE | stays for the WebKit stage | WebKit's files call zlib |
| compiler/libc/icu (mlx-unicode) | ICU everywhere | stays: it is the port's ICU | |
| compiler/libc/pulse, compiler/libc/pipewire | MLX Audio's and MLX Capture's client libraries | not the port's to retire | WebKit's media (M8) uses std.audio directly |
| ANGLE (`webkit-angle`) | WebGL | stays built, unused until WebGL (M8) | |

## Open points

Where WebKit's abstractions may not be met by configuration, selection
or same-name replacement alone. Each is checked when its milestone comes;
none is solved by a patch.

- **Unknown platforms in WebCore.** WebCore was never built without a
  `PLATFORM()`: some generic files may `#error` or assume
  `PLATFORM(GTK) || PLATFORM(WPE)` for Linux behaviour. M2 finds them;
  the answer is a `USE_`/`HAVE_` that selects the generic branch, or a
  port file, else it is listed here.
- **Network headers.** Generic code that branches on `USE(SOUP)`,
  `USE(CURL)` or `USE(CFURLCONNECTION)` with no `#else` (in
  NetworkStorageSession.h, CookieStorage, the cache) would leave a port
  with neither without members or functions it needs. The port's
  `platform/network/mlx` headers supply what an include path can;
  a member declared only under one backend's `#if` cannot be added.
- **`GraphicsLayer` without compositing.** Whether WebCore paints
  correctly with accelerated compositing off in 2.54 (fixed and sticky
  positioning, transforms, opacity, `will-change`) is to be seen at M4;
  if not, the port's `GraphicsLayer` over Skia comes forward from M8.
- **Shaping without HarfBuzz's API.** `SkiaHarfBuzzFont.h` (with
  `hb_font_t`) is part of FontPlatformData's Skia half: our shaping can
  replace ComplexTextControllerSkia.cpp, but the header keeps HarfBuzz's
  types in the build (mlx-harfbuzz's headers) as long as it is WebKit's.
- **The XML parser and SQLite** keep their C APIs (above) as long as
  their headers are WebKit's.
- **ICU's data size and speed** in WebCore's hot paths (line breaking on
  every layout) are mlx-unicode's to make fast; nothing in WebKit to
  change.
- **mlx-cc**: M2's pace is the C++ compiler's (WEBCORE.md); WebCore's
  units need 2 to 4 GB each, so they build with `-j 2` in this
  machine's memory.
- **Absolute paths of the build tree.** jsc names
  `mlx-out/libc/libmlxc.so.1` and mlx-unicode by absolute paths of the
  checkout (mlx-cc writes no runpath); installed, they are
  /lib/mlx/libmlxc.so.1 and a library found in /lib/mlx.

What M2 found, and how the port meets it without a patch:

- **Types a port's configuration must name.** CryptoKeyRSA.h and
  CryptoKeyEC.h declare their key containers for Darwin, GCrypt and
  OpenSSL only, and WebCrypto is always compiled: the port's prefix
  header (port/MlxPrefix.h, included after WebCorePrefix.h as a CMake
  port's prefix header is) names `PlatformRSAKey`,
  `PlatformRSAKeyContainer` and `PlatformECKeyContainer`.
  DNSResolveQueue.cpp makes its singleton of `DNSResolveQueuePlatform`,
  which only GLib's, curl's and CFNetwork's headers name: the port's
  DNSResolveQueueMlx.cpp defines its class and compiles WebKit's file
  after it (the file is skipped from its unified source).
  SharedMemory.h has a handle type only with `USE_UNIX_DOMAIN_SOCKETS`
  (GTK's and WPE's, set in config.MLX) or Darwin's and Windows'.
- **GL in the Skia sources.** GraphicsContextSkia, NativeImageSkia,
  SkiaUtilities and the others include GLContext.h, GLFence.h and
  texmap's BitmapTexture.h for their accelerated paths, compiled whatever
  the configuration: Khronos's headers (ANGLE's copy in the tree) and
  platform/graphics/texmap are on the include path, no GL library is
  linked, and PlatformDisplayMlx.cpp answers that there is no display.
- **Generic headers with a member for one backend.** The generic branch
  of NetworkStorageSession.h is enough (a session and its networking
  context); the cookie functions are the port's
  (NetworkStorageSessionMlx.cpp). FontCache.h's Skia members are kept;
  `FontSetCache`, which only FontCacheSkia.cpp defines, is the port's
  own class in FontCacheMlx.cpp.
- **Bundles too big for mlx-cc.** The dense JSBindings bundle of
  bindings/js (64 sources) exceeds mlx-cc's 8 GB arena: the Mlx port
  bundles bindings/js by directory (webcore.mlx, `Derived.dense`).
- **Accessibility's platform shape.** AXCoreObject's wrapper member
  exists for Cocoa, Windows, PlayStation, Haiku and ATSPI only, and
  accessibility is always compiled: config.MLX sets `WTF_PLATFORM_HAIKU`,
  the Haiku port's shape of it (an AccessibilityObjectWrapper that is a
  RefCounted nothing, which the PlayStation stubs the port compiles make;
  ChromeClient's three accessibility notification functions, which
  EmptyClients defines and the embedder will), and Pasteboard.h's
  members for GTK, WPE and Haiku: all WebKit asks PLATFORM(HAIKU) for.
