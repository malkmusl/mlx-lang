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

    ./mlx build webkit-bmalloc        bmalloc and libpas (libbmalloc.a)
    ./mlx build webkit-wtf            WTF, the JSCOnly port, on mlx-unicode (libWTF.a)
    ./mlx build webkit-jsc-derived    what JavaScriptCore's build generates
    ./mlx build webkit-jsc            JavaScriptCore, C_LOOP (libJavaScriptCore.a)
    ./mlx build webkit-jsc-shell      mlx-out/webkit/bin/jsc
    ./mlx build webkit                all of it

    tools/check_webkit_generators.sh  the generated sources against those of a build by WebKit's generators

Objects go to mlx-out/webkit/obj, archives to mlx-out/webkit/lib, what
is generated to mlx-out/webkit/generated (JavaScriptCore/ for JSC's).

## Parts

| File | What |
|---|---|
| `build.mlx` | The steps: the libraries from WebKit's source lists, the generators and their outputs, the forwarding headers (`<bmalloc/...>`, `<JavaScriptCore/...>`), the configuration header |
| `config.mlx` | The JSCOnly configuration (`cmakeconfig.h`'s defines) and its feature defines |
| `unified.mlx` | Unified sources: generate-unified-source-bundles.py's bundles of a source list (Sources.txt), made while the steps are declared |
| `lists/` | Source lists the build takes from WebKit's CMake files: WTF's and bmalloc's sources for the port, the `.lut.h` sources, JSC's headers, builtins and inspector domains |
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

Every source JavaScriptCore's build generates (299 files, and the 167
unified bundles) is made so; WebCore's generators follow.
