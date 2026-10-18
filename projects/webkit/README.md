# WebKit, built by this repository

WPE WebKit and the libraries it needs, built with this repository's own
toolchain alone: `./mlx build` (build.mlx, std.build) declares the steps,
mlx-cc compiles every C and C++ file, mlx-ar makes the archives, the
Mlx compiler's linker links, mlxlibcpp and mlx-unicode (projects/unicode,
ICU's C API in Mlx) are the libraries WebKit runs on. What WebKit's
CMake build makes with Perl, Python, Ruby and gperf is made here by
programs written in Mlx (generators/), and their output is that of
WebKit's own generators to the byte.

The sources are WPE WebKit 2.54.1's tree: `WEBKIT_SOURCE` (the directory
with `Source/`), by default /home/user/csrc/wpe/wpewebkit-2.54.1.

Two of WebKit's ports are built, by the same steps from a description
of each (`Port` in build.mlx): JSCOnly (JavaScriptCore alone, the
generic event loop) and WPE, whose lower layers stand on GLib, GObject
and GIO, here mlx-glib (projects/glib).

    ./mlx build webkit-bmalloc        bmalloc and libpas (libbmalloc.a)
    ./mlx build webkit-wtf            WTF, the JSCOnly port, on mlx-unicode (libWTF.a)
    ./mlx build webkit-jsc-derived    what JavaScriptCore's build generates
    ./mlx build webkit-webcore-derived  what WebCore's build generates (the WPE port)
    ./mlx build webkit-jsc            JavaScriptCore, C_LOOP (libJavaScriptCore.a)
    ./mlx build webkit-jsc-shell      mlx-out/webkit/bin/jsc

    ./mlx build webkit-wpe-bmalloc    the WPE port's bmalloc
    ./mlx build webkit-wpe-wtf        WTF for WPE (PlatformWPE.cmake's sources) on mlx-glib and mlx-unicode
    ./mlx build webkit-wpe-jsc-derived  what JavaScriptCore's build generates for WPE
    ./mlx build webkit-wpe-jsc        JavaScriptCore for WPE, C_LOOP, with its GLib API and the remote inspector's GLib sources
    ./mlx build webkit-wpe-jsc-shell  mlx-out/webkit/wpe/bin/jsc
    ./mlx build webkit-wpe-wtf-test   mlx-out/webkit/wpe/bin/wtf-test (tests/wtf-wpe.cpp)
    ./mlx build webkit-wpe-jsc-glib-test  mlx-out/webkit/wpe/bin/jsc-glib (tests/jsc-glib.c)

    ./mlx build webkit                all of it

    tools/check_webkit_generators.sh [jsconly|wpe]  the generated sources against those of a build by WebKit's generators (JSC_REFERENCE)
    tools/check_webcore_generators.sh WebCore's likewise (WEBCORE_REFERENCE, a WPE build's WebCore/DerivedSources)
    tools/check_wpe_wtf.sh            WTF (WPE) at work: run loop, timers, work queues, strings, URLs
    tools/check_wpe_jsc.sh            JavaScriptCore (WPE) at work: jsc on tests/wpe.js, the GLib API's tests/jsc-glib.c

The JSCOnly port's objects go to mlx-out/webkit/obj, its archives to
mlx-out/webkit/lib, what is generated to mlx-out/webkit/generated
(JavaScriptCore/ for JSC's, WebCore/ for WebCore's); the WPE port's to
mlx-out/webkit/wpe/obj, lib, generated and bin.

## The ports

A `Port` says what WebKit's CMake build takes from `Options<PORT>.cmake`
and the `Platform<PORT>.cmake` files of each library: the configuration
header's lines (`cmakeconfig.h`, NAME VALUE, in config.mlx), the
feature defines the generators take, where the files go, the defines
of bmalloc's, WTF's and JavaScriptCore's sources (as the reference
build's compile lines have them), the source lists, the include
directories and libraries beyond every port's. The steps of a port
other than JSCOnly are named with its tag (`webkit-wpe-wtf`,
`wpe-libWTF.a`, `wpe-wtf/WTF/wtf/URL.cpp`); the generators are
programs shared by the ports.

| | JSCOnly | WPE |
|---|---|---|
| configuration | `config.JSC_ONLY` | `config.WPE`: the `cmakeconfig.h` of a WPE build with C_LOOP, Skia, no video or web audio |
| WTF's sources | `lists/wtf.txt` | `lists/wtf-wpe.txt` (the reference build's 189: `wtf/glib`, `generic/MainThreadGeneric`, `WorkQueueGeneric`, `linux`, `posix`, `unix`, `android`) |
| JavaScriptCore's source lists | `Sources.txt` | and `inspector/remote/SourcesGLib.txt` |
| JavaScriptCore's sources apart | | `lists/jsc-glib-sources.txt` (the GLib API, `API/glib`) |
| forwarding headers | `lists/jsc-headers.txt` | and `lists/jsc-headers-glib.txt` (the remote inspector's) |
| JavaScriptCore's GLib API | | `JavaScriptCoreGLib/`: `jsc/*.h` of `lists/jsc-glib-api.txt` (the 2022 API), `JSCVersion.h`, `Headers/jsc` |
| defines | `BUILDING_JSCONLY__` | `BUILDING_WPE__`, `JSC_GLIB_API_ENABLED`, `GETTEXT_PACKAGE`, `PKGDATADIR` |
| beyond mlx-unicode | | mlx-glib's headers and `libmlxglib.so` |

The inspector's protocol follows each port's feature defines
(`ENABLE_REMOTE_INSPECTOR` and the others of WPE). For WPE, every file
of JavaScriptCore's DerivedSources is the reference build's to the
byte but `LLIntAssembly.h`: the interpreter's offsets are those of the
compiler that compiled the offsets extractor, and GCC lays out
`[[no_unique_address]]` members, which mlx-cc does not, otherwise (in
the WPE configuration `JSC::VM`'s first members). The check compares
it with what WebKit's own offlineasm (Ruby) makes of mlx-cc's
extractor, and that is the same to the byte.

`tests/wtf-wpe.cpp` runs WTF (WPE) on mlx-glib: `RunLoop::run()` on
GLib's main loop with a repeating `RunLoop::Timer` and
`RunLoop::dispatchAfter`, `WorkQueue::create` and `dispatch` from its
thread back to the main one, `WTF::String` (UTF-8, case, splitting,
numbers) and `URL` (parsing, relative URLs, file paths).
`tests/wtf-wpe.expected` is what the same program prints built by GCC
against the reference build's WTF and GLib 2.80.

`tests/jsc-glib.c` is a C program of JavaScriptCore's GLib API for WPE
(`<jsc/jsc.h>`: a context evaluating scripts, values made from C, C
functions and a C class called from JavaScript, an exception), and
`tests/wpe.js` a script for the WPE port's jsc; their `.expected` files
are what JavaScriptCore prints for them.

## Parts

| File | What |
|---|---|
| `build.mlx` | The steps: the libraries from WebKit's source lists, the generators and their outputs, the forwarding headers (`<bmalloc/...>`, `<JavaScriptCore/...>`), the configuration header |
| `config.mlx` | The JSCOnly and WPE configurations (`cmakeconfig.h`'s defines) and their feature defines |
| `webcore.mlx` | WebCore's generated sources (WPE): the generators run as WebCore's CMake files run its scripts, gperf after them, the copies of the source lists and their unified sources |
| `unified.mlx` | Unified sources: generate-unified-source-bundles.py's bundles of a source list (Sources.txt), made while the steps are declared |
| `lists/` | Source lists the build takes from WebKit's CMake files: WTF's and bmalloc's sources for each port, the `.lut.h` sources, JSC's headers, builtins and inspector domains; WebCore's IDL files (in CMake's order, which the bindings' batches follow), builtins, user agent style sheets, media controls' style sheets and scripts, AT-SPI interfaces |
| `generators/` | WebKit's code generators in Mlx (below) |
| `tests/` | Programs of the WPE port and what they print: `wtf-wpe.cpp` (WTF), `jsc-glib.c` (JavaScriptCore's GLib API), `wpe.js` (for its jsc) |

## The generators

| Program | Port of | Makes |
|---|---|---|
| `hashtable.mlx` (webkit-hash-table) | create_hash_table (Perl) | the 102 `.lut.h` static hash tables |
| `bytecodes.mlx` (+ `bytecodelist.mlx`) | generator/main.rb (Ruby DSL of BytecodeList.rb) | Bytecodes.h, BytecodeStructs.h, BytecodeIndices.h, InitBytecodes.asm, BytecodeDumperGenerated.cpp |
| `offlineasm/` (webkit-offlineasm) | offlineasm (Ruby), the C_LOOP backend | LLIntDesiredSettings.h, LLIntDesiredOffsets.h, LLIntAssembly.h (from the extractors mlx-cc compiles) |
| `builtins.mlx` (+ `builtinsmodel.mlx`, `pyregex.mlx`) | generate-js-builtins.py, wkbuiltins | JSCBuiltins.h/.cpp |
| `preferences.mlx` (+ `preferencesyaml.mlx`) | WTF's GeneratePreferences.rb and its ERB template, a YAML reader | JSCWebPreferenceOptions.h |
| `inspectorjson.mlx`, `inspector.mlx` (+ `inspector*.mlx`) | generate-combined-inspector-json.py, generate-inspector-protocol-bindings.py | CombinedDomains.json, inspector/*.h, .cpp, .js |
| `airopcodes.mlx` | b3/air/opcode_generator.rb | AirOpcode.h, AirOpcodeUtils.h, AirOpcodeGenerated.h |
| `wasmops.mlx`, `wasmomg.mlx` (+ `wasmjson.mlx`) | generateWasmOpsHeader.py, generateWasmOMGIRGeneratorInlinesHeader.py | WasmOps.h, WasmOMGIRGeneratorInlines.h |
| `keywordlookup.mlx` | KeywordLookupGenerator.py | KeywordLookup.h |
| `lexerunicode.mlx` | generateLexerUnicodePropertyTables.py | LexerUnicodePropertyTables.h |
| `regextables.mlx` | yarr/create_regex_tables | yarr/RegExpJitTables.h |
| `yarrcanonicalize.mlx` | generateYarrCanonicalizeUnicode | yarr/YarrCanonicalizeUnicode.cpp |
| `yarrunicode.mlx` (+ `ucdsupport.mlx`) | generateYarrUnicodePropertyTables.py | yarr/UnicodePatternTables.h |
| `glibapi.mlx` (webkit-glib-api) | Scripts/glib/generate-api-header.py with unifdef; CMake's configure_file | the GLib API's headers (WPE: JavaScriptCoreGLib/DerivedSources/jsc/*.h, JSCVersion.h) |
| `support.mlx`, `rapidhash.mlx` | | what the generators share; WTF's string hash |

WebCore's (`webcore.mlx` runs them):

| Program | Port of | Makes |
|---|---|---|
| `gperf.mlx` (webkit-gperf, + `gperf*.mlx`) | GNU gperf 3.1, as the scripts and make-hash-tools.pl run it | CSSPropertyNames.cpp, CSSValueKeywords.cpp, HTTPHeaderNames.cpp, the SelectorPseudo*Map.cpp lookups, ColorData.cpp |
| `cssproperties.mlx` (+ `cssprops*.mlx`, `csstext.mlx`) | css/scripts/process-css-properties.py | CSSPropertyNames.h/.gperf, CSSPropertyParsing, the Style* builders, extractors and interpolation maps, CSSStyleProperties+PropertyNames.idl |
| `cssvalues.mlx` | process-css-values.py | CSSValueKeywords.in/.h/.gperf |
| `cssselectors.mlx` | process-css-pseudo-selectors.py | CSSSelectorEnums.h, CSSSelectorInlines.h, UserAgentParts, the SelectorPseudo*Map.gperf inputs |
| `cssarrays.mlx` (+ `csspreprocessor.mlx`) | css/make-css-file-arrays.pl | UserAgentStyleSheets.h, UserAgentStyleSheetsData.cpp |
| `cssjsarrays.mlx`, `cssmin.mlx`, `cssjsmin.mlx`, `cssxxd.mlx` (+ `cssminify.mlx`) | make-js-file-arrays.py, cssmin.py, jsmin.py, xxd.pl | UserAgentScripts.h/Data.cpp, XMLViewer.min.css/.js, XMLViewerCSS.h, XMLViewerJS.h |
| `csscat.mlx` (webkit-cat) | `cmake -E cat` | ModernMediaControls.css, ModernMediaControls.js |
| `names.mlx` (+ `names*.mlx`) | dom/make_names.pl | HTMLNames, SVGNames, MathMLNames, XLink/XML/XMLNS names, the element factories and wrapper factories, WebKitFontFamilyNames, TagName, NodeName, Namespace |
| `events.mlx`, `eventsfactory.mlx` | make-event-names.py, make_event_factory.pl | EventNames.h/.cpp; EventFactory.cpp, EventInterfaces.h, EventHeaders.h and EventTarget's |
| `httpheaders.mlx`, `entities.mlx` | create-http-header-name-table, create-html-entity-table | HTTPHeaderNames.gperf/.h/.serialization.in; HTMLEntityTable.cpp |
| `builtins.mlx` | generate-js-builtins.py (--framework WebCore, --wrappers-only) | the *Builtins.h/.cpp of each object, WebCoreJSBuiltins, WebCoreJSBuiltinInternals |
| `bindings.mlx` (+ `bindings*.mlx`, `idlparser.mlx`, `idlmodel.mlx`) | generate-bindings-all.pl: preprocess-idls.pl, CodeGeneratorJS.pm | the JS*.h/.cpp/.dep of the IDL files, DOMIsoSubspaces.h, DOMConstructors.h, the global scopes' constructor IDL files, supplemental_dependency.tmp |
| `webcoresettings.mlx` | Scripts/GenerateSettings.rb and its ERB templates | Settings.h/.cpp, InternalSettingsGenerated.h/.cpp/.idl |
| `webcorelog.mlx`, `webcoresync.mlx` | generate-log-declarations.py, generate-process-sync-data.py | WebCoreLogDefinitions.h, WebCoreVirtualLogFunctions.h; Document/FrameTree SyncData and SyncClient |
| `webcoredomjit.mlx` | domjit/generate-abstract-heap.rb | DOMJITAbstractHeapRepository.h |
| `webcoreatspi.mlx` | gdbus-codegen 2.80 (--interface-info-header/-body) | AccessibilityAtspiInterfaces.h/.c |

Every source JavaScriptCore's build generates (JSCOnly: 300 files, the
167 unified bundles and the copy of Sources.txt among them; WPE: 302,
with 168 bundles and SourcesGLib.txt, and the 9 headers of its GLib
API) is made so, and every file of
WebCore's DerivedSources for the WPE port (5464 files: the 518 unified
bundles and the copies of the source lists among them). Two of them
name paths and differ by those alone: ColorData.cpp's header repeats gperf's command line with its
output file, and supplemental_dependency.tmp lists the IDL files by
path, the generated one among them.
