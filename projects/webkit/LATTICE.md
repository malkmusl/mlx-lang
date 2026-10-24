# Lattice

Lattice is the engine Voyager moves to after the WebKit port's bootstrap
(PORT.md's milestones): WebKit translated into Mlx, then grown into an
engine of its own, as Blink grew out of WebKit. It starts as WebKit 2.54.1
(projects/webkit/upstream, kept exactly as released: UPSTREAM.md,
tools/check_webkit_upstream.sh) and is built, like everything here, only
with this repository's toolchain.

Voyager names its engine in one place, `ENGINE` in
port/platform/mlx/UserAgentMlx.cpp: today `Voyager/0.1 (WebKit 2.54.1;
Wayland)`, with Lattice in a shipping build `Voyager/0.x (Lattice 0.0.1;
Wayland)`. The compatibility part of the user agent (`AppleWebKit/605.1.15
... Safari/605.1.15`) stays as long as Lattice behaves like WebKit, as
Blink kept `AppleWebKit/537.36`.

## The principles

- **Translate first, change later.** A component is first translated so
  that it does what the C++ does, line for line where that is sensible,
  with Mlx's means where they fit (std containers, std.heap, `@noncopy`
  with `deinit`, tagged optionals, errors instead of crashes). Redesign
  and optimisation come once a component is at parity, measured with
  mlx-profile, never in the same step.
- **The C++ is the reference.** projects/webkit/upstream is never
  edited. Every translated component is tested against the C++ one it
  replaces, on the same inputs (differential tests), and the browser's
  checks (layout, paint, storage, network, crypto, compositing,
  check_voyager.sh) pass the same with either.
- **One component at a time, in the running browser.** A component
  replaces its C++ counterpart behind the interface the two share, so
  Voyager runs with any mix of translated and C++ components. A build
  switch selects them (`LATTICE=translator,...`); the default moves to
  the translated one once it is at parity, and the C++ one is retired
  from the build (PORT.md's "What is retired").
- **Along narrow interfaces.** Mlx calls C and is called as C (an
  `export fn` and its header from mlx-cheaders); it does not call C++
  templates. A component is therefore cut out where its interface is
  C-like or can be made so by a thin C++ shim of the port's own: ANGLE's
  EGL/GLES API, the shader translator's `sh::` entry points, the port's
  clients and strategies, Skia's canvas. Large cores whose insides are
  templates throughout (WTF, JavaScriptCore, WebCore) are translated as
  whole layers, not function by function; how is decided with the
  experience of the pilot below.
- **The code map knows both sides.** mlx-codemap links an Mlx `export fn`
  with its C++ callers; the index of what is compiled (C++ and Mlx) is
  the map of what is left to translate (`mlx-codemap reach`, `refs`).

## The tool

Translation is not done by hand from the text. mlx-cc already parses and
type-checks all of WebKit (templates instantiated, overloads resolved,
names bound); a translator built on its front end (`mlx-cc --emit-mlx`, a
step of L0) writes a first Mlx version of a component from that typed
tree: declarations, control flow, expressions with their resolved types,
instantiated templates as generic functions or concrete copies. The hand
then makes it Mlx (ownership, errors, containers), and the differential
tests say when it is right. What the translator cannot express is listed
per component, so the tool improves before the hand work grows.

## The pilot: ANGLE

ANGLE (Source/ThirdParty/ANGLE) comes first: it is self-contained, its
interfaces are C (EGL, GLES) and a small C++ API (the shader
translator's), it is needed for WebGL (PORT.md, M8 part 2), and
std.vulkan and std.spirv exist already. What WebGL needs of it, in lines
of C++ (2.54.1):

| Part | Lines | Role |
|---|---|---|
| `src/common` | ~91,000 | utilities, math, hashing, the platform layer |
| `src/compiler/translator` | ~161,000 | GLSL ES → SPIR-V (the parts the Vulkan backend uses) |
| `src/libANGLE/renderer/vulkan` | ~170,000 | the Vulkan backend |
| rest of `src/libANGLE` | ~200,000 | GL state, validation, the front end (without the backends WebGL does not use) |
| `src/libGLESv2` | ~115,000 | the entry points, mostly generated |

The order:

1. **The shader translator** (`projects/lattice/angle/translator`): GLSL
   ES 1.00/3.00 to SPIR-V on std.spirv, behind the `sh::` API the C++
   one has (a C shim exports it to ANGLE's C++). Differential tests: every
   shader of the WebGL LayoutTests and the conformance tests run, through
   both translators: the same compile errors, the same uniforms and
   attributes reported, and SPIR-V that validates and renders the same
   pixels on lavapipe.
2. **The Vulkan backend** on std.vulkan directly (no C Vulkan API in
   between once it is Mlx): the paint and WebGL checks the same.
3. **GL state, validation and the entry points**: libmlxangle with the
   EGL/GLES C API WebCore's GraphicsContextGLANGLE calls, then the C++
   ANGLE retired.

## Milestones

| | Goal | Done when |
|---|---|---|
| L0 | Groundwork | WebKit in the repository unmodified and guarded (done); the build takes it from there (done); the code map indexes everything compiled; `mlx-cc --emit-mlx` translates a sample of ANGLE's common/ that compiles with mlx4 and passes ANGLE's own tests of it |
| L1 | ANGLE's shader translator in Mlx | the differential tests above pass for every shader of the WebGL tests; Voyager runs WebGL with it |
| L2 | ANGLE in Mlx | WebGL checks and paint pages the same with the Mlx ANGLE; the C++ ANGLE retired from the build |
| L3 | The plan for WebKit | from L0–L2's experience: the order of WTF, JavaScriptCore, WebCore and the port, what is translated and what is rewritten, written here |
| L4 and on | WebKit, layer by layer | each layer as ANGLE was: at parity, then the default, then the C++ retired; `ENGINE` becomes Lattice with the first layer in a shipping build |

## Open questions

- How templates become Mlx: generic functions with `comptime` types
  where the C++ is generic in earnest, concrete copies where it is
  instantiated a few times.
- Where the translator's output is kept: generated files in the
  repository (reviewable, edited by hand afterwards) versus a build step;
  the first, as hand work follows.
- Threads and memory: WTF's RefPtr and the garbage collector's rules
  versus Mlx's ownership; settled with JavaScriptCore at L3, not before.
