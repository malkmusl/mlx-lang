# Wayland: protocol-AST pipeline and transport

**Normative source:** `spec/06-wayland/wayland.xml` (`MlxWaylandIntegration`),
`spec/06-wayland/protocol-ast.xml` (`MlxWaylandProtocolAST`),
`spec/06-wayland/transport.xml` (`MlxWaylandTransport`). **Companion to:**
[`docs/WAYLAND.md`](../WAYLAND.md) and [`docs/BOOTSTRAP.md`](../BOOTSTRAP.md),
which give the one-page architecture summary; this page goes one level
deeper into the pipeline, the AST shape, and the transport contract, and
assumes you've already read those two.

This page does not re-derive the top-level picture — "Wayland is
`std.wayland`, generated from canonical XML during stdlib bootstrap, with no
compiler special case" — that's `docs/WAYLAND.md`. What follows is what that
summary elides: what the protocol AST actually contains, what the transport
layer normatively guarantees, and what the bootstrap-input directory and the
example apps look like in practice.

## Why this is a stdlib-bootstrap extension, not a compiler feature

`spec/06-wayland/wayland.xml` states the constraint three separate ways:

```xml
<NoCompilerSpecialCase>true</NoCompilerSpecialCase>
<NoBuildRegistration>true</NoBuildRegistration>
<NoSourceBuiltin>true</NoSourceBuiltin>
<ForbiddenDesigns>
  <Design>@protocol(...)</Design>
  <Design>@importSchema(...)</Design>
  <Design>Build.addProtocol(...)</Design>
  <Design>Project-local required Wayland XML generation</Design>
</ForbiddenDesigns>
```

(`spec/06-wayland/wayland.xml`)

That list of forbidden designs is the point: it's not merely that no such
builtin exists yet, it's that a `@protocol(...)` builtin, a
`@importSchema(...)` builtin, or a `Build.addProtocol(...)` build-system hook
are explicitly ruled out as future designs too. The root `README.md`'s "Core
rules" section restates the same constraint at the top level:

> XML/JSON are normal stdlib modules, not compiler schema features
> Wayland client+server are provided by `std.wayland`; canonical XML is
> consumed during stdlib bootstrap, never required in ordinary project
> builds

(`README.md`, Core rules)

The reasoning follows directly from the rest of the spec's design: the
compiler's job is fixed to "modules, comptime, types and reflection" — see
`<Rule>The compiler itself knows modules, comptime, types and reflection. It
does not know XML or Wayland semantics.</Rule>` in `wayland.xml`. XML
parsing, protocol-AST construction, and Wayland wire encoding are all
expressible as ordinary Mlx code once the language has enough of itself
(modules, structs, slices, comptime) — so putting Wayland knowledge into the
compiler would just be scope creep with no expressiveness gain. Concretely,
this also means a project can never be blocked by an out-of-date compiler
XML schema: `std.wayland` is a library dependency like any other, versioned
and rebuilt independently of the compiler binary.

`docs/BOOTSTRAP.md` places this in the bootstrap chain explicitly: Wayland
is listed under "Stage-1 extensions (non-blocking)", alongside `std.xml`,
`std.json`, and broader `std.posix`/`std.os.*` — i.e. it's optional stdlib
surface that Stage-1 can add without blocking the path to `mlx2` and
brixOS (`docs/BOOTSTRAP.md`).

## The protocol-AST pipeline in detail

`docs/WAYLAND.md`'s diagram gives the pipeline stages by name:

```
canonical wayland.xml / extension XML -> std.xml -> std.wayland.protocol parser
  -> Protocol AST -> client decls / server decls -> std.wayland
```

`wayland.xml`'s `<ImplementationPipeline>` names the same stages
normatively, and adds one this diagram compresses away — schema/version
validation happens *between* AST construction and code generation, not
folded into either:

```xml
<ImplementationPipeline>
  <Stage>std.xml tokenizer/parser implemented in Mlx when Stage-1 capabilities permit</Stage>
  <Stage>std.wayland.protocol parser implemented in Mlx</Stage>
  <Stage>Wayland Protocol AST</Stage>
  <Stage>schema/version validation</Stage>
  <Stage>generation/materialization of typed Mlx declarations for client and server</Stage>
  <Stage>compile as ordinary std.wayland module</Stage>
</ImplementationPipeline>
```

(`spec/06-wayland/wayland.xml`)

### The Protocol AST shape

`spec/06-wayland/protocol-ast.xml` is the normative shape of the AST that
the `std.wayland.protocol` parser must produce from parsed XML. It is small
enough to quote in full:

```xml
<Protocol>name, copyright, description, interfaces[]</Protocol>
<Interface>name, version, description, requests[], events[], enums[]</Interface>
<Message>name, opcode assigned by declaration order, since(default 1), type(normal|destructor), description, args[]</Message>
<Arg>name, type, interface?, allowNull(default false), enum?, summary?</Arg>
<Enum>name, since(default 1), bitfield(default false), entries[]</Enum>
<Entry>name, value(expression parsed as integer), since(default 1), summary?</Entry>
<Validation>Unknown required XML constructs are hard errors. Unknown non-semantic documentation elements may be ignored only if explicitly listed as ignorable by the schema parser.</Validation>
```

(`spec/06-wayland/protocol-ast.xml`)

Two details worth calling out because they aren't obvious from the element
list alone:

- **Opcodes are positional, not declared.** A `Message`'s opcode is "assigned
  by declaration order" — the parser must count request/event declarations
  within an interface rather than reading an explicit opcode attribute from
  the XML (upstream Wayland protocol XML doesn't carry opcodes either; they
  come from source order, same as this spec requires).
- **Validation is a strict/permissive split, not a strict/lenient one.**
  Unknown constructs are hard errors by default. The parser may only skip an
  unrecognized element when the schema parser itself has an explicit
  allow-list of ignorable documentation elements (e.g. `<description>`
  variants) — there's no generic "ignore what I don't understand" fallback,
  which matches the project's "never invent behavior" rule: an unrecognized
  *semantic* construct must fail loudly rather than be silently dropped.

`wayland.xml`'s `<XMLMapping>` block is the normative link between the raw
XML argument `type` attribute and the Mlx type each one lowers to:

```xml
<Arg name="int">i32</Arg>
<Arg name="uint">u32</Arg>
<Arg name="fixed">wl.Fixed, signed 24.8 value represented by i32</Arg>
<Arg name="string">nullable or non-null UTF-8 byte slice according to allow-null; wire includes trailing NUL and 4-byte padding</Arg>
<Arg name="object">typed object reference/proxy/resource according to interface and nullability</Arg>
<Arg name="new_id">typed created object identity; generic new_id follows Wayland interface/version metadata rules</Arg>
<Arg name="array">wl.ArrayView raw byte slice with Wayland alignment</Arg>
<Arg name="fd">wl.Handle transported out of band by the transport backend</Arg>
```

(`spec/06-wayland/wayland.xml`)

So `Arg.type` in the Protocol AST is not itself a Mlx type — it's the raw
Wayland wire-type name (`"int"`, `"fixed"`, `"new_id"`, ...); the
`std.wayland` code generator is the piece responsible for mapping each one to
a concrete Mlx type (`i32`, `wl.Fixed`, a generated proxy/resource type,
etc.) according to this table, and generated enums "use explicit integer
backing and exact XML values" with bitfield enums going through "std.wayland
flag helpers without operator overloading" — i.e. flag combination is a
named helper call, not `|`/`&` on the enum type.

### Bootstrap input location

`std/protocols/wayland/README.md` is short but normative about intent, not
just location:

```
Canonical `wayland.xml` and selected extension XML files are placed or
acquired here by the Stage-0/Stage-1 stdlib bootstrap process. They are
source inputs for materializing `std.wayland`; they are not user-project
build inputs.

The repository intentionally does not vendor an invented substitute XML
file. Bootstrap tooling must preserve upstream protocol contents exactly and
record source/version/hash metadata.
```

(`std/protocols/wayland/README.md`)

As of this writing that directory contains only the `README.md` — the
canonical `wayland.xml`/extension XML itself has not been vendored in yet,
which is consistent with `spec/06-wayland/wayland.xml`'s framing of this as
Stage-0/Stage-1 bootstrap work still ahead, and with the project's rule
against inventing a stand-in schema file. `protocols/README.md` at the repo
root makes the same "this isn't ordinary project configuration" point from
the caller's side:

```
Protocol source data used to construct standard-library modules belongs to
the toolchain/stdlib source process, not ordinary Mlx project configuration.
Wayland's canonical XML is handled under `std/protocols/wayland` during
Stage-0/1 stdlib bootstrap.
```

(`protocols/README.md`)

## The transport layer

`spec/06-wayland/transport.xml` (`MlxWaylandTransport`) is the normative
contract underneath `std.wayland.client`/`std.wayland.server` — it's what
`docs/WAYLAND.md`'s diagram means by the `Linux`/`BSD`/`brixOS` branches at
the bottom of the pipeline. Five constraints, in full:

```xml
<Handle>Opaque transport-level transferable handle. On Linux/BSD it wraps an fd; on brixOS it may wrap a native transferable handle.</Handle>
<MessageAtomicity>The library must preserve byte ordering and associate received ancillary handles with the message arguments that consume them, including partial stream reads/writes.</MessageAtomicity>
<Buffering>Transport may buffer without heap allocation only when caller-provided buffers suffice; otherwise explicit allocator ownership is required.</Buffering>
<Wait>Must integrate with target event wait primitive and support multiple connections/listeners.</Wait>
<SharedMemory>Must allow a compositor and client to map the same exported memory object with explicit lifetime management.</SharedMemory>
```

(`spec/06-wayland/transport.xml`)

Reading this against `wayland.xml`'s `<Transport>` block, the platform
split is: **one public API** (`<SamePublicAPI>true</SamePublicAPI>`)
backed by different concrete transports —

- **Linux**: native Mlx Unix-domain socket transport, ancillary
  (`SCM_RIGHTS`-style) descriptor transfer, shared memory, and a Linux wait
  primitive.
- **BSD**: the same shape, with a poll/kqueue-compatible wait primitive.
- **brixOS**: native local IPC and shared memory, which "need not be POSIX"
  as long as message ordering, peer lifecycle, handle transfer, and
  shared-memory semantics are preserved.

`Handle` is the abstraction that makes this possible: it's the type
`wl.Handle` referenced in the `fd` argument-type mapping above, and it's
deliberately opaque at the `std.wayland` API level — a client/server never
sees a raw fd, only a `Handle` — so that a brixOS backend can substitute its
own native transferable handle type without changing generated client/server
code. `MessageAtomicity`'s "including partial stream reads/writes" clause is
what rules out a naive implementation that reads a Wayland message header
and payload as two separate blocking reads without tracking how many
ancillary handles have arrived relative to how many bytes of the message
body have been consumed — a short read must not desynchronize the handle-to
argument association.

`Buffering`'s allocator rule is the same one from
`spec/00-language/memory-model.xml` applied to this specific subsystem: the
transport is allowed a fixed/caller-supplied buffer fast path with no heap
involvement, but as soon as it needs a buffer the caller didn't provide, that
allocation must go through an explicit, named allocator — never a hidden
`malloc`-equivalent inside the transport.

## The wire protocol contract

Separate from the transport (which moves bytes/handles) and the AST (which
describes message shape), `wayland.xml`'s `<WireProtocol>` block fixes the
on-the-wire encoding generated client/server code must produce and consume:

```xml
<Header>Wayland object id plus packed message size/opcode header exactly according to the Wayland wire protocol.</Header>
<Alignment>Arguments use Wayland 32-bit alignment/padding rules.</Alignment>
<Strings>Length includes trailing NUL and byte payload is padded to 4-byte boundary.</Strings>
<Arrays>Length prefix plus bytes padded to 4-byte boundary.</Arrays>
<Handles>Descriptors/handles are associated out-of-band in message order and are not serialized as integer payload fields.</Handles>
<ObjectIds>u32 connection-local identifiers; zero means null only where allowed.</ObjectIds>
```

(`spec/06-wayland/wayland.xml`)

This is what makes `<Conformance>`'s wire-compatibility rule checkable:
"Generated client and server declarations from the same XML are
wire-compatible with established Wayland implementations." Because the
encoding rules (header layout, 4-byte alignment, NUL-terminated
length-prefixed strings, out-of-band handle association) are fixed
independently of which XML was fed in, an Mlx Wayland client generated from
the standard `wl_compositor`/`wl_surface`/etc. protocol can talk to a
non-Mlx compositor, and vice versa — the pipeline's job is only to turn XML
into typed Mlx declarations, not to invent a new wire format.

## Client code: what actually gets imported

Both `docs/WAYLAND.md` and `wayland.xml`'s `<UserAPI>` block agree on the
one import an application needs:

```mlx
const wl = @import("std.wayland")
```

The two example apps under `examples/` show this in the two directions the
spec calls out — `<Client>` and `<Server>` in `wayland.xml`. The client:

```mlx
const wl = @import("std.wayland")
const std = @import("std")

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator)
    defer arena.deinit()

    var display = try wl.client.Display.connect(arena.allocator())
    defer display.disconnect()

    const registry = try display.getRegistry()
    _ = registry

    while display.running() {
        try display.dispatch()
    }
}
```

(`examples/wayland-client/main.mlx`)

And the server/compositor side, using `wl.server` instead of `wl.client`:

```mlx
const wl = @import("std.wayland")
const std = @import("std")

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator)
    defer arena.deinit()

    var display = try wl.server.Display.init(arena.allocator())
    defer display.deinit()

    try display.addSocket("wayland-0")

    while display.running() {
        try display.dispatch()
    }
}
```

(`examples/wayland-server/main.mlx`)

Both examples pass an explicit `arena.allocator()` into `Display`
connect/init, consistent with `memory-model.xml`'s allocator rule and with
`transport.xml`'s `Buffering` constraint — connection/display state that
needs to outlive a single stack frame is allocator-owned, not hidden. Note
what these examples do *not* do: neither one imports `std.xml`, registers
protocol XML in a build file, or calls anything schema-shaped. That absence
is itself the point the rest of this document is about — from an
application's point of view, `std.wayland` is exactly as ordinary an import
as any other stdlib module, and the entire protocol-AST/XML pipeline above
has already run by the time this code is compiled.

`wayland.xml`'s `<Client>`/`<Server>` rules describe what's behind
`wl.client.Display`/`wl.server.Display` at a level neither example currently
exercises: `std.wayland.client` additionally provides object ID allocation,
request marshaling, event decode/dispatch, delete-id handling, object
version gating, and protocol-error handling; `std.wayland.server`
additionally provides client/global/registry bookkeeping, resource
management, request decode/dispatch, event send, and the same version-gating
and protocol-error handling on the server side. Both example apps above are
minimal — connect-and-dispatch-loop — because they're demonstrating the
import surface, not exercising the full generated protocol API.
