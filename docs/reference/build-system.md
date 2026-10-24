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
./mlx build --no-memory-limit    # -j jobs whatever the memory
./mlx build --dry-run            # what would be built
./mlx build --force mlx-sh       # build it even when it is current
./mlx build --keep-going         # go on after a step failed
./mlx build --retry-failed       # run steps whose failure is remembered
./mlx build --verify-epoch 20    # then compile 20 cached objects again, compare
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
The compiler it replaces stays as `mlx-out/bin/compiler/mlx4.previous`:
the compiler's sources, and everything they import, must stay what that
previous compiler and mlx0 build, so they never import the std root and
use no language feature the previous compiler lacks
(`compiler/selfhost/README.md`, "Bootstrapping"; `tools/check_bootstrap.sh`
checks it, and `codemap.layers` states it for `mlx-codemap layers`).
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
a depfile), `programEpoch` (the command's program is known by an epoch
stamp, not its bytes; below), `dependOn` and `add` (order and
membership),
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

### Programs keyed by an epoch

A command's entries are kept under the bytes of the program it runs, so
a new build of a compiler would make every object it made again,
although almost all of them come out the same: for WebKit, a change to
mlx-cc anywhere (a diagnostic, the crash report's wiring, a faster
lookup) meant ~1800 compiles, hours. `Build.programEpoch(b, step, STAMP)`
says the step's program is known by the contents of STAMP instead, a
file the program's build writes, which names its *code generation
epoch*: a number the program's authors move whenever a change can make
another file from the same inputs (compiler/cc/epoch.mlx states mlx-cc's
rule). STAMP is an input of the step; the program's bytes are not. So a
new mlx-cc that writes the same stamp leaves every object current, every
cached object valid in every checkout, and a moved epoch builds them all.

In build.mlx: the `mlx-cc-epoch` step runs `mlx-cc
--codegen-epoch=mlx-out/bin/mlx-cc.epoch` whenever mlx-cc is built (it
says `mlx-cc codegen epoch N`), and every object step (`cObject`,
projects/webkit's `compileAs`) names that file with `programEpoch` and
depends on the step. mlx-ld and mlx-ar steps, and Mlx programs (mlx4's
own hash), are keyed by bytes as before.

An epoch is a promise; `--verify-epoch N` checks it: after the build, N
of the wanted steps keyed by an epoch whose files are there (a random
sample, those taken from the cache or current before those made now)
are made again by the program as it is, and each new file is compared
with the one it replaces. A step whose file differs means the program
changed what it makes under the same epoch: the run says `[verify 3/20]
DIFFERS NAME` and asks for the epoch to be bumped, and fails.
`tools/check_codegen_epoch.sh` does this on a probe project and on 20 of
WTF's objects (with the WebKit sources there).

### Failures remembered

A command that failed (exit status 1 to 127, not a signal: a unit the
memory killer stopped is not remembered) after writing its depfile is
remembered too, under `failures/KEY/INPUTS`: the key with the program
**by its bytes** (never its epoch), the files the failing run read (its
depfile) and the declared inputs with their hashes, the exit status and
the last 64 KiB of its log. When the step is next not current and not
cached, and the program and every file are as they were, it fails at
once without a job: `FAILED NAME (exit 1, remembered: ...)` and its log,
back in mlx-out/build/logs. `--retry-failed` runs such steps instead,
`--force` too. Since a failure's key has the compiler's bytes and a
success's the epoch, a fix of mlx-cc that keeps the epoch runs exactly
the steps that failed before (and those whose files changed) and takes
everything else from the manifests or the cache. A failure before the
depfile is written (a missing header) is not remembered: what it read
is not known. Remembered failures go after two weeks unused.

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
otherwise, the step whose last build took longest first, and only as many
as the memory has room for (`std/src/build/memory.mlx`): a step is
expected to take its peak resident memory of the last time it ran (the
child's `ru_maxrss` from `wait4`, kept in its manifest and its result
cache entry beside the time it took, `took MS PEAK_KIB`), an eighth more,
or, before it ever ran, what build.mlx declared with `Build.memory(b,
step, MEGABYTES)` (projects/webkit declares 4 GB for a WebCore unit, 2 GB
for JavaScriptCore's, 1 GB for the others'); nothing known, no limit. A
job starts only when that fits in what is available now: MemAvailable
(what other processes take, another build beside this one included, is
out of it already), or less when the cgroup's limit says so, less a
margin of an eighth of the memory (at least 1 GiB), less what the running
jobs are still expected to grow by (their expectation less their
resident memory now). A ready step that does not fit waits for a running
job to end; with none running it starts anyway. So `-j 4` (the default
on four processors) is safe for WebKit, whose units take 1 to 4 GB: the
build runs as many as fit instead of being killed (exit 137) or being
held at `-j 2` throughout. The summary says how often steps waited for
memory and the largest job's peak; `--no-memory-limit` starts `-j` jobs
whatever the memory. (Jobs are started by vfork, and a child's
`ru_maxrss` counts the runner's resident memory until its execve, about
280 MB: a job smaller than that is recorded as that.) Each job's output
goes to `mlx-out/build/logs/NAME.log`, shown when the step fails. After a
failure no new job starts (the running ones finish) unless `--keep-going`.

## Versions and build numbers

A version is `MAJOR.MINOR.BUILD`, for the toolchain as for a project
(Voyager 0.1.1228):

- `MAJOR.MINOR` is the first line of the project's `VERSION` file at its
  root (`0.1`; the repository's own `VERSION` is the toolchain's), as
  HEAD's tree holds it: a change is made by committing it.
- `BUILD` is the number of commits reachable from HEAD that are not
  reachable from the commit that last changed `VERSION`'s content, that
  commit counted: `git rev-list --count HEAD ^CHANGED` + 1. The commit
  setting `0.2` is 0.2.1, the next ones 0.2.2, 0.2.3... Until `VERSION`
  first changes (the commit that adds it is not a change), `BUILD` is
  every commit reachable from HEAD, `git rev-list --count HEAD`. The
  changing commit is the first one on HEAD's first-parent line whose
  first parent's `VERSION` has other bytes (a branch changing it and
  merged: the merge).
- A tree without `.git` (an archive) is `MAJOR.MINOR.dev`. A shallow clone
  counts to its boundary (`.git/shallow`), as git does.

They are found by `std.git` ([stdlib.md](stdlib.md#git-repositories-stdgit)),
without the git program: `git.buildNumberOf(directory, "VERSION",
&number)`. A git submodule (its `.git` file leading into the
superproject's `.git/modules/NAME`) is counted on its own, so a project
moved into a submodule keeps its own numbers. For scripts,
`mlx-out/bin/mlx-build-number [DIRECTORY] [--file=PATH]`
(tools/build-number) prints `VERSION.BUILD COMMIT` (`0.1.1211 5d1c...`),
`MAJOR.MINOR.dev` alone without a repository.

The toolchain says its version: `mlx4 --version`, `mlx-cc --version`
and `mlx-ld --version` print `NAME MAJOR.MINOR.BUILD (COMMIT)`. It is
built into them from `mlx-out/version.mlx`, a module of two constants
(`VERSION`, `COMMIT`; no import, so the compiler's closure stays off the
std root) that build.mlx (`toolchainVersion`) writes each time the build
is declared, from `std.git`, and only when it changed: a new HEAD makes
mlx-cc and mlx-ld again (a second or two; their objects are keyed by the
code generation epoch, which does not change). mlx4 is built by
`tools/ensure_compiler.sh`, which writes the module first
(`tools/version_module.sh`, with mlx-build-number built by the compiler
it starts from): its version is that of the HEAD it was last built at.
Without a compiler (mlx0's bootstrap) the module is a `dev` one.

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
