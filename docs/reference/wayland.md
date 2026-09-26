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

`std/protocols/wayland/` holds the canonical XML exactly as published
upstream, the provenance record, and the bootstrap tool:

| File | Content |
| --- | --- |
| `wayland.xml` | core protocol, wayland 1.26.0 |
| `xdg-shell.xml` | stable xdg-shell, wayland-protocols 1.49 |
| `linux-dmabuf-v1.xml` | stable linux-dmabuf (`zwp_linux_dmabuf_v1`, version 6), wayland-protocols 1.49 |
| `SOURCES` | upstream URL, release and SHA-256 of each XML file |
| `materialize.mlx` | Stage-1 tool: XML to `std/src/wayland/generated/` |
| `materialize.sh` | verifies the hashes, builds the tool, runs it |

```sh
std/protocols/wayland/materialize.sh           # regenerate std.wayland
std/protocols/wayland/materialize.sh --check   # fail if generated code is stale
```

The script refuses to run when an XML file no longer matches the hash in
`SOURCES`, which enforces the README's rule that bootstrap tooling "must
preserve upstream protocol contents exactly and record source/version/hash
metadata" (`std/protocols/wayland/README.md`). The generated modules are
committed, so ordinary builds never read XML: they compile
`std/src/wayland/generated/*.mlx` like any other std source.

### The implemented pipeline

Each stage of `<ImplementationPipeline>` is an ordinary Mlx module:

| Stage | Module | Fixture |
| --- | --- | --- |
| XML tokenizer / pull parser | `std.xml` (`std/src/xml.mlx`) | `tests/240_std_xml_tokenizer_runtime.mlx` |
| protocol parser, AST, validation | `std.wayland.protocol` | `tests/241_wayland_protocol_parser_runtime.mlx` |
| typed declaration generation | `std.wayland.materialize` | `tests/246_wayland_generated_api_runtime.mlx` |
| ordinary `std.wayland` module | `std/src/wayland.mlx` + `generated/` | `tests/244_wayland_client_server_runtime.mlx` |

`std.xml` is an allocation-free pull tokenizer. It reports declarations,
comments, element starts, one token per attribute, element ends
(self-closing ones flagged), text and CDATA, checks well-formedness (matching
end tags, unique attributes, a single root, valid references) and decodes
the predefined entities and numeric character references on request.

`std.wayland.protocol` builds the AST of `protocol-ast.xml` and applies
wayland-scanner's hard errors: identifier rules, `since`/`deprecated-since`
bounds, `frozen` interfaces at version 1, `destroy` requests that must be
destructors, argument types, `interface` only on object/new_id, `allow-null`
only on object/string, enum-typed arguments only on int/uint, bitfield enums
only on uint, and non-empty enums. It also rejects every element or attribute
the schema does not define; the only ignored constructs are comments,
whitespace and `<description>` inside `<arg>`/`<entry>`. The parser test
checks these errors one by one against small documents, and parses both
vendored files.

`std.wayland.materialize` produces, for the whole protocol set:

- `generated/interfaces.mlx`: the `Interface` enum and the metadata the
  runtimes dispatch on — names, versions, request/event counts,
  libwayland-format signatures (`"?oii"`, `"usun"` for a generic new_id),
  destructor flags and the interface of every object/new_id argument. Every
  signature and argument interface was checked against `wayland-scanner`'s
  output for the same XML.
- `generated/<protocol>/types.mlx`: a client `Proxy` type with one method
  per request and a server `Resource` type with one `send…` method per
  event.
- `generated/<protocol>/<interface>.mlx`: the per-interface namespace
  (`wl.wl_surface`, `wl.xdg_toplevel`, ...) with `VERSION`,
  `REQUEST_*`/`EVENT_*` opcodes and `*_SINCE` versions, enums with the exact
  XML values and `u32` backing, `Proxy`/`Resource`, payload structs, and
  `decodeEvent`/`decodeRequest` returning tagged unions.

Argument types follow `<XMLMapping>`: `int` is `i32`, `uint` is `u32`,
`fixed` is `wl.Fixed`, strings are `[]const u8` (`?[]const u8` when
nullable) in sent messages and `wl.String` in decoded ones, objects are the
typed proxy or resource (`?*const T` when nullable), `array` is
`wl.ArrayView`, and `fd` is `wl.Handle`. A typed new_id becomes the method's
return value. Bitfield enums combine through `std.wayland.flags`. The naming
convention is recorded as a provisional choice in `SPEC_CONFLICTS.md`.

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

## Implementation notes: runtime and transport

`std.wayland.client` (`std/src/wayland/client.mlx`) owns the connection:
`Display.connect` reads `WAYLAND_SOCKET` or `WAYLAND_DISPLAY` (relative to
`XDG_RUNTIME_DIR`) from the process environment, and `connectTo` or
`connectToHandle` take an explicit path or socket. Object ids are allocated
from the client range and recycled after `wl_display.delete_id`. New objects
take their parent's version, as in libwayland, and a request newer than its
object's version is refused. Failures inside request methods are sticky: the
request returns an inert proxy and the next `flush`, `dispatch` or
`roundtrip` returns the error. The server's `wl_display.error` surfaces as
`error.ProtocolError`, and the object id, code and message are available
from the display. `failureReason()` says in words why a display stopped
running; for an event the client cannot decode (unknown object or opcode,
malformed arguments) `rejectedObjectId`, `rejectedOpcode` and
`rejectedInterface` name it. Events go to per-object handler functions and are decoded
with the generated `decodeEvent`. Objects that the server creates through a
new_id event argument are registered before their event is dispatched.

`std.wayland.server` (`std/src/wayland/server.mlx`) listens on
`$XDG_RUNTIME_DIR/<name>` behind the `<name>.lock` convention, accepts or
adopts clients, and dispatches from an epoll loop. It answers
`wl_display.sync` and `get_registry` itself, validates `wl_registry.bind`
against the global's name, interface and version, and advertises and
withdraws globals on every registry. Typed new_id arguments become resources
before the request handler runs, and destructor requests destroy their
resource afterwards (sending `delete_id`). Unknown objects, unknown opcodes,
malformed arguments, requests newer than the resource version and invalid
new ids are answered with `wl_display.error` using libwayland's codes, and
the client is then disconnected (`tests/245_wayland_server_errors_runtime.mlx`).

The Linux transport (`std/src/wayland/transport/linux.mlx`) implements the
five `transport.xml` rules with direct syscalls. Unix stream sockets carry
`SCM_RIGHTS` descriptors. Received descriptors are queued in arrival order,
and a message is only dispatched once its bytes and all of its handles are
buffered. Queued handles are attached to the first `sendmsg` that carries
their bytes. Connection buffers come from the caller's allocator. Waiting
uses `poll` for one connection and `epoll` for any number of connections and
listeners. `memfd_create` objects are mapped with `MAP_SHARED`
(`tests/243_wayland_linux_transport_runtime.mlx`,
`tests/242_wayland_wire_runtime.mlx`). `wl.transport.SharedMemory` and
`wl.transport.socketPair` expose these to applications.

### Wire compatibility

`<Conformance>` requires wire compatibility with established
implementations. `tools/check_wayland_interop.sh` checks it against
libwayland in both directions:

- `examples/wayland-client` opens an `xdg_toplevel` window on a headless
  weston and presents frames through `wl_shm`;
- `examples/wayland-server` is enumerated by `wayland-info` and drives
  `weston-simple-shm`;
- the Mlx client and compositor exchange frames whose pixel checksums the
  compositor verifies.

`tests/run_wayland.sh` runs every fixture above with the self-hosted
compiler (`mlx1` or the fixed-point `mlx4`) and checks that the generated
modules are current.

### Bootstrap compiler constraints

The runtime is written in the subset of Mlx that the self-hosted compiler
lowers correctly today, and each workaround is commented at its use site:

- field addresses (`&value.field`) are avoided, so the code uses pointers
  to separately allocated state and copy-modify-write on tables;
- error unions carry only scalar or void payloads, so constructors
  initialize in place, as in `Display.connect(&display, allocator)`;
- a struct's methods may only name types declared earlier, so generated
  types are emitted in dependency order;
- decoded strings use `std.string.String` rather than optional slices;
- signed words are converted arithmetically rather than with
  `@bitCast(i32, u32)`, and early exits from `!void` functions return
  `success()` instead of a bare `return`.

## Client and server code

Both directions import only `std.wayland`. A client:

```mlx
const std = @import("std")
const wl = @import("std.wayland")

fn onRegistry(context: *anyopaque, event: wl.client.Event) -> void {
    const decoded = wl.wl_registry.decodeEvent(event)
    if event.opcode == wl.wl_registry.EVENT_GLOBAL {
        const global = decoded.global
        // global.name, global.interface (wl.String), global.version
    }
}

pub fn main() -> !void {
    var display: wl.client.Display = undefined
    try wl.client.Display.connect(&display, std.page_allocator.init())
    const registry = wl.displayProxy(display).getRegistry()
    var state: u32 = 0
    unsafe { registry.setHandler(onRegistry, @ptrCast(*anyopaque, &state)) }
    try display.roundtrip()
    while display.running() {
        try display.dispatch()
    }
    display.disconnect()
}
```

A server:

```mlx
const std = @import("std")
const wl = @import("std.wayland")

fn bindCompositor(context: *anyopaque, client: wl.server.Client, object: wl.server.Object, version: u32) -> void {
    // set a request handler on wl.wl_compositor.Resource.fromObject(object)
}

pub fn main() -> !void {
    var display: wl.server.Display = undefined
    try wl.server.Display.init(&display, std.page_allocator.init())
    try display.addSocket("wayland-0")
    var state: u32 = 0
    var context: *anyopaque = undefined
    unsafe { context = @ptrCast(*anyopaque, &state) }
    const name = display.createGlobal(wl.Interface.wl_compositor, 6, bindCompositor, context)
    while display.running() {
        try display.dispatch(-1)
    }
    display.deinit()
}
```

`examples/wayland-client/main.mlx` and `examples/wayland-server/main.mlx`
are the complete programs, with shared memory, xdg-shell configuration and
frame callbacks.

## Larger examples: a nested compositor and a terminal

Two programs use both halves of `std.wayland` with real input:

- [`examples/wayland-compositor`](../../examples/wayland-compositor/README.md)
  is a compositor that runs freestanding (DRM/KMS output, evdev input, the
  devices and VT switching from systemd-logind over its own D-Bus client,
  the keymap from libxkbcommon) or nested. Nested, it is a client of the
  session compositor (its output is one window there) and a server for its
  own clients. It routes
  the session's pointer and keyboard to the window under the pointer or
  with focus, and hands clients the session's xkb keymap. It moves windows
  through `xdg_toplevel.move` or Alt+drag, resizes them by their border,
  Alt+right-drag or `xdg_toplevel.resize` (one configure at a time, the
  opposite edges kept), places `xdg_popup` windows with
  `xdg_positioner`, and launches programs such as a terminal on Alt+Enter.
  Its `wl_data_device_manager` carries the clipboard between clients (GTK 4
  applications such as Nautilus need it to use the display). With
  `--fullscreen` it takes the host monitor's resolution, which is how
  `install_compositor.sh` installs it as a GDM/SDDM session
  (inside cage or weston's kiosk shell). It waits on
  both connections with `wl.transport.waitAny`.
- [`examples/wayland-terminal`](../../examples/wayland-terminal/README.md)
  is a terminal emulator: a shell on a pseudo-terminal, keyboard input with
  key repeat, and a built-in bitmap font.

`tools/check_wayland_compositor.sh` runs both under
`tools/wayland-test-host`, a scripted host compositor with a seat. Typed
commands must reach the shells (they create marker files), Alt+Enter must
open a second terminal, and Alt+drag must move it to the expected pixel
position in the host's screenshot. Resizing a terminal by its border and
with Alt+right-drag must give exact window sizes and positions, and its
shell must learn each size. weston-terminal, when installed, must
accept Shift through the forwarded keymap and move by its title bar;
wl-clipboard, when installed, must paste in one client what another copied;
gtk4-widget-factory, when installed, must open its window.
`tools/check_compositor_drm.sh [cpu|vulkan]` runs the freestanding
compositor against an emulated kernel (DRM card, evdev devices) and an
emulated logind on a private dbus-daemon, with real clients: modeset at the
preferred mode, typing, mouse, touchpad, hotplug, a VT switch and the
console restored on exit; with `vulkan`, composing on lavapipe into the
emulated dumb buffers (exported as dma-bufs), then copying frames into
them.
`tools/check_wayland_interop.sh` also runs the nested compositor inside
weston. `tests/268_compositor_damage_runtime.mlx` (run by
`tests/run_wayland.sh`) plays random changes to a scene (windows moved,
raised, resized and redrawn, focus, the cursor, a popup) against the
compositor's damage tracking: frames composed only in what each output
buffer lacks must equal frames composed from scratch.

### GPU buffers: linux-dmabuf and Vulkan

`wl.zwp_linux_dmabuf_v1`, `wl.zwp_linux_buffer_params_v1` and
`wl.zwp_linux_dmabuf_feedback_v1` are generated like the core interfaces.
A client collects a buffer's planes (`params.add(fd, plane, offset, stride,
modifier_hi, modifier_lo)`) and creates the `wl_buffer` with
`createImmed(width, height, format, flags)`; a server decodes the same
requests (`AddRequest.fd` is a received `wl.Handle`) and answers `create`
with `sendCreated()` (which creates the `wl_buffer` resource) or
`sendFailed()`.

- [`examples/vulkan-wayland-client`](../../examples/vulkan-wayland-client/README.md)
  renders with Vulkan (`std.vulkan`, no C loader) and hands frames over as
  dma-bufs, or renders straight into its `wl_shm` pool.
- The compositor offers linux-dmabuf (ARGB8888/XRGB8888, linear) and,
  with `--renderer vulkan`, composes on the GPU, reading client pools and
  dma-bufs in place and rendering into the output in place: nested, the
  host window's `wl_shm` buffer; freestanding, the DRM dumb buffer the
  monitor scans out, exported as a dma-buf. `--renderer auto` (the
  default) takes Vulkan when a driver works and the CPU otherwise, and the
  freestanding compositor checks that the GPU's first frame reached the
  dumb buffer before trusting the import.

`tools/check_vulkan_wayland.sh` runs the client inside the compositor with
both renderers, over `wl_shm` and linux-dmabuf, and compares the frames
pixel by pixel (see [the Vulkan reference](vulkan.md#examples)).

Neither program imports `std.xml`, registers protocol XML
in a build file, or calls anything schema-shaped. From an application's
point of view `std.wayland` is an ordinary stdlib import, and the whole
XML-to-Mlx pipeline above ran when the standard library was bootstrapped.
