# Build and package system

**Normative source:** `spec/05-build/build-system.xml`, `spec/05-build/packages.xml`,
`spec/05-build/targets.xml`

**Cross-referenced implementation:** root `build.zig`, `examples/coreutils/build.sh`

Mlx distinguishes two build systems, per the "Bootstrap chain" in the root
`README.md`:

- **Stage-0**: the Zig build (`build.zig`) that builds the bootstrap compiler
  `mlx0` (and the language server) from the Zig sources in
  `compiler/bootstrap/`. This is scaffolding only — "Zig is Stage-0 only. The
  canonical compiler, standard library, normal Mlx programs and brixOS must
  not depend on Zig."
- **Canonical**: the `build.mlx`-driven, `mlx build`/`mlx run`/`mlx test`
  system that `spec/05-build/build-system.xml` normatively specifies, meant
  to be self-hosted once the compiler chain (`mlx1`, `mlx2`, ...) exists.

This chapter documents the normative spec first, then says explicitly how
much of it `build.zig` (Stage-0) currently reflects versus what remains
aspirational for the canonical system.

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
anywhere in `spec/05-build/packages.xml`, and no `build.mlx` file exists yet
anywhere in this repository (a repo-wide search finds none, and none of the
example programs under `examples/` — `examples/hello/`,
`examples/coreutils/`, `examples/wayland-client/`,
`examples/wayland-server/` — includes one). This guide therefore does not
show an example manifest: doing so would mean inventing a shape the spec
does not define, which the project's own rule in `SPEC_INDEX.md` forbids
("the implementation must never silently invent behavior").

## Stage-0 (`build.zig`) versus the canonical system

The root `build.zig` is Zig's own build-script format, not `build.mlx`, and
it builds exactly two Zig executables plus one Mlx artifact:

```zig
const exe = b.addExecutable(.{
    .name = "mlx0",
    .root_module = b.createModule(.{
        .root_source_file = b.path("compiler/bootstrap/main.zig"),
        .target = target,
        .optimize = optimize,
        .strip = optimize != .Debug,
    }),
});
b.installArtifact(exe);
```

(`build.zig`; this is the bootstrap compiler `mlx0`)

It also builds `mlx-lsp` (the language server, from
`compiler/bootstrap/lsp.zig`) and defines a `mlx1` build step that *runs*
`mlx0` against the self-hosted compiler's entry point to produce `mlx1`:

```zig
const build_mlx1 = b.addRunArtifact(exe);
// mlx0 resolves the self-hosted compiler's imports itself, so Zig cannot
// infer those transitive inputs from main.mlx. Always rerun this cheap
// bootstrap step to avoid installing a stale mlx1 from the build cache.
build_mlx1.has_side_effects = true;
build_mlx1.addFileArg(b.path("compiler/selfhost/main.mlx"));
const mlx1_output = build_mlx1.addPrefixedOutputFileArg("-o", "mlx1");
```

(`build.zig`)

The comment here is itself informative about the boundary between the two
systems: Zig's build graph cannot see *into* `mlx0`'s own `@import`
resolution for Mlx source files, so it cannot do proper incremental dependency
tracking for `mlx1` — it always reruns that step. This is exactly the kind of
concern a canonical `build.mlx`-based system (with its own `std.Build`-style
APIs, per `BuildHostAccess` above) would need to own once Mlx builds itself.
`build.zig` has a `test` step (running the Zig unit tests for
`compiler/bootstrap/main.zig` and `compiler/bootstrap/lsp.zig`) and an `lsp`
run step, but no `run`, `fmt`, or `check` step — so of the five canonical
subcommands (`mlx build`, `mlx run`, `mlx test`, `mlx fmt`, `mlx check`),
Stage-0 only has direct Zig-side analogues for `build` (`zig build`) and
`test` (`zig build test`); `run`/`fmt`/`check` are not present in `build.zig`
at all.

Outside `build.zig`, `examples/coreutils/build.sh` shows how an Mlx program
is actually compiled today — by invoking a compiler binary directly against
a source file with `-o`, no `build.mlx` involved:

```sh
"$compiler" \
    "$repo_root/examples/coreutils/$utility/main.mlx" \
    -o "$bin_dir/$utility"
```

(`examples/coreutils/build.sh`)

Summary of what's implemented versus aspirational:

| Spec concept | Status |
| --- | --- |
| `mlx build`/`run`/`test`/`fmt`/`check` subcommands | Not implemented; `build.zig` only provides Zig-level `build`/`test` steps for Stage-0 |
| `build.mlx` project configuration | Not implemented; no file of this name exists in the repository |
| Target model (`arch os abi endian pointer_bits cpu_features`, `builtin` module) | Normative in `spec/05-build/targets.xml`; not exercised by `build.zig`, which uses Zig's own `standardTargetOptions`/`standardOptimizeOption` |
| Sandboxed build-script host I/O (`std.Build` APIs only) | Applies to the canonical `build.mlx` system; `build.zig` is ordinary unsandboxed Zig build-script code, since Zig tooling is explicitly Stage-0-only |
| Package identity/mapping/versioning rules | Normative in `spec/05-build/packages.xml`; no package manifest or resolver exists yet anywhere in the repository |
| Direct compiler invocation (`compiler source.mlx -o output`) | The actual mechanism in use today, shown by `examples/coreutils/build.sh` |

This gap is expected at this stage of the project: per the bootstrap chain,
the canonical build/package system is downstream of `mlx1`/`mlx2` and the
Stage-1 standard library, which themselves are still being built up through
`build.zig`.
