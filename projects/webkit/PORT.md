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
| Event loop | `USE_GENERIC_EVENT_LOOP` (RunLoopGeneric.cpp, WorkQueueGeneric.cpp, MainThreadGeneric.cpp; JSCOnly's lists/wtf.txt) | No GLib; the generic loop is WTF's own code, timers included |
| Graphics | `USE_SKIA`, CPU raster; no `USE_CAIRO` | Skia compiles here already (`webkit-skia`); WebCore's only living 2D back end besides Cairo (GTK/WPE moved to Skia); CPU first, no EGL |
| Compositing | `USE_TEXTURE_MAPPER` and `USE_COORDINATED_GRAPHICS` off; accelerated compositing off in the Page's settings | TextureMapper is GL; a view painted whole into a buffer needs no layers at first |
| Network | neither `USE_SOUP` nor `USE_CURL` | A load is the embedder's (`LoaderStrategy`), over std.http and std.tls |
| Images | WebCore's `ScalableImageDecoder`, with our PNG, JPEG and WebP decoders; WebKit's BMP, GIF, ICO as they are | WebCore's decoders call libpng, libjpeg, libwebp; ours call std.png, std.jpeg, std.webp |
| Colour | `USE_LCMS` off, colour management in our decoders (std.colorprofile) | Little CMS is only the decoders' |
| Fonts | Skia's typefaces and our font manager over std.fontconfig | below |
| Text paths | `USE_FONT_VARIANT_VIA_FEATURES` off (GTK and WPE: on) | Kerned and shaped text goes to the complex (HarfBuzz) path through FontCascade::codePath, since GTK's and WPE's own branch for it is under `PLATFORM()`; GLib's layout tests expect it kerned |
| Text | `USE_ICU` through mlx-unicode | below |
| Theme | `USE_THEME_ADWAITA` (the class RenderTheme::singleton() is, ScrollbarThemeAdwaita: GraphicsContext drawing, light and dark; their GLib resources under `USE(GLIB)`), with the port's members and controls in std.ui.theme's colours (below, "The theme"); `ENABLE_DARK_MODE_CSS` (the page's dark appearance: the embedder's `Page::setUseColorAppearance`) | WebKit's own non-native theme, which has the dark variant the desktop's scheme asks for |
| Crypto | PAL's digests ours (std.sha1, std.sha2: `port/pal/CryptoDigestMlx.cpp`); WebCrypto, which 2.54 always compiles (there is no `ENABLE_WEB_CRYPTO` any more), is the port's `crypto/mlx` over std.crypto, std.aes, std.asn1 and std.random (M7: "Compression, WOFF, crypto") | no libgcrypt, no libtasn1 |
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
(CSS filters, masks) is composited as an image: not darkened.

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
| compiler/libc/gcrypt, compiler/libc/tasn1 | WebCrypto, PAL's digests | M2 for PAL's digests (CryptoDigestMlx), M7 for WebCrypto (`crypto/mlx`): retired, nothing of the Mlx port's build uses them | std.crypto, std.aes, std.asn1, std.random |
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
  BitmapTextureMlx.cpp, a stub until M8, as no texture is ever made
  without a display). Skia's src/core/SkStrikeRef.cpp, which WebKit's
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
