# PlatformMlx: WebKit's port to Mlx

Voyager, our browser, first runs on WebKit, the reference engine. Our
own engine, Lattice, comes later: WebKit translated to Mlx by the C++ to
Mlx translator, then reshaped. This page is the design of the WebKit port
that comes first, **PlatformMlx**: what WebKit 2.54.1's sources
(`WEBKIT_SOURCE`, by default projects/webkit/upstream, the release as
it is: UPSTREAM.md) are built as, what the port supplies, in what order, and what of this
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

### The toolchain, not WebKit

The tree is projects/webkit/upstream, the release tarball as it is
(UPSTREAM.md; tools/check_webkit_upstream.sh fails on any change to it).
What WebKit's sources need of a compiler that ours lacks is added to the
toolchain, never patched around in them. The tree used until M8,
/home/user/csrc/wpe/wpewebkit-2.54.1, had two headers patched by hand;
mlx-cc compiles both as released now:

- **libpas's double-free check** (pas_segregated_page_inlines.h,
  `pas_segregated_page_deallocate_with_page`): x86-64 inline assembly
  that clears the bit and, when it was clear, calls
  `pas_segregated_page_deallocation_did_fail` (`btrl; jc 0f; movq ...,
  %rdi; call ...; 0:`). The patch left the check out for `__mlxcc__`
  (the C fallback of ARM and RISC-V instead). mlx-cc's inline assembler
  now calls a symbol out of the text as GNU as does (a relocation for
  the linker, the stack 16-byte aligned at the call):
  compiler/cc/tests/inlineasm.c.
- **WTF's SIMD helpers** (SIMDHelpers.h: `isNonZero`,
  `findFirstNonZeroIndex`, `findLastNonZeroIndex`): on `CPU(X86_64)`
  they convert SIMDe's NEON-style vectors to `__m128i` and test them with
  SSE2 (`simde_uint8x16_to_m128i`, `_mm_test_all_zeros`,
  `_mm_movemask_epi8`), which SIMDe defines only on its native x86 path
  (`SIMDE_X86_SSE2_NATIVE`). The patch took the generic code unless that
  path was there. SSE2 is x86-64's baseline: every compiler for it has
  the intrinsics and SIMDe takes that path, as WebKit expects. mlx-cc's
  <mmintrin.h>, <xmmintrin.h> and <emmintrin.h> now have all of MMX's,
  SSE's and SSE2's intrinsics (unions of lanes, the compiler having no
  vector registers; compiler/cc/tests/ssebaseline.c and
  tests/cpp/ssebaseline.cpp), and the port tells SIMDe so with
  `-DSIMDE_X86_SSE2_NATIVE` beside `-DSIMDE_NO_VECTOR` (build.mlx): a
  command-line define of SIMDe's own configuration that turns a code
  path on, to what it is with any x86-64 compiler. mlx-cc does not
  predefine `__SSE2__` itself: it has no GCC vector types, and code that
  tests `__SSE__` or `__SSE2__` takes them to come with the intrinsics
  (Skia's SkSwizzler_opts.inc divides and compares `__m128` values with
  the operators), so that would turn on code mlx-cc cannot compile.
  Without `__SSE2__` SIMDe's half floats are its portable ones (GCC 12's
  `_Float16` on SSE2 being another of mlx-cc's gaps), which WebKit does
  not use.

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
- **M2, done**: the port is described and builds. `config.MLX`
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
- **M3, done**: webcore-dump (tests/webcore-dump.cpp,
  `./mlx build webkit-mlx-webcore-dump`) loads a page with the port's
  clients and WebKitTestRunner's settings (hinting off, mock scrollbars,
  its GLib fonts.conf over Liberation and DejaVu, Ahem as a test font),
  lays it out at 800x600 and prints the render tree. 37 of the 44 layout
  tests of tests/layout pass against the GLib/WPE baselines at 2.54.1
  (tools/check_webkit_mlx_layout.sh); the 7 others are listed with their
  causes in tests/layout/failures.txt, all mlx-cc bugs already fixed in
  epoch 5 (a braced assignment `p = { x, y }` through the wrong
  constructor, an enum argument not promoted to int) and to pass after
  the next full rebuild. The bugs M3 found were mlx-cc's code generation
  (CODEGEN_EPOCH 3 to 5: casts to classes, branch temporaries, elided
  braced temporaries, 128-bit constants, constexpr pointers into
  globals), mlx-unicode's UText API, and the port's font cache. A full
  rebuild of WebCore, the port and webcore-dump takes about 18 minutes
  (`-j 4`, memory-aware); a port-only change relinks. With epoch 5 and 6
  all 44 pass.
- **M4, done**: `webcore-dump --png OUT.png` paints the view into a Skia
  raster surface (CPU, N32) through GraphicsContextSkia and writes it
  with std.png (the port's `mlxweb_image_write_png`); `--reference`
  compares it with a PNG. tools/check_webkit_mlx_paint.sh paints the
  pages of tests/paint (boxes and a translucent background, borders and
  an outline, Ahem text, linear gradients, a PNG image through the port's
  std.png decoder at its size and scaled) against references that
  make_references.py computes from what each page draws: 5 of 5 match to
  the pixel (a channel off by 1 at most; the gradient page crashed until
  mlx-cc's decltype(auto) fix of epoch 8). M4 found mlx-cc's class template instance
  made through a reference while its argument was only declared (no size:
  BorderEdges returned in registers, epoch 6).
- **M5, done**: Voyager (projects/voyager, `./mlx build
  webkit-mlx-voyager`, mlx-out/webkit/mlx/bin/voyager). engine.cpp is
  WebCore in the process (the port's clients, a chrome client that notes
  repaints, an editor client that lets text be edited, the keys' editing
  commands) and calls libvoyager.so (voyager.mlx), a std.ui.host window on
  the compositor drawn into wl_shm buffers: each frame the engine paints
  the view into the buffer's pixels; the pointer, the wheel, keys with
  their text, the window's size and a 15 ms tick (WTF's RunLoop cycle and
  the page's rendering update) go to the engine; Ctrl+C and Ctrl+V go
  through the compositor's clipboard. tools/check_voyager.sh runs it
  under tools/wayland-test-host: the page shows, the wheel scrolls it, a
  click focuses a text field and keys type into it, a word copied and
  pasted, a link followed to another file: page, and the screenshot shows
  that page as painted. mlx-cc now refuses a call whose result is a class
  only declared (Voyager's first engine took EventHandler's results so).
- **M6, done**: http and https. The port's loader (embed/MlxNetwork.cpp,
  a work queue of its own) runs each request with std.http's client on
  the Mlx side (port/mlx/http.mlx: keep-alive, chunked, gzip and deflate;
  https with std.tls, the chain checked by std.x509 against the system's
  CA bundle and `MLXWEB_CA_FILE`) and gives the response, the data and
  the end to WebCore's ResourceLoader on the main thread; redirects are
  WebCore's (`willSendRequest`, 303 and a POST's 301/302 made GETs), the
  cookie jar std.http's (with its public suffix list) behind
  NetworkStorageSessionMlx and the page's CookieJar. Forms (GET, POST,
  urlencoded and multipart), `fetch` and XHR go the same way;
  synchronous loads too. Caching is WebCore's memory cache with the
  response's headers (no disk cache: M7). WebSockets (embed/MlxWebSocket.cpp,
  the page's SocketProvider; port/mlx/websocket.mlx) do the handshake,
  frames, permessage-deflate and wss: (std.tls) on a reading thread of
  their own. Voyager has a toolbar: an address field (Ctrl+L or a click,
  Enter loads, http: when no scheme is typed), back and forward (the
  buttons, Alt+Left and Alt+Right). tools/check_webkit_mlx_network.sh
  starts tools/http-test-server (Mlx: a redirect chain, cookies set and
  echoed, a form's POST echoed, JSON, gzip, chunked, a WebSocket echo,
  and the same over https from std.tls's new server side,
  std/src/tls/server.mlx, with a test root and certificate it makes at
  start with std.asn1's DER writer and Ed25519) and compares what
  webcore-dump prints of 16 pages (the user agent's among them), then
  that an unknown root is refused: 17 of 17. tools/check_voyager.sh types an address and goes back and
  forward. https://example.com loads headless (its text printed) when
  this machine reaches it; no check depends on the network. M6 found
  mlx-cc's generic lambda converted to a function pointer (the closure's
  address taken: epoch 7).
- **M7, part 1, done** (storage and the HTTP cache): a profile
  (embed/MlxProfile: Voyager's ~/.local/share/voyager and
  ~/.cache/voyager, `--profile DIR`; webcore-dump's a temporary one unless
  `--profile` is given), localStorage persisted per origin in SQLite and
  sessionStorage per tab (embed/MlxStorage), WebKit's IDB server in the
  process over SQLiteIDBBackingStore (embed/MlxIndexedDB), the HTTP cache
  on disk (port/mlx/httpcache.mlx) and the cookie jar kept in SQLite
  (port/mlx/cookies.mlx): "Storage" and "Network" below. Frames are made
  now (MlxFrameLoaderClient::createFrame, as WebKit2's
  WebFrame::createSubframe), window.open's page has its opener and a copy
  of its session storage, and webcore-dump runs WebKit's dumpAsText tests
  (`--test`). Checks: tools/check_webkit_mlx_storage.sh (62 of 62 layout
  tests of storage/domstorage and storage/indexeddb),
  check_webkit_mlx_network.sh (24 of 24: the cache's max-age, ETag and
  no-store across two runs of a profile, cookies kept with their expiry),
  check_voyager.sh (localStorage across a restart, the profile on
  voyager://version). M7 found in mlx-cc (epochs 11 and 12, each with a
  test in compiler/cc/tests/cpp): a scoped enum converted implicitly to
  an integer in overload ranking (WTF::Persistence::Encoder wrote an
  enum's tag as one byte where the decoder reads eight: no IndexedDB key
  path could be read back; scopedenumoverload), and a bit-field of an
  `enum class : bool` read back sign-extended (ScriptElement's
  ParserInserted: no external script blocked the parser, so a page's
  inline scripts ran before the scripts they use; boolenumbitfield).
- **M7, part 2, done** (WebCrypto, downloads, uploads): WebCrypto's
  platform half is the port's `crypto/mlx` over std.crypto, std.aes,
  std.asn1 and std.random (port/mlx/crypto.mlx): SHA-1 and the SHA-2s,
  HMAC, AES-CBC, AES-CTR, AES-GCM, AES-KW, PBKDF2, HKDF, ECDSA and ECDH
  on P-256, P-384 and P-521, Ed25519, X25519, RSASSA-PKCS1-v1_5,
  RSA-PSS and RSA-OAEP, keys imported and exported raw, as JWK, SPKI and
  PKCS #8 ("Crypto" below); getRandomValues and randomUUID take the
  kernel's random bytes. Downloads (embed/MlxDownloads: an attachment, a
  link's download attribute, a blob:, data: or file: URL, a type no frame
  shows) go to the embedder's folder, Voyager's XDG_DOWNLOAD_DIR, listed
  in its sidebar and on voyager://downloads; `<input type=file>` opens
  std.ui.chooser in Voyager (webcore-dump `--upload PATH`), and the files
  reach forms (multipart bodies read from the files), FormData, File,
  FileReader and Blob (blob: URLs are loaded now: "Downloads and
  uploads" below). Checks: tools/check_webkit_mlx_crypto.sh (152 layout
  tests of crypto/subtle and crypto/, a third of each large group: 354
  of the 355 that need no worker pass), check_webkit_mlx_network.sh (30
  of 30: Ed25519, X25519 and ECDSA known answers, downloads of each kind
  with the files' contents, uploads by form, several files, script),
  check_voyager.sh (a download into a temporary XDG_DOWNLOAD_DIR shown on
  voyager://downloads; an upload through the chooser), storage 63 of 63:
  keypath-basics's abort was mlx-cc's (epoch 13, datamemberhides): a
  data member named unqualified in a member function was taken for a
  base's member function of the same name, so ConcurrentPtrHashSet's
  Table read TrailingArray's `size()` as its size, never grew, and the
  collector found its opaque roots' table full. std.crash now reports
  one thread's crash while another thread crashes (the collector's
  markers both failed the check: two half reports, no frame named).
- **M7, part 3, done** (its open points closed): mlx-cc's two "gaps" of
  part 2 were the port's own mistakes (WTF's Vector has a
  `(FillWith, size, value)` constructor, no `(size, value)` one;
  `blobRegistry()` gives a CheckedRef, reached with `->`), found by
  compiling the unit with the original lines; the workarounds are gone
  and mlx-cc's messages now say so (the class and the arguments' types
  spelled as C++ writes them, "did you mean '->'?";
  compiler/cc/tests/cpp/diagnostics). RSA takes keys of up to 16384 bits
  and of a few (rsa-import-spki-small-key), and of more primes; pairs
  are made off the main thread; the crypto check runs crypto/workers'
  78 tests too (231 of 231). Downloads come from the load WebCore made
  (a POST's attachment with one request), streamed to
  "NAME.EXT.download", paused and resumed with Range, cancelled, and
  kept in the profile; Voyager shows them as Safari does (the toolbar's
  button, its ring and popover; the sidebar's list is gone), and its
  file chooser has a path field and places, opening where the app last
  chose. Checks: network 37 of 37 (a POST's attachment, a large
  download paused, resumed and cancelled, the list across two runs),
  check_voyager.sh's scenarios 18 to 21.
- **M8, part 1, done** (composited layers on the CPU: scrolling,
  animations; "Compositing" below): accelerated compositing on, with
  WebKit's coordinated graphics (GraphicsLayerCoordinated, tiled backing
  stores painted on demand, SkiaCompositingLayer) and its threaded
  scrolling tree; the port's MlxLayerTreeHost composites on a thread of
  its own at the display's rate (synchronously in webcore-dump), with
  damage passed down to the wl_shm buffer's; wheel, Page Down and a
  scrollbar's drag scroll without painting again; CSS animations of
  transform and opacity run on the compositor's clock, as
  requestAnimationFrame does (MlxDisplayRefreshMonitor); forced dark
  and the port's theme as before. mlx-cc (epochs 15 and 16, tests
  nestedfriendadl and implicitcopyvptr): argument-dependent lookup for
  an operand of a nested class started in the class around it
  (ScrollingStateNode's Variant comparisons), and an implicit copy
  constructor did not store the class's vtable pointer (a copied
  GraphicsLayerKeyframeValueList's values called through address 0 when
  an animation started). mlx-profile samples a thread by name
  (`--thread MlxCompositor`), lets the program run while it names a
  sample's addresses, and names them by a binary search of an index of
  each file's functions (Voyager, 1.5 GB: seconds an address before).
  Its profiles: the compositor painted an animated 200x200 layer in
  90 ms a frame (Skia's portable blends and its raster pipeline for a
  tile's damaged parts); with the port's row blends and fills
  (SkiaRoutinesMlx.cpp) and the compositing canvas's sprites, 3.5 ms
  (60 frames a second). Scrolling a saved copy of
  en.wikipedia.org/wiki/WebKit by 40 wheel events of 60 pixels, the
  main thread (mlx-profile, from 60 s on, once laid out): without
  compositing busy 3.2 s of the 5.5 s, 89% of it painting the view
  again; composited 1.1 s, 33% of it recording the 10 tiles that come
  into view, 31 frames composed. Checks: check_webkit_mlx_paint.sh 15 of
  15 (composited.html, fixed.html, overflow.html, perspective.html,
  composited.html@forcedark), check_webkit_mlx_compositing.sh 103 of
  103 (WebKit's compositing/ layer trees), check_voyager.sh's
  scenarios 22 and 23.
- **M8, part 2, done** (WebGL; "WebGL" below): WebGL 1 and 2 on ANGLE's
  Vulkan backend (`webkit-angle`, as WebKit's Linux ports configure it,
  with its SPIR-V translator, no GL or EGL library of the system), whose
  Vulkan is the port's own libmlxvulkan.so (compiler/libc/vulkan: Vulkan's
  C API over std.vulkan's loader, its headers by mlx-cheaders from
  Vulkan-Headers' registry) over the system's driver, lavapipe where there
  is no GPU. The port's GraphicsContextGLMlx draws into a texture of a
  context without a window and reads each frame back into the canvas'
  layer for the CPU compositor. What it took: in mlx-cc (epochs 17 to
  25, each with its test in compiler/cc/tests/cpp) decltype through
  qualified template arguments, a data member hiding a type, anonymous
  structs in unions, a specialized conversion operator, `{}` for a union,
  piecewise pairs, a partial specialization's members defined late, the
  address of an overloaded function template's instance, a file's own
  unnamed namespace, unions copied as bytes (bison's YYSTYPE in ANGLE's
  GLSL parser), a class template's copy constructor defaulted outside the
  class (ANGLE's FixedVector copied empty: lavapipe drew with garbage
  image views), rvalue streams (`(std::ostringstream() << x).str()`),
  `std::min<double>` as a pointer, a conversion to a base through `Ref`'s
  `operator T &`, class template argument deduction beside a
  constructor template's pack, and a braced pointer for one of two
  initializer_list constructors (ANGLE's SeparateDeclarations: `float c
  = a, s = b;` crashed its translator); in mlxlibc, `RTLD_LAZY` and
  glibc's names for lavapipe's libraries (LLVM, libstdc++), a failed
  dlopen unloaded, `sysconf(_SC_PHYS_PAGES)`, which read the wrong fields
  of `struct sysinfo` (lavapipe's heap was 48 KiB), and its loader's own
  hash index of the Mlx compiler's libraries' symbols, which they chain
  in one bucket (a WebGL context's first start, LLVM's relocations
  looked up, took 13 s; 2.5 s now). MlxLayerTreeHost composes a rendering
  update as soon as it is committed, as WebKit2's ThreadedCompositor
  does (left to the clock's next tick, a page changing each frame ran at
  a quarter of the display's rate). Checks: tools/check_mlx_vulkan.sh,
  tools/check_webkit_mlx_webgl.sh 62 of 62 (WebKit's fast/canvas/webgl and
  webgl tests, conformance wrappers among them), check_webkit_mlx_paint.sh's
  WebGL pages (a triangle, a texture from an image, uniforms, WebGL 2's
  instancing), check_voyager.sh's scenario 24 (a WebGL canvas animated
  on the frame clock: about 38 frames a second on lavapipe in Voyager).
- **M8, part 3, done** (compositing on the GPU; "Compositing" below):
  the layers are composed by WebKit's TextureMapper (its GL painter,
  BitmapTexture, texmap's backing stores, as WPE composes them) over
  ANGLE's GLES on the same surfaceless Vulkan display as WebGL, on the
  compositor thread's own GL context, into a framebuffer whose damaged
  bounds are read back into the wl_shm frame; tiles are still painted on
  the CPU (Skia raster, worker threads) and uploaded into textures
  (Ganesh over ANGLE is not trivial here: WebKit's GPU painting engine is
  SkiaPaintingEngine.cpp's atlases and per-thread GL contexts); WebGL
  canvases are textures shared with the compositor (no read back). The
  CPU's compositor stays for no Vulkan device, `MLX_COMPOSITING=cpu` and
  Voyager's `--no-gpu-compositing`. On lavapipe (no GPU in the machine
  measured; ms of the compositor's frame from `MLX_TRACE_COMPOSITOR`,
  busy samples of mlx-profile from 8 s on, CPU's then GPU's): the
  animation of check_voyager.sh's scenario 23 at 60 frames a second both
  ways, 3.9 then 5.2 ms a frame, the compositor thread 271 then 216
  samples, the main thread 727 then 702, the process 14.2 then 16.0 s
  of processor time (lavapipe's threads); a long page scrolled by 24
  wheel events (the whole view damaged each frame) 6.6 then 15 ms a frame,
  11 ms of it the read back of the whole frame waiting for lavapipe's
  rendering, compositor 36 then 48 samples, main thread 158 then 169;
  scenario 24's WebGL canvas 38 then 50 frames a second. Reading BGRA
  had ANGLE convert on the GPU: 15.5 ms an animation frame, 41 frames a
  second, before reads were RGBA swapped on the CPU. Checks:
  check_webkit_mlx_compositing.sh 103 of 103, check_webkit_mlx_paint.sh
  19 of 19 (composited.html@forcedark among them), check_webkit_mlx_webgl.sh
  62 of 62, check_webkit_mlx_layout.sh 44 of 44, check_voyager.sh all,
  on the GPU path; the CPU's path (`MLX_COMPOSITING=cpu`) paints the
  same pixels as the GPU's for composited, forced dark and WebGL pages.
- **M8, part 4, done** (tiles painted on the GPU, frames presented
  directly; "Compositing" below): with the GPU's compositor, Ganesh over
  ANGLE paints the layers' tiles into textures by default
  (`MLX_PAINTING=cpu` for the CPU's painting and uploads), after mlx-cc's
  epochs 27 to 31 fixed what Skia's GPU code met (remove_if copying
  unique_ptrs, deleted assignments taken as bytes, `new (n) T` with a
  size, a `T &` bound to a temporary of another type, a member function's
  result instantiated while its class was laid out, a member template's
  definition applied twice, an overloaded name for a function reference);
  ANGLE has its share context lock. On a GPU the frame is copied by the
  device into the window's wl_shm buffer; on a CPU's device it is read
  back (`MLX_COMPOSITOR_PRESENT=gpu|read`). On lavapipe (ms a frame of
  the compositor thread, then of the embedder's take, from
  `MLX_TRACE_COMPOSITOR`, medians past the load), CPU painting with read
  back, GPU painting with read back, GPU painting presented directly:
  check_voyager.sh's animation 1.4 + 1.7, 1.4 + 1.7, 1.2 + 3.9; a long
  page scrolled by whole views (wheel and Page Down) 4.0 + 1.9,
  3.3 + 1.9, 3.0 + 3.7; the WebGL page 1.3 + 1.7, 1.3 + 1.7,
  1.0 + 4.8 (60, 60 and 56 frames a second). Checks, on the GPU path and
  with `MLX_COMPOSITING=cpu`: check_webkit_mlx_compositing.sh 103 of
  103, check_webkit_mlx_paint.sh 19 of 19, check_webkit_mlx_webgl.sh 62
  of 62, check_webkit_mlx_layout.sh 44 of 44, storage 63 of 63, network
  37 of 37, crypto 231 of 231, check_voyager.sh (also with
  `MLX_COMPOSITOR_PRESENT=gpu`), tools/check_cpp.sh.
- **M8, part 5, done** (JavaScriptCore's LLInt in assembly, the Baseline
  JIT and the DFG): JSCOnly and Mlx build JavaScriptCore with
  `ENABLE_C_LOOP` off and `ENABLE_JIT` and `ENABLE_DFG_JIT` on (FTL and
  WebAssembly off; WPE keeps the C loop). offlineasm's X86_64 back end
  is the port's (generators/offlineasm/x86.mlx, a port of x86.rb and of
  asm.rb's assembler, LLIntAssembly.h byte for byte as the Ruby one
  writes it), and mlx-cc assembles LowLevelInterpreter.cpp's top-level
  asm statement (and MacroAssemblerX86_64.cpp's probe trampolines) as
  GNU as does: relaxed jumps, sections, `.globl`/`.type`/`.size`/
  `.p2align`, data directives, symbolic displacements, RIP-relative,
  GOTPCREL and PLT relocations, VEX moves (epoch 32, tests fileasm and
  typedefenumalias). The JIT's code is mlx-cc's to compile: epochs 33
  to 39 fixed what JavaScriptCore's JIT, DFG and YARR met, each with a
  test in compiler/cc/tests/cpp (using-declarations of using-declared
  functions, a static member hiding a using-declared overload, a class's
  own overloads before its using-declaration's, a base instance's
  template parameter taken as a member, default arguments looked up where
  they were used, a generic lambda in a lambda without `this`, template
  moves of noncopyable classes, `= default` moves beside declared copies
  (the DFG's switch tables were copies its code did not point into),
  a derived object copied through a deleted conversion template,
  std::array<T, 0> returned in xmm0 over the caller's saved rbp,
  log-deep make_index_sequence and calls of 4096 arguments for WTF's
  enumNames, partial ordering, CTAD of the template's own class, empty
  folds, unary operators of classes, and more: compiler/cc/epoch.mlx).
  The extractors and the jsc shell take JavaScriptCorePrefix.h as the
  library does (InlineCacheCompiler.h names `CCallHelpers::Jump` of a
  class it only declares), and port/JavaScriptCoreMlxPrefix.h gives the
  JIT without WebAssembly its CompilationResult.h. mlxlibc needed
  nothing new: the executable allocator's RWX `mmap`, the VM traps'
  signals and `ucontext_t` (a watchdog stops a JIT-compiled loop) were
  there. std.sql freed an INSERT's values as wide as its table was at
  finalization, past them when IndexedDB's store finalized its statements
  after its tables changed (storage/indexeddb/key-generator.html crashed
  one run in five with the JIT's timing; compiler/libc/sqlite3/tests/
  writes.c). Numbers (C loop, LLInt in assembly, Baseline and DFG):
  projects/webkit/tests/bench/bench.js in jsc 8.4 s, 2.35 s, 1.05 s
  (loops 2.6 s, 20 ms, 20 ms; objects 1.7 s, 0.8 s, 0.15 s; arrays 2.8 s,
  0.6 s, 0.23 s; strings 0.4 s, 0.15 s, 0.08 s; regexp 0.19 s, 0.17 s,
  0.11 s; JSON 0.6 s, 0.5 s, 0.45 s); bench.html in webcore-dump 10.0 s,
  3.6 s, 1.05 s; the saved en.wikipedia.org/wiki/WebKit page loads in
  56 s, 56 s, 32 s (without scripts 25 s before, 12.5 s now). jsc's scripts
  print what they did with `--forceEagerCompilation=true`,
  `--useConcurrentJIT=false` and low tier-up thresholds too. Checks:
  check_jsc_mlxlibc.sh (it runs bench.js), check_cpp.sh, check_cc.sh,
  check_sqlite.sh,
  layout 44 of 44, paint 19 of 19, storage 63 of 63, network 37 of 37,
  crypto 231 of 231, compositing 103 of 103, webgl 62 of 62,
  check_voyager.sh, check_webkit_upstream.sh.
- **M8, part 6** (JavaScriptCore's JIT finished): WebCore's CSS selector
  JIT is on (`ENABLE_CSS_SELECTOR_JIT`, as WebKit has it with the JIT on
  x86-64). The selector it compiled for `:root::view-transition` crashed
  on the saved en.wikipedia.org/wiki/WebKit page, a pointer read a byte
  off: the compiler's StackAllocator (cssjit/StackAllocator.h) computes
  `stackReference - stackUnitInBytes()`, a StackReference converting to
  unsigned, and mlx-cc took LayoutUnit's `operator-(int, const
  LayoutUnit &)` for it (the class through its conversion, the unsigned
  through LayoutUnit's converting constructor) instead of the built-in
  subtraction: popAndDiscardUpTo's offsets came out in LayoutUnit's
  sixty-fourths, the generated code addressed its stack slots wrongly.
  mlx-cc weighs the built-in operator as a candidate now
  ([over.match.oper]; epoch 40, test builtinoperatorcandidate), with the
  fixes B3, Air, the FTL and WebAssembly's sources needed (the epoch's
  list in compiler/cc/epoch.mlx). The Wikipedia page lays out with the
  selector JIT as without the JIT (its render tree the same), in 40 s.
  Checks: layout 44 of 44, paint 19 of 19, storage 63 of 63, network 37
  of 37, crypto 231 of 231, compositing 103 of 103, webgl 62 of 62,
  check_voyager.sh, check_cpp.sh (585 programs), check_jsc_mlxlibc.sh,
  check_webkit_upstream.sh.

  An exception thrown in JIT code escaped its `try` (a direct eval's, a
  `replace` callback's, a JSON reviver's, a sort comparator's once the
  caller was compiled): mlx-cc made every JIT operation's
  `ExceptionOperationResult` through ExceptionOperationImplicitResult's
  conversion to a bare `T` (the value alone, the exception flag lost)
  instead of its conversion to `ExceptionOperationResult<T>`; and
  `c ? T(a) : T(b)` moved the branch's temporary into the result by its
  bytes, so the DFG's `Vector<CallVariant, 1>` of a call's status
  pointed into a dead temporary's inline buffer and libpas freed it
  (handleArraySort). Epoch 41 (conversiontemplateordering,
  conditionalinlinebuffer); tests/jsc/exceptions.js
  (check_jsc_mlxlibc.sh) throws each 3000 times.

  The rare stop of webcore-dump on a CheckedPtr to a dead ScheduledTask
  of the generic RunLoop's timer tree (about one run in 30 of crypto's
  RSA key generation, with or without the JIT) is WebKit's, not a
  miscompilation: RedBlackTree::remove takes a node out but leaves its
  m_left and m_right, CheckedPtrs, at the children it had; when one of
  those children's timer is destroyed, WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR
  finds it still counted (scribbles and leaks it), and the removed task's
  next insert releases the dead child. Three timers do it every time
  (tests/wtf-runloop.cpp, built as `webkit-wtf-runloop`; WebKit's main
  branch has the same `remove`). port/wtf/RunLoopGenericMlx.cpp compiles
  RunLoopGeneric.cpp with an explicit specialization of the tree's
  `remove` for the run loop's tasks (WebKit's body, then the node's
  reset), declared before its first use; wtf-runloop's 20000 random
  timer steps pass (check_jsc_mlxlibc.sh), and crypto's five RSA
  key-generation tests passed 150 runs of 150.

  fast/selectors: tools/check_webkit_mlx_selectors.sh runs the
  dumpAsText tests of WebKit's fast/selectors that pass
  (tests/layout/selectors.txt: 218 of the 251 that dump text) with the
  selector JIT.

  mlx-codemap knew little of JavaScriptCore's JIT: a header's shard of
  the index is written by the first unit that reads it, and the first
  to read LinkBuffer.h had the assembler off (one `#if
  ENABLE(ASSEMBLER)` line of it). mlx-cc counts what each file's false
  groups leave out (an `S` record) and writes a shard again when a unit
  leaves out less (tools/check_codemap_cpp.sh, arcs.h).
- **Real sites** (after M6): news.ycombinator.com, en.wikipedia.org,
  www.wikipedia.org, info.cern.ch, example.com, google.com and
  lite.duckduckgo.com load and paint at webcore-dump's 1280x800 (the
  layout and paint checks give `--size 800x600`), and Voyager shows
  example.com. They found in mlx-cc (epochs 8 and 9, each with a test in
  compiler/cc/tests/cpp): a class returned from a `decltype(auto)`
  function copied by its bytes (WTF::switchOn, std::apply: a gradient's
  color stops pointing into a dead temporary; decltypeautoclass);
  `c ? f() : g()` of a class moved into its result by its bytes (a
  GlyphBuffer cached with its inline vectors pointing at the stack;
  conditionalselfpointer); `Vector<T> v { n }` taken as a list of one
  element (pagination's line adjustments; listctornumber); `++it` of a
  class whose only increment is its `operator unsigned &()` (GridSpan's
  range-for ran past its span; conversionincrement); in mlxlibcpp, a
  ranges projection's reference copied (a WebAnimation destroyed while
  referenced; rangesprojectionref). std.crash's report now has 64 frames
  with long C++ names shortened on stderr (whole in crashes.log), and an
  abort says abort() was called.
- **Voyager's chrome on std.ui, dark pages** (after real sites):
  Voyager's toolbar is std.ui's widgets (std.ui.view laid out each
  frame): back and forward as std.ui.controls' chevron buttons, reload
  (stop while the page loads) as plain icon buttons with std.ui.glyphs'
  marks (a circular arrow, new there; the cross), the address field
  std.ui.field (its editing, selection, caret; Ctrl+C, Ctrl+X, Ctrl+V
  with the compositor's clipboard; a first click selects all of it), its
  colours std.ui.theme's for the desktop's scheme (std.ui.appearance:
  the panel's Watch, std.ui.host.lightness), fading with it. The page
  gets the scheme too: `ENABLE_DARK_MODE_CSS` is on (config.MLX; GTK and
  WPE have it on: the `color-scheme` property, `<meta
  name="color-scheme">`, the Page's dark appearance), and the engine's
  setDark calls `Page::setUseColorAppearance` as the GLib ports' web
  views do when the desktop prefers dark, at once when it changes (the
  media queries evaluated again, the styles and the view's background
  resolved again, no reload). `prefers-color-scheme: dark` matches then,
  and a page that allows dark (`color-scheme: light dark`) is drawn in
  its dark variant: RenderThemeAdwaita's system colours for dark
  (Canvas, CanvasText, Field, FieldText, ButtonText, GrayText), its form
  controls, ScrollbarThemeAdwaita's scrollbars and the view's base
  background (Window, the dark Canvas). `MLX_COLOR_SCHEME=dark|light`
  overrides the desktop's. `webcore-dump --dark` paints a page so;
  tools/check_webkit_mlx_paint.sh paints tests/paint/scheme.html light
  and dark (`scheme.html@dark` in pages.txt), and tools/check_voyager.sh
  starts Voyager on a dark desktop once (the toolbar, the page's canvas,
  its prefers-color-scheme box and its text field dark) and switches a
  light one to dark while it runs (both follow, the page is not loaded
  again).
- **Tabs, the port's theme, forced dark** (Voyager as a browser):
  std.ui.field holds text of any length (a block of std.heap that grows,
  wiped when given up; found by halving where a click or the view falls,
  so a field of kilobytes stays quick), so the address field takes long
  URLs whole and scrolls to the caret. Voyager has tabs, each a Page of
  its own in the engine (the bridge's functions take a tab's id: open,
  close, select, and each tab's title, URL, loading, reload and stop;
  only the tab shown is painted and gets the events, every tab's loads,
  timers and rendering updates go on), in a sidebar of std.ui.sidebar
  (title, a dot while loading, close button; a row for a new tab) that
  folds to a strip of the tabs' initials, gliding (Ctrl+Shift+B);
  Ctrl+T, Ctrl+W, Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+1 to 9; window.open and
  `target=_blank` (MlxFrameLoaderClient's dispatchCreatePage, then the
  chrome client's createWindow) open a tab shown, a middle click or
  Ctrl+click a link's tab behind; closing the last tab closes the window.
  The port's theme ("The theme") and forced dark mode ("Forced dark
  mode") are below; Voyager has no setting of its own for the scheme, it
  follows the desktop's (std.ui.appearance; MLX_COLOR_SCHEME, std.ui's
  override, for tests). They found in
  mlx-cc (epoch 10, condreference): a condition declaring a reference
  (`if (auto& entry = cache[key])`, SystemFontDatabase's cache of a
  std::optional) tested the reference, never null, instead of the object
  (its `operator bool`): the system font of every form control was the
  empty one of 0px, so text fields, buttons and selects had no size and
  no text. tools/check_voyager.sh opens, shows and closes tabs (a
  `target=_blank` link, window.open, a middle click whose tab loads
  behind, the keys), folds the sidebar, loads a URL of 875 bytes and
  checks forced dark following the desktop; check_webkit_mlx_paint.sh paints a
  button, a check box, a select and selected text in both schemes, and
  forced dark.

### The user agent

`Mozilla/5.0 (X11; Linux x86_64) Voyager/0.1 (WebKit 2.54.1; Wayland)
AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15`:
WebKit's compatibility string exactly as the GLib ports build it
(UserAgentGLib.cpp's buildUserAgentString, Version/60.5 on purpose: sites
turn away low and some high Safari versions; the first parentheses stay
"X11; Linux ..." whatever shows the windows, as servers read the platform
there), with the embedder's product right after the platform, as
Ladybird puts its own, and a comment naming the engine and
the window system (`MlxFrameLoaderClient::setApplicationNameForUserAgent`:
Voyager's "Voyager", "0.1", "Wayland"; webcore-dump's "webcore-dump/0.1
(WebKit 2.54.1)", no windows). The engine is the one constant `ENGINE` in
port/platform/mlx/UserAgentMlx.cpp: Lattice makes it "Lattice 0.0.1", the
compatibility string staying as it is (as Blink kept "AppleWebKit/537.36").
navigator.userAgent and the requests'
User-Agent header are the same string (the client's userAgent(); std.http's
client sends the request's own User-Agent instead of its default).
Per-site quirks are WebKit's platform/glib/UserAgentQuirks.cpp, compiled
unmodified (lists/webcore-mlx-sources.txt; the port defines
WTF::chassisType(), desktop, which GLib's reads from /etc/machine-info):
a quirk's string is built as buildUserAgentString builds it, without the
embedder's part. tools/check_webkit_mlx_network.sh's
user-agent page checks the header and navigator.userAgent;
tools/check_voyager.sh Voyager's string.

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
| Event loop | `USE_GENERIC_EVENT_LOOP` (RunLoopGeneric.cpp, through port/wtf/RunLoopGenericMlx.cpp, WorkQueueGeneric.cpp, MainThreadGeneric.cpp; JSCOnly's lists/wtf.txt) | No GLib; the generic loop is WTF's own code, timers included |
| Graphics | `USE_SKIA`, CPU raster; no `USE_CAIRO` | Skia compiles here already (`webkit-skia`); WebCore's only living 2D back end besides Cairo (GTK/WPE moved to Skia); CPU first, no EGL |
| Compositing | `USE_COORDINATED_GRAPHICS` and `USE_TEXTURE_MAPPER` on (since M8), layers composited on the GPU by TextureMapper over ANGLE's GLES (since M8's third part), on the CPU by `SkiaCompositingLayer` without a Vulkan device or on a CPU's (lavapipe), unless `MLX_COMPOSITING=gpu`; asynchronous scrolling on a scrolling thread | WPE's and GTK's layers, tiles, TextureMapper and scrolling tree as they are ("Compositing") |
| WebGL | `ENABLE_WEBGL` and `USE_ANGLE` (since M8): `GraphicsContextGLANGLE` with the port's GraphicsContextGLMlx, ANGLE on Vulkan (libmlxvulkan.so) | ANGLE is WebKit's WebGL everywhere; Vulkan is what std.gpu speaks, and lavapipe runs it without a GPU ("WebGL") |
| Network | neither `USE_SOUP` nor `USE_CURL` | A load is the embedder's (`LoaderStrategy`), over std.http and std.tls |
| Images | WebCore's `ScalableImageDecoder`, with our PNG, JPEG and WebP decoders; WebKit's BMP, GIF, ICO as they are | WebCore's decoders call libpng, libjpeg, libwebp; ours call std.png, std.jpeg, std.webp |
| Colour | `USE_LCMS` off, colour management in our decoders (std.colorprofile) | Little CMS is only the decoders' |
| Fonts | Skia's typefaces and our font manager over std.fontconfig | below |
| Text paths | `USE_FONT_VARIANT_VIA_FEATURES` off (GTK and WPE: on) | Kerned and shaped text goes to the complex (HarfBuzz) path through FontCascade::codePath, since GTK's and WPE's own branch for it is under `PLATFORM()`; GLib's layout tests expect it kerned |
| Text | `USE_ICU` through mlx-unicode | below |
| Theme | `USE_THEME_ADWAITA` (the class RenderTheme::singleton() is, ScrollbarThemeAdwaita: GraphicsContext drawing, light and dark; their GLib resources under `USE(GLIB)`), with the port's members and controls in std.ui.theme's colours (below, "The theme"); `ENABLE_DARK_MODE_CSS` (the page's dark appearance: the embedder's `Page::setUseColorAppearance`) | WebKit's own non-native theme, which has the dark variant the desktop's scheme asks for |
| Crypto | PAL's digests ours (std.sha1, std.sha2: `port/pal/CryptoDigestMlx.cpp`); WebCrypto, which 2.54 always compiles (there is no `ENABLE_WEB_CRYPTO` any more), is the port's `crypto/mlx` over std.crypto, std.aes, std.asn1 and std.random (M7: "Compression, WOFF, crypto") | no libgcrypt, no libtasn1 |
| Off | `ENABLE_VIDEO`, `ENABLE_WEB_AUDIO`, `ENABLE_MEDIA_STREAM`, `ENABLE_WEB_RTC`, `ENABLE_ENCRYPTED_MEDIA`, `ENABLE_WEBXR`, `ENABLE_GAMEPAD`, `ENABLE_SPEECH_SYNTHESIS`, `ENABLE_SPELLCHECK`, `ENABLE_XSLT`, `ENABLE_DRAG_SUPPORT` (until M5), `ENABLE_REMOTE_INSPECTOR`, `ENABLE_NOTIFICATIONS`, `ENABLE_GPU_PROCESS`, `USE_GSTREAMER`, `USE_ATSPI`, `USE_LIBWPE`, `USE_GBM`, `USE_LIBDRM`, `USE_AVIF`, `USE_JPEGXL`, `USE_WOFF2` | media, devices and accessibility come after the page itself |
| JavaScript | `ENABLE_JIT` and `ENABLE_DFG_JIT` (since M8's fifth part): the LLInt in x86-64 assembly (offlineasm's X86_64 back end, which mlx-cc assembles), the Baseline JIT and the DFG; no FTL (B3/Air), no WebAssembly; WebCore's CSS selector JIT (`ENABLE_CSS_SELECTOR_JIT`, since M8's sixth part) | JavaScriptCore's own tiers on x86-64, made by the port's own toolchain: bench.js runs in 1.05 s, 8.4 s with the C loop (below) |

### WTF, bmalloc, JavaScriptCore

As M1 has them: JSCOnly's sources (lists/wtf.txt: `generic/`,
`posix/`, `unix/`, `linux/` memory status), on mlxlibc. `FileSystemPOSIX`,
`ThreadingPOSIX` (signals for thread suspension: they work on mlxlibc),
`MemoryPressureHandlerUnix`, `LanguageUnix`, `TimeZone.cpp` with mlx-unicode
and std.timezone, `TextBreakIteratorInternalICUUnix`.

**The main loop.** WebCore and the embedder run on the main thread's
`RunLoop` (RunLoopGeneric: a condition variable, a tree of timers;
port/wtf/RunLoopGenericMlx.cpp compiles WebKit's file with its tree's
`remove` leaving the removed task unlinked, M8's sixth part). The
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

### Graphics: Skia on the CPU

WebCore paints through `GraphicsContext`, here `GraphicsContextSkia`
(platform/graphics/skia, compiled as WebKit has it), into Skia raster
surfaces. Without compositing (`webcore-dump --no-compositing`,
`voyager --no-compositing`, as the port was until M8) the embedder paints
the `LocalFrameView` whole into a surface made over the pixels of a
wl_shm buffer of our compositor's surface (std/src/wayland's client),
then damages and commits it.

### Compositing

Since M8 accelerated compositing is on (`MlxLayerTreeHost::configure`:
accelerated compositing, forced compositing mode, fixed position layers,
asynchronous frame and overflow scrolling, damage propagation), on the
GPU since its third part ("On the GPU" below), on any Vulkan device
(software ones too), on the CPU where there is none or when asked. What WebKit has for it, and what the
port takes:

- **The layers: WebKit's coordinated graphics, as they are.**
  `GraphicsLayerCoordinated`, `CoordinatedPlatformLayer` (a layer's state
  for the compositor, double-buffered under a lock),
  `CoordinatedBackingStoreProxy` (256-pixel tiles over the visible rect
  and a margin, painted when dirty or uncovered), the image backing
  stores, `TextureMapperAnimation` (animations of transform, opacity and
  filters played by the compositor) and `SkiaCompositingLayer` (2.54's
  compositor of a layer tree on a Skia canvas: transforms, opacity,
  clips, masks, preserve-3D's planes, and damage per frame) are compiled
  from upstream (lists/webcore-mlx-sources-apart.txt,
  `USE_COORDINATED_GRAPHICS`, `USE_TEXTURE_MAPPER` for TextureMapper's
  animation and geometry sources, `ENABLE_DAMAGE_TRACKING`). WPE and GTK
  use them in WebKit2's web process; a `GraphicsLayer` of the port's own
  would have repeated their tiling, their animations and their
  scrolling tree glue, and only coordinated graphics have
  `SkiaCompositingLayer`. On the GPU, TextureMapper (its GL painter,
  `BitmapTexture` and its pool, its shaders), `TextureMapperLayer`,
  texmap's `CoordinatedBackingStore` and `CoordinatedPlatformLayerBufferRGB`
  (CoordinatedPlatformLayerBufferRGBMlx.cpp: GLES 2's headers lack
  `GL_RGBA8`), PlatformDisplay and platform/graphics/egl's `GLContext`,
  `GLDisplay` and `GLFence` are compiled as they are, over ANGLE's GLES and
  EGL (libGLESv2.a carries ANGLE's libEGL entry points). PlatformDisplayMlx
  is ANGLE's surfaceless display on Vulkan, the one WebGL's contexts are
  made on (`mlxANGLEDisplay`), made the shared display only when a host
  composes on the GPU. `ProcessCapabilities::setCanUseAcceleratedBuffers(false)`:
  no canvas or image buffer is painted on the GPU; layers' tiles are
  (Ganesh over ANGLE, a GL context per painting thread: "The tiles'
  painting" below) unless `MLX_PAINTING=cpu`, and only then has the
  display a Skia context (`PlatformDisplay::skiaGLContext`, the port's):
  WebKit accelerates a canvas' buffers (`RenderingPurpose::Canvas`, a
  WebGL canvas drawn into a 2D one) whenever there is one.
- **The tiles' painting, the port's** (WebKit's files take GL for
  granted): `SkiaPaintingEngineMlx.cpp` records a layer's dirty region on
  the main thread (`GraphicsContextSkia` over an `SkPictureRecorder`) and
  plays the recording back into each dirty tile's raster buffer on a
  `WorkerPool` (half the cores, 1 to 8, `WEBKIT_SKIA_CPU_PAINTING_THREADS`
  as WebKit reads it); `SkiaBackingStoreMlx.cpp` keeps the tiles as raster
  `SkSurface`s on the compositor's side, copying in a buffer's dirty
  rectangle when its painting is complete, and draws them (with the
  damage's culling) for `SkiaCompositingLayer`. On the GPU's compositor
  the recordings are played back by Ganesh over ANGLE's GLES instead:
  the host gives its painting engine the proxy of the main thread's Skia
  context, and each recording (forced dark applied when it was recorded,
  images as they are) is played on a GPU painting thread
  (`WEBKIT_SKIA_GPU_PAINTING_THREADS`, 2 by default), each with its own
  GL context and GrDirectContext (PlatformDisplayMlx.cpp), into a
  texture of BitmapTexturePool's (CoordinatedAcceleratedTileBuffer),
  which the tile takes: no upload. ANGLE is built with its share context
  lock and context mutexes (angle.mlx: those contexts, the compositor's
  and WebGL's are current on several threads at once). With
  `MLX_PAINTING=cpu` the tiles are painted on the CPU as above and go to
  texmap's backing store, which uploads each into a texture
  (`BitmapTexture::updateContents`); an image a layer shows directly is
  uploaded once (CoordinatedPlatformLayerBufferNativeImageMlx.cpp). What
  kept Ganesh off was mlx-cc's (epochs 27 to 31: SkSL's module loader
  reading freed elements, GrSkSLFP built at address 0, a processor's
  flags never cleared, skvx::Vec<4,float>'s halves at one offset, which
  drew no anti-aliased path).
- **The host: `port/embed/MlxLayerTreeHost`**, what WebKit2's
  `LayerTreeHost`, `ThreadedCompositor` and `DrawingArea` are to WPE, in
  one process: the chrome client's `GraphicsLayerFactory`,
  `attachRootGraphicsLayer`, `triggerRenderingUpdate`,
  `displayRefreshMonitorFactory`, `createScrollingCoordinator` and
  `ensureScrollbarsController` go to it; it is the layers'
  `CoordinatedPlatformLayer::Client` and keeps the scene
  (`CoordinatedSceneState`'s committed layers). A rendering update
  (`Page::updateRendering`, the layers' flush, the scene committed) is on
  the main thread; the compositor (a `WorkQueue`, "MlxCompositor") paints
  the committed scene into the frame's pixels (`SkSurfaces::WrapPixels`)
  with `SkiaCompositingLayer::paint` and the frame's damage, and tells
  the embedder a frame is ready. Two modes: **Threaded** (Voyager) and
  **Synchronous** (webcore-dump: `composite()` flushes and paints on the
  calling thread, for tests; on the GPU on the compositor's thread,
  waited for, since the GL context is that thread's).
- **On the GPU** (ThreadedCompositor's TextureMapper path, WPE's): the
  compositor thread owns a GL context of ANGLE's display (surfaceless,
  sharing textures with the display's sharing context as every
  compositor's does: BitmapTexturePool's are the process's, outliving
  any page: WebKit's pool releases the textures unused for 3 seconds on
  the main thread's run loop, with the sharing context current, whichever
  host asked for them; Voyager's `VOYAGER_TRACE` counts the share group's
  textures, check_voyager.sh's 25 sees them come back after tabs close), the layers'
  targets are `TextureMapperLayer`s, and a frame (`paintFrameOnGPU`)
  applies the animations, collects the damage (the layers' damage in the
  view's coordinates, one `Damage` shared by all, as LayerTreeHost's),
  clears only what it covers to the page's white and has TextureMapper
  compose there, Y flipped, into a framebuffer of the view's size (a
  colour texture, depth and stencil for TextureMapper's clips and 3D).
  The damage's bounds are then read back into the raster frame, which
  goes to the wl_shm buffer as before: in the Threaded mode without
  waiting (GLES 3's pixel pack buffers, ANGLE's contexts being 3.1: the
  rectangle read into a buffer on the GPU's queue, a fence after it, at
  most three reads going), the frame composed without the frame's lock,
  and each read taken into the raster frame with its damage once its
  fence has signalled (at the next frame, or by a poll of the compositor
  thread every millisecond), a frame ready then; `MLX_COMPOSITOR_READ=sync`
  reads as webcore-dump's Synchronous mode does, `glReadPixels` waiting
  for the GPU. RGBA is swapped on the CPU, since ANGLE converts a BGRA
  read on the GPU (10 ms of a frame on lavapipe). On lavapipe the
  compositor thread's frame went from 5.4 to 2.5 ms (the animation page),
  12.1 to 4.7 ms (a page scrolled by whole views) and 6.0 to 2.2 ms
  (WebGL's), the read's part from 3.9, 8.4 and 4.6 ms to 0.8, 1.2 and
  0.6. On a GPU the frame is presented directly instead
  (port/angle/vulkan_present.cpp, `MlxLayerTreeHost::presentFrame`): the
  framebuffer's colour is a B8G8R8A8 VkImage of the port's on ANGLE's
  VkDevice (EGL_ANGLE_device_vulkan's handles), which ANGLE draws into as
  an EGLImage (EGL_ANGLE_vulkan_image); when the embedder takes the
  frame, the window's wl_shm buffer is imported into that device as host
  memory (VK_EXT_external_memory_host, which ANGLE enables for OpenCL
  only: the port's display asks for its feature `supportsExternalMemoryHost`
  through EGL_FEATURE_OVERRIDES_ENABLED_ANGLE, and is made without it
  where the driver has no such extension), and a copy on ANGLE's queue
  (under eglLockVulkanQueueANGLE, between glReleaseTexturesANGLE and
  glAcquireTexturesANGLE) writes the frame into it: no read back, no swap
  and no copy on the processor. On a CPU's device (lavapipe, llvmpipe,
  SwiftShader) frames are read back by default, since there the copy is
  the processor's too and the take waits for the frame's drawing, which
  the asynchronous read leaves to the device's threads (on lavapipe the
  take of an animation frame 3.9 ms presented against 1.7 ms read back and
  copied, a whole view scrolled 3.7 against 1.9, WebGL's 4.8 against
  1.7). `MLX_COMPOSITOR_PRESENT=gpu` presents directly on any device
  (check_voyager.sh passes so on lavapipe), `=read` reads back on any;
  `MLX_TRACE_COMPOSITOR` says which, why, and each take's time. Forced dark: tiles and
  images were darkened when recorded; a layer's solid colour is given to
  its TextureMapperLayer darkened after each flush (`darkenSolidColors`:
  CoordinatedPlatformLayer keeps the colour private, read through a
  pointer to the member taken in an explicit instantiation), the
  background cleared darkened. WebGL canvases are textures in the same
  share group ("WebGL"). The CPU's compositor stays: no Vulkan device
  (no display or no context), `MLX_COMPOSITING=cpu`, or an embedder's
  `MlxLayerTreeHost::setGPUCompositingAllowed(false)` (Voyager's
  `--no-gpu-compositing`). A software device (lavapipe, llvmpipe,
  SwiftShader) composes on the GPU too; the trace says it is one (ANGLE's
  EGL device's `VkPhysicalDevice`, EGL_ANGLE_device_vulkan, whose
  `VkPhysicalDeviceProperties::deviceType` is
  `VK_PHYSICAL_DEVICE_TYPE_CPU`; port/angle/vulkan_device_type.cpp). The
  checks (tools/check_webkit_mlx_*.sh, check_voyager.sh) run the default,
  and with `MLX_COMPOSITING=cpu` the CPU's compositor. On lavapipe, the
  GPU's compositor with its asynchronous reads: a frame of the animation
  page 1.7 ms (0.45 of it the read), of a page scrolled by whole views
  3.9 ms (0.8), of the WebGL page 1.5 ms (0.4), at the display's 60
  frames a second. WebGL uses ANGLE on whatever
  device there is either way. A WebGL context made current tells WebKit's
  GLContexts so (GLContextWrapper's current context dropped, as
  upstream's GraphicsContextGLTextureMapperANGLE is one): else Skia's or
  the compositor's context, current before, was taken for current still
  and their calls went to WebGL's. `MLX_TRACE_COMPOSITOR` names the renderer
  and the Vulkan device (or why the CPU's) and each frame's path; Voyager's `VOYAGER_TRACE`
  says where its tabs compose and why.
- **The frame clock: `MlxDisplayLink`**, a thread ticking at the
  display's rate (60 Hz; `MLX_REFRESH_RATE` 1 to 240), started only while
  someone wants frames: the compositor while animations run or a
  scroll animates, and the port's `DisplayRefreshMonitor`
  (`MlxDisplayRefreshMonitor`), which dispatches `displayDidRefresh` to
  the main thread, so `requestAnimationFrame` and the rendering updates
  follow the compositor's frames, not timers.
- **Scrolling off the main thread**: `ThreadedScrollingCoordinator` with
  `ScrollingTreeCoordinated` (WebKit's, `ENABLE_ASYNC_SCROLLING`,
  `ENABLE_SCROLLING_THREAD`, `ENABLE_WHEEL_EVENT_REGIONS`,
  `ENABLE_KINETIC_SCROLLING`): the frame and `overflow: scroll` boxes are
  scrolling nodes; `MlxLayerTreeHost::handleWheelEvent` does what
  WebKit2's `EventDispatcher` does (the scrolling tree asked on the
  scrolling thread whether it takes the event, the main thread's
  handlers only when a wheel event region or a non-fast-scrollable area
  needs them), and the scrolling tree moves the layers'
  positions in the compositor's scene, with fixed and sticky nodes kept
  in place, without a rendering update: no tile is painted for a scroll
  unless new tiles come into view. Keys and a scrollbar's drag go
  through the main thread (EventHandler, the scroll animators), which
  hands the new position to the scrolling tree: the layers move, and
  only what comes into view is painted.
  Scrollbars are layers: `MlxScrollbarsController` (WebKit2's
  `ScrollbarsControllerCoordinated`, on `ScrollbarsController`: the
  generic controller's `create` would be a second definition) and
  `ScrollerCoordinatedMlx.cpp` (WebKit's ScrollerCoordinated.cpp draws
  with GL textures; the port's draws the Adwaita overlay scrollbar into a
  raster image, the layer's contents).
- **Damage**: each frame's damage (`SkiaCompositingLayer`'s, the layers'
  changed and moved rectangles) goes with the frame
  (`MlxLayerTreeHost::takeFrame`: up to 32 rectangles, else the whole
  view), and Voyager passes it on: std.ui's panel damages only those
  rectangles of the wl_shm buffer (`ui.host.damage`, at most 16, else the
  whole), so a blinking caret or an animated box uploads only itself.

Skia's portable paths (projects/webkit/skia.mlx) are slow under mlx-cc
where they go a pixel at a time through SkVx's arrays or the raster
pipeline: the port sets SkOpts's row blends and fills to plain integer
versions (SkiaRoutinesMlx.cpp, the same results), and the compositor
paints through `MlxCompositingCanvas`, which draws a damaged part of a
tile at whole pixels as the whole tile under a clip (a sprite: rows
copied or blended) rather than as an image shader. `MLX_TRACE_COMPOSITOR`
prints each frame's times (the flush, the wait for the frame's pixels,
the paint), `MLX_TRACE_LAYER_PAINTS` each recording of a layer.

Voyager presents frames as they come: the compositor wakes the window's
loop through a pipe (`RunLoop::setWakeUpCallback`, `ui.host.watchDescriptor`),
and the window copies the frame and commits it. `VOYAGER_TRACE=1` prints
`voyager: compositor paints N frames M fps F`: N the layers' recordings
on the main thread, M the frames composited, F the compositor's frames in
the last second; tools/check_voyager.sh scrolls a long page (frames
composited, no recording) and runs a CSS animation (no recording, the
compositor at the display's rate, `requestAnimationFrame` at it too).

webcore-dump composites too, synchronously (`--no-compositing`: the view
painted whole, as before M8): `--png` is the composited frame, `--layers`
prints the layer tree after the render tree
(`RenderLayerCompositor::layerTreeAsText`), and with `--test` a page has
`internals.layerTreeAsText(document, flags)` and its `LAYER_TREE_*`
flags (a shim of the test runner's: WebCore's testing library, with the
rest of `internals`, is not built). tools/check_webkit_mlx_paint.sh paints
composited pages (transforms and opacity on their own layers, fixed
positions over a scrolled page, an `overflow: scroll` box scrolled,
perspective without 3D) against references, and
tools/check_webkit_mlx_compositing.sh compares WebKit's compositing/
tests' layer trees (tests/layout/compositing.txt).

Encoding images (`toDataURL`, `toBlob`): `ImageUtilitiesSkia.cpp` is not
compiled (it calls Skia's encoders, which call libpng, libjpeg and libwebp,
and includes GLContext.h); `ImageUtilitiesMlx.cpp` defines
`platformEncodeData` over std.png, std.jpeg and std.webp (a stub until
M4). Skia is to be built for the port without its codecs and encoders
(lists/skia-mlx.txt, M4), so that Skia needs no image library at all; M2
links webkit-skia's libSkia.a as it is, whose codecs the port's sources
do not reach.

### WebGL

Since M8 WebGL 1 and 2 are on (`ENABLE_WEBGL`, `USE_ANGLE`; the
`WebGLEnabled` setting on in the port's MlxSettings.yaml), on ANGLE,
WebKit's copy of it built by `webkit-angle` (projects/webkit/angle.mlx,
lists/angle*.txt) as WebKit's Linux ports configure it, but with its
Vulkan backend and SPIR-V translator in place of the GL backend over the
system's EGL:

- **Vulkan: libmlxvulkan.so, no Khronos loader.** ANGLE calls Vulkan's
  functions directly (not `ANGLE_SHARED_LIBVULKAN`). compiler/libc/vulkan
  makes `vulkan/vulkan.h` (mlx-cheaders of modules generated from
  std/registry/vulkan/vk.xml, Vulkan-Headers 1.4.362, the revision of
  ANGLE's DEPS) and libmlxvulkan.so, which opens the system's driver with
  std.vulkan's loader (VK_DRIVER_FILES and the icd.d directories, the
  first driver with a device) and dispatches each command to it through
  a table the dispatchable object's first word points to (the loader's
  word of the driver interface); `MLX_VULKAN_TRACE=1` says each command.
  SPIR-V's headers (spirv.hpp, GLSL.std.450.h) are mlx-cheaders' of
  std/registry/spirv's grammar (SPIRV-Headers' revision of ANGLE's DEPS).
  WebKit's ANGLE copy leaves out the third-party libraries its Vulkan
  backend uses: the port's projects/webkit/port/angle has its own
  `vma::` (vk_mem_alloc_wrapper.cpp: dedicated allocations, virtual
  blocks first fit; no Vulkan Memory Allocator), SPIR-V validation off
  (angle_spirv_utils.cpp, as ANGLE's release builds have it; no
  SPIRV-Tools), and libvulkan_loader.cpp (OpenLibVulkan: the
  libmlxvulkan.so the program links). The driver lavapipe is the
  system's (Mesa's), loaded at run time, never built: its libraries
  (LLVM, libstdc++) are glibc's, which mlxlibc's loader binds lazily
  (`RTLD_LAZY`, as the Khronos loader opens drivers) with the glibc names
  they call (compiler/libc/gnu.mlx).
- **The context: GraphicsContextGLMlx.** WebKit's `GraphicsContextGLANGLE`
  (its drawing buffer a texture of a framebuffer object, multisampled or
  not, preserved or not) with the platform part GTK's and WPE's
  GraphicsContextGLTextureMapperANGLE has, as the port's
  port/platform/graphics/mlx/GraphicsContextGLMlx.cpp: ANGLE's EGL display
  on Vulkan without a window system (`EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE`,
  `EGL_PLATFORM_SURFACELESS_MESA`: DisplayVkOffscreen), one for the
  process; each context surfaceless, made with WebGL's attributes
  (WebGL compatibility, robust resource initialization, no client arrays).
  ANGLE's Vulkan backend says what costs it time as debug messages
  ("GPU stall due to ReadPixels"), which WebGL writes on the console;
  other ports' GL backend says none, so they are turned off (errors stay).
- **Presenting on the GPU compositor: a shared texture.** When the
  compositor composes on the GPU (the shared display exists), a context
  is made sharing the display's sharing context, and each frame WebGL
  presents swaps the drawing buffer's texture with the compositor's
  (WPE's `swapCompositorTexture`), handed to the canvas' layer as
  `CoordinatedPlatformLayerBufferRGB` with an EGL fence sync of ANGLE's,
  which the compositor waits for on the GPU (made, it submits the
  context's open render pass, which `glFlush` leaves open; WebKit's
  `GLFence` is `HAVE(GL_FENCE)`'s, off here, and WebGL 1 has no GL sync
  objects). Nothing is read back.
- **Presenting on the CPU compositor: read back into the canvas' layer.**
  At each frame WebGL presents (`prepareForDisplay`) the
  drawing buffer, resolved from its samples, is read back (`ReadPixels`),
  flipped to the page's way up and made a raster image in the
  compositor's format (BGRA, premultiplied: drawn by the port's row
  blends, not Skia's portable pipeline a pixel at a time, which took the
  compositor 90 ms a frame for a 300x300 canvas) and handed to the canvas'
  layer (`CoordinatedPlatformLayerBufferNativeImage` through the
  coordinated contents display delegate), which SkiaCompositingLayer draws.
  The read is before WebGL clears a drawing buffer it does not preserve,
  so the layer keeps the frame presented; `preserveDrawingBuffer`,
  `toDataURL`, `readPixels`, `drawImage` of a WebGL canvas and textures
  from images are WebKit's own code over the context. A 300x300 canvas
  animated by requestAnimationFrame draws on the frame clock, about 38
  frames a second in Voyager on lavapipe ("What M8's second part
  leaves").

tools/check_webkit_mlx_webgl.sh runs WebKit's tests of fast/canvas/webgl
and webgl (tests/layout/webgl.txt; webcore-dump's `internals.settings`
has `setWebGLErrorsToConsoleEnabled` for them, and synchronous XHR reads
file: URLs, as their shaders are read), check_webkit_mlx_paint.sh paints
four WebGL pages against references, and check_voyager.sh's scenario 24
animates one in Voyager; each says so and skips them where there is no
Vulkan driver manifest.

### The theme

Pages look like the desktop: their system colours, form controls and
selection are std.ui.theme's light and dark palettes, which the port's
C++ asks of the Mlx side (`mlxweb_theme_color`, port/mlx/theme.mlx:
std.ui.theme's colours in their order, `MlxThemeColors.h`).
RenderTheme.h declares `RenderTheme::singleton()` as a
`RenderThemeAdwaita&` and that class's members are `final`, so a theme of
the port's own cannot be another class: `port/rendering/RenderThemeMlx.cpp`
defines RenderThemeAdwaita's members (WebKit's RenderThemeAdwaita.cpp,
which lists/webcore-mlx-skipped.txt leaves out, with its geometry and
sizes as they are, so layout does not move) with the desktop's colours:
Canvas (light white, dark the desktop's background), CanvasText and
FieldText (light black, as pages and WebKit's tests expect; dark the
desktop's text), Field, ButtonFace, ButtonText, ButtonBorder, GrayText,
Highlight, Mark, SelectedItem, AccentColor and LinkText (dark: the
accent), the selection (the accent at 38%, so std.ui's selection colour
over the canvas, focused or not), the controls' accent and focus rings
(ThemeAdwaita's accent set to std.ui's). Its style sheet adds a meter's
colours (RenderThemeAdwaita.h has no `supportsMeter`, so a meter is its
shadow tree, whose own style sheet only an `!important` rule of the user
agent's comes before: a page's own meter styles lose to it). The
controls are drawn by `ControlFactoryMlx.cpp` (ControlFactory::create(),
WebKit's Adwaita control parts skipped): buttons and selects of the
button colour with a chevron, check boxes and radio buttons of the
control colours and the accent, text fields of the field colour in a
control-coloured outline (the accent's with the focus), progress bars and
slider tracks of a switch's off colour filled with the accent, a white knob,
spin buttons; Adwaita's focus rings. Scrollbars stay
ScrollbarThemeAdwaita's (overlay scrollbars in the scheme's grey).

### Forced dark mode

A page with no dark design of its own is painted dark on a dark desktop,
always (Voyager has no button, key or setting for it: it follows the
desktop's scheme, as it does for its toolbar and for the pages'
`prefers-color-scheme`; on a light desktop pages are painted as they
are), as Chrome's "Auto Dark Mode for Web Contents" does, by the port's
painting layer (`MlxDarkMode.h`), not by WebKit: the embedder paints the
view through `MlxDarkModeCanvas`, a Skia canvas (an `SkNWayCanvas` in
front of the target) whose draws change their paint: a solid colour
(boxes, borders, text, SVG's paths) gets its OKLab lightness inverted
between the desktop's dark background's (0.215) and its text's (0.945),
its hue and chroma kept (less chroma where it leaves sRGB), so white
pages turn #1b1b1b, black text light and brand colours stay what they
are; a gradient or other shader goes through a colour matrix of the same
inversion in sRGB's luma (chroma kept); raster images (images, image
backgrounds, canvases, video frames: drawn as images, or as image
shaders) are drawn as they are. A user style sheet was the other way:
it cannot reach colours in images of SVG, gradients, `!important` author
rules and inline styles without rewriting a page's CSS, and it would
take a page's media queries and computed styles for its own; at paint
time every colour that reaches the canvas is covered, by one function,
and nothing of the page changes (scripts read their own colours).
Whether a page designs for dark (`mlxPageHasDarkDesign`): its root's or
its meta `color-scheme` allowing dark (`Document::resolvedColorScheme`),
or a media query on `prefers-color-scheme` with `dark` in a style sheet
of it, in one it imports or in a rule nested in another (WebCore keeps
which rules a dynamic media query affects, but not by feature: the
sheets' rules are read). `webcore-dump --dark --force-dark` paints so (flags for the tests);
tools/check_webkit_mlx_paint.sh paints `forcedark.html@forcedark` (a
light page: its canvas, Ahem text and boxes inverted, its image's pixels
kept), and pages with designs of their own (`scheme.html@forcedark`,
color-scheme; `mediadark.html@forcedark`, a media query alone) left as
they are. Content WebCore paints into an image buffer of its own first
(CSS filters, masks) is composited as an image: not darkened. With
compositing (M8) the filter is where the layers are painted: a layer's
contents are recorded through `MlxDarkModeCanvas` (SkiaPaintingEngineMlx.cpp,
when the host is told the page is forced dark: `setForcedDark` repaints
every layer), so its tiles hold darkened colours and its images as they
are, and the compositor paints through one too, for what it draws itself
(a layer's solid colour, the view's background); the tiles and image
layers it draws are images, untouched. tools/check_webkit_mlx_paint.sh
paints composited now (webcore-dump's default): `forcedark.html@forcedark`
keeps its image's pixels through the layers, `composited.html@forcedark`
darkens layers with transforms and opacity.

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
  std.tls (HTTP/1.1; HTTP/2 later), cookies
  with std.http's cookie jar and public suffix list, and gives the
  response and the data back through `ResourceLoader::didReceiveResponse`,
  `didReceiveData`, `didFinishLoading` on the main thread. `data:` URLs
  are WebCore's (DataURLDecoder), `file:` the port's (std.fs), first.
- Not soup, not curl: platform/network/soup is not compiled, nor
  `platform/soup`, `platform/network/glib`.
- **The HTTP cache on disk** (M7; port/mlx/httpcache.mlx, on the network
  thread, behind WebCore's memory cache): a private cache as RFC 9111
  has one, in the profile's cache directory (http/). GET responses of a
  cacheable status with a lifetime (max-age, Expires) or a validator
  (ETag, Last-Modified) are stored, never with `no-store` (the request's
  or the response's) or `Vary: *`, and a response to a request with
  credentials only when its Cache-Control allows a shared one; an entry
  is the head, the body and the request's and response's times, under a
  key of the method, the URL and the request's values of the headers the
  response's Vary names (each URL's Vary in a file beside its entries).
  A request (its cache mode from WebCore's ResourceRequestCachePolicy:
  MlxNetwork's `cacheModeFor`) is answered from a fresh entry with no
  request sent (RFC 9111's age against the lifetime: max-age, else
  Expires less Date, else a tenth of the time since Last-Modified, as
  WebCore's heuristic; never with `no-cache`, the request's `no-cache`,
  `max-age=0` or `Pragma: no-cache`), a stale one is revalidated
  (If-None-Match, If-Modified-Since) and a 304 updates its head and
  times, anything else replaces it; WebCore's own conditional requests
  pass through. An unsafe method that succeeds takes the URL's entries
  away. The directory stays under 64 MiB (`MLXWEB_HTTP_CACHE_LIMIT`
  bytes): the entries used longest ago go first (the files' modification
  times, read into the index at start). `MLX_NETWORK_TRACE` says which
  answers came from the cache.

### Downloads and uploads

**Downloads** are embed/MlxDownloads' (M7). MlxFrameLoaderClient decides
on them: a navigation of a link with a download attribute is downloaded
at once (PolicyAction::Download: WebCore's startDownload, while a Blob's
URL is still registered), a response is when it is an attachment
(Content-Disposition), a link's download attribute asked for it, or no
frame shows its type (application/octet-stream) and it is no error (the
load turned into a download is no failure of the frame's).

The network streams: a load's head reaches its loader as soon as it is
in, and a body that is long (more than 64 KiB) or of no known length
stays on its connection, which leaves the session (std.http.client's
requestHead), and is read on a thread of its own (readBody, through
`mlxweb_http_stream_read`) while the network queue goes on; each piece
reaches the loader on the main thread (such a body is not stored in the
HTTP cache). A frame's load turned into a download
(convertMainResourceLoadToDownload) is diverted (MlxNetwork::divert):
the body of that one request goes on into the download's file, so a
POST answered with an attachment and a URL good once download right;
the loader strategy's `remove` stops a body nobody takes. A link's
download attribute (WebCore's startDownload) and a download resumed
make a request of their own (MlxNetwork::fetch: cookies, redirects);
a blob: URL's bytes come from the process's blob registry
(BlobResourceHandle), a data: URL is decoded, a file: one read.

MlxDownloads writes the bytes as they come, on the reading thread, into
"NAME.EXT.download" (port/mlx/download.mlx, std.fs: Safari's partial
file) in the embedder's folder, NAME.EXT the name the page or the
response suggests (sanitized; the type's extension when it has none),
one free with its partial file ("NAME (1).EXT", made with O_EXCL);
once all came it is renamed (RENAME_NOREPLACE, another free name if
NAME.EXT was taken meanwhile). Pause stops the reading and keeps the
partial file; resume asks for the rest (`Range: bytes=N-`, If-Range
the response's ETag or Last-Modified): a 206 goes on, a 200 starts
again; a download that is no GET cannot be resumed and pausing it
cancels it; cancel (and a failure) removes the partial file. Each
download has its rate, start and end, type and state (running,
paused, done, failed, cancelled), and the list is kept in the profile
(`downloads.sqlite`, WebCore's SQLiteDatabase): a restart lists it, a
download that was running paused with its partial file. The embedder
sees each change (MlxDownloads::setObserver) and the list.

Voyager's folder is the user's download folder (std.xdg.userDirectory:
$XDG_DOWNLOAD_DIR, else user-dirs.dirs' XDG_DOWNLOAD_DIR, else
~/Downloads). Its downloads are Safari's: once there is one, a button
at the toolbar's right end (std.ui.glyphs' download mark), a progress
ring round it while anything downloads, the mark dropping into it when
one starts; its popover (std.ui.popup) lists them newest first, each
its file's mark by type (std.ui.glyphs.fileKind), name, progress bar
and "12.3 MB of 45.6 MB — 20 s remaining", then its size and "Done"
(or Paused, Failed, Cancelled), a stop button while it runs, resume and
cancel once paused, a magnifier once done that shows it in the
desktop's Files (std.xdg.open with org.mlx.files.desktop, which selects
a file it is given; else the folder with the folders' application); a
double click on a finished one opens it with its application
(std.xdg.open: mimeapps.list's default, else a desktop entry whose
MimeType lists the type); Clear takes the finished ones off the list.
voyager://downloads is the full list with the same actions
(voyager://downloads/ACTION?ID). webcore-dump's `--downloads DIR` saves
into DIR and lists them after its text; `--download-pause-at` and
`--download-cancel-at` pause (then resume) or cancel one at a byte
count, for the network check.

**Uploads**: `<input type=file>` asks the page's chrome client
(runOpenPanel). Voyager shows std.ui.chooser over the page (new in
std.ui: a folder's entries, folders first, those the input's `accept`
does not take dim; one file or several; keys: Up, Down, Enter,
Backspace, Escape; a path field: a path typed or pasted, Tab completing
a name as far as the folder's entries agree, Enter going to a folder or
opening a file, "/" or "~" over the list starting one; the places on
the left: Home, the user's folders that exist by std.xdg's user-dirs,
the root), in the folder Voyager last chose in (the chooser keeps it
per app in $XDG_STATE_HOME/mlx/chooser/APP_ID), else the home folder; webcore-dump's `--upload PATH` answers the chooser (clicking the
page's first file input with a user gesture). The files chosen are
File objects of WebCore's; the port's network sends a form's or a
FormData's files read from disk (MlxNetwork's body: FormData's files
and blobs resolved and read, where it sent only the data elements), and
blob: URLs, which the port did not load before, are loaded from the
blob registry (MlxPlatformStrategies' loadBlob: FileReader, fetch() of
a blob URL, Blob.text()).

### Internal pages

WebCore itself knows only about:blank and about:srcdoc; a browser's own
pages are the embedder's (Epiphany registers `ephy-about:` through
WebKit's URI scheme API, Chrome has chrome://). The port's equivalent is
`embed/MlxSchemeHandler` (WebKit2's WebURLSchemeHandler in small): the
embedder registers a scheme and a callback, and every load of a URL of it
(the main resource, subresources, fetch and XHR, synchronous ones too)
goes to the callback instead of the network, which answers with a status,
a MIME type, a charset and the bytes, at once or later on the main
thread (`MlxSchemeTask::respond`, `fail`). The loader strategy
(MlxPlatformStrategies.cpp) routes the URLs of registered schemes there.

The scheme is registered with WebCore's `LegacySchemeRegistry` as:

- **display-isolated**: `SecurityOrigin::canDisplay` lets only documents
  of the same scheme display it, so an http:, https:, file: or data:
  page can neither navigate to it (a link, a form, location: refused in
  `FrameLoader::loadFrameRequest`), frame it, load it as an image, a
  script or a style sheet (`CachedResourceLoader::canRequest`,
  `ResourceLoader::init`), fetch it or XHR it, nor redirect there
  (`DocumentLoader::willSendRequest`); a synchronous XHR is checked by the
  strategy itself. A load the embedder starts (`FrameLoader::load`: the
  address bar) belongs to no page and is not checked;
- **secure** (its pages are secure contexts);
- **handled by a scheme handler**, as WebKit2's custom schemes are: its
  URLs get a tuple origin (scheme, host, port), not an opaque one, so a
  page of it fetches its own URLs same-origin;
- not **local**: its pages get no access to file: URLs.

Voyager registers `voyager:` (projects/voyager/engine.cpp): its pages
are made by projects/voyager/pages.mlx (`voyager_page` of
libvoyager.so) from the facts the engine gathers, in std.ui.theme's
colours for light and dark through `prefers-color-scheme` (a design of
their own, so forced dark mode leaves them alone):

- `voyager://`: the index of the pages;
- `voyager://about` ("About Voyager"): the icon
  (`voyager://about/icon.png`, projects/voyager/org.mlx.voyager.png put
  into the program by the build), Voyager's version, the engine's
  (UserAgentMlx.cpp's `ENGINE`, `mlxEngine()`), a line on what it is and
  a link to the details;
- `voyager://version` ("Voyager Version", as chrome://version): Voyager,
  the engine and JavaScriptCore, Skia's milestone, the Mlx libraries
  WebKit sees as ICU, HarfBuzz, zlib, SQLite and libxml2 with the API
  versions their headers declare, std.unicode's Unicode and CLDR data,
  the toolchain with mlx-cc's code generation epoch, the git commit
  (both read at build time by projects/webkit/build.mlx into
  `voyager/VoyagerBuild.h`, the commit from .git without running git),
  the build's date (the program file's), the whole user agent, the
  kernel and the architecture (uname), the window system, the command
  line, the executable, the settings file;
- anything else: a 404 page ("Not Found").

`about:` URLs typed in the address bar (or given on the command line)
other than about:blank and about:srcdoc are Voyager's pages, as Chrome
maps about:version to chrome://version: about:about is voyager://about,
about:version voyager://version.

Checks: tools/check_webkit_mlx_network.sh registers a test scheme
(`webcore-dump --scheme test`, which lists every URL the scheme was
asked for) and has an http page try fetch, XHR (asynchronous and
synchronous), an image, a script, a style sheet, an iframe, a redirect
(the test server's /to-scheme), a link, a form and location on it: all
refused, the scheme asked nothing; the scheme's own page loads and
fetches its own URLs, a 404 among them. tools/check_voyager.sh opens
voyager://about, about:version (the versions and the user agent in its
text, which VOYAGER_TRACE writes for internal pages), about:about,
voyager://nothing and voyager://, and voyager://about on a dark desktop
(its own dark design, not forced dark).

### Storage

WebCore's `SQLiteDatabase` and `SQLiteStatement` are written on
SQLite's C API (`sqlite3*` members in SQLiteDatabase.h, which every
storage file includes). **mlx-sqlite3** (compiler/libc/sqlite3, over
std.sql) stays the port's SQLite: a minimal ABI of 59 functions WebKit
calls, implemented in Mlx; replacing the class means replacing a header
WebKit shares.

What a browser keeps is in a **profile** (embed/MlxProfile, M7), whose
providers every page of the process shares (`MlxProfile::configure` of
a PageConfiguration): a data directory (Voyager's
`$XDG_DATA_HOME/voyager`, ~/.local/share/voyager) and a cache directory
(`$XDG_CACHE_HOME/voyager`, ~/.cache/voyager). `voyager --profile DIR`
and `webcore-dump --profile DIR` take DIR for both (the cache in
DIR/cache); webcore-dump without it makes a temporary profile and removes
it at the end, so the checks stay hermetic. voyager://version shows the
two directories.

- **Web storage** (embed/MlxStorage, WebKit2's WebStorageNamespaceProvider
  and the network process's storage managers in one):
  `MlxStorageNamespaceProvider`. localStorage is one area an origin,
  shared by the process's pages and frames, read from its database the
  first time a script uses it and written back a second after a change
  and when the profile is closed: LocalStorage/<the origin's database
  identifier>.localstorage, with WebKit's `ItemTable` (WebKit2's
  SQLiteStorageArea: values as UTF-16 blobs). sessionStorage is a
  namespace a page (a tab) and top-level origin, in memory, copied into a
  page window.open makes. Quotas are WebKit's: 5 MB an origin (WebCore's
  localStorageDatabaseQuotaInBytes) and the page's SessionStorageQuota.
  Each change fires `storage` at the origin's other windows
  (StorageEventDispatcher): every page's for local storage (other tabs),
  the same page's for session storage. WebCore's settings have both off
  (`LocalStorageEnabled`, `SessionStorageEnabled`): the embedders turn
  them on.
- **IndexedDB** (embed/MlxIndexedDB): WebKit's IDB server in the process,
  as WebKitLegacy's InProcessIDBServer had it (that file is not in the
  tarball; the port's is its equivalent). `IDBServer::IDBServer` with
  its UniqueIDBDatabases and SQLiteIDBBackingStore over SQLiteDatabase
  (on mlx-sqlite3) runs on a WorkQueue of its own; the pages'
  `IDBConnectionToServer` (the PageConfiguration's DatabaseProvider)
  sends each call there with its data copied for that thread
  (`crossThreadCopy`), and the server's IDBConnectionToClient sends each
  answer back to the main thread the same way (WebKit2 sends both
  through IPC). The databases are in IndexedDB/ (IDBServer's v1/ layout);
  without a profile, in memory (MemoryIDBBackingStore). No quota is
  counted yet (the space requester grants every request).
- **Cookies**: http.mlx's jar (std.http.cookiejar) is kept in
  cookies.sqlite through std.sql's engine (port/mlx/cookies.mlx), as
  libsoup's SoupCookieJarDB keeps one: its `moz_cookies` table, read at
  start (the expired rows dropped), each change of the jar written as it
  happens (the jar's change callback); session cookies are not kept.
- **The HTTP cache** is in "Network".

Checks: tools/check_webkit_mlx_storage.sh runs WebKit's layout tests of
storage/domstorage and storage/indexeddb that tests/layout/storage.txt
lists (copied with their resources and expectations into
tests/layout/LayoutTests, as the layout tests are) with `webcore-dump
--test`, as WebKitTestRunner runs them: a `testRunner` in every frame's
window (dumpAsText, dumpChildFramesAsText, waitUntilDone, notifyDone;
any other method does nothing), made by a script the embedding puts in
when the window object is made (MlxFrameLoaderClient's window-cleared
callback, where WebKitTestRunner's injected bundle puts its own), the
console's messages, alerts and window.open (a page of its own); the text
is compared with the test's -expected.txt as the layout check compares
trees, in US/Pacific's time zone as run-webkit-tests has it: 63 of 63
pass (21 of storage/domstorage: clear, quota, complex keys and values,
the localstorage and sessionstorage tests with window.open, storage
events in frames; 42 of storage/indexeddb: basics, object stores,
indexes, cursors, key ranges, key generators, transactions, Mozilla's
and the modern ones). Known failures, left out of the list:
storage/indexeddb/keypath-basics.html (63 of 63 with it) aborted in
JavaScriptCore's collector until mlx-cc's epoch 13 (a data member
hiding a base's function: "Where it stands", M7); the tests that need
`internals` (storage-close-database-on-idle and the like) or a second
process are not taken.
tools/check_voyager.sh restarts Voyager on a page that counts its visits
in localStorage (the count goes on, sessionStorage's note is gone), and
tools/check_webkit_mlx_network.sh runs pages twice in one profile for the
HTTP cache and the cookies (below).

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
`pal/crypto/gcrypt`). WebCrypto (M7) is the port's `port/crypto/mlx`,
the `CryptoAlgorithm*::platform*` functions and the keys' platform
halves, as crypto/gcrypt and crypto/openssl are for their ports, over
the Mlx side's port/mlx/crypto.mlx (MlxWebPort.h's `mlxweb_crypto_*`):

- **Digests, MACs, key derivation**: std.crypto.hash's SHA-1 and
  SHA-2s, HMAC, HKDF and PBKDF2 (new in std.crypto.hash, with RFC 6070's
  vectors in tests/362; its keyed state is copied word by word, as an
  assignment of the struct takes a temporary of an export's value arena
  each of a hundred thousand rounds).
- **AES**: CBC (PKCS #7 padding), CTR (the counter's bits as given, a
  counter that would be used twice refused), GCM (std.crypto.gcm, which
  now takes an IV of any length and tags of 4 to 16 bytes: the GCM
  specification's cases 5 and 6), KW (std.aes's key wrap, now under
  128, 192 and 256-bit KEKs: RFC 3394's 4.2 and 4.3), on std.aes's
  constant-time cipher. AES-CFB-8's platform functions are there, the
  algorithm is not registered (WebKit deprecated it; its Linux ports
  register none).
- **EC**: std.crypto.ecc on P-256, P-384 and P-521: keys (a scalar and
  the uncompressed point; compressed points taken), ECDSA (r | s), ECDH.
- **Ed25519, X25519** (std.crypto.curve25519): WebKit's CryptoKeyOKP.cpp
  and its two algorithms define their platform functions, unsupported,
  for a port that is neither Cocoa nor GCrypt; the port's
  CryptoKeyOKPMlx.cpp compiles those three files apart with
  USE(GCRYPT)'s selection (nothing they include depends on it;
  lists/webcore-mlx-skipped.txt leaves them out of the bundles) and
  defines the platform half.
- **RSA**: std.crypto.rsa (up to 16384 bits, and a modulus of a few:
  rsa-import-spki-small-key's 64; a key of more than 4096 bits keeps its
  numbers and work on the heap), keys made with std.crypto.mpi on a work
  queue of their own (CryptoKeyRSA::generatePair; the keys made and
  handed back on the context's thread with
  ScriptExecutionContext::postTaskTo, as WebKit's Cocoa port does: the
  other operations are on SubtleCrypto's queue already), the numbers
  kept for export (JWK's, PKCS #8's CRT values). A key of more than two
  primes is taken and given back (JWK's `oth`, PKCS #8's version 1 with
  OtherPrimeInfos), its primes' product checked to be n; it works with d
  alone (no CRT over more than two primes). RSA-PSS needs HAVE(RSA_PSS), which Platform.h sets only with
  a named crypto library: CryptoKeyRSAMlx.cpp, compiled apart, sets it
  and compiles WebKit's CryptoAlgorithmRSA_PSS.cpp, and registers the
  algorithms. As WebKit's other ports, a PSS salt longer than the digest
  is refused.
- **DER**: SPKI and PKCS #8 of every kind (rsaEncryption, id-ecPublicKey
  and ECDH's id-ecDH, Ed25519, X25519), read and written with std.asn1.
- **Keys kept** (IndexedDB, postMessage): the profile's CryptoClient
  (embed/MlxProfile) serializes and wraps them as WebKit2's UI process
  does, the serialized key as it is (crypto/gcrypt's choice: WebKit's bug
  173883).
- **Random values**: page/Crypto.cpp is the port's (port/page/CryptoMlx.cpp:
  getRandomValues and randomUUID over the kernel's getrandom through
  std.random, where WebKit's reads WTF's ARC4 stream; WebKit's file is
  skipped).

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
beside it), its arena lives until it returns (state on the heap, no
slices of locals that escape), and callbacks into C++ are C
function pointers with a context pointer, called on the main thread
(through `RunLoop::dispatch`) or on the I/O thread when the C++ side says
it is thread-safe. The plugin's exports have mapped arenas, not
`--stack-arena`'s 64 KB of the caller's stack: the first font match reads
std.fontconfig's fonts and cache in one call, with more aggregates than
that holds. Its C header, `port/MlxWebPort.h`, is made by mlx-cheaders of
port/mlx/api/MlxWebPort.mlx into the port's generated directory
(`webkit-mlx-port-headers`), as every C header of the repository is.

## Milestones

| | Milestone | Done when | Check |
|---|---|---|---|
| M1 | JavaScriptCore on mlxlibc, no glibc | jsc's scripts print what the glibc-linked jsc prints, nothing of glibc mapped | tools/check_jsc_mlxlibc.sh (done) |
| M2 | WebCore compiles and links for PlatformMlx | `./mlx build webkit-webcore` makes libPAL.a and libWebCore.a with the configuration above; a program linking WebCore, JavaScriptCore, WTF, bmalloc, Skia and the port's sources on mlxlibc links with no undefined symbol | a link check: `nm -u` against mlxlibc, mlxlibcpp, mlx-unicode and the port plugin is empty |
| M3 | A page loaded and laid out headless | `mlx-out/webkit/bin/webcore-dump FILE.html` makes a Page with the port's clients, loads a `file:` or `data:` URL, lays it out at 800x600 and prints `externalRepresentation()` (rendering/RenderTreeAsText.cpp: DumpRenderTree's render tree) | render trees of LayoutTests pages that use the Ahem font (metrics independent of the system's fonts) compared with LayoutTests' `-expected.txt` of WebKit's repository at 2.54.1's revision (`platform/wpe` and `platform/glib` baselines first), and with a WPE build's WebKitTestRunner where one can be built |
| M4 | Painting into a buffer | the same program paints the view into a Skia raster surface and writes a PNG (std.png); images decoded by the port's decoders | pixel tests: reference PNGs of simple pages (Ahem text, boxes, borders, gradients, images) within a small tolerance |
| M5 | A Voyager window | Voyager shows a page on our compositor (wl_shm buffers of std/src/wayland's client), scrolls, follows links, takes keys and the mouse (focus, text fields, selection), copies and pastes | a scripted session (key and pointer events through the compositor's test interface) and a screenshot compared |
| M6 | The network | http and https pages load (std.http, std.tls with std.x509's chain checks), redirects, cookies, caching headers, forms, `fetch` and XHR, WebSockets | tools/check_webkit_mlx_network.sh: a local std.http server's pages over http and https (done); real sites by hand |
| M7 | Storage and the rest of a browser's basics | local storage, IndexedDB (mlx-sqlite3), the HTTP cache on disk, WebCrypto (`crypto/mlx`), downloads, file uploads (std.fs) | WebKit's LayoutTests of those areas that run in a single process: tools/check_webkit_mlx_storage.sh and check_webkit_mlx_crypto.sh, the network check's downloads and uploads, check_voyager.sh's (done, both parts) |
| M8 and on | Beyond the bootstrap | composited layers on the CPU (scrolling, animations), the JIT, media (std.audio, std.matroska), WebGL (ANGLE on std.gpu's Vulkan), accessibility (AT-SPI over std.dbus), processes per tab | composited layers: tools/check_webkit_mlx_compositing.sh, check_webkit_mlx_paint.sh's composited pages, check_voyager.sh's scenarios 22 and 23 (part 1 done); WebGL: tools/check_mlx_vulkan.sh, check_webkit_mlx_webgl.sh, check_webkit_mlx_paint.sh's WebGL pages, check_voyager.sh's scenario 24 (part 2 done); the JIT: tools/check_jsc_mlxlibc.sh's scripts and bench.js (part 5 done: the LLInt in assembly, Baseline and DFG) |

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
| compiler/libc/gcrypt, compiler/libc/tasn1 | WebCrypto, PAL's digests | M2 for PAL's digests (CryptoDigestMlx), M7 for WebCrypto (`crypto/mlx`): retired, nothing of the Mlx port's build uses them | std.crypto, std.aes, std.asn1, std.random |
| compiler/libc/freetype | Skia's typefaces | after M5, with the port's `SkTypeface` over std.truetype | |
| compiler/libc/harfbuzz | WebCore's Skia text code, hb-icu | after M5, if the shaping replacement meets the open point below | |
| Fontconfig (no C layer was made; std.fontconfig is Mlx) | FontCacheSkia | never needed: `FontCacheMlx.cpp` calls std.fontconfig from M3 | |
| compiler/libc/sqlite3 | WebCore's SQLite wrapper | stays for the WebKit stage | SQLiteDatabase.h is shared by all storage code |
| compiler/libc/xml2 | XMLDocumentParserLibxml2 | stays for the WebKit stage | XMLDocumentParser.h includes libxml2 |
| compiler/libc/zlib | ZStream, WebSocket deflate, WOFF, ANGLE | stays for the WebKit stage | WebKit's files call zlib |
| compiler/libc/icu (mlx-unicode) | ICU everywhere | stays: it is the port's ICU | |
| compiler/libc/pulse, compiler/libc/pipewire | MLX Audio's and MLX Capture's client libraries | not the port's to retire | WebKit's media (M8) uses std.audio directly |
| ANGLE (`webkit-angle`) | WebGL | stays: it is the port's WebGL since M8, on its Vulkan backend over libmlxvulkan.so; the GL backend over the system's EGL and GL libraries is retired with it (lists/angle.txt has no GL backend, no `USE_SYSTEM_EGL`) | WebKit's WebGL is ANGLE on every port |

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
- **Classes moved by their bytes in mlx-cc.** A class value moved into
  a temporary is still copied by its bytes where it is no call's result
  (a conditional branch whose own temporaries end first:
  `c ? f(String("x")) : g()`): an object pointing into itself (a Vector's
  inline buffer) would point into the dead one. The calls' results are
  made in place (epoch 8); the rest is open.
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
  `PlatformRSAKeyContainer` and `PlatformECKeyContainer` (an
  `MlxCryptoKey` of crypto/mlx's MlxCryptoUtilities.h).
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
- **Settings a generic file reads that WebKit generates for some ports
  only.** Page.cpp reads `settings().hardwareAccelerationEnabled()` for
  any Skia port, but UnifiedWebPreferences.yaml has the setting for GTK
  and the WPE platform only: the port's port/MlxSettings.yaml, read by
  webkit-settings after WebKit's preferences (webcore.mlx's
  `Derived.settings`), has it for the Mlx port, off.
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
- **What the link needs of the port.** Linked whole (the program's
  object, then the archives' members it needs), WebCore names functions
  a port defines that no generic file does: `PlatformKeyboardEvent::
  currentStateOfModifierKeys` and `disambiguateKeyDownEvent` (GTK's and
  libWPE's: port/platform/mlx/PlatformKeyboardEventMlx.cpp, the modifiers
  the embedder sets), `WTF::Persistence::Coder<CertificateInfo>`
  (WebCorePersistentCoders.cpp has Cocoa's, soup's and Windows' only:
  CertificateInfoMlx.cpp, the DER chain and the TLS errors in soup's
  layout), SynchronousLoaderClient's `didReceiveAuthenticationChallenge`
  and `platformBadResponseError` (soup's and curl's: a stub until M6), and
  texmap's BitmapTexture (`~BitmapTexture`, `updateContents`,
  `createSkiaBackendTexture`, which SkiaGPUAtlas names for its GL path:
  BitmapTextureMlx.cpp, stubs, as no texture is ever made
  without a display: M8's compositing paints raster tiles). Skia's src/core/SkStrikeRef.cpp, which WebKit's
  CMake list leaves out though SkFont::makeStrikeRef needs it (a shared
  libWPEWebKit tolerates the name undefined), is in lists/skia.txt. The
  sources lists/skia-skipped.txt leaves out (SkXMLParser for expat,
  SkFontConfigInterface_direct for fontconfig) and those CMake's list
  lacks (SkPathDump.cpp, GrGLAssembleWebGLInterfaceAutogen.cpp) are named
  only by archive members the program never takes (SkDOM,
  SkFontConfigInterface, SkPathOpsDebug, GrGLAssembleInterface): nothing
  to add. mlxlibc gained `inet_pton` (WebCore's IPAddress::fromString);
  the long double functions (`ldexpl` of WTF's Int128) are bound by
  mlxlibc's loader itself (compiler/libc/x87_long.mlx), which
  tools/check_webkit_mlx_port.sh counts as defined; mlx-unicode gained
  `u_strToTitle` (RenderText's `text-transform: capitalize` with a
  locale) and the `uset_` sets SmartReplace.cpp builds from patterns.
- **Binding integrity and mlx-cc's vtables.** mlx-cc's vtable pointer is
  the `_ZTV` symbol's address itself, where the Itanium ABI's points two
  slots in (`&_ZTV[2]`, past the offset to top and the type info).
  WebCore's generated bindings check a wrapped object's vtable against
  `&_ZTV[2]` (`ENABLE_BINDING_INTEGRITY`, verifyVTable) and crashed on
  XMLHttpRequest, FetchResponse and WebSocket; config.mlx sets
  `ENABLE_BINDING_INTEGRITY 0` for MLX (and JSC_ONLY) until mlx-cc lays
  vtables out as the ABI does. Open: the layout, then the define back to
  WebKit's default.
- **What M6 leaves.** A `text/plain` document crashes in
  TextDocumentParser (its fake `<meta>`'s QualifiedName is null in its
  unified unit: mlx-cc's static initialization order, to be found); the
  test server's echo answers in HTML meanwhile. Pings and beacons are
  refused, a Blob is not sent over a WebSocket, DNS prefetch and the
  network state notifier are stubs (their `STUB` lines). mlx-cc gaps
  M6 met and went around: two `static` functions of one name in one
  unified unit are merged silently (MlxNetwork's `tracing` renamed), and
  a `std::span<const char>` was taken as a `const char*`
  (`String::fromLatin1(span)` compiled; the loader parses bytes itself).
- **What M7's first part leaves.** IndexedDB counts no quota (the IDB
  server's space requester grants every request) and its server is never
  shut down, only drained at exit (`MlxProfile::close`). Local storage is written a
  second after a change and at a clean exit: a process killed in that
  second loses it. The HTTP cache keeps one response a key (no partial
  content, no Range requests, no stale-while-revalidate, no serving a
  stale entry when the network fails) and has no index file (the
  directory is listed at start, which a cache of many thousand entries
  would feel); a request's cache mode is the one WebCore gives, Voyager
  has no way yet to clear the cache, the cookies or the storage, nor a
  private (ephemeral) profile, though an ephemeral session already keeps
  local storage in memory. Storage events reach other pages of the
  process only (one process: Voyager's tabs).
- **What M7 leaves** (after its third part). A key of more than two
  primes works with d alone (std.crypto.rsa's CRT takes two). A gzip or
  deflate body is read whole and given decoded at its end (std.flate
  undoes a whole stream), so such a download shows no progress until
  then; a streamed body (more than 64 KiB, or of no known length) is not
  stored in the HTTP cache, and each has a thread of its own while it is
  read. A download that is no GET cannot be resumed (pausing it cancels
  it); there is no dialog to choose a name or a folder (Safari's default
  has none either). The file chooser has no previews.
- **What M8's fifth part leaves** (JavaScriptCore's JIT):
  - The FTL (B3 and Air) and WebAssembly are off: the next tiers.
  - Half of the Wikipedia page's time is bmalloc's (libpas') allocation,
    which mlx-cc compiles without inlining (about 41% of a profile with
    the LLInt).
- **What M8's first part leaves** (composited layers on the CPU):
  - Voyager presents a frame on its window's thread, which is WebCore's
    main thread: the compositor composites, scrolls and animates on its
    own, but a long script or layout on the main thread holds the frame
    until it ends (WebKit2's compositor presents to the UI process
    itself). std.ui's window on a thread of its own, the engine's main
    thread beside it, would end that.
  - The frame clock is 60 Hz (`MLX_REFRESH_RATE`), not the output's
    (wl_output's mode, wp_presentation's feedback).
  - The chrome clients (Voyager's, webcore-dump's) derive from WebCore's
    EmptyChromeClient and override members it declares `final`
    (`attachRootGraphicsLayer`, `triggerRenderingUpdate`,
    `displayRefreshMonitorFactory` and others, as `createWindow` already
    was): mlx-cc accepts it, a conforming compiler would not. A
    ChromeClient of the port's own, whole, is the way out.
  - Overlay scrollbars are not shown (as before M8: no
    ScrollbarsControllerGeneric to fade them in, whose `create` is a
    second definition of ScrollbarsController's), though a drag of one
    scrolls; their layers' contents are drawn on the CPU
    (ScrollerCoordinatedMlx.cpp) once they show.
  - Skia under mlx-cc takes its portable paths (no vector registers):
    the port's row blends and fills (SkiaRoutinesMlx.cpp) and the
    compositor's sprites (MlxCompositingCanvas) make translated layers
    cheap (an animated layer of 200x200 in 3.5 ms a frame, was 90 ms),
    but a scaled or rotated layer's tiles, filters and masks still go
    through the raster pipeline a pixel at a time.
  - 103 of the 374 tests of compositing/ that dump a layer tree pass
    (tests/layout/compositing.txt). The others: functions of
    `internals` webcore-dump's shim does not have (its settings,
    repaint tracking, `setUsesOverlayScrollbars`), the generic
    expectations' layer sizes from macOS's fonts, tile coverage and
    visible rects (`LAYER_TREE_INCLUDES_*` WebKit2 fills), iframes'
    text in dumpAsText, and layers WebKit2's drawing area adds.
- **What M8's third part leaves** (compositing on the GPU):
  - Presenting without a copy at all: on a GPU the frame is copied by
    the device into the window's wl_shm buffer (imported as host memory),
    on a CPU's device read back. Handing the Wayland compositor the frame
    itself needs a dmabuf (linux-dmabuf: projects/desktop/compositor
    accepts one linear ARGB8888 plane) of ANGLE's framebuffer in
    exportable memory of a DRM device (VK_EXT_external_memory_dma_buf with
    DRM format modifiers; the port has no GBM, `USE_GBM` off), which
    lavapipe without a render node cannot give, so nothing here could test
    it; or a Vulkan swapchain of std.ui.gpu_canvas's
    (VK_KHR_wayland_surface), which needs ANGLE's image and std.ui's
    device to meet (std.vulkan's instance is not ANGLE's) and the page its
    own wl_subsurface, which std.ui's panel does not make yet. The direct
    copy is waited for on the main thread (the take, through the
    compositor's thread for its context); a copy into a buffer the window
    commits later would not be.
  - Images and 2D canvases are painted on the CPU and uploaded (tiles are
    Ganesh's): no GPU atlases of images, no deferred display lists, and
    2D canvases stay on the CPU (ProcessCapabilities says no accelerated
    buffers).
  - Forced dark's solid colour layers are darkened through a pointer to
    CoordinatedPlatformLayer's private `m_contentsColor` (taken in an
    explicit instantiation). A hook the port owns needs one upstream
    does not offer: the colour RenderLayerBacking gives
    `GraphicsLayer::setContentsToSolidColor` is kept privately by
    `GraphicsLayerCoordinated` (a `final` class, so no port subclass from
    the layer factory), by `CoordinatedPlatformLayer` (whose setter is
    public, no getter; its client is told only that something changed)
    and by `TextureMapperLayer` (setter only); the colour is drawn by
    `TextureMapperSolidColorLayer`, inline in TextureMapperLayer.cpp, through
    `TextureMapper::drawSolidColor`, neither virtual nor the port's. A
    `contentsColor()` getter on CoordinatedPlatformLayer upstream (or a
    colour filter of the TextureMapper's, as MlxDarkModeCanvas is the
    CPU compositor's) would let the trick go.
  - `HAVE(GL_FENCE)` is off in the port's configuration (turning it on
    rebuilds WebCore whole): WebKit's `GLFence::create` gives no fence,
    so the WebGL frame's fence is the port's (an EGL fence sync); tile
    uploads need none (the compositor's own context).
- **What M8's second part leaves** (WebGL):
  - Voyager's WebGL page runs at about 38 of the display's 60 frames a
    second on lavapipe: a frame's main thread is the script and ANGLE's
    draw, the read back (about 3 ms waiting for lavapipe's fence), the
    image made (1.5 ms) and Voyager's window drawn again, its translucent
    sidebar blended (std.ui's fills, 4 ms), past the frame's 16 ms. The
    window's chrome drawn only where it changed, and the read back
    asynchronous (a pixel buffer read the frame after), would make it.
  - The Mlx compiler's shared libraries chain their symbols in one
    System V bucket (compiler/selfhost/object/elf64_dynamic.mlx); the
    loader indexes them itself. Real buckets (or a GNU hash) belong in
    the compiler, with its next canonical build.
  - On the CPU compositor the drawing buffer is still read back each
    frame and copied into a raster image (a 300x300 canvas: about 0.4 MB
    a frame); on the GPU compositor it is a shared texture.
  - WebGL runs in the web process's main thread, as WebKitLegacy had it
    (no GPU process); a context's ANGLE work, lavapipe's draws included,
    is the main thread's time.
  - vk_helpers.cpp of WebKit's ANGLE calls a computeRowDepthSkipBytes
    of seven arguments that ANGLE's own copy does not declare (WebKit's
    changes.diff); projects/webkit/port/angle/VkHelpersMlx.h, included
    before it, maps that call to the declared one. The port's `vma::`
    (no Vulkan Memory Allocator in WebKit's copy) allocates each image
    and buffer memory of its own, first fit in ANGLE's pools' blocks: no
    defragmentation, no budget.
  - The tests of fast/canvas/webgl and webgl that do not pass yet (not in
    tests/layout/webgl.txt): out-of-range draws that ANGLE's Vulkan
    backend allows with robust buffer access where the expectations
    (other backends) have INVALID_OPERATION (draw-elements-out-of-bounds,
    index-validation-with-resized-buffer, oes-element-index-uint),
    webgl-compressed-texture-size-limit's INVALID_VALUE for the expected
    INVALID_OPERATION, WEBGL_depth_texture present where the GLib
    expectation has none, the draft extensions of base vertex and base
    instance not offered (webgl-draft-extensions-flag-*), a blank line
    more in the text of pages whose canvases are inline
    (copy-tex-image-and-sub-image-2d, tex-sub-image-2d-bad-args),
    uninitialized-test past webcore-dump's minute on lavapipe,
    OffscreenCanvas and SharedArrayBuffer (off in the port), and two
    conformance tests not looked into yet: buffer-preserve-test (driven
    by requestAnimationFrame; its harness' buffered messages come out on
    the console) and conformance2's instanced-arrays (a pixel of an
    instanced draw not the expected red; the paint check's instanced
    page is drawn right).
