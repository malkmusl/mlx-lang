# Build and package system

**Normative source:** `spec/05-build/build-system.xml`, `spec/05-build/packages.xml`,
`spec/05-build/targets.xml`

**Cross-referenced implementation:** root `build.mlx`, `std/src/build.mlx`
(and `std/src/build/`), `compiler/selfhost/driver/build_command.mlx`, the
root `mlx` script

Mlx distinguishes two build systems, per the "Bootstrap chain" in the root
`README.md`:

- **Stage-0**: Zig builds the bootstrap compiler `mlx0` (and the Zig
  language server) from `compiler/bootstrap/`. There is no `build.zig`
  any more: `build.mlx`'s `bootstrap` group runs `zig build-exe` for them
  (and `tools/ensure_compiler.sh` does the same on a machine without an
  Mlx compiler). "Zig is Stage-0 only. The canonical compiler, standard
  library, normal Mlx programs and brixOS must not depend on Zig."
- **Canonical**: `build.mlx` run by `mlx build`, as
  `spec/05-build/build-system.xml` specifies; `mlx run`, `mlx test`,
  `mlx fmt` and `mlx check` are not there yet.

This chapter documents the normative spec first, then what is implemented.

## The normative build system

`spec/05-build/build-system.xml` in full:

```xml
<MlxBuildSystem version="1.0" normative="true">
  <Config>build.mlx</Config>
  <Commands>mlx build; mlx run; mlx test; mlx fmt; mlx check</Commands>
  <TargetModel>arch + os + abi + cpu feature set</TargetModel>
  <InitialTargets>x86_64-linux x86_64-freebsd x86_64-windows x86_64-brixos x86_64-freestanding</InitialTargets>
  <BuildHostAccess>Build scripts may perform host I/O only through explicit std.Build APIs.</BuildHostAccess>
  <GeneratedProtocols>Build graph can register declarative protocol schemas such as Wayland XML; generated declarations are compiler/build artifacts and need not be checked into source.</GeneratedProtocols>
</MlxBuildSystem>
```

Key normative points:

- The canonical project configuration file is `build.mlx` (the Mlx analogue
  of Zig's `build.zig`), not a data-only manifest format.
- Five subcommands are specified: `mlx build`, `mlx run`, `mlx test`,
  `mlx fmt`, `mlx check`.
- The target model is `arch + os + abi + cpu feature set` (elaborated in
  `targets.xml`, below).
- The initial target list is `x86_64-linux`, `x86_64-freebsd`,
  `x86_64-windows`, `x86_64-brixos`, `x86_64-freestanding`.
- Build scripts are sandboxed: "Build scripts may perform host I/O only
  through explicit `std.Build` APIs" — an unrestricted host-access build
  script is not conforming.
- The build graph can register "declarative protocol schemas such as Wayland
  XML" whose generated declarations are compiler/build artifacts that "need
  not be checked into source" — this matches `docs/BOOTSTRAP.md`'s
  description of `std.wayland`'s canonical XML being consumed during stdlib
  bootstrap rather than at ordinary project build time.

## Target model

`spec/05-build/targets.xml`:

```xml
<MlxTargets version="1.0" normative="true">
  <TargetFields>arch os abi endian pointer_bits cpu_features</TargetFields>
  <BuiltinModule name="builtin">target, cpu features, optimize mode, language version</BuiltinModule>
  <Initial arch="x86_64" os="linux freebsd windows brixos freestanding"/>
  <FeatureQuery><![CDATA[if comptime builtin.cpu.has(.avx2) { ... }]]></FeatureQuery>
  <FunctionFeatureRequirement><![CDATA[fn fast(...) void requires(.avx2) { ... }]]></FunctionFeatureRequirement>
  <Rule>A function requiring a CPU feature may only be called when the compilation target guarantees the feature or from code guarded by an explicit runtime dispatch mechanism that targets a feature-specialized function.</Rule>
</MlxTargets>
```

A target is fully described by six fields: `arch`, `os`, `abi`, `endian`,
`pointer_bits`, `cpu_features`. A built-in `builtin` module exposes the
current target, CPU feature set, optimize mode, and language version to
comptime code, and the spec gives two concrete syntax examples for feature
gating:

- A comptime feature query: `if comptime builtin.cpu.has(.avx2) { ... }`
- A per-function feature requirement:
  `fn fast(...) void requires(.avx2) { ... }`

The accompanying rule constrains when a `requires(.avx2)`-marked function may
be called at all: only when the compilation target guarantees the feature, or
from code behind an "explicit runtime dispatch mechanism" that targets a
feature-specialized function — i.e., no implicit fallback path is invented by
the compiler.

`x86_64` is currently the only specified `arch`; `linux freebsd windows
brixos freestanding` are the specified `os` values, matching
`build-system.xml`'s `InitialTargets` list one-for-one (each is
`x86_64-<os>`).

## Packages

`spec/05-build/packages.xml`:

```xml
<MlxPackages version="1.0" normative="true">
  <Identity>package name plus resolved source identity</Identity>
  <Mapping>build.mlx maps package imports to roots</Mapping>
  <Network>Compiler core never requires network access. Package acquisition is separate tooling.</Network>
  <Versioning>Package semantic versioning belongs to package tooling, not source-language syntax.</Versioning>
</MlxPackages>
```

This is deliberately thin. It fixes four points and no more:

- A package's identity is its name plus a "resolved source identity" (the
  spec does not further define what constitutes a resolved source identity —
  e.g. a git commit, a tarball hash, or a registry version — so no manifest
  schema can be inferred from this text alone).
- `build.mlx` is where package imports are mapped to source roots — package
  resolution is a build-graph concern, not a separate manifest format.
- The compiler core itself never requires network access; acquiring a
  package's sources is explicitly "separate tooling" outside the compiler.
- Semantic versioning of packages is a package-tooling concern, not
  something the source language's syntax encodes.

No package manifest file format, field list, or example is specified
anywhere in `spec/05-build/packages.xml`. The implementation keeps to the
spec's one firm point, that `build.mlx` is the configuration and not a
separate data-only manifest: the project's name, version and description
(what a Zig project keeps in `build.zig.zon`) come from a function in
`build.mlx` itself, `pub fn package() -> Build.Package`. Package imports
mapped to roots (`<Mapping>`) and the resolved source identity are not
implemented: this repository has no packages from elsewhere.

## The implementation: `mlx build`

### Running it

```sh
./mlx build                      # the default group (all, in this repository)
./mlx build desktop login        # groups or single steps by name
./mlx build --list               # the groups and steps
./mlx build -j 8 --verbose       # eight jobs; name the current steps too
./mlx build --dry-run            # what would be built
./mlx build --force mlx-sh       # build it even when it is current
./mlx build --keep-going         # go on after a step failed
./mlx build --clean              # forget what was built
./mlx build --cache-stats        # what the result cache holds
./mlx build --cache-clear        # empty the result cache
```

`./mlx` (the root script) runs this checkout's compiler,
`mlx-out/bin/compiler/mlx4`, after making it when it is missing or a file
of `compiler/selfhost` or `std/bootstrap` changed (`tools/ensure_compiler.sh`:
from an older `mlx4`, else from the bootstrap compiler, which needs zig; in
two stages, and one or two more until the compiler builds itself byte for
byte, when the older one still had a bug the new sources fixed).
`mlx build ARGS` is a subcommand of the compiler
(`compiler/selfhost/driver/build_command.mlx`): it writes
`mlx-out/build/runner.mlx`, a main that imports `../../build.mlx` and
`std.build`, compiles it once into `mlx-out/build/runner`, and runs that
with the arguments. From then on the runner keeps itself current: when
`build.mlx`, `std.build` or the compiler changed it compiles `build.mlx`
again and runs the new runner. Like every program it installs the crash
report first (`std.crash`), and says the step it is checking, starting
or finishing, so a crash of the runner names its function and the step.

### `build.mlx`

```mlx
const Build = @import("std.build")

pub fn package() -> Build.Package {
    return Build.Package.{ .name = "hello", .version = "0.1.0", .description = "a greeting" }
}

pub fn build(b: *Build.Builder) -> void {
    const all = Build.group(b, "all", "everything")
    const hello = Build.program(b, "hello", "src/main.mlx")
    Build.add(b, all, hello)
    const so = Build.plugin(b, "libhello.so", "src/library.mlx")
    Build.option(b, so, "--soname=libhello.so")
    Build.add(b, all, so)
    Build.byDefault(b, all)
}
```

(`Build`, not `build`: the module and the function would have the same
name.) The steps:

| Function | Makes |
| --- | --- |
| `group(b, name, description)` | nothing: stands for what `add` puts in it |
| `program(b, name, root)` | the Mlx program `mlx-out/bin/NAME` |
| `sharedObject(b, name, root)` | `mlx-out/lib/NAME` (`--shared`) |
| `plugin(b, name, root)` | `mlx-out/lib/NAME` (`--plugin`) |
| `command(b, name, output, line)` | OUTPUT by a host command (words split at spaces, `"..."` keeps spaces); `""` for a command that only runs |
| `link(b, name, target)` | the symbolic link `mlx-out/bin/NAME` |
| `copy(b, name, source, output)` | OUTPUT, a copy of SOURCE with its mode |

and what changes them: `option` (a compiler option), `output` (another
place for the file), `input` and `inputTree` (the files a command reads),
`depfile` (the make-style dependency file a command writes, as
`mlx-cc -MD -MF` does), `cacheable` (a command's output may come from the
result cache, below), `uncached` (it never does, though the command names
a depfile), `dependOn` and `add` (order and membership),
`byDefault`, `describe`,
`step(b, name)` (a step declared before) and `concat` (a string made from
two). Host I/O happens only through these (`<BuildHostAccess>`): `build()`
declares, the runner does the work.

### Only what is needed

Each step that was built has a manifest in `mlx-out/build/cache`: a key
(what made it: the kind, the root or command line, the output, the
options, the compiler's contents, the names of the files in its input
trees) and one line per file it read, with the file's size, modification
time and a hash of its contents. For an Mlx step those files are the
modules the compiler loaded, which it writes with `--depfile=PATH` (one
path per line); for a command, its inputs and the files of its input
trees. A step is current when its file is there, the key is the same and
each input is unchanged: the same size and time, or, when those differ,
the same contents, so a file saved again unchanged costs a hash and not a
build. Editing `projects/desktop/shared/palette.mlx` rebuilds the seven
programs that import it, and nothing else; a run with nothing to do takes
a few hundredths of a second for this repository's 128 steps.

The manifest goes before a step runs and is written after it succeeded; a
step's file is made beside its place (`.building.NAME`) and moved there
when it is complete, so an interrupted build is never taken for a current
one, and a running program is never written over. A step whose dependency
was built now is looked at again then (a command that reads another step's
output).

### The result cache

The manifests only know the last build of each step. Switching branches,
bisecting the compiler, reverting a change or building in another checkout
of the repository (the agents' worktrees under `.claude/worktrees`) would
make every step whose inputs differ from that last build again, although
the same file was made before: for WebKit, about 1250 compiles of 25 to 60
seconds each. So std.build keeps what steps made in a result cache
(`std/src/build/store.mlx`), shared by all of the user's checkouts, as
ccache's direct mode does:

- **Where:** `$MLX_BUILD_CACHE`, else `$XDG_CACHE_HOME/mlx/build`
  (`~/.cache/mlx/build`); `MLX_BUILD_CACHE=off` turns it off.
- **What:** the output of every Mlx step (program, shared object,
  plugin; not an APK, signed with a key no depfile names), and of every
  command that names a depfile (`depfile`, as the compiles with
  `mlx-cc -MD -MF` and WebKit's archives do) or is marked `cacheable`
  (`libmlxcpp.a`'s archive): steps whose one output file is all they make.
  A command that writes more than its output (a generator of a directory
  of headers, named by one of them) is never cached, since only the output
  would come back; nor is one marked `uncached` (mlx-cc's prefix
  snapshots: a gigabyte each, made again in seconds, and in no object's
  key, since an object made with one is the object made without it).
- **Keys:** a step's entries are kept under its key: the manifest key
  (kind, command line or root module, output path, options, the Mlx
  compiler's contents, the input trees' file names) and, for a command,
  the contents' hash of the program it runs (`mlx-out/bin/mlx-cc`, by its
  bytes, not its path or time). Under it, an entry for each set of inputs
  it was built from: every file the build read (its manifest's inputs:
  the depfile's, the declared inputs, the input trees') with its
  contents' hash, the paths in the checkout relative to its root (mlx-cc
  names its own headers by absolute paths), and the object it made.
- **Hits:** a step that is not current, before its command runs, looks at
  the entries of its key; when all of one entry's inputs have the
  recorded hashes now (sizes and times first, the contents when those
  differ, each file hashed once a run), its output is that entry's file:
  linked into place (copied when the cache is on another file system)
  and checked against its name (size and hash), and the step's manifest
  is written as if it had been built. The progress line says so,
  `[12/1108] cached jsc/UnifiedSource-runtime-3.cpp`, and the summary
  counts it (`3 built, 1105 cached, 0 current`). Since each set of inputs
  keeps its entry, going back to a branch (A, B, A) finds its files again.
  `--force` builds instead of looking, `--dry-run` does not look.
- **Files:** `objects/HH/HASH-SIZE-MODE`, named by their contents, so
  identical outputs (of other steps, of other inputs) are kept once; an
  object shares its inode with the outputs it was linked to, so it costs
  no room while they are there. `--cache-stats` shows how much is shared.
- **Safety:** a file goes into the cache under a name of its own in
  `tmp/` and is then renamed into place, so concurrent builds (the main
  checkout and agent worktrees at once) never see one half written, and
  two builds storing the same entry store the same bytes. Because an
  output may be the cache's own inode, the output of a cached command is
  removed before the command runs: mlx-cc and mlx-ar truncate what they
  write to, and would otherwise write into the cache (Mlx steps and copies
  already make a new file beside it and rename it over the old one). An
  object that does not match its name (damaged) is removed and the step
  built.
- **Size:** `MLX_BUILD_CACHE_SIZE` (bytes, or with `K`, `M` or `G`; `6G`
  by default). After a build that stored something, when the objects
  pass the limit, the least recently used go (by the time their inode
  last changed, which a hit's link renews) down to four fifths of it, and
  the entries naming them; below the limit that costs one status call per
  object.

A hit is only as right as the inputs a step names: like ccache's direct
mode it assumes that a step reading the same bytes makes the same file.
A header that appears earlier on the include path than the one the build
read, without any file it read changing, is not noticed (nor is it by the
manifests). The compilers here are reproducible: `mlx4` makes the same
binary from the same sources in any directory (mlx-cc built in a worktree
is byte for byte the main checkout's), and mlx-cc's `__DATE__` and
`__TIME__` are fixed.

### Jobs

Jobs run side by side, as many as there are processors unless `-j` says
otherwise, the step whose last build took longest first. Each job's output
goes to `mlx-out/build/logs/NAME.log`, shown when the step fails. After a
failure no new job starts (the running ones finish) unless `--keep-going`.

## What is implemented

| Spec concept | Status |
| --- | --- |
| `build.mlx` project configuration | Implemented: root `build.mlx`, run by `mlx build` (`./mlx build`) |
| `mlx build` | Implemented, with incremental builds and parallel jobs |
| `mlx run`/`test`/`fmt`/`check` | Not implemented |
| Target model (`arch os abi endian pointer_bits cpu_features`, `builtin` module) | Normative in `spec/05-build/targets.xml`; steps choose a target with the compiler's `--target=` option |
| Build-script host I/O only through `std.Build` APIs | `build.mlx` declares through `std.build`; it is not sandboxed (it is an ordinary program that could call other modules) |
| Package identity/mapping/versioning | `package()` gives name and version; no package imports or resolver |
| Stage-0 | `build.mlx`'s `bootstrap` group (`mlx0`, the Zig `mlx-lsp`, `mlx1`) and `bootstrap-test` (the Zig unit tests); `build.zig` is gone |
