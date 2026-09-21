# Standard library

**Normative source:** `spec/04-stdlib/std.xml`, `spec/04-stdlib/mem.xml`,
`spec/04-stdlib/fmt.xml`, `spec/04-stdlib/io.xml`, `spec/04-stdlib/fs.xml`,
`spec/04-stdlib/posix.xml`, `spec/04-stdlib/meta.xml`,
`spec/04-stdlib/testing.xml`, `spec/04-stdlib/thread.xml`

**Implementation:** `std/bootstrap/` (Stage-1 Core), `std/src/` (Stage-1
Extensions)

**Source tests:** see the **Source tests** line under each module section
below; all cited fixtures live in [`tests/`](../../tests).

This chapter is descriptive, not normative. Where it disagrees with
`spec/04-stdlib/`, the specification wins, and where the specification names
a module or operation that has no implementation or runtime fixture yet, that
gap is called out explicitly rather than an invented signature being shown.
See [`SPEC_CONFLICTS.md`](../../SPEC_CONFLICTS.md) for the open normative
gaps this chapter references (`std.net`, the high-level `std.Thread` API).

## Two layers: Stage-1 Core vs. Stage-1 Extensions

The standard library is built in two layers, described in the root
[`README.md`](../../README.md)'s "Bootstrap chain" section, in
[`docs/BOOTSTRAP.md`](../BOOTSTRAP.md), and in `compiler/selfhost/README.md`:

```text
Zig on Linux
   -> mlx0
   -> Stage-1 compiler std foundation      (std/bootstrap/  = "Stage-1 Core")
   -> mlx1 (canonical compiler written in Mlx)
      +-> mlx2 (self-compiled compiler)
      +-> Stage-1 extensions: std.xml + std.json + broader std.posix/std.os
          +-> Stage-1 protocol extensions such as std.wayland
   -> full std + tools + brixOS
```

**Stage-1 Core** (`std/bootstrap/`) is the explicit-allocation foundation the
self-hosted compiler itself depends on. Per `std/bootstrap/README.md` and the
"bootstrap std" paragraph of `compiler/selfhost/README.md`, it "provides
growing byte and record vectors, string symbol maps, arena/fixed/page
allocators, Linux files and process arguments, and direct ELF64 executable
output" — the minimum needed to implement the Mlx1 lexer, parser, AST,
semantic pipeline, LIR, and backend. Every module in `std/bootstrap/` must
compile with `mlx0` and must not depend on Zig; it deliberately uses only
language features already covered by an end-to-end runtime test.

**Stage-1 Extensions** (`std/src/`) are the "non-blocking" layer built once
the compiler core works: `std.xml`, `std.json`, broader `std.posix`/`std.os`
coverage, and Wayland. Per `std/src/README.md`, `std.mlx` is currently a thin
public facade re-exporting the bootstrap foundation (raw streams, direct text
output, typed `std.fmt.print`/`std.io.printFmt`, files, allocation and
collections):

```mlx
pub const mem = @import("../bootstrap/mem.mlx")
pub const ascii = @import("../bootstrap/ascii.mlx")
pub const ranges = @import("../bootstrap/ranges.mlx")
pub const os = @import("./os.mlx")
pub const linux = @import("../bootstrap/os/linux.mlx")
pub const allocator = @import("../bootstrap/allocator.mlx")
...
```

(`std/src/std.mlx`)

Bootstrap format arguments use explicit constructors such as
`std.fmt.string`/`std.fmt.unsigned` rather than the normative `anytype` tuple
adapter — `std/src/README.md` notes this is a stand-in "until Mlx1 can
instantiate reflection-heavy generics itself." Tests that `@import("std")`
(e.g. `tests/163_standard_fmt_runtime.mlx`) exercise this Stage-1 Extensions
facade; tests that `@import("../std/bootstrap/root.mlx")` directly (e.g.
`tests/133_bootstrap_collections_elf_runtime.mlx`) exercise Stage-1 Core in
isolation.

`spec/04-stdlib/std.xml` additionally names the full target module surface —
`std.mem std.heap std.fmt std.io std.fs std.math std.meta std.atomic
std.Thread std.time std.process std.testing std.posix std.os std.json
std.xml std.wayland` — most of which (`std.heap`, `std.math`, `std.atomic`,
`std.json`, `std.xml`, `std.wayland`) has no implementation under `std/` yet;
those names are spec-only until a bootstrap or extensions module exists.

## Core: allocators

**Normative source:** `spec/04-stdlib/mem.xml`
**Implementation:** `std/bootstrap/allocator.mlx`, `page_allocator.mlx`,
`arena_allocator.mlx`, `fixed_buffer_allocator.mlx`, `mem.mlx`
**Source tests:** `120_bootstrap_mem_runtime.mlx`,
`131_bootstrap_allocator_runtime.mlx`, `132_page_allocator_runtime.mlx`,
`137_bootstrap_arena_allocators_runtime.mlx`

`spec/04-stdlib/mem.xml` specifies an `Allocator` with `alloc`/`resize`/`free`
operations, four allocator kinds (`PageAllocator`, `ArenaAllocator`,
`FixedBufferAllocator`, `GeneralPurposeAllocator`), endian helpers
(`readInt`, `writeInt`, `byteSwap`, `nativeToLittle`/`nativeToBig`/
`littleToNative`/`bigToNative`), and memory ops (`copy`, `copyBackwards`,
`set`, `zero`, `eql`).

The bootstrap implementation matches the allocator contract exactly. The
interface is a struct of function-pointer fields plus an opaque `usize`
context (function values are ordinary indirect-callable values in Mlx;
opaque-pointer ergonomics richer than `usize` context await Stage 1):

```mlx
pub const AllocFn = fn(context: usize, length: usize, alignment: usize) -> ?[*]u8
pub const ResizeFn = fn(context: usize, memory: [*]u8, old_length: usize, new_length: usize, alignment: usize) -> ?[*]u8
pub const FreeFn = fn(context: usize, memory: [*]u8, length: usize, alignment: usize) -> void

pub const Allocator = struct {
    context: usize
    allocFn: AllocFn
    resizeFn: ResizeFn
    freeFn: FreeFn
    ...
}
```

(`std/bootstrap/allocator.mlx`)

Three of the four spec'd allocators are implemented: `page_allocator.mlx`
(backed directly by `mmap`/`munmap` via `std.os.linux`),
`arena_allocator.mlx` (bump-allocates out of one page-allocated block, with
`reset`/`deinit`), and `fixed_buffer_allocator.mlx` (bump-allocates over a
caller-owned buffer, with `reset`). `GeneralPurposeAllocator` from
`mem.xml` has no bootstrap implementation yet. A runtime fixture exercises
fixed-buffer alignment/reset and arena alignment/reset/deinit together:

```mlx
var fixed_state = bootstrap.fixed_buffer_allocator.init(@ptrCast([*]u8, storage[0..16]), 16)
const fixed = bootstrap.fixed_buffer_allocator.interface(&fixed_state)
const first = fixed.alloc(3, 1).?
const second = fixed.alloc(8, 8).?
if @intFromPtr(second) % 8 != 0 { return 1 }
...
const pages = bootstrap.page_allocator.init()
var arena = bootstrap.arena_allocator.init(pages, 4096)
const arena_interface = bootstrap.arena_allocator.interface(&arena)
```

(`tests/137_bootstrap_arena_allocators_runtime.mlx`)

`std/bootstrap/mem.mlx` implements the raw memory ops (`copy`,
`copyBackwards`, `set`, `zero`, `eql`, all over `[*]u8`/length pairs, 8-byte
unrolled) plus little-endian read/write helpers specialized to 16/32/64 bits
(`readU16Little`, `readU32Little`, `readU64Little`,
`writeU16Little`/`writeU32Little`/`writeU64Little`). This covers the
`MemoryOps` half of `mem.xml` but is narrower than its `EndianHelpers`
surface: there is no generic `readInt`/`writeInt`, `byteSwap`, or
`nativeToLittle`/`nativeToBig`/`littleToNative`/`bigToNative` — only the
concrete little-endian read/write pairs above. `132_page_allocator_runtime.mlx`
and `131_bootstrap_allocator_runtime.mlx` exercise the page allocator and a
custom `fakeAlloc` plugged into the `Allocator` interface directly;
`120_bootstrap_mem_runtime.mlx` exercises `copy`/`set`/`eql` over fixed byte
arrays.

## Collections

**Normative source:** none specific (collections are not separately named in
`spec/04-stdlib/`; they underlie the modules `std.xml` lists)
**Implementation:** `std/bootstrap/array_list.mlx`, `vector.mlx`,
`hash_map.mlx`, `string.mlx`
**Source tests:** `133_bootstrap_collections_elf_runtime.mlx`,
`138_bootstrap_core_runtime.mlx`, `143_bootstrap_vector_runtime.mlx`

Per `std/bootstrap/README.md`, these are "deliberately concrete bootstrap
specializations" the self-hosted compiler needs immediately — a growing byte
list, a type-erased growing record vector, and a `String -> usize` hash map —
rather than a generic `ArrayList(T)`, which awaits Mlx1's own comptime
pipeline.

`ArrayList` is a growing `u8` buffer (source/output buffers) with both a
pointer-based in-place API (`appendAt`, `initAt`) and a value-returning API
(`append`, `resize`, `appendSlice`, `pop`, `clear`):

```mlx
pub const ArrayList = struct {
    items: [*]u8
    length: usize
    capacity: usize
    allocatorContext: usize
    allocFn: AllocFn
    resizeFn: ResizeFn
    freeFn: FreeFn

    pub fn init(allocator: Allocator, initial_capacity: usize) -> Self { ... }
    pub fn append(list: Self, value: u8) -> Self { ... }
    pub inline fn asSlice(list: Self) -> []const u8 { ... }
}
```

(`std/bootstrap/array_list.mlx`)

`Vector` is the type-erased analogue used for Mlx1 token/AST records: it
tracks `elementSize`/`elementAlignment` and copies raw bytes in/out via
`[*]const u8`/`[*]u8` (`std/bootstrap/vector.mlx`). `143_bootstrap_vector_runtime.mlx`
instantiates it over a `Record` struct with `kind`/`flags` fields to show a
non-`u8` element type working through the byte-copying interface.

`HashMap` is open-addressing (linear probing) over parallel arrays of
hashes/key-pointers/key-lengths/values/occupied flags, doubling (`grow`) once
load exceeds 50%:

```mlx
pub fn put(map: HashMap, key: String, value: usize) -> HashMap {
    var result = map
    if result.length * 2 >= result.capacity { result = grow(result) }
    const hash_value = strings.hash(key)
    var index = hash_value % result.capacity
    while result.occupied[index] != 0 { ... }
    ...
}
```

(`std/bootstrap/hash_map.mlx`)

`get` returns a native multiple return `(bool, usize)` for found/value, e.g.
`const lookup = bootstrap.hash_map.get(map, right); if !lookup.0 { return 8 }`
(`tests/133_bootstrap_collections_elf_runtime.mlx`), which also round-trips a
`put`/`get` pair, checks the hash bucket layout directly, and chains into
`bootstrap.fmt.unsignedDecimal` and `bootstrap.elf.writeExecutableHeaders` to
show allocator-backed collections feeding the compiler's own ELF emission.
`138_bootstrap_core_runtime.mlx` is a broader smoke test over the same
`ArrayList`/allocator combination.

`string.mlx` provides a borrowed `String { pointer: [*]const u8, length:
usize }` with `eql`, `startsWith`, `endsWith`, `slice`, `compare`
(lexicographic), and `hash` (FNV-1a, seen above) — the key type `HashMap` is
specialized over.

## Filesystem & process

**Normative source:** `spec/04-stdlib/fs.xml`
**Implementation:** `std/bootstrap/fs.mlx`, `process.mlx`,
`os/linux/process.mlx`
**Source tests:** `134_bootstrap_fs_runtime.mlx`,
`142_bootstrap_process_arguments_runtime.mlx`,
`204_process_environment_runtime.mlx`, `205_bootstrap_fs_status_runtime.mlx`,
`139_bootstrap_elf_file_runtime.mlx`

`fs.xml` names a portable filesystem API (`open create read write seek stat
iterateDirectory makeDir delete rename`) "over std.os/std.posix backends,"
with paths treated as byte strings at the OS boundary and directory
iteration/owned-path helpers using explicit allocators. `std/bootstrap/fs.mlx`
implements the Linux-syscall-backed core of this directly (not yet the
portable `std.fs` facade split by backend): `open`/`openAt`, `close`,
`read`/`pread`/`readToEnd`/`readToEndAlloc`, `write`/`pwrite`/`writeAll`/
`writeAllBytes`, `seek`, `status`/`statusAt`/`fileStatus`/`extendedStatus`
(`stat`/`statx`), `getWorkingDirectory`/`changeDirectory`, `delete`/
`deleteAt`, `rename`/`renameWithFlags`, `makeDir`/`removeDir`, `chmod`,
`changeOwner`, `hardLink`/`symbolicLink`/`readLink`, `makePipe`/`splice`, and
`sync*`. `readDirectory` (`getdents64`) is present as the raw primitive
`iterateDirectory` from the spec would be built on, but there is no typed
directory-iterator wrapper yet.

A round-trip open/write/seek/read/close/delete over `/tmp`:

```mlx
const file = bootstrap.fs.open(path_pointer, bootstrap.fs.READ_WRITE | bootstrap.fs.CREATE | bootstrap.fs.TRUNCATE, 384)
if file.descriptor < 0 { return 1 }
if bootstrap.fs.write(file, @ptrCast([*]const u8, source[0..1]), 1) != 1 { return 2 }
if bootstrap.fs.seek(file, 0, 0) != 0 { return 3 }
...
if !bootstrap.fs.close(file) { return 7 }
if !bootstrap.fs.delete(path_pointer) { return 8 }
```

(`tests/134_bootstrap_fs_runtime.mlx`)

`205_bootstrap_fs_status_runtime.mlx` checks `std.fs.Stat`'s ABI size
(`@sizeOf(std.fs.Stat) != 144`) and calls `stat` on real paths through the
Stage-1 Extensions `std` facade rather than bootstrap directly.

Process arguments and environment are exposed as borrowed bootstrap
`String`s reconstructed from the raw `argv`/`envp` layout the Linux ABI hands
`main`:

```mlx
pub fn main(arguments: [][*]const u8) -> u8 {
    if process.count(arguments) != 3 { return 1 }
    if !string.eql(process.argument(arguments, 1), string.fromSlice("first")) { return 3 }
    ...
}
```

(`tests/142_bootstrap_process_arguments_runtime.mlx`)

`std.process.environment(arguments, name)` walks the `envp` block that
follows `argv` in memory (`environmentEntries` computes its address from
`argv`'s own count) and returns an `?String`:

```mlx
const path = std.process.environment(arguments, "PATH")
if path == null || path.?.length == 0 { return 1 }
if std.process.environment(arguments, "MLX_ENVIRONMENT_NAME_THAT_DOES_NOT_EXIST") != null { return 2 }
```

(`tests/204_process_environment_runtime.mlx`)

`std/bootstrap/os/linux/process.mlx` adds the lower-level process primitives:
`execve`, signal disposition (`ignoreSignal`/`defaultSignal` via
`rt_sigaction`), `blockSignals`/`unblockSignals`, and a liveness check
(`isAlive` via `kill(pid, 0)`).

ELF64 executable emission — the "direct ELF64 executable output" the
`compiler/selfhost/README.md` bootstrap paragraph names as part of Stage-1
Core — lives in `std/bootstrap/elf.mlx`: `writeExecutableHeaders` writes a
complete ELF header plus one `PT_LOAD` program header into a caller-owned
buffer and returns the header length. `133_bootstrap_collections_elf_runtime.mlx`
checks its output byte-for-byte (`scratch[0] != 127` — the ELF magic byte —
and `scratch[18] != 62`, the `e_machine` field for x86_64);
`139_bootstrap_elf_file_runtime.mlx` writes a full in-memory image including
machine code and exercises the end-to-end on-disk executable path.

## I/O

**Normative source:** `spec/04-stdlib/io.xml`
**Implementation:** `std/bootstrap/io.mlx`, `std/src/io.mlx`
**Source tests:** `159_bootstrap_io_runtime.mlx`,
`160_bootstrap_io_read_runtime.mlx`, `161_standard_io_runtime.mlx`

`io.xml` specifies `Reader`/`Writer` as structural (comptime duck-typed)
interfaces — `read(destination []u8) !usize` and `write(source []const u8)
!usize` / `writeAll(source) !void` — plus target-provided `stdout`/`stderr`/
`stdin` stream objects requiring no libc, and a `print` that delegates to
`std.fmt`.

The bootstrap layer implements this over `std.os.linux` file descriptors
rather than yet exposing the generic structural `Reader`/`Writer` contract
itself:

```mlx
pub fn stdin() -> File { return File.{ .descriptor = 0 } }
pub fn stdout() -> File { return File.{ .descriptor = 1 } }
pub fn stderr() -> File { return File.{ .descriptor = 2 } }

pub fn read(file: File, destination: []u8) -> isize { ... }
pub fn write(file: File, source: []const u8) -> isize { ... }
pub fn writeAll(file: File, source: []const u8) -> bool { return fs.writeAll(file, source) }
```

(`std/bootstrap/io.mlx`)

It also provides allocation-free `writeUnsigned`/`writeHex` (via
`fmt.unsignedDecimal`/`fmt.unsignedHex` into a stack buffer),
`writeLine`/`print`/`println`/`eprint`/`eprintln` convenience wrappers, and
the `formatArguments`/`format`/`printFmt`/`eprintFmt` entry points into
`std/bootstrap/fmt.mlx`. `std/src/io.mlx` re-exports this surface one-to-one
as the Stage-1 Extensions `std.io` facade (see excerpt in "Two layers"
above).

`159_bootstrap_io_runtime.mlx` writes to real stdout
(`bootstrap.io.write(output, "mlx") != 3`); `160_bootstrap_io_read_runtime.mlx`
opens a real `/tmp` file and reads it back; `161_standard_io_runtime.mlx`
exercises the same surface through the `std` facade:

```mlx
const io = @import("std.io")

pub fn main() -> u8 {
    if !io.print("standard ") { return 1 }
    if !io.writeUnsigned(io.stdout(), 13) { return 2 }
    if !io.println("") { return 3 }
    return 13
}
```

(`tests/161_standard_io_runtime.mlx`)

## fmt

**Normative source:** `spec/04-stdlib/fmt.xml`
**Implementation:** `std/bootstrap/fmt.mlx`, `fmt/argument.mlx`,
`fmt/integer.mlx`, `fmt/write.mlx`, `std/src/fmt.mlx`
**Source tests:** `162_bootstrap_fmt_runtime.mlx`,
`163_standard_fmt_runtime.mlx`, `164_standard_fmt_import_runtime.mlx`,
`165_bootstrap_fmt_file_runtime.mlx`, `166_bootstrap_fmt_error_runtime.mlx`,
`169_standard_fmt_generic_args_runtime.mlx`,
`170_standard_fmt_any_struct_runtime.mlx`,
`172_standard_fmt_many_args_runtime.mlx`,
`173_standard_fmt_nested_any_runtime.mlx`

`fmt.xml` fixes a `print(writer, comptime format, args) !void` signature,
specifiers (`{}` default, `{any}`, `{d}` decimal, `{x}`/`{X}` hex, `{b}`
binary, `{o}` octal, `{c}` char, `{s}` string, `{f}` float, `{e}` scientific,
`{p}` pointer), `{{`/`}}` escapes, width/zero-pad/precision syntax
(`{d:8}`, `{d:08}`, `{f:.2}`, `{f:8.2}`), comptime-validated arity/type
checking, `{any}` recursion to a maximum depth of 32 (ellipsis beyond that),
and a custom-formatting hook detected via `@hasDecl(T, "format")`.

The bootstrap writer (`std/bootstrap/fmt/write.mlx`) implements the integer
specifiers (`d`/`x`/`X`/`b`/`o`), `c`, `s`, `p`, `{any}` (including recursive
struct/tuple/slice/array formatting via `writeAggregateAny`/`writeSequenceAny`
with a hard `depth >= 32` `Error.RecursionLimit` check matching the spec's
maximum depth), width and zero-padding. `{f}`/`{e}` (float/scientific) are
recognized as specifier characters but return
`Error.UnsupportedSpecifier` — float formatting is not implemented. There is
no `@hasDecl(T, "format")` custom-formatting hook yet; `{any}` always uses
the built-in aggregate representation.

Two argument-passing surfaces coexist: an explicit `Argument` (tagged
two-word struct, `printArguments`) used directly in bootstrap code before
Mlx1's reflection is ready to specialize `anytype`,

```mlx
const values = [8]bootstrap.fmt.Argument{
    bootstrap.fmt.string("mlx"),
    bootstrap.fmt.unsigned(42),
    ...
}
```

(`tests/162_bootstrap_fmt_runtime.mlx`, header)

and the spec's `anytype` tuple form (`print(file, format, values: anytype)`),
which is what `std.io.printFmt`/`std.fmt.print` expose and what most `std.*`
runtime tests use:

```mlx
try std.io.printFmt("{s} {o:4} {p}\n", .{ "format", 15, @ptrFromInt(*u8, 255) })
```

(`tests/163_standard_fmt_runtime.mlx`)

`{any}` over a user struct, showing field names and declaration order
retained per spec:

```mlx
const Point = struct {
    x: u8
    y: i16
}

pub fn main() -> !void {
    const point = Point.{ .x = 7, .y = -2 }
    try std.io.printFmt("point={any}\n", .{point})
}
```

(`tests/170_standard_fmt_any_struct_runtime.mlx`)

`164_standard_fmt_import_runtime.mlx` imports `std.fmt`/`std.io` directly
(rather than the aggregate `std` facade) and formats a negative signed value
with zero-padding (`{d:04}`); `165_bootstrap_fmt_file_runtime.mlx` formats
into a real file rather than stdout; `166_bootstrap_fmt_error_runtime.mlx`
exercises the `Error` set (`InvalidFormat`, `MissingArgument`,
`ExtraArgument`, `TypeMismatch`, `UnsupportedSpecifier`, `WriteFailed`,
`RecursionLimit`) on malformed format strings; `169_standard_fmt_generic_args_runtime.mlx`
and `172_standard_fmt_many_args_runtime.mlx` cover the generic-tuple call
convention with named and multiple (8) positional arguments respectively;
`173_standard_fmt_nested_any_runtime.mlx` nests `{any}` structs inside each
other.

## POSIX / Linux (`std.os.linux`, `std.posix`)

**Normative source:** `spec/04-stdlib/posix.xml`
**Implementation:** `std/bootstrap/os/linux.mlx` and
`std/bootstrap/os/linux/{thread,socket,event,system,process}.mlx`,
`std/src/os.mlx`
**Source tests:** `118_bootstrap_std_runtime.mlx`,
`129_bootstrap_linux_runtime.mlx`, `180_linux_futex_runtime.mlx`,
`181_linux_socket_epoll_runtime.mlx`, `185_std_os_linux_runtime.mlx`,
`187_linux_io_uring_runtime.mlx`, `203_bootstrap_pipe_splice_runtime.mlx`

`posix.xml` specifies a "portable POSIX-compatible API surface implemented in
Mlx over target-native backends" (native Mlx programs need no libc), naming
`open close read write pread pwrite lseek fstat mmap munmap mprotect socket
bind listen accept connect send recv sendmsg recvmsg poll clock_gettime
nanosleep pipe dup fcntl ioctl memfd_create ftruncate shm_open getenv
process/thread primitives as supported`, a `WaylandMinimum` list (Unix
sockets, fd-passing, shared memory, pollable descriptors, clocks, page-aligned
mmap, close-on-exec), and per-backend notes for Linux (direct syscalls), BSD
(kqueue/poll), and Brix (native handles, POSIX facade optional).

What exists today is the **Linux raw syscall gateway** (Stage-1 Core), not
yet a portable `std.posix` facade abstracting over multiple backends.
`std/bootstrap/os/linux.mlx` defines the `syscall0`..`syscall6` extern
gateway (`extern("syscall")`, arguments loaded per the x86_64 Linux ABI) plus
`SYS_*` numeric constants for the file/process syscalls it wraps in `fs.mlx`:

```mlx
pub extern("syscall") fn syscall3(number: usize, a1: usize, a2: usize, a3: usize) -> isize {}
...
pub const SYS_read: usize = 0
pub const SYS_write: usize = 1
pub const SYS_open: usize = 2
```

(`std/bootstrap/os/linux.mlx`)

It re-exports five sub-namespaces, each a thin syscall wrapper module:

- `os/linux/thread.mlx` — `clone`/`clone3`/`futex` numbers and `CLONE_*`
  flags, plus `futex`/`futexWait`/`futexWake`, `getTid`, `yield`, `exit`,
  `setRobustList`, `registerRestartableSequence`. It explicitly does **not**
  select a public thread handle: "This module intentionally does not select
  a public std.Thread handle, stack policy, or error-propagation model" — see
  the Thread API gap below.
- `os/linux/socket.mlx` — `AF_UNIX`/`AF_INET`/`AF_INET6`,
  `SOCK_STREAM`/`SOCK_DGRAM`, and syscall wrappers `open` (socket),
  `bind`/`listen`/`connect`/`accept`/`accept4`, `send`/`receive`/`sendTo`/
  `receiveFrom`/`sendMessage`/`receiveMessage`, `setOption`/`getOption`,
  `setNonBlocking` (via `fcntl`).
- `os/linux/event.mlx` — raw `poll`, `epollCreate`/`epollControl`/
  `epollWait`, and `io_uring` setup/enter/register wrappers, with kernel ABI
  event-buffer layouts left to a future portable event-loop layer.
- `os/linux/system.mlx` — `getSystemInfo` (`uname`, filling a `UtsName` of
  six 65-byte fields) and `availableProcessorCount` (via
  `sched_getaffinity`'s returned CPU mask, popcounted by hand).
- `os/linux/process.mlx` — `execve`, `ignoreSignal`/`defaultSignal`
  (`rt_sigaction`), `blockSignals`/`unblockSignals`, `isAlive` (`kill(pid,
  0)`).

A socketpair + non-blocking + epoll round-trip:

```mlx
if socket.socketPair(socket.AF_UNIX, socket.SOCK_STREAM | socket.SOCK_CLOEXEC, 0, &descriptors) != 0 { return 1 }
if socket.setNonBlocking(first) != 0 { return 2 }
const epoll = event.epollCreate(event.EPOLL_CLOEXEC)
if event.epollControl(epoll, event.EPOLL_CTL_ADD, first, @intFromPtr(&registration)) != 0 { return 5 }
```

(`tests/181_linux_socket_epoll_runtime.mlx`)

A futex wait/wake pair (`futexWait` on a mismatched value returns `-11`,
`EAGAIN`, exercising the raw syscall's error path directly):

```mlx
var value: u32 = 7
const mismatch = thread.futexWait(&value, 8, 0, true)
if mismatch != -11 { return 3 }
if thread.futexWake(&value, 1, true) != 0 { return 4 }
```

(`tests/180_linux_futex_runtime.mlx`)

`118_bootstrap_std_runtime.mlx` and `129_bootstrap_linux_runtime.mlx` are
smoke tests over `bootstrap.ranges`/`bootstrap.ascii` and a raw `getpid`
syscall respectively; `185_std_os_linux_runtime.mlx` calls
`std.os.linux.thread.getTid`/`yield` through the Stage-1 Extensions `std`
facade (`std/src/os.mlx` re-exports `linux = @import("../bootstrap/os/linux.mlx")`
directly — there is no separate `std.posix` module file yet, so
`std.os.linux.*` is the only POSIX-shaped surface actually reachable from
`std`); `187_linux_io_uring_runtime.mlx` checks that an invalid
`io_uring_setup` call fails without needing a real ring;
`203_bootstrap_pipe_splice_runtime.mlx` exercises `std.fs`'s `makePipe`/
`splice`/`setPipeSize` wrappers.

## Threads (`std.Thread`) — not yet normative

**Normative source:** `spec/04-stdlib/thread.xml`; flagged as an open gap in
`SPEC_CONFLICTS.md` ("Standard thread API details")

`thread.xml` sketches a high-level lifecycle API — `var thread = try
std.Thread.spawn(allocator, worker, .{arg})`, `join` (waits for completion,
the handle owns target thread resources until joined/detached), `detach`
(transfers cleanup to the runtime/OS and invalidates `join`), explicit
allocation for stacks/control blocks, and ordinary error-union
spawn/join errors — but per `SPEC_CONFLICTS.md`:

> `spec/04-stdlib/thread.xml` fixes the `Thread.spawn(allocator, worker,
> args)` shape and ownership rules at a high level, but does not define the
> `Thread` handle layout, worker return/error propagation, `join` and
> `detach` signatures, stack-size policy, or allocator lifetime
> requirements. Raw target thread primitives may be exposed independently,
> but the high-level API cannot invent these observable contracts.

Consistent with that, there is no `std.Thread` type anywhere under `std/`.
What does exist and is exercised at runtime is the raw Linux thread/futex
syscall layer in `std/bootstrap/os/linux/thread.mlx` (`clone`/`clone3`
constants, `futex`/`futexWait`/`futexWake`, `getTid`, `yield`, `exit`), whose
own header comment says as much: "This module intentionally does not select
a public std.Thread handle, stack policy, or error-propagation model." See
`tests/180_linux_futex_runtime.mlx` and `tests/185_std_os_linux_runtime.mlx`
above for what's actually exercised — `getTid`/`yield`/`futexWait`/
`futexWake` through the raw syscall gateway, not a handle-based spawn/join
API. No fixture calls `clone`/`clone3` to actually spawn an OS thread.

## Networking (`std.net`) — not yet normative

**Normative source:** none (see gap below)

`spec/04-stdlib/posix.xml` requires the raw syscalls a network stack would
sit on (`socket bind listen accept connect send recv sendmsg recvmsg poll`),
and those are implemented and tested — see "POSIX / Linux" above,
particularly `os/linux/socket.mlx` and
`tests/181_linux_socket_epoll_runtime.mlx`. But per `SPEC_CONFLICTS.md`
("Networking API surface"):

> `spec/04-stdlib/posix.xml` requires socket operations by name and the
> Linux backend requires direct syscalls, but no normative `std.net` module
> or socket address, endpoint, TCP/UDP, resolver, or event-loop API is
> specified. Raw `std.os.linux`/`std.posix` wrappers can follow the platform
> ABI; a portable `std.net` interface requires an added normative contract.

There is accordingly no `std.net` module, and this chapter does not describe
one. Programs needing sockets today go directly through
`std.os.linux.socket`/`std.os.linux.event` (Unix-domain sockets, raw
`AF_INET`/`AF_INET6` constants, `epoll`, and a raw `io_uring` setup/enter/
register gateway), as `tests/181_linux_socket_epoll_runtime.mlx` and
`tests/187_linux_io_uring_runtime.mlx` do.

## meta

**Normative source:** `spec/04-stdlib/meta.xml`

`meta.xml` describes `std.meta` as a thin convenience layer — "Convenience
wrappers over language comptime reflection; no runtime reflection registry
is required" — naming `fields`, `declarations`, `enumValues`, `tagType`,
`hasMethod`, and trait-style comptime assertions as its expected helpers.
There is no `std/bootstrap/meta.mlx` or `std/src/meta.mlx` yet, and no
runtime fixture under `tests/` exercises a `std.meta` module by name; the
underlying comptime reflection builtins it would wrap (`@fieldCount`,
`@fieldName`, `@field`, `@isStruct`, `@isTuple`, `@tagOf`, etc.) are already
used directly inside `std/bootstrap/fmt/write.mlx`'s `{any}` formatter (see
the fmt section above), which is the closest thing to `std.meta` coverage
that currently exists at runtime.

## testing (`std.testing` and the `test` declaration)

**Normative source:** `spec/04-stdlib/testing.xml`;
`spec/00-language/grammar.ebnf` (`test_decl`)

`testing.xml` specifies `std.testing.expect`, `expectEqual`, `expectError`,
`expectPanic`, and allocator leak checks "where supported," and separately
notes the compiler harness supports expected-diagnostic-code tests
independently of runtime `std.testing`. The grammar admits a `test`
declaration form at the top level:

```ebnf
declaration      = const_decl | var_decl | fn_decl | test_decl ;
test_decl        = "test", STRING, block ;
```

(`spec/00-language/grammar.ebnf`)

Neither surface has a runtime fixture: no file under `tests/` contains a
`test "..."` declaration, and there is no `std/bootstrap/testing.mlx` or
`std/src/testing.mlx` implementing `expect`/`expectEqual`/`expectError`/
`expectPanic`. The conformance suite's actual mechanism is the ordinary
`fn main() -> u8` return-code convention shown throughout this chapter (and
`tests/run_error.sh` for compile-error fixtures, per
[`docs/guide/README.md`](../guide/README.md)), not the `test` declaration or
a `std.testing` module. This chapter does not describe an `expect*` API
surface beyond the four names `testing.xml` gives, since none of them is
grounded in source or a runtime fixture.
