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
| GLib, GObject, GIO | WTF's run loop and threads (done); GIO streams, files, D-Bus, network monitor, proxies, resolver, TLS types (libsoup's headers name GTlsCertificate) in WebCore's soup and GLib platform code; WebKit's whole GObject API, IPC, WPEPlatform | 173 / 502 (571 together) | 644 (all: WTF's headers) | mlx-glib (compiler/capi/glib): 397 of the 571, and GIO's TLS types (GTlsCertificate from PEM and DER, chains, keys; GTlsPassword; a backend without TLS whose database fails cleanly), GInitable, the stream classes and `G_DECLARE_INTERFACE` that libsoup's headers declare with; missing the rest of WebCore's 50 (GDBus, GResolver, GProxyResolver, GNetworkMonitor, the streams' functions, TLS connections) and 124 more of WebKit's | 1: extend mlx-glib, WebCore's first; the headers every unit needs are there |
| libsoup 3 | HTTP: every network load (NetworkProcess), cookies, HSTS, authentication, proxies, the public suffix list (WebCore's soup platform code, WebKit's NetworkProcess/soup) | 82 / 75 (143) | 643 (ResourceRequest/Response headers, in the prefix) | none (no HTTP in std) | 1: mlx-soup, a C API layer over a new std HTTP client: HTTP/1.1 first, cookies, HSTS; TLS through GIO's TLS (a TLS implementation is the largest part) |
| ICU | text: breaking, collation, normalization, bidi, encodings, search, IDNA | 65 / 6 (66) | 643 | mlx-unicode (compiler/capi/icu): all but `u_strToTitle` and the 6 `uset_*` of SmartReplace and ResourceResponseSoup (declared, not yet made); `ubidi.h` (std.unicode.bidi: UAX #9, BidiTest and BidiCharacterTest all passed), `usearch.h` (std.unicode.search) and `ucnv.h`, `ucnv_err.h`, `ucnv_cb.h` (std.unicode.converters: what TextCodecICU asks for, as ICU 74 does) are there | 2: `u_strToTitle` and sets |
| HarfBuzz (+hb-icu) | text shaping (FontCascade, Skia's) | 46 / 0 (/ 57) | 643 | mlx-harfbuzz (compiler/capi/harfbuzz, std.shaping): all 46 and Skia's 57 declared | done (to be checked at run time) |
| FreeType | font files, glyphs (Skia's font host) | 0 / 0 (/ 52) | 0 | mlx-freetype (compiler/capi/freetype, std.truetype): Skia's 52 declared | done for compiling |
| Fontconfig | finding fonts: Skia's font manager, WebCore's FontCache (FreeType platform) | 21 / 0 (/ 53) | 2 | none | 2: mlx-fontconfig, matching over the fonts directories (fonts.conf's essentials) |
| libxml2 | XML documents (XHTML, SVG as XML) parsed by XMLDocumentParserLibxml2; XSLT's input | 59 / 4 (59) | 10 | std.xml: a pull tokenizer, a base | 2: mlx-libxml2: the SAX2 push parser WebCore drives (xmlCreatePushParserCtxt, xmlParseChunk, the callbacks), entities, encodings through ICU (its headers' `<iconv.h>` is mlxlibc's now) |
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
| gettext (`libintl.h`), `iconv.h` | GLib's `gi18n-lib.h` (the `_()` of WebKit's GLib code); libxml2's headers | | | mlxlibc: `libintl.h` (the gettext family returning the message untranslated, the domains remembered) and `iconv.h` (UTF-8, -16, -32, UCS-2 and -4, Latin-1, ASCII and std.unicode.charmaps' single-byte charsets, `//TRANSLIT` and `//IGNORE`; tools/check_iconv.sh against glibc) | done |
| expat, libpsl, brotli, libatomic | expat: Skia on Android only; libpsl, brotli: libsoup's own; libatomic: GCC's atomics | 0 | 0 | | not needed by WebKit directly |

So the order: the headers and small layers every unit needs (GIO's TLS
types, `libintl.h`, `iconv.h`, ICU's bidi, search and converter
headers: done), then libsoup and its HTTP and TLS, the image decoders,
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
| (a) a header of a missing library | 1: libxml2's `<iconv.h>` (mlxlibc had none then) |
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

### With the headers made here

GIO's TLS types, `libintl.h`, `iconv.h` and ICU's `ubidi.h`,
`usearch.h`, `ucnv.h`, `ucnv_err.h`, `ucnv_cb.h` are made of Mlx modules
now (above). `./mlx build -j 1 --keep-going webkit-wpe-webcore` then
stops at one header only: all 653 C++ units (518 unified sources and 135
compiled apart) fail at `libsoup/soup.h` (CertificateInfo.h:31, from
the prefix's ResourceRequest), and the one C source
(AccessibilityAtspiInterfaces.c) compiles.

A scratch run with the system's libsoup 3 headers added after mlx-glib's
(nothing of it kept: the build was patched for the run alone) took
libsoup's headers over mlx-glib's once `G_DECLARE_INTERFACE` and GIO's
input stream classes were there. It compiled a sample of whole unified
sources, every tenth of the 518 (52 units, about ten files each, unlike
the single files above), one or two at a time:

| | Units |
|---|---|
| compile | 11 of 52 (Modules-57, -66, cssjit-1, dom-18, domjit-1, platform-20, -39, -48, -57, -66, plugins-1; and dom-1 outside the sample) |
| (a) a header of a missing library | 3: `gcrypt.h` (2: crypto, the bindings of WebCrypto), `sqlite3.h` (1: workers) |
| (b) mlx-cc's memory: "too many names" or the arena | 14 (accessibility, css, html, inspector, layout, mathml, platform, rendering, style) |
| (b) mlx-cc errors | 24, below |

No unit stopped at a header of GLib, GIO, ICU, `iconv.h` or `libintl.h`.

| mlx-cc error | Units | Where |
|---|---|---|
| `expected ';'` in StylePrimitiveNumeric.h:126 | 3 | style values |
| `static assertion failed` in WTF::switchOn's visitor | 3 | Variant.h:1415 |
| `expected ';'` reading ConversionResult's `ReturnType` | 2 | JSDOMConvertResult.h:159 |
| WTF::Function's constructors (`too few arguments`, `no such member`) | 2 | Function.h:56, 59 |
| mpark::variant's and `std::optional<variant>`'s converting constructors | 2 | GPURenderPassColorAttachment.h:49, IDBRequest.cpp:105 |
| one each: `not a namespace` (SVGPathSegImpl.h:46), `expected an expression` (WebLock.h:39), `not a compile-time constant` (CSSPrimitiveNumericRaw.h:55), `invalid pointer dereference` of a WeakHashMap (UserActionElementSet.cpp:38), `a conversion operator takes a parameter list` (ImageDataArray.cpp:196), `no such name in the namespace` (WebGLExtension.h:123), `no such member` (FontCustomPlatformData.h:129), `no conversion to bool` (RectEdges.h:186), SpaceSeparatedArray's constructor (CSSBorderRadius.h:66), `expected a function template` (StyleValueTypes.h:513), `invalid numeric constant` (SVGToOTFFontConversion.cpp:163), `expected ','` (SVGPropertyOwnerRegistry.h:72) | 12 | |

## The compiler's second pass

### Memory

mlx-cc never frees its arena, so a unit costs what it ever made. Of the
heavy units' 8 GB and 4 million names, most went to four things:

- each check of a partial specialization's constraints made a name of
  its own (`partial#check`: 2.6 million of a unit's 4 million names),
  and weighing the partial specializations copied the arguments for
  each (1.7 GB);
- a qualified type name (`typename A<B>::C::D`, nested in the style
  types' templates) was read again whole wherever it was named, each
  reading reading its own qualifiers again (3.6 GB of types);
- every header's path was copied while it was looked for, its file read
  into a buffer grown by doubling, and its tokens copied into the
  stream.

Now a partial specialization is weighed on one flattened copy of the
arguments with no names made, a qualified type name read where nothing
it depends on changed (the scope, the cursor, the instance being made)
is remembered, and a header is read in its size (`fstat`), looked for
in one buffer, its tokens spliced into the stream. Arena bytes
(`MLX_CC_STATS=1`), peak RSS and seconds of a unit, alone:

| Unit | Before | After |
|---|---|---|
| JavaScriptCore's UnifiedSource-runtime-1 | 2.33 GB, 1668 MB, 27.7 s | 1.91 GB, 1420 MB, 24.7 s |
| JavaScriptCore's UnifiedSource-bytecode-1 | 2.40 GB, 1740 MB, 29.2 s | 1.95 GB, 1466 MB, 26.4 s |
| NavigatorCredentials.cpp | 3.26 GB, 2527 MB, 46.5 s | 2.36 GB, 1884 MB, 38.6 s |
| Storage.cpp | 3.46 GB, 2722 MB, 51.0 s | 2.52 GB, 2049 MB, 43.2 s |
| JSANGLEInstancedArrays.cpp | 4.18 GB, 3391 MB, 67.5 s | 3.23 GB, 2759 MB, 58.2 s |
| TrailingObjects.cpp | 4 million names at 94 s (7.4 GB, 6.4 GB RSS) | compiles to an error at 61 s, 2807 MB |
| CSSPropertyParserConsumer+Inline.cpp | 4 million names | compiles: 2.46 GB, 1938 MB, 42 s |

None of the survey's eight units that ran out of names or arena runs out
now: each compiles, or stops at an error of the compiler, under 3.9 GB of
RSS (InspectorCSSAgent.cpp, the largest, 124 s). JavaScriptCore's units
make the same functions as before (their objects compared function by
function).

### The survey's errors

Each with a program in compiler/cc/tests/cpp:

| mlx-cc | Test | Survey error |
|---|---|---|
| a function only declared names its result's class template instance, does not make it | declaredresult | ConversionResult made too early (JSDOMConvertResult.h:159) |
| a class template's default argument names its class, does not make it | defaultnamedonly | `MarkableTraits<GlyphOverflow>` specialized after its instance |
| template value parameters of reference and pointer-to-member types | referenceparams | SVGPropertyOwnerRegistry.h:72 |
| CSS literals: `E unit` deduced, both constraints of a declaration kept, inherited constructors weighed; std::is_constructible weighs constructor templates | valueliteral, constructibletemplates | StylePrimitiveNumeric.h:126 |
| a trailing requires-clause sees a function parameter pack; a class without a conversion to bool fails a requirement, not the unit; a requires-expression leaves no temporaries | requirespack, requiresnot, requirestemporaries | the style values' constraints |
| a base's member type hides a namespace's name in the derived class (not an instance's dependent bases); a base's typedef of a class qualifies names; `using Base::Base;` through such a typedef | basememberhides, inheritedqualifier, inheritedctoralias | the style types |
| algorithms, iterators and constexpr objects in constant expressions; reverse_iterator's converting constructor, no `const T *` to `T *` | constexpralgorithms, reverseconvert | |
| a missing name inside a qualified name or a template argument fails the candidate (SFINAE) | sfinaenested | |
| `Ts&&` of a class's own parameters is no forwarding reference in its implicit guides | ctadrvalue | |
| of two partial specializations otherwise alike, the one without a pack expansion; an alias's pack given one argument is that argument | partialpackorder | |
| an abbreviated call operator's parameters found after `operator()` | abbreviatedcall | StyleValueTypes.h:513 |
| a member specialized explicitly before its class's instance keeps that definition | memberspecializedfirst | JSDOMConstructorNotConstructable.h:48 |
| a call of one function in SFINAE checks how its references bind | invocablerefs | `std::is_invocable<void(int&&), int&>` held |
| member function templates weighed by their ref-qualifiers (`get_alt<I>() &&` of a moved variant) | refqualifiedtemplates | WTF::switchOn with `T&&` visitors (Variant.h:1415) |
| a constructor template's signature weighs its own conversions; a copy constructor takes a derived class's object | convertingtemplate | mpark::variant's converting constructor (`"none"_s`, `return options;`) |
| a range-based for keeps end()'s sentinel as its own type | rangesentinel | ElementChildIterator's and ElementDescendantIterator's constructors |
| an empty pack an inner `...` expands leaves the outer expansion | nestedpackexpansion | Brigand.h:358 |
| braces choose a constructor template taking an initializer_list | listconstructortemplate | brigand::any always true (a union taken for one holding undefined) |
| an instance being read is told apart by its template too | openinstances | |

The 52 files of the survey above (the first file of every tenth unified
source, each alone, with the same stand-in headers):

| | Files |
|---|---|
| compile | 35 of 52 (28 before this pass) |
| (a) a header of a missing library | 1: `<iconv.h>` (the survey's stand-ins predate mlxlibc's) |
| (b) mlx-cc's memory | none |
| (b) mlx-cc errors | 16, below |

| mlx-cc error | Files | Where |
|---|---|---|
| `dereferencing a void pointer`: WTF's VariantBestMatch, through `using Bases::operator()...` of a pack of bases | 5 | CompactVariantOperations.h:99 |
| `more names in the structured binding than the class has members` | 3 | ColorModels.h:94 |
| EnumSetOrKeywordBase's constructor | 2 | StyleComputedStyleProperties+InitialCustomInlines.h:130 |
| mpark::variant's move assignment visiting `Expected<Ref<T>, String>` (`invoke` of its generic lambda) | 1 | Variant.h:1340 |
| PrimitiveNumericRaw's constructor | 1 | StylePrimitiveNumericTypes+CSSValueConversion.h:96 |
| `no operator<<` | 1 | StyleKeyword+Logging.h:36 |
| `not a compile-time constant` | 1 | StylePrimitiveNumericTypes+EvaluationMinimum.h:82 |
| `static assertion failed` | 1 | StdLibExtras.h:870 |
| `expected a type as the template argument` | 1 | StdMap.h:33 |

## What is next

- libsoup: every unit includes its headers (mlx-soup, its C API over an
  HTTP client in std, is the first library to write).
- mlx-cc's memory: the single files fit now (above); whole unified
  sources are about ten such files, and still heavier ones may need the
  instances' tokens and mangled names shared (the way an alias's
  expansion is shared) or a failed candidate's dropped.
- The remaining compiler errors of the survey, most common first.
- Then the libraries in the order above, libsoup's HTTP and TLS being the
  largest.
