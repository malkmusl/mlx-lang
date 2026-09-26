# Mlx 1.0 normative conflicts

## Open normative gaps

These are omissions rather than contradictions, but they prevent a conforming
implementation from choosing behavior without designing new language rules.

### Atomic builtin call shapes

`spec/00-language/atomics-tls.xml` names `@atomicLoad`, `@atomicStore`,
`@atomicRmw`, `@cmpxchgWeak`, `@cmpxchgStrong`, and `@fence`, and defines the
available memory orders. It does not define argument order, result types, the
representation of an RMW operation, or the valid order combinations per
builtin. Stage 0 and the canonical compiler recognize every name and report
MLX-E9001; neither lowers an invented calling convention.

### Vector builtin call shapes

`spec/00-language/vectors.xml` names `@splat`, `@shuffle`, `@reduce`, and
`@select`, but does not define their arguments, reduction-operation encoding,
shuffle-mask representation, or exact result typing. Stage 0 recognizes every
name and reports a structured lowering error until those contracts are
normative.

### Reflection metadata schemas

`spec/00-language/comptime.xml` requires `@typeInfo`, `@hasDecl`, and `@decl`,
but does not define the value schema returned by `@typeInfo` or declaration
metadata/lookup behavior. Stage 0 implements reflection operations whose result
is unambiguous from the specification and emits MLX-E5005 for these unresolved
metadata operations.

### Language-version value

`spec/00-language/modules.xml` requires `@languageVersion`, but does not define
its result type or value format. Stage 0 recognizes the zero-argument builtin
and emits MLX-E5005 rather than selecting a private representation.

### Conversion builtin signatures

`spec/00-language/types.xml` names the conversion builtins without defining
their call shapes. The only concrete example is target-first
`@ptrFromInt(*volatile u32, address)` in `spec/00-language/unsafe.xml`. Stage 0
provisionally applies that target-first, two-argument shape consistently to the
other conversion builtins; this must not be treated as a finalized normative
contract.

### Non-pointer optional layout

`spec/00-language/types.xml` fixes the null niche representation for `?*T`, but
does not define the size, alignment, tag placement, or ABI representation of a
general non-pointer `?T`. Stage 0 therefore implements null-niche optional
pointers and preserves the existing present-value path for other optionals,
without exposing an invented general optional layout through reflection.

### Thread-local storage ABI

`spec/00-language/atomics-tls.xml` fixes the source spelling, declaration
scope, initializer requirement, and absence of heap allocation for
`threadlocal`, but does not define the executable TLS model, TLS relocation
model, per-thread initialization protocol, or how a freestanding executable
obtains its thread pointer. The compiler preserves and validates the
declaration marker, and the canonical compiler reports MLX-E9001 before object
emission rather than silently treating TLS as ordinary global storage. It
cannot emit a private TLS ABI until that contract is normative.

### Standard thread API details

`spec/04-stdlib/thread.xml` fixes the `Thread.spawn(allocator, worker, args)`
shape and ownership rules at a high level, but does not define the `Thread`
handle layout, worker return/error propagation, `join` and `detach`
signatures, stack-size policy, or allocator lifetime requirements. Raw target
thread primitives may be exposed independently, but the high-level API cannot
invent these observable contracts.

### Networking API surface

`spec/04-stdlib/posix.xml` requires socket operations by name and the Linux
backend requires direct syscalls, but no normative `std.net` module or socket
address, endpoint, TCP/UDP, resolver, or event-loop API is specified. Raw
`std.os.linux`/`std.posix` wrappers can follow the platform ABI; a portable
`std.net` interface requires an added normative contract.

### Materialized Wayland declaration names

`spec/06-wayland/wayland.xml` requires typed client/server declarations
generated from protocol XML but does not define how XML names become Mlx
identifiers. XML names may start with a digit (`wl_output.transform` entry
`90`) or collide with Mlx keywords (the `wl_display.error` event). The
materializer keeps XML spelling, prefixes a leading digit with `_`, appends
`_` to keywords, uses PascalCase for enum and payload types, camelCase for
request methods and `send` + PascalCase for event methods. This is a
provisional std convention, not a normative mapping.

### Wayland public runtime API shape

`spec/06-wayland/wayland.xml` lists what `std.wayland.client` and
`std.wayland.server` provide but not their signatures, the event delivery
model, or how nullability of object and string arguments is expressed in Mlx
types. `std.wayland` provides per-object handler functions with typed
`decodeEvent`/`decodeRequest` unions, libwayland-style sticky request
failures, `?*const Proxy` parameters for nullable objects, `?[]const u8` for
nullable request strings, and `std.string.String` (null as a zero pointer)
for decoded strings. fd arguments are duplicated when sent, so callers keep
ownership, and received handles are owned by the handler.

### Function inline modifier strength

`spec/00-language/grammar.ebnf` admits `inline` and `noinline` declaration
modifiers, but no normative rule says whether `inline` is mandatory, a strong
request, or an ordinary optimization hint, nor what happens when a requested
function cannot be inlined. The canonical compiler therefore treats `inline`
as a conservative best-effort request and `noinline` as a veto; failure to
inline does not change program semantics or produce an invented diagnostic.

### Foreign C functions on x86_64 Linux

`spec/03-formats/elf64.xml` allows dynamic linking "after the static/bootstrap
path" without fixing its shape, and `spec/01-abi/foreign-abi.xml` listed only
`sysv`, `win64` and `syscall`. The compiler now accepts `extern("c")` (and
`extern` without a string) as the target's C ABI, as on the aarch64-android
branch, and writes a program that imports or exports functions as an
`ET_EXEC` loaded by the system dynamic linker with `DT_NEEDED libc.so.6` plus
each `--library NAME`. The `--library` option and always linking libc are
provisional choices; `spec/01-abi/foreign-abi.xml` records the rules.

### Vulkan in the standard library

`spec/04-stdlib/std.xml` does not mention Vulkan. `std.vulkan` follows the
Wayland precedent: the canonical registry (`vk.xml`) is materialized while
the standard library is bootstrapped, by Mlx code, into a normal module. The
naming (enum members in camelCase without their prefix, `VK_`-less constants,
wrappers taking their dispatch table first), the selected extensions and the
loader's behavior (one driver per `Driver`, no device merging or layers)
are provisional std conventions, not normative mappings.
