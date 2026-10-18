# WebKit, built by this repository

WPE WebKit and the libraries it needs, built with this repository's own
toolchain alone: `./mlx build` (build.mlx, std.build) declares the steps,
mlx-cc compiles every C and C++ file, mlx-ar makes the archives, the
Mlx compiler's linker links, mlxlibcpp and mlx-unicode (compiler/capi/icu,
ICU's C API over std.unicode) are the libraries WebKit runs on. What WebKit's
CMake build makes with Perl, Python, Ruby and gperf is made here by
programs written in Mlx (generators/), and their output is that of
WebKit's own generators to the byte.

The sources are WPE WebKit 2.54.1's tree: `WEBKIT_SOURCE` (the directory
with `Source/`), by default /home/user/csrc/wpe/wpewebkit-2.54.1.

    ./mlx build webkit-bmalloc        bmalloc and libpas (libbmalloc.a)
    ./mlx build webkit-wtf            WTF, the JSCOnly port, on mlx-unicode (libWTF.a)
    ./mlx build webkit-jsc-derived    what JavaScriptCore's build generates
    ./mlx build webkit-webcore-derived  what WebCore's build generates (the WPE port)
    ./mlx build webkit-jsc            JavaScriptCore, C_LOOP (libJavaScriptCore.a)
    ./mlx build webkit-jsc-shell      mlx-out/webkit/bin/jsc
    ./mlx build webkit-angle          ANGLE, WebGL's OpenGL ES (libANGLE.a, libGLESv2.a)
    ./mlx build webkit                all of it

    tools/check_webkit_generators.sh  the generated sources against those of a build by WebKit's generators
    tools/check_webcore_generators.sh WebCore's likewise (WEBCORE_REFERENCE, a WPE build's WebCore/DerivedSources)

Objects go to mlx-out/webkit/obj, archives to mlx-out/webkit/lib, what
is generated to mlx-out/webkit/generated (JavaScriptCore/ for JSC's,
WebCore/ for WebCore's).

## Parts

| File | What |
|---|---|
| `build.mlx` | The steps: the libraries from WebKit's source lists, the generators and their outputs, the forwarding headers (`<bmalloc/...>`, `<JavaScriptCore/...>`), the configuration header |
| `config.mlx` | The JSCOnly configuration (`cmakeconfig.h`'s defines) and its feature defines; the WPE port's feature defines |
| `webcore.mlx` | WebCore's generated sources (WPE): the generators run as WebCore's CMake files run its scripts, gperf after them, the copies of the source lists and their unified sources |
| `angle.mlx` | ANGLE as WPE builds it: its configuration (defines, include directories, ANGLEPrefix.h first), the headers it gives WebCore (ANGLE/Headers, includes adjusted as adjust-angle-include-paths.py does) |
| `unified.mlx` | Unified sources: generate-unified-source-bundles.py's bundles of a source list (Sources.txt), made while the steps are declared |
| `lists/` | Source lists the build takes from WebKit's CMake files: WTF's and bmalloc's sources for the port, the `.lut.h` sources, JSC's headers, builtins and inspector domains; WebCore's IDL files (in CMake's order, which the bindings' batches follow), builtins, user agent style sheets, media controls' style sheets and scripts, AT-SPI interfaces; ANGLE's sources of libANGLE.a and libGLESv2.a and its public headers |
| `generators/` | WebKit's code generators in Mlx (below) |

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

Every source JavaScriptCore's build generates (299 files, and the 167
unified bundles) is made so, and every file of WebCore's DerivedSources
for the WPE port (5464 files: the 518 unified bundles and the copies of
the source lists among them). Two of them name paths and differ by those
alone: ColorData.cpp's header repeats gperf's command line with its
output file, and supplemental_dependency.tmp lists the IDL files by
path, the generated one among them.

ANGLE's 305 sources (WebKit's CMake build compiles 307: two are its
precompiled headers, here `-include ANGLEPrefix.h`) compile with mlx-cc,
and libANGLE.a and libGLESv2.a are made. Four include `<zlib.h>`
(MemoryProgramCache.cpp, Program.cpp, angletypes.cpp and
third_party/zlib/google/compression_utils_portable.cc use zlib's deflate
and inflate streams): theirs are mlx-zlib's headers
(mlx-out/include/mlx-zlib, made by the step zlib-headers;
compiler/capi/zlib, over std.flate), and the program linking ANGLE
links mlx-out/lib/libmlxz.so.
