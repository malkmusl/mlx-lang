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
std.xml std.wayland` — several of which (`std.heap`, `std.math`,
`std.atomic`) have no implementation under `std/` yet; those names are
spec-only until a bootstrap or extensions module exists.

A module may differ by platform: `std.X` is `std/src/X.<platform>.mlx`
when the standard library has one for the target's platform (`linux` for
`x86_64-linux` and `aarch64-linux`, `android` for `aarch64-android`), else
`std/src/X.mlx`. `std.platform` (`platform.linux.mlx`,
`platform.android.mlx`) names the platform, and `std.ui.host` is how a
std.ui app runs on each (see [UI](ui.md)), so a program's own source is the
same for every target (`tests/290_platform_module_runtime.mlx`).

`std.fs` works on folders with everything in them too, as a file manager
does, by the kernel's calls alone: `copyTree` (files with their mode and
modification time, symbolic links as links), `moveTree` (a rename on the
same file system, else copied and then removed), `removeTree`, `exists`
and `freeName` (a name nothing in a folder has: "photo 2.png"). Nothing is
overwritten, and nothing is copied into itself
(`tests/291_fs_tree_runtime.mlx`).

`std.xml` (`std/src/xml.mlx`) and `std.wayland` (`std/src/wayland.mlx` and
`std/src/wayland/`) are implemented as Stage-1 extensions. They are imported
by their own names (`@import("std.xml")`, `@import("std.wayland")`) and are
deliberately not re-exported from `std.mlx`, so programs that only use the
compiler-core std do not load the generated protocol modules. See
[Wayland](wayland.md) for the pipeline, runtime and tests
(`tests/240_std_xml_tokenizer_runtime.mlx` covers `std.xml`).

`std.xml.sax` (`std/src/xml/sax.mlx`, test 374) is the streaming parser
libxml2's C API is made of; `std.xml.text` beside it holds, for programs
writing XML, text escaped for character data, an attribute's value,
ASCII-only output and HTML (`writeEscaped(sink, text, mode)`, the escapes
of xmlEncodeEntitiesReentrant and xmlEncodeSpecialChars) and names
checked as Name, NCName, QName or Nmtoken (`validName`, `isName`,
`isNCName`, `isQName`, `isNmtoken`, `nameEnd`; test 383).

`std.json` (`std/src/json.mlx`), `std.vulkan` (`std/src/vulkan.mlx`,
materialized from the Vulkan registry, with `std.vulkan.loader` and
`std.vulkan.icd` in `std/src/vulkan/`) and `std.spirv.core`,
`std.spirv.builder`, `std.spirv.module` and `std.spirv.compute`
(`std/src/spirv/`) and `std.gpu` with `std.gpu.shaders`, `std.gpu.text`
and `std.gpu.swapchain` (`std/src/gpu.mlx`, `std/src/gpu/`) are Stage-1
extensions of the same kind; see [Vulkan, SPIR-V and JSON](vulkan.md)
(`tests/248_json_runtime.mlx` covers `std.json`).

`std.truetype` (`std/src/truetype.mlx` and `std/src/truetype/`, see [Fonts](truetype.md))
and `std.ui` (`std/src/ui.mlx` and `std/src/ui/`: layout, a canvas on the
CPU or the GPU, and widgets for any platform, see
[User interfaces](ui.md)) are extensions for drawing text and building
pixel user interfaces.

`std.terminal` (`std/src/terminal.mlx`) is a terminal emulator's screen for
any front end: `feed()` takes the bytes a program writes (UTF-8 with wide
characters, the C0 controls, ECMA-48 cursor movement, erasing, inserting
and deleting, scrolling regions, SGR with 16, 256 and direct colours and
the attributes, the DEC modes for application cursor keys, autowrap,
origin, the cursor, the alternate screen, bracketed paste and the mouse,
the title, the DSR and DA reports), the cells come out through
`visibleCell()` (with the history behind `scrollView()`), the replies the
program is owed through `takeResponses()`. `std.terminal.keys` encodes a
key (Linux key code, modifiers, the text it types and the text without
Control and Alt) as xterm does; `std.terminal.pty` runs a program on a
pseudo-terminal. `std.ui.terminal` draws a screen on a `std.ui` canvas with
the monospaced font of `std.ui.text` and handles its keys and wheel, for
`projects/desktop/terminal` and any app with a terminal pane. A
`std.ui.host` app watches a descriptor (the pty) with `watchDescriptor()`
and gets `EVENT_WATCH`; `plainTyped()` is the key's text without Control
and Alt. Without a compositor the same screen and drawing run on the
Linux framebuffer (`projects/console`, mlx-console): `std.ui.text`'s
`loadFontFile()` takes a font by path, and `std.ui.wayland.keyboard`
reads keys from evdev with a keymap of the program's own through
`useKeymap()` and `press()`. The emulation is checked by
`tests/301_terminal_core_runtime.mlx`, the drawing and the pty by
`tests/support/terminal_render.mlx` and `tests/support/terminal_pty.mlx`
(`tools/check_terminal.sh`), the console's picture against
`tests/support/console_expected.mlx` (`tools/check_console.sh`).

`std.crash` (`std/src/crash.mlx`) is a program's crash report: `install()`
sets a signal handler on a stack of its own that names the signal, the
function and its callers (from the symbol table the compiler writes, see
[formats](formats.md); a frame in a module loaded from a memfd, as the
compositor's shell module is, is named through `/proc/self/fd`) and keeps
the crash in
`$XDG_STATE_HOME/mlx/crashes.log`; `fail(reason)` ends the program the same
way for a failure it cannot go on after. The report goes to stderr. Every
desktop program, every coreutil and mlxlibc (for the programs its loader
starts) install it. `std.wayland.crash.watch(display)`
(`std/src/wayland/crash.mlx`) makes a Wayland client's failed connection
such a failure (`tools/check_crash_report.sh` holds every client to it).
The handler's own values come from the compiler's value arena too: when a
program crashed because that arena is full, it takes room of its own and
says so ("the values' arena is full").

`std.diagnostics` (`std/src/diagnostics.mlx`) is the one closed interface
the runtime's diagnostics write through, for the processor and the GPU
(`Source.processor`, `Source.gpu`): `keepCrash(source, record)` appends a
crash to `crashes.log` as a `crash` or `gpu-crash` line, `openProfile`,
`profileLine` and `closeProfile` write a profile whose header names its
source (to a `.part` file renamed into place), and `runtimeDirectory`,
`programName` and `environmentValue` find where and for whom. `std.crash`,
`mlx-profile` and `std.gpu.diagnostics` use it. It also keeps the GPU's
memory for the memory snapshots: `gpuDevice`, `gpuHeap` and `gpuType`
note the device a context opened (its name, how many allocations it
allows, its heaps and memory types), `gpuAllocated` and `gpuFreed` each
block of device memory (the bytes asked, the bytes the device took, the
memory type, a kind: a buffer, exported, imported, host memory; and the
functions that asked, along the frame pointers). `std.gpu` writes them,
`std.leaks` reads them under `gpuLock`, in a mapping of its own made when
the first context opens. With `MLX_FRAMES` set (and not `0`) it keeps a
program's frames: `frameBegin` and `frameEnd` around each frame drawn
(the panels of `std.ui.wayland.panel` call them) note the processor's
time, `frameGpu` adds the GPU's (std.gpu, below), and the last 1024 are
written about once a second (and at the tenth frame) to
`$XDG_RUNTIME_DIR/mlx/frames/PROGRAM-PID.frames`: a header with the
frames drawn and the µs from the process's start to the end of its first
frame, then a line per frame (serial, start, processor and GPU µs).
`framesFlush` writes them now. `mlx-codemap frames` and *Hot* in MLX
Observatory read them.

`std.gpu.diagnostics` (`std/src/gpu/diagnostics.mlx`, used by `std.gpu`
only) watches the GPU work a program records. After each pass a barrier
and a fill write a breadcrumb (the frame and the passes done) into a
buffer the host can read, and the functions that recorded the pass are
kept. A frame not done in 5 s, or a lost device, is a GPU crash: said
like a processor's crash, in the pass that failed with the functions that
recorded it, and kept as a `gpu-crash` line. With `MLX_GPU_PROFILE` (`1`,
or a directory) timestamp queries time each pass and the time goes on
those functions, written as a profile with the source `gpu`
(`tools/check_gpu_diagnostics.sh`, on lavapipe, with
`tests/support/gpu_diagnostics_program.mlx`: a pass that faults, timed
passes, small and large buffers, slow frames). With `MLX_FRAMES` each
frame's command buffer gets a timestamp at its start and its end, and the
GPU's time goes on the frame the program was drawing when it began.

`std.timezone` (`std/src/timezone.mlx`) gives local time: `load` reads the
zone from `$TZ` (a zone name, a path or a POSIX rule) or `/etc/localtime`
(TZif, RFC 8536, with the POSIX rule past its last transition),
`loadSetting` takes such a setting itself, `offsetAt` gives the offset
from UTC at a moment and `abbreviationAt` the zone's name for it
("CEST"; `tests/358_timezone_names_runtime.mlx`), `civil` and
`daysFromCivil` convert between seconds and the proleptic Gregorian
calendar (`tests/279_timezone_runtime.mlx`).
`std.process.environmentFrom(entries, name)` looks a name up in any
NUL-terminated environment list, which `load` uses. `std.timezone.database`
is the whole IANA database built in, by zone ID, reckoned with the same
civil calendar; `std.unicode` is Unicode and CLDR's locale data
(collation, normalization, IDNA, calendars, formatting): see
[Unicode and locales](unicode.md).

`std.xkb` (`std/src/xkb.mlx`, with `std/src/xkb/`) is XKB without
libxkbcommon, behaving as libxkbcommon 1.6 does. `std.xkb.parse` reads
keyboard maps in the XKB text format (what a Wayland compositor sends in
`wl_keyboard.keymap`): key names, types, the compatibility map and the
symbols, with libxkbcommon's derived parts (automatic key types, actions
and virtual modifiers from the compatibility map, which keys repeat).
`std.xkb.compile` makes keymaps from the XKB data (`/usr/share/X11/xkb`,
or `$XKB_CONFIG_ROOT`, `~/.xkb`, `~/.config/xkb`) as
`xkb_keymap_new_from_names`: a model, layouts, variants and options go
through the rules (`std.xkb.rules`) to include strings, whose files are
read section by section and merged (`std.xkb.merge`: include, augment,
override, replace, layouts in other groups such as `ru:2`, defaults such
as `key.type`); `fromEnvironment` takes the names from `XKB_DEFAULT_*`.
`std.xkb.write` gives a keymap's text for clients (every key with its
types, actions, virtual modifiers and repeat, so readers need not work
them out). `std.xkb.state` keeps the modifiers and group: from a
compositor's serialization (`updateMask`), or following the keys
(`updateKey`: SetMods, LatchMods, LockMods and the group actions, as
`xkb_state_update_key`; `depressedMods`, `latchedMods`, `lockedMods`,
`effectiveGroup` for `wl_keyboard.modifiers`); and gives a key's group,
level, keysyms and text (`keyUtf8`, with the Caps Lock and Control rules
of `xkb_state_key_get_utf8`). `std.xkb.keysym` names keysyms, gives their
characters and cases; its table (`std/src/xkb/keysyms.mlx`) is generated
by `tools/materialize_xkb_keysyms.py` from the X11 keysym headers in
`std/registry/xkb/` (`--check` says whether it is current).
`tests/280_xkb_keymap_runtime.mlx` types with a German and Russian keymap
(`tests/support/xkb/de-ru.xkb`); `tests/283_xkb_compile_runtime.mlx`
compiles keymaps from a small data tree (`tests/support/xkb/data`),
follows keys through them and reads back what `std.xkb.write` gives.

`std.x11` (`std/src/x11.mlx`) speaks the X11 wire protocol on a connected
stream socket, without libc or libxcb: the connection setup, the requests
a window manager sends (InternAtom, CreateWindow, ChangeWindowAttributes,
MapWindow, ConfigureWindow, ChangeProperty, GetProperty, SendEvent,
SetInputFocus, SetSelectionOwner, KillClient, QueryExtension and
Composite's RedirectSubwindows) and the messages it reads back. It never
waits for the server: `fill` reads what arrived, `take` hands out one
complete message (the setup's answer, a reply, an error or an event) at a
time, and requests with a reply return their sequence number. The
compositor's X window manager (see [Wayland](wayland.md)) uses it.

`std.dbus` (`std/src/dbus.mlx`) is the D-Bus wire format without libc or
libdbus: marshalling (`Reader`, `Writer`, either byte order, D-Bus
alignment), message headers (`parseHeader`, `writeMessage`, `newHeader`),
a stream connection with Unix descriptor passing (std.wayland's
transport), and a client: `connectSession` (or `connectClient` with a
socket path) authenticates with EXTERNAL, negotiates descriptor passing
and says Hello; `send`, `call` and `reply` write messages, `next` waits
for the next one (its body at `bodyOf`, its descriptors at
`descriptorOf`) and `waitReply` for the reply to a call; after
`keepSignals` the signals that come while `waitReply` waits are kept, with
their descriptors, for `next` to return first (a logind client must not
lose a PauseDevice that arrived during a call). `addressPath` takes the
socket path out of a bus address list and `systemPath` is the system
bus's (DBUS_SYSTEM_BUS_ADDRESS, else `MLX_SYSTEM_BUS` or `SYSTEM_BUS`).
MLXIPC's bus (`projects/desktop/ipc`, mlx-ipcd) and its tool mlx-ipc use
it, as do the compositor's logind session and the desktop's system bus
clients; `tools/check_ipc.sh` runs both against dbus-send, gdbus, busctl
and dbus-monitor, and `tests/361_dbus_keep_signals_runtime.mlx` keeps a
signal across a call against a bus played by a child.

`std.audio` (`std/src/audio.mlx`) is MLX Audio's protocol and client:
`connect` finds the sound server on the session bus and gets a direct
MLXIPC channel to it (starting it when needed), `openStream` opens a
playing or recording stream (any rate, 1 to 8 channels, S16, S32 or
F32; `refusal` says when the app's MLXIPC permissions refused it),
`write` plays as fast as the server asks, `read` returns what was
recorded, `drain`, `setVolume`, `list` and `closeStream`;
`openStreamOn` names the sink to play to or the source to record from
(`input`, `monitor`, `monitor:SINK`), `status` and `set` read and change
the server's state (devices, volumes, sinks, each app's route). `std.wav`
(`std/src/wav.mlx`) reads WAVE headers (8 to 32-bit PCM, 32-bit float,
extensible files) and writes them. `projects/desktop/audio` (mlx-audiod,
mlx-audio) uses both; `tools/check_audio.sh` checks them end to end.
`std.appid` (`std/src/appid.mlx`) says which app a process is, as the
desktop names apps: `identify` gives a Flatpak app's id (from
`/proc/PID/root/.flatpak-info`, then sandboxed) or its program's name
(`/proc/PID/exe`, without a `-bin` at its end). MLXIPC's permissions and
MLX Audio's PulseAudio socket name apps with it.
`std.pulse` (`std/src/pulse.mlx`) has the numbers of PulseAudio's
native protocol (the version, the packet descriptor, the commands, the
errors, the subscription facilities) and `std.pulse.tagstruct` writes and
reads the tagstructs its commands are made of, into a buffer or (for a
client library) into memory that grows; MLX Audio's server
(`projects/desktop/audio`) and its libpulse (`compiler/libc/pulse`) are
the two ends that use them.
`std.pipewire` (`std/src/pipewire.mlx`) has the numbers of PipeWire's
native protocol (the core's, client's, registry's and client node's
methods and events, SPA's params, IO areas, commands, formats, buffer
data and struct pw_node_activation's layout) and `std.pipewire.pod`
writes and reads the SPA pods its messages are made of; MLX Capture's
server (`projects/desktop/capture`) and its libpipewire
(`compiler/libc/pipewire`) are the two ends that use them.

`std.jpeg` (`std/src/jpeg.mlx`) encodes premultiplied ARGB words as
baseline JPEG (4:2:0, standard tables scaled by a quality of 1 to 100,
each row of blocks a restart interval); `startVideo` and `encodeFrame`
code again only the rows a frame changed. `std.matroska`
(`std/src/matroska.mlx`) writes Matroska files: Motion JPEG and 16-bit
PCM tracks, blocks with their own times, clusters, Cues and a SeekHead
for seeking. mlx-capture's recordings use both
(`tools/check_capture_record.sh`).

`std.png` (`std/src/png.mlx`) decodes PNG images into premultiplied ARGB
words (`0xAARRGGBB`, the `wl_shm` and compositor convention), for icons
and the like:

```mlx
const png = @import("std.png")

var image: png.Image = undefined
if png.loadFile(&image, path, allocator) == png.Status.ok {
    var small: png.Image = undefined
    const status = png.scale(&small, &image, 48, 48, allocator)
    png.deinit(&image)
}
```

`loadFile(image, path, allocator)` and `decode(image, data, length,
allocator)` return a `Status` (`ok`, `unreadable`, `notPng`,
`unsupported`, `corrupt`, `noMemory`; `statusMessage` words it). Every
colour type at every bit depth PNG allows for it is read, with palette
transparency (`tRNS`), all five row filters and zlib streams of stored,
fixed and dynamic Huffman blocks; interlaced (Adam7) images are
`unsupported`. Malformed data gives `corrupt`, never a trap; chunk CRCs
and the zlib checksum are not checked. `scale(target, source, width,
height, allocator)` resizes by area averaging, which suits shrinking an
icon to the size it is shown at, and `deinit` frees the pixels. The dock
and the launcher of the compositor (see [Wayland](wayland.md)) draw app
icons with it. `tests/272_png_runtime.mlx` decodes the images in
`tests/support/png` (written by `tests/support/png/make_pngs.py`, one per
colour type and depth, every filter, all three block kinds) and compares
them with the pixels they must give, refuses an interlaced image and a
file that is not a PNG, finds a truncated one corrupt, and checks
`scale`.

`saveFile(path, pixels, width, height, stride, alpha, allocator)` writes
premultiplied ARGB words (rows `stride` words apart) as an 8-bit RGBA PNG,
or RGB with `alpha` false, and returns whether it was written: each row
with the filter that leaves the smallest differences, compressed by
`std.flate` (zlib's level 6), the chunk CRCs by `std.crc`. `mlx-capture`
saves captures with it. `tests/292_png_encode_runtime.mlx` writes an image
both ways and reads it back with `loadFile`.

`std.flate` (`std/src/flate.mlx`, its parts in `std/src/flate/`) is the
repository's one DEFLATE implementation: raw deflate data (RFC 1951), zlib
streams (RFC 1950) and gzip members (RFC 1952), compressed and
decompressed. The compressor is zlib's (1.3.1) step for step, every level
(0 stored, 1 to 3 fast, 4 to 9 lazy matching) and strategy, so its bytes
are zlib's; the decompressor is zlib's state machine, taking input and
output in pieces of any size and checking the check values.

```mlx
const flate = @import("std.flate")

// All at once, into the caller's buffer (0: did not fit, or damaged):
const room = flate.compressBound(length, flate.Format.zlib)
const made = flate.compress(data, length, output, room, 6, flate.Format.zlib, allocator)
const size = flate.decompress(input, count, buffer, capacity, flate.Format.gzip)

// All at once, into an allocator's memory, growing as needed:
var result: flate.Buffer = undefined
if flate.decompressAlloc(allocator, input, count, flate.Format.detect, &result) == flate.Status.done {
    // result.bytes[0..result.length]
    flate.freeBuffer(allocator, &result)
}

// In pieces:
var deflater: flate.Deflater = undefined
if flate.deflaterInit(&deflater, allocator, flate.options(9, flate.Format.gzip)) == flate.Status.ok {
    var progress: flate.Progress = undefined
    const status = flate.deflaterRun(&deflater, piece, pieceLength, out, 4096, flate.Flush.finish, &progress)
    // progress.consumed taken, progress.produced written; again until Status.done
    flate.deflaterEnd(&deflater)
}
```

`Format` is `raw`, `zlib`, `gzip` or `detect` (decompression: zlib or
gzip, as the header says); `Status` is `ok`, `done`, `needDictionary`,
`damaged`, `noMemory`, `stuck` (no progress without more input or room)
or `misuse`; `Flush` and `Strategy` are zlib's flushes and strategies, and
`options(level, format)` gives a compressor's `Options` with zlib's
defaults for the rest (a 32 KiB window, memory level 8). The deflater
also takes a dictionary, a gzip `Header`, new parameters on the way and
`deflaterBound`; the inflater a dictionary and a place for the gzip
header it reads, and `message(&inflater.stream)` words what was wrong
("incorrect header check"...). `decompress` needs no allocator (its state
is 8 KiB of stack). `inflate` (raw data), `gunzip` and `gzipSize` are the
earlier one-call forms. The engine itself (`std.flate.deflate`,
`std.flate.inflate`, on a `Stream` with zlib's return codes) is what the
zlib C API, `compiler/libc/zlib` (`libmlxz.so`, `<zlib.h>` for C and C++
programs), is made of. `tests/343_flate_runtime.mlx` checks zlib's bytes,
round trips in every format and in pieces, and damaged data.

`std.flate.gzip` (`std/src/flate/gzip.mlx`) is gzip files, what zlib's
gz* functions do, for Mlx programs: a `Reader` (`openRead` or
`readerInit` over a file, `read` in pieces of any size, `readToEnd`,
`readerClose`) gives the uncompressed bytes of a file's gzip members one
after another, reads a file that is not gzip as it is, leaves alone what
follows the last member and names what went wrong (`readerError`: a
damaged member, a truncated one, the file); a `Writer` (`openWrite` or
`writerInit` at a level, `write`, `flush`, `writerClose`, which finishes
the member) writes one member; `readFile` and `writeFile` do a whole
file at once. Each buffers 8 KiB in memory of the allocator.
`tests/385_flate_gzip_runtime.mlx` writes and reads files in odd pieces,
two members, a plain file, trailing data, damaged and truncated members.

`std.crc` (`std/src/crc.mlx`) has the check values: `crc32(crc, data,
length)` (the CRC-32 of gzip, zlib, PNG and ZIP, 0 to start),
`adler32(adler, data, length)` (1 to start), `crc32Join` and `adler32Join`
(the value of two pieces put together), and `cksum(data, length)` with
`cksumUpdate` and `cksumFinish` (the CRC of POSIX cksum).

## Core: allocators

**Normative source:** `spec/04-stdlib/mem.xml`
**Implementation:** `std/bootstrap/allocator.mlx`, `page_allocator.mlx`,
`tinypage_allocator.mlx`, `arena_allocator.mlx`,
`fixed_buffer_allocator.mlx`, `general_purpose_allocator.mlx`,
`debug_allocator.mlx`, `mem.mlx`; `std/src/leaks.mlx`
**Source tests:** `120_bootstrap_mem_runtime.mlx`,
`131_bootstrap_allocator_runtime.mlx`, `132_page_allocator_runtime.mlx`,
`137_bootstrap_arena_allocators_runtime.mlx`,
`296_general_purpose_allocator_runtime.mlx`,
`314_debug_allocator_runtime.mlx`, `315_tinypage_runtime.mlx`
(`tests/run_debug_allocator.sh`)

`spec/04-stdlib/mem.xml` specifies an `Allocator` with `alloc`/`resize`/`free`
operations, four allocator kinds (`PageAllocator`, `ArenaAllocator`,
`FixedBufferAllocator`, `GeneralPurposeAllocator`), endian helpers
(`readInt`, `writeInt`, `byteSwap`, `nativeToLittle`/`nativeToBig`/
`littleToNative`/`bigToNative`), and memory ops (`copy`, `copyBackwards`,
`set`, `zero`, `eql`).

The bootstrap implementation matches the allocator contract exactly. The
interface is a struct of function-pointer fields plus an opaque context
pointer (function values are ordinary indirect-callable values in Mlx), as
Zig's `Allocator` with its vtable flattened into the struct:

```mlx
pub const AllocFn = fn(context: *anyopaque, length: usize, alignment: usize) -> ?[*]u8
pub const ResizeFn = fn(context: *anyopaque, memory: [*]u8, length: usize, alignment: usize, new_length: usize) -> bool
pub const RemapFn = fn(context: *anyopaque, memory: [*]u8, length: usize, alignment: usize, new_length: usize) -> ?[*]u8
pub const FreeFn = fn(context: *anyopaque, memory: [*]u8, length: usize, alignment: usize) -> void

pub const Allocator = struct {
    context: *anyopaque
    allocFn: AllocFn
    resizeFn: ResizeFn
    remapFn: RemapFn
    freeFn: FreeFn
    ...
}
```

(`std/bootstrap/allocator.mlx`)

All four spec'd allocators are implemented: `page_allocator.mlx` (backed
directly by `mmap`/`munmap` via `std.os.linux`, small blocks on tiny
pages, below), `arena_allocator.mlx`
(bump-allocates out of one page-allocated block, with `reset`/`deinit`;
as Zig's, its last block grows and shrinks in place, so a list growing at
the arena's end is not copied and what it gives back is used again),
`fixed_buffer_allocator.mlx` (bump-allocates over a caller-owned buffer,
with `reset`), and `general_purpose_allocator.mlx`, the heap for blocks of
any size freed in any order, for every thread of a process at once:

```mlx
var heap = std.general_purpose_allocator.init(std.page_allocator.init())
const allocator = std.general_purpose_allocator.interface(&heap)
const block = allocator.alloc(100, 8).?
allocator.free(block, 100, 8)
std.general_purpose_allocator.deinit(&heap)   // gives the slabs back
```

Blocks up to 32 KiB come from 22 size classes (16, 32, 48, 64, 96, ...,
32768 bytes, every one a multiple of 16), carved out of 256 KiB slabs from
the child allocator and kept on a free list per class when freed; a
power-of-two class in a page-aligned slab meets alignments above 16 up to a
page. Larger blocks are page mappings of their own (one aligned beyond a
page carries a header naming its mapping). Since the interface hands the
length and alignment back on `free`, no block needs a header; `resize`
succeeds in place while the new length stays in the block's class (or the
large block's pages). One lock, a futex word taken with `@cmpxchgWeak` and
slept on through the kernel when contended, covers the lists.
`liveBytes`, `slabCount` and `largeCount` report the heap's state;
`blockSize(length, alignment)` the class a block occupies.
`296_general_purpose_allocator_runtime.mlx` exercises every class, the
alignments, resizing, deinit and four threads allocating and freeing
through one heap. mlxlibc's `malloc`/`free`/`realloc` sit on it.

### Leak checks: `std.debug_allocator` and `std.leaks`

`std.debug_allocator` is Zig's `DebugAllocator` (the `GeneralPurposeAllocator`
of older Zig with its safety checks): it wraps any allocator and keeps each
block it hands out with the functions that asked for it (12 return
addresses along the frame pointers every Mlx function keeps). What goes
wrong is said on stderr with where it happened:

- a **leak**: a block not freed by `deinit` (or `report`), the blocks of one
  place said once with their count and bytes, the largest places first;
- a **double free**: with where the block was allocated, first freed and
  freed again; the second free is not passed on;
- an **invalid free**: an address it never handed out (not passed on);
- a **wrong size**: a block freed with another length or alignment than it
  was allocated with (freed with the right ones).

Freed memory is overwritten with `0xAA` first (as Zig's `undefined`), so a
block used after it went shows; the last 4096 frees are remembered to tell
a double free from an invalid one. Its own tables are mappings of their
own, one futex lock covers them.

```mlx
var checked: std.debug_allocator.DebugAllocator = undefined
std.debug_allocator.init(&checked, std.page_allocator.init())
checked.describe = crash.describeReturn       // names: path:line:function
const allocator = std.debug_allocator.interface(&checked)
...
const leaked = std.debug_allocator.deinit(&checked)   // 0: none (Zig's .ok)
```

```
mlx-files[4211]: leak: 3 blocks, 12288 bytes
  allocated at projects/desktop/files/icons.mlx:88:load+0x4c at projects/desktop/files/icons.mlx:91
  from projects/desktop/files/main.mlx:310:redraw+0x2f3 at projects/desktop/files/main.mlx:322
mlx-files[4211]: 3 blocks leaked (12288 bytes) from 1 place; 940 allocations, 937 frees, 2097152 bytes out at most
```

Most programs take `std.page_allocator.init()` wherever they need memory
instead of passing one allocator around. `watchPages(state)` has the page
allocator tell a debug allocator every block it maps and unmaps (through
two function pointers, so the compiler, which takes its pages there too,
does not depend on it); the pages stay what they were, fresh zeroed
mappings, and frees of pages mapped before the watch go through unsaid.
`std.leaks` does that for a whole program when `MLX_LEAK_CHECK` is set:

```mlx
const watched = leaks.watch()   // first thing, or right before the loop
leaks.mark()                    // what came before is the program's to keep
...the loop...
defer leaks.finish()            // the blocks the loop kept, and the arena
```

It is part of the runtime: `std.crash.install()`, which every program
calls first, calls `leaks.watch()`, so with `MLX_LEAK_CHECK` set any
program is watched from its start (`tests/support/leaks_program.mlx`, in
`tests/run_debug_allocator.sh`). `MLX_LEAK_CHECK=1` reports on stderr,
`MLX_LEAK_CHECK=/a/path` appends to
that file (each line names the program and its process), `0` or nothing
watches nothing. While a watched program runs, its memory is written
about once a second (from the main thread, as blocks come and go, and at
each mark and report) to `$XDG_RUNTIME_DIR/mlx/memory/PROGRAM-PID.memory`:
the totals, how full the tiny pages are, each place with its functions
(as crashes name them), each block (address, bytes, place, since the mark
or kept, what it takes in pages) and the program's mappings; with a GPU
context open also the GPU's memory: the device, its heaps and memory
types, and each block of device memory `std.gpu` allocated with the bytes
asked, the bytes the device took and its place (the functions that asked,
`std.gpu`'s own left out). Every buffer is an allocation of its own, so
what the GPU's memory wastes is what the device rounds up, and many small
allocations against its limit. A loop calls `leaks.tick()` each turn (the
panels of `std.ui.wayland.panel` do) for a snapshot when only the GPU's
memory changed. `mlx-codemap memory [PROGRAM]` and Memory under Findings in MLX
Observatory read it: what the page blocks waste by size class, the holes
between them and how scattered they are, what the GPU holds by heap and
size class, and the places that hold the most or grow. The report also says when the compiler's value arena
grew by more than 64 KiB since the mark: a loop whose turns do not give
their values back (`std.turns`) grows a program as a leak would (a loop on
`std.turns` is not said). A block a program keeps on purpose until it
ends, a cache's or state it replaces as it changes, is left out of the
reports with `leaks.keep(memory)` (`std.debug_allocator.keep`): the
symbols of the top bar and its popups, a terminal's grids. The
loops of `std.ui.wayland.panel` (the dock, launcher, top bar, files,
settings, terminal, notifications, the Observatory), the compositor,
mlx-ipcd, mlx-audiod, mlx-netd, mlx-console and mlx-splash call these.
The compositor's modules are images of their own with their own page
allocator, and are not watched.

#### Tiny pages: `std.tinypage_allocator`

Most programs take `std.page_allocator.init()` for every block, and a
mapping of its own for a block of 40 bytes takes a page of 4 KiB: 99% of
it wasted, and a mapping more for the kernel to keep. So the page
allocator puts every block of up to 2 KiB (`LIMIT`) on tiny pages: a page
is split into 64 tiny pages of 64 bytes, and a block takes a run of them
in one page, the first run that fits. A small block so squeezes in where
one went before, between the blocks still there, and gives its tiny
pages back when it is freed. Two bitmaps per page say what is taken (the
tiny pages in use, and where each block starts), a byte per page its
longest free run and one per 64 pages theirs, so a search skips full
pages without looking at their bits. A free needs no length: one freed
with a wrong length still frees the block it names, and one freed twice
is seen and left be. Blocks come zeroed, as fresh pages do; alignments
up to 2 KiB are honoured, larger blocks and page alignments get whole
pages as before.

The pages lie in one region at a fixed address (`BASE`,
0x200000000000), reserved with `MAP_FIXED_NOREPLACE` and made writable a
megabyte at a time, which every image of a process shares: the program,
its shared objects and plugins. The first that needs the region maps it,
the others find it there (a header with a magic word and a futex lock),
so a block taken in one image can be freed in another, and whether an
address is a tiny block is a range check. When the region cannot be had
(a kernel without `MAP_FIXED_NOREPLACE`, a small address space) every
block gets whole pages. Empty pages keep their memory while there are
few (64); past that, an emptied page's memory goes back to the kernel
(`MADV_DONTNEED`).

```mlx
const pages = std.page_allocator.init()
const name = pages.alloc(40, 8).?      // 64 bytes of a shared page
pages.free(name, 40, 8)                // the run is free for the next one
var usage: std.tinypage_allocator.Usage = undefined
std.tinypage_allocator.usage(&usage)   // pages, blocks, bytes, empty pages
```

`std.page_allocator.footprint(address, length)` says what a block takes
from the address space: its tiny pages or its whole pages. The debug
allocator's watch still sees every block, and std.leaks' snapshots give
each block its footprint and say how full the tiny pages are (Memory in
MLX Observatory draws them as a strip of pages, each filled as far as it
is taken). In MLX Observatory reading this repository, the blocks below
4 KiB took 24 KiB of pages instead of 84 KiB (10% wasted instead of
74%), those below 256 bytes 640 bytes instead of 20 KiB.
`315_tinypage_runtime.mlx` takes and frees blocks of every size,
squeezes one into a freed run between two others, frees with a wrong
length and twice, and has four threads take and free at once.

A runtime fixture exercises fixed-buffer alignment/reset and arena
alignment/reset/deinit together:

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
unrolled) plus little- and big-endian read/write helpers specialized to
16/32/64 bits (`readU16Little`, `readU32Little`, `readU64Little`,
`writeU16Little`/`writeU32Little`/`writeU64Little`, and the network-order
`readU16Big`/`readU32Big`/`readU64Big`, `writeU16Big`/`writeU32Big`/
`writeU64Big`). This covers the `MemoryOps` half of `mem.xml` but is
narrower than its `EndianHelpers` surface: there is no generic
`readInt`/`writeInt`, `byteSwap`, or
`nativeToLittle`/`nativeToBig`/`littleToNative`/`bigToNative` — only the
concrete read/write pairs above. `132_page_allocator_runtime.mlx`
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
`139_bootstrap_elf_file_runtime.mlx`, `277_fs_entries_runtime.mlx`,
`353_fs_whole_files_runtime.mlx` (run by `tests/run_std_modules.sh`)

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
`sync*`. `readDirectory` (`getdents64`) is the raw primitive; `entries`
and `nextEntry` iterate a directory through a buffer the caller gives
(`Entries`, each `Entry` a borrowed name, its `ENTRY_*` kind and inode,
`failed` set when a read or a record goes wrong), and `isDotName` tells
`.` and `..` apart (`tests/277_fs_entries_runtime.mlx`):

```mlx
var reader = std.fs.entries(directory, buffer, 8192)
var entry: std.fs.Entry = undefined
while std.fs.nextEntry(&reader, &entry) {
    if std.fs.isDotName(entry.name) { continue }
    ...
}
if reader.failed { ... }
```

Whole files, by path, the way a program reads a setting or a /proc value
and writes a cache (`tests/353_fs_whole_files_runtime.mlx`):
`readFile(path, into, capacity)` reads all of a file or its first
`capacity` bytes and returns how many (0 when it could not be opened;
`readFileResult` returns -1 then), `readFileAlloc(path, allocator)` reads
one of any length into an `ArrayList` (null when it could not be opened),
`writeFile(path, bytes, length, mode)` writes one (made or emptied first),
and `replaceFile(path, bytes, length, mode)` writes one whole or not at
all: into `PATH.new`, made anew with `mode`, then renamed over `PATH`.
`makeDirectories(path, mode)` is `mkdir -p` (for a slice),
`makeParents(path, mode)` makes the folders a file goes in, `isDirectory`
follows links, and `executablePath(into, capacity)` is /proc/self/exe's
target, NUL-terminated. The desktop's settings files, the coreutils'
operands, mlx-init, mlx-netd, mlx-sh, the generators under std/registry
and the compiler's own output use them.

```mlx
var line: [256]u8 = undefined
const bytes: [*]u8 = &line
const length = std.fs.readFile("/proc/sys/kernel/hostname".pointer, bytes, 256)
// `path` a slice, `c_path` the same path NUL-terminated.
if !std.fs.makeParents(path, 493) || !std.fs.replaceFile(c_path, text.pointer, text.length, 420) { ... }
```

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

`std.process.optionValue(arguments, &index, attached)` takes a command-line
option's value: `attached` (the rest of its own argument, "5" of `-n5`)
when it is not empty, else the next argument (`-n 5`, moving `index` to
it), or null after the last one
(`tests/278_option_value_and_bases_runtime.mlx`).

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
`writeUnsignedBase` (any base from 2 to 36, lowercase digits;
`tests/278_option_value_and_bases_runtime.mlx`),
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

## math (`std.math`)

**Implementation:** `std/src/math.mlx`
**Source tests:** `325_standard_math_runtime.mlx`, `324_float_rounding_runtime.mlx`,
`350_math_helpers_runtime.mlx`

Floating-point math in Mlx on the doubles' bits, without libm. Import it as
`@import("std.math")`; it imports nothing itself, so a `--plugin` library
can use it too.

- Constants: `pi`, `tau`, `e`, `ln2`, `ln10`, `sqrt2` (`f64`), and
  `infinity()`, `negativeInfinity()`, `nan()`, `isNan(x)`, `abs(x)`.
- Whole numbers: `floor`, `ceil`, `trunc` are the builtins `@floor`,
  `@ceil`, `@trunc` (one instruction); `round(x)` rounds halves away from
  zero.
- `min(a, b)`, `max(a, b)`, `clamp(x, low, high)` and `absolute(x)` on
  `f64`, and `minOf`, `maxOf`, `clampOf`, `absoluteOf` for any number type
  named first (`math.minOf(u32, version, 4)`); an `anytype` parameter
  would take a literal's type from the literal, so the type is given.
  `sqrt(x)` is `@sqrt` but 0 from 0 down (NaN stays NaN), what lengths
  want; `smoothstep(from, to, x)` is 0 up to `from`, 1 from `to` on and
  3t² − 2t³ between. The splash, MLX Observatory, `std.ui.vector`, the
  compositor and the Android runtime use these instead of copies of
  their own.
- `sin(x)`, `cos(x)`: argument reduction by pi/2 in three parts, then
  fdlibm's minimax kernels; within an ulp of the exact value for |x| up to
  about 2^20 pi, 0 past 2^52 (NaN for the infinities and NaN).
- `exp(x)`, `ln(x)`, `log2(x)`, `log10(x)`: fdlibm's reductions and
  polynomials, within an ulp; `log2` and `log10` take the exponent exactly,
  so whole powers of two and of ten come out exact. `ln` of 0 is minus
  infinity, of a negative number NaN; `exp` overflows to infinity past
  709.78 and underflows to 0 below -745.13.
- `pow(x, y)` for `x > 0` (`exp(y ln x)`: a few ulps for results up to
  about 2^50), `cbrt(x)` for any sign.

```mlx
const math = @import("std.math")

pub fn main() -> u8 {
    if @bitCast(u64, math.sin(1.0)) != 4605754516372524270 { return 1 }   // the correctly rounded sin(1)
    if math.log10(1000.0) != 3.0 || math.log2(1024.0) != 10.0 { return 6 }
    if math.round(-2.5) != -3.0 { return 8 }
    if !math.isNan(math.ln(-1.0)) { return 9 }
    return 13
}
```

(`tests/325_standard_math_runtime.mlx`)

## Encodings (`std.base64`)

**Implementation:** `std/src/base64.mlx`
**Source tests:** `347_base64_runtime.mlx` (run by `tests/run_std_modules.sh`)

The encodings of RFC 4648 — `Encoding.base64`, `base64url`, `base32`,
`base32hex` and `base16` — without allocating: the caller sizes the
output with `encodedLength(encoding, length, padded)` and
`decodedLength(encoding, length)`. `encode(encoding, bytes, out, padded)`
and `decode(encoding, text, out, ignoreGarbage)` work in one call; an
`Encoder` (`encoderInit`, `encodeUpdate`, `encodeFinal`) and a `Decoder`
(`decoderInit`, `decodeUpdate`) take the data in pieces and keep the bits
of an unfinished symbol or byte between them. Decoding skips line breaks,
spaces and tabs, stops at the padding, and refuses (or, with
`ignoreGarbage`, skips) bytes outside the alphabet. `hexEncode` writes
lower-case hex, the way digests are printed, and `hexDecode` reads hex
of either case. The coreutils `base64`, `base32` and `basenc` and GLib's
`g_base64_*` (`compiler/libc/glib/strings.mlx`) use it.

```mlx
const base64 = @import("std.base64")

pub fn main() -> u8 {
    var out: [8]u8 = undefined
    const bytes: [*]u8 = &out
    const count = base64.encode(base64.Encoding.base64, "foo", bytes, true)   // "Zm9v"
    const back = base64.decode(base64.Encoding.base64, bytes[0..count], bytes, false)
    return if back.ok && back.length == 3 { 13 } else { 1 }
}
```

## Digests (`std.md5`, `std.blake2`, `std.sha1`, `std.sha2`)

**Implementation:** `std/src/md5.mlx`, `std/src/blake2.mlx`,
`std/src/sha1.mlx`, `std/src/sha2.mlx`
**Source tests:** `348_md5_blake2_runtime.mlx`, `349_sha2_runtime.mlx`
(run by `tests/run_std_modules.sh`); `tools/check_wifi.sh` for SHA-1's
HMAC and PBKDF2

Each digest has a state that takes the message in pieces (`…Init`,
`…Update`, `…Final`, which wipes the state) and a one-call function;
`std.base64.hexEncode` prints a digest the way the `…sum` tools do.

- `std.md5`: `Md5`, `md5Init`, `md5Update`, `md5Final(&state, *[16]u8)`,
  `md5(bytes, *[16]u8)` and `md5Hex(bytes, *[32]u8)` (RFC 1321; for the
  formats that still use it, not for security). md5sum, the thumbnail
  cache's file names (`projects/desktop/shared/thumbs.mlx`) and GLib's
  `GChecksum` use it.
- `std.blake2`: BLAKE2b (RFC 7693) with digests of 1 to 64 bytes, keyed
  with up to 64 bytes or not: `Blake2b`, `blake2bInit(&state, length,
  key)`, `blake2bUpdate`, `blake2bFinal(&state, out)`, `blake2b(bytes,
  *[64]u8)` (512 bits, unkeyed, what b2sum prints) and
  `blake2bWith(bytes, key, out, length)`.
- `std.sha1`: SHA-1, HMAC-SHA1 and PBKDF2-HMAC-SHA1 (what WPA2 needs).
- `std.sha2`: SHA-256 and SHA-512 (`Sha256`, `Sha512`, `sha256Init`,
  `sha256Update`, `sha256Final`, `sha256`, and the same for 512), SHA-224
  and SHA-384 on the same states from their own initial words
  (`sha224Init` and `sha224Final(&state, *[28]u8)` on a `Sha256`,
  `sha384Init` and `sha384Final(&state, *[48]u8)` on a `Sha512`;
  `sha224`, `sha384` in one call), HMAC-SHA256 and PBKDF2-HMAC-SHA256.

```mlx
const md5 = @import("std.md5")
const blake2 = @import("std.blake2")

pub fn main() -> u8 {
    var text: [32]u8 = undefined
    md5.md5Hex("abc", &text)                  // 900150983cd24fb0d6963f7d28e17f72
    var digest: [64]u8 = undefined
    blake2.blake2b("abc", &digest)            // ba80a53f...
    return if text[0] == 57 && digest[0] == 186 { 13 } else { 1 }
}
```

## Random numbers (`std.random`)

**Implementation:** `std/src/random.mlx`
**Source tests:** `351_random_runtime.mlx` (run by `tests/run_std_modules.sh`)

- From the kernel (getrandom(2)): `fill(into, length)` (false only on a
  kernel without getrandom), `random64()` and `random32()` (mixed from the
  monotonic clock if the kernel gives nothing, so they never fail), and
  `getrandom(into, length, flags)` as the kernel answers it. Keys,
  nonces and ids come from these: mlx-netd's DHCP transaction ids and
  WPA nonces, MLXIPC's bus id, libpipewire's `pw_getrandom`/`pw_random`
  and shuf without `--random-source`.
- Seeded, for numbers that repeat with their seed: `Xoshiro256`
  (xoshiro256**, its 256-bit state filled from the seed by splitmix64)
  with `seed(&g, value)`, `seedFromKernel(&g)`, `next(&g)`,
  `below(&g, bound)` (unbiased) and `unit(&g)` (a double in [0, 1));
  `splitmix64(&state)` itself; and `Xorshift64Star` (Marsaglia's
  xorshift64*) with `xorshift64StarNext` and `xorshift64StarUnit`, the
  sequence the boot splash draws its stars from. None of these is for
  secrets.

```mlx
const random = @import("std.random")

pub fn main() -> u8 {
    var generator: random.Xoshiro256 = undefined
    random.seed(&generator, 42)
    const roll = random.below(&generator, 6)       // 0 to 5, the same for seed 42
    var key: [16]u8 = undefined
    if !random.fill(&key, 16) { return 1 }
    return if roll < 6 { 13 } else { 2 }
}
```

## Terminals (`std.terminal.tty`)

**Implementation:** `std/src/terminal/tty.mlx`
**Source tests:** `352_terminal_tty_runtime.mlx` (run by `tests/run_std_modules.sh`)

The terminal a program runs on, on Linux: `isTerminal(descriptor)`;
`windowSize(descriptor, &size)`, `columns(descriptor)` and
`rows(descriptor)` (0 when unknown); the modes as a `Termios`
(`getAttributes`, `setAttributes`) and `makeRaw(&cooked, &raw)` for a line
editor (no echo, no canonical input, no signals from keys, no CR
translation or flow control, reads of one byte); and job control:
`foregroundGroup`, `setForegroundGroup`, `setProcessGroup` and
`processGroup`. mlx-sh's line editor and job control and ls's column
width use it; the test runs it on a pseudo-terminal of its own.

## Base directories (`std.xdg`)

**Implementation:** `std/src/xdg.mlx`
**Source tests:** `354_xdg_runtime.mlx` (run by `tests/run_std_modules.sh`)

Where a program keeps its files, by the XDG Base Directory
Specification, from an environment list (`[*][*]const u8`, the program's
or a child's). `home(environment, kind, into, capacity)` writes the
user's directory of `kind` — `CONFIG` (`$XDG_CONFIG_HOME`, else
`~/.config`), `DATA` (`~/.local/share`), `CACHE` (`~/.cache`) or `STATE`
(`~/.local/state`) — NUL-terminated and returns its length, 0 when there
is no home or it does not fit; `path(environment, kind, rest, into,
capacity)` is the same with `rest` after it, all of it or nothing.
`runtimeDirectory` is `$XDG_RUNTIME_DIR`; `dataDirectories` and
`configDirectories` are the system's colon-separated lists (with the
specification's defaults), walked by `nextDirectory(list, &at,
&directory)`. As the specification asks, an empty or relative variable is
ignored, and so is a `$HOME` that is not absolute. The desktop's settings
and caches (the audio server, the compositor, the settings app, the dock's
pins, the clock, the top bar, the trash, the thumbnails, MLXIPC's policy,
service files and prompts, the greeter's sessions), std.ui's appearance,
std.xkb, std.vulkan's loader and the code map use it.

```mlx
const xdg = @import("std.xdg")

var path: [512]u8 = undefined
const bytes: [*]u8 = &path
const length = xdg.path(environment, xdg.CONFIG, "/mlx/audio.conf", bytes, 512)
if length == 0 { ... }                          // no home, or too long
```

## Accounts and groups (`std.passwd`)

**Implementation:** `std/src/passwd.mlx`
**Source tests:** `355_passwd_runtime.mlx` (run by `tests/run_std_modules.sh`)

The accounts of /etc/passwd (`User`: `name`, `password`, `id`, `group`,
`gecos`, `home`, `shell`) and the groups of /etc/group (`Group`: `name`,
`password`, `id`, `members`), their fields slices of the file's text.
`parseUser` and `parseGroup` read one line (false for a line with too
few or too many fields or a number that is not one); `findUserNamed`,
`findUserById`, `findGroupNamed` and `findGroupById` search a file's text;
`userById`, `userNamed`, `groupById` and `groupNamed` read the file into
the caller's buffer first. Nothing allocates. chown, chgrp and ls (the
coreutils), libpulse's user name and home and libpipewire's user name use
it.

## Settings files (`std.conf`)

**Implementation:** `std/src/conf.mlx`
**Source tests:** `356_conf_runtime.mlx` (run by `tests/run_std_modules.sh`)

The "key = value" files the desktop and its services keep: blanks around
keys and values, `#` comment lines, `[Group]` lines. `parseLine(line,
&entry)` says what a line is (`BLANK`, `ENTRY`, `GROUP` or `OTHER`, a line
without `=`); `lines(text)` and `next(&lines, &entry)` walk a file's
entries, each with the group it is in; `find(text, key)` is the first
value of a key; `parseSwitch(value, &on)` reads on/off, yes/no, true/false
and 1/0.

```mlx
const conf = @import("std.conf")

var lines = conf.lines(text)
var entry: conf.Entry = undefined
while conf.next(&lines, &entry) {
    if std.text.same(entry.key, "volume") { ... entry.value ... }
}
```

## Patterns (`std.glob`)

**Implementation:** `std/src/glob.mlx`
**Source tests:** `357_glob_runtime.mlx` (run by `tests/run_std_modules.sh`)

The shell's patterns, as fnmatch has them: `*`, `?` and sets (`[a-z]`,
`[!...]` or `[^...]`, a `]` first a member, a `[` without its `]` itself),
over whole byte strings. `matches(pattern, text)` takes a backslash as
making the next byte stand for itself; `matchesQuoted(pattern, literal,
text)` takes the bytes a shell quoted instead (`literal[i] & 1`). mlx-sh's
pathname expansion, `case` and `${...#...}`, and ls's `--ignore` and
`--hide` use it.

## Starting programs (`std.spawn`)

**Implementation:** `std/src/spawn.mlx`
**Source tests:** `360_spawn_runtime.mlx` (run by `tests/run_std_modules.sh`)

A child is described by `Options` (`options(program, arguments,
environment)` changes nothing else): `searchPath` looks a name without a
slash up in the environment's PATH (`defaultPath` when it has none),
`input`/`output`/`errors` become its standard descriptors, `extra` is
handed over at `extraAt`, `keep`/`keepAlso` stay open across execve and
`closing`/`closingAlso` are closed in it, `directory` is its working
directory, `newSession` gives it a session of its own, `unblockSignals`
clears the mask a parent with a signal descriptor left, `detach` starts
the program from an intermediate child reaped at once, and `shareMemory`
vforks (for large parents). `start` returns the pid (or -errno);
`startReading` hands back the read end of a pipe the child writes to;
`capture` runs it to its end with its output kept; `wait` and `ended`
give the exit code or 128 plus the signal; `execute` and `executeIn` are
the PATH lookup for children that fork themselves, and `find` is where a
name would run from. A program that cannot run exits with 127.

```mlx
const spawn = @import("std.spawn")

var options = spawn.options(program, argv, envp)
options.searchPath = true
var output: [4096]u8 = undefined
var status: usize = 0
const length = spawn.capture(&options, true, &output, 4096, &status)
```

## Finding fonts (`std.fontconfig`)

**Implementation:** `std/src/fontconfig.mlx` and `std/src/fontconfig/`
**Source tests:** `373_fontconfig_runtime.mlx` (run by `tests/run_std_modules.sh`);
`tools/check_fontconfig.sh` for the C API over it (compiler/libc/fontconfig)

Fontconfig 2.15's font matching without a C library: `load.current()`
reads fonts.conf and what it includes (or Fontconfig's fallback
configuration) and the fonts of its directories, each face read by
`std.truetype` into a pattern of Fontconfig's objects and kept in a
cache of the module's own under `$XDG_CACHE_HOME/mlx-fontconfig`.
`pattern.parseName` makes a pattern of a name ("sans-serif:bold"),
`config.substitute` and `config.defaultSubstitute` apply the rules and
the defaults, and `matching.fontMatch`, `matching.fontSort` and
`matching.fontList` match, sort and list as Fontconfig does. Patterns,
character sets, language sets and the configuration are records reached
by their addresses, as the C API hands them out; strings compare with
Fontconfig's case folding (`strings`, of CaseFolding.txt). The languages'
orthographies (`std/registry/fontconfig`, Fontconfig's fc-lang) and the
folding are made into `std/src/fontconfig/data.mlx` by
`std/registry/fontconfig/generate.mlx`.

```mlx
const fc = @import("std.fontconfig")

const config = fc.load.current()
const p = fc.pattern.parseName(name)
_ = fc.config.substitute(config, p, fc.config.MATCH_PATTERN)
fc.config.defaultSubstitute(p)
var result: i64 = 0
const font = fc.matching.fontMatch(config, p, &result)
const file = fc.pattern.firstString(font, fc.objects.FILE)
```

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
the `SYS_*` numbers of every system call the repository makes (x86_64's;
the aarch64 backend translates them, so programs keep no table of their
own):

```mlx
pub extern("syscall") fn syscall3(number: usize, a1: usize, a2: usize, a3: usize) -> isize {}
...
pub const SYS_read: usize = 0
pub const SYS_write: usize = 1
pub const SYS_open: usize = 2
```

(`std/bootstrap/os/linux.mlx`)

Beside them it has the kernel's constants as the system calls take them:
errors negated (`EINTR`, `EAGAIN`, ... as `isize`), `O_*`, `AT_FDCWD`,
`F_*`, `S_IF*`, `PROT_*`/`MAP_*`/`MFD_*`, the signal numbers (`SIGHUP` ...
`SIGWINCH`), `SIG_BLOCK`..., `WNOHANG`..., `CLOCK_*`; and thin wrappers
over raw descriptors (`isize`, -errno on failure): `read`, `write`,
`writeAll` (again after `EINTR`), `close` (a negative descriptor is left
alone), `open`, `openAt`, `ioctl`, `fcntl`, `keepOnExec` (clears
`FD_CLOEXEC` for a child), `setNonBlocking`, `dup`, `dup2`, `pipe`,
`seek`, `truncate`, `getdents`, `unlink`, `makeDirectory`, `chmod`,
`chdir`, `mmap`, `mapShared`/`mapAnonymous`/`munmap`, `memfdCreate`, `getpid`,
`exitGroup`, `sleepMilliseconds`, and `failed` (whether a raw result is
an error rather than an address).

It re-exports nine sub-namespaces, each a thin syscall wrapper module or
the kernel's definitions for one kind of device:

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
  `setNonBlocking` (via `fcntl`); Unix sockets by path or abstract name
  (`unixAddress`, `connectUnix`, `listenUnix`), `peerCredentials`
  (`SO_PEERCRED`), and descriptors passed along (`sendWithDescriptors`,
  `receiveWithDescriptors`: `SCM_RIGHTS`, `EINTR` retried, descriptors
  beyond the caller's room closed).
- `os/linux/event.mlx` — raw `poll`, `epollCreate`/`epollControl`/
  `epollWait`, and `io_uring` setup/enter/register wrappers; `poll` records
  in byte arrays (`pollSet`, `pollResult`, `waitReady`); the event loop the
  services share: an epoll set whose descriptors carry a token of the
  caller's (`createEventLoop`, `watch`/`rewatch`/`unwatch`, `waitEvents`
  into `Events`, `eventFlags`, `eventToken`; std.wayland's and std.dbus's
  loops are this one); signals as a descriptor (`signalDescriptor(mask)`
  blocks them, `readSignal`), timers (`createTimer`, `setTimer`), event
  counters (`createCounter`, `addToCounter`, `readCounter`) and inotify
  (`createWatcher`, `addWatch`).
- `os/linux/system.mlx` — `getSystemInfo` (`uname`, filling a `UtsName` of
  six 65-byte fields) and `availableProcessorCount` (via
  `sched_getaffinity`'s returned CPU mask, popcounted by hand).
- `os/linux/process.mlx` — `execve`, `ignoreSignal`/`defaultSignal`
  (`rt_sigaction`), `blockSignals`/`unblockSignals`, `isAlive` (`kill(pid,
  0)`); `fork`, `waitProcess` (`wait4`, `EINTR` retried) with `exitStatus`
  (the code, or 128 plus the signal), `succeeded`, `stopped`,
  `stopSignal`, `continued`; `kill`, `setSession`, `prctl`; signal sets as
  a word (`signalBit`, `changeSignalMask`, `unblockAllSignals`,
  `setSignalHandler`).
- `os/linux/tty.mlx`, `os/linux/fbdev.mlx`, `os/linux/evdev.mlx`,
  `os/linux/drm.mlx` — the kernel's definitions for terminals and virtual
  consoles (`TCGETS`, `TIOCSCTTY`, `KDSETMODE`, `VT_ACTIVATE`, ...), the
  framebuffer (`query` fills a `Screen`), input devices (event records,
  `EV_*`/`KEY_*`/`BTN_*`, the `EVIOCG*` request numbers, `isKeyboard`) and
  KMS (the mode-setting ioctls, PRIME flags, capabilities, formats), which
  mlx-console, mlx-splash and the compositor share.

`tests/359_linux_descriptors_runtime.mlx` (run by
`tests/run_std_modules.sh`) takes a pipe, a timer, a signal and an event
counter through the event loop and poll, passes descriptors over a socket
pair, connects to a Unix socket by path and forks and waits for a child.

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

## URIs (`std.uri`)

**Normative source:** none yet (RFC 3986)

`std/src/uri.mlx` splits a URI reference into its parts (`parse(text,
&uri)`: scheme, userinfo, host with an IPv6 literal's brackets, port,
path, query, fragment, each with whether it is there; a port that is not
digits, a space, a control character or a broken `%XX` is refused),
resolves a reference against a base (`resolve(&base, reference, out)`,
RFC 3986 5.2 with `removeDotSegments`; 0 when it does not fit), writes a
URI back (`write`, `writeNormalized` with the scheme and host in lower
case and a default port left out, `writeTarget` for a request's path and
query, `writeHostHeader`), knows the default ports (`portOf`,
`defaultPort`, `usesDefaultPort`, `hostName`, `sameOrigin`, `isHttp`,
`isHttps`), percent-codes (`decode` with `+` as a space when asked,
`encode` with what to keep, `PATH_ALLOWED`, `QUERY_ALLOWED`) and walks a
query's pairs (`nextPair`). It allocates nothing: a `Uri` borrows its
text. `tests/380_uri_message_runtime.mlx` checks RFC 3986 5.4's
examples among others.

## Public-key arithmetic (`std.crypto.mpi`, `std.crypto.rsa`)

**Normative source:** none yet (RFC 8017 for RSA; `tools/check_gcrypt.sh`
compares the libgcrypt shim, whose limb arithmetic is this module's, with
libgcrypt 1.10)

`std/src/crypto/mpi.mlx` is signed integers of any size, 64-bit limbs in
memory of an `Allocator` (`init`, `deinit`): read from and written as
hexadecimal (`fromHex`, `writeHex`) or big-endian bytes (`fromBytes`,
`toBytes`), `add`, `sub`, `mul` and their `U64` forms, `divRem`
(truncating) and `floorDivRem`, `mod`, `remainderU64`, `compare`,
bits (`bitLength`, `testBit`, `setBit`, `clearBit`, `clearHigh`,
`shiftLeft`, `shiftRight`), `powm` (an odd modulus goes through
std.crypto.bignum's constant-time ladder, so a secret exponent is safe;
`powmPublic` is faster for one that is not), `gcd`, `invm`,
`isProbablePrime` (trial division, then Miller-Rabin with random bases)
and `generatePrime(p, bits, e)` with gcd(p - 1, e) = 1. An operand may
be the result; every operation answers false when memory ran out. The
limb routines (`addLimbs`, `mulLimbs`, `divLimbs`...) are public for
code with limbs of its own.

`std/src/crypto/rsa.mlx` adds to its verification a `PrivateKey`
(`privateKeyInit` from n, e, d and optionally p and q, the CRT's values
worked out and p q = n checked; 6 KB, kept where it was initialized):
`signPkcs1`, `signPss` (a salt given) and `signPssRandom`,
`encryptPkcs1` (`encryptPkcs1With` a padding given) and `decryptPkcs1`,
`encryptOaep` (a seed given) and `encryptOaepRandom` and `decryptOaep`,
and `generateKey(key, allocator, bits, e)`. The private operation runs
by the CRT on std.crypto.bignum and its result is checked with e before
it is given out. Test 384 checks the arithmetic and signatures and
ciphers against values Python computed, and a generated key.

## HTTP (`std.http.*`)

**Normative source:** none yet (the modules follow libsoup's behaviour,
which WebKit's network code expects; `tools/check_soup.sh` compares the C
shim over them with libsoup 3.4)

`std/src/http/` holds HTTP's pieces that do not depend on how bytes move:
`std.http.headers` (header lists, q-values, parameters, the names libsoup
knows), `std.http.date`, `std.http.status`, `std.http.form`,
`std.http.chunked`, `std.http.sniff`, `std.http.psl` (the Public Suffix
List), `std.http.cookie` and `std.http.websocket` (see `std/src/README.md`
for each). Tests 370 and 371 cover them. `std.http.mime` is the other
side of sniffing: the media type of a local file, guessed from the
name's extension (the web's common types, as GIO answers from the
shared-mime-info database, with whether it calls the answer uncertain;
`byName`), from the first bytes (`byData`: signatures, tags, text) or
both (`guess`, g_content_type_guess's rule); test 383.

### Message heads (`std.http.message`)

`std/src/http/message.mlx` reads and writes HTTP/1.1 heads as libsoup
does: `parseRequestLine` (the status a bad one deserves: 400, 505),
`parseStatusLine` (HTTP/1.x and Shoutcast's ICY, the version said even
when the code is bad), `headerLines`/`headerLinesFrom` with `nextHeader`
(continuation lines folded to one space, trailing spaces off, carriage
returns made spaces, lines that are not headers passed over, a head cut
short told by `truncated`), `headEnd` (past the blank line);
`writeRequestLine`, `writeStatusLine`, `writeHeader`, `writeHeadEnd`;
`framingOf` (chunked, Content-Length, to the connection's end, or no
body), `responseHasNoBody`; and the rules of redirects as libsoup
follows them (`redirectOf`: 303, and 302 of an unsafe method, and 301 of
a POST as a GET; 301, 302, 307 and 308 of a safe method as they are;
`isSafeMethod`, `MAX_REDIRECTS`). mlx-soup's soup_headers_parse,
soup_headers_parse_request, soup_headers_parse_status_line and the
session's redirect handler are over it; test 380 checks it.

### The client (`std.http.client`)

`std/src/http/client.mlx` is an HTTP/1.1 client for Mlx programs (the
browser Voyager's), on std.net, std.tls and the modules above. A
`Session` (`init`, `deinit`) keeps up to eight connections by scheme,
host and port and reuses them while they stay open; `request(&session,
&req, &response)` (`get`, `post`) sends a `Request` (method, URL, extra
header lines, a body) and gives a `Response`: the status, version and
phrase, the head as it came (`header(&response, name, folded, &value)`
finds one), the body with gzip or deflate undone (`body`), the URL it
came from and how many redirects were followed (`freeResponse` gives the
memory back). It follows redirects as libsoup does (`std.http.message`,
up to `maxRedirects`), answers a 401's Basic or Digest challenge once
with what the `setCredentials` callback gives for the host and realm and
keeps the credentials for the host's later requests, sends the cookies
of a jar and takes the ones set (`setCookieJar`), rewrites http:// to
https:// for hosts an enforcer knows (`setHsts`), reads bodies framed by
Content-Length, chunks (trailers passed over) or the connection's end,
passes over informational responses, and reads no body for HEAD, 1xx,
204 and 304. https:// runs on std.tls with ALPN http/1.1, the server's
chain verified against the system's CA bundle
(`/etc/ssl/certs/ca-certificates.crt`, loaded when first needed) or a
trust store given (`setTrustStore`), and refused otherwise unless
`insecure` is set. Each wait on the network takes at most `timeoutMs`
(30 s); a body past `maxBody` (64 MiB) is `Error.tooLarge`. The errors
name what went wrong: `badUrl`, `resolve`, `connect`, `tls`, `send`,
`receive`, `malformed`, `tooManyRedirects`, `noMemory`. Everything is
synchronous. `tests/382_http_client_runtime.mlx` runs a session against
a server of its own in a child process on the loopback.

### Authentication (`std.http.auth`)

`std/src/http/auth.mlx` is HTTP authentication (RFC 7235) as libsoup does
it, allocating nothing:

- `challenges(header)` and `nextChallenge(&it, &challenge)` tell the
  challenges of a `WWW-Authenticate` or `Proxy-Authenticate` value apart
  (an item that is a scheme alone, or a scheme and its first parameter,
  starts one); `schemeIs(challenge, "Digest")`, `paramsOf(challenge,
  scheme)`, and `findParam(params, name, out, &length)` (the first of
  that name, unquoted; `Found.bare` for a name alone).
- Basic (RFC 7617): `basicToken(user, password, out)` (the base64 of
  `user:password`, in ISO-8859-1 when every character has a code there,
  as libsoup's `g_convert` makes it; `basicTokenRoom` sizes `out`),
  `writeBasic(sink, token)`, `basicSpace(path)` (the directory a Basic
  realm covers).
- Digest (RFC 7616, MD5 and MD5-sess, `qop=auth`): `digestChallenge(params,
  &d)` reads a `DigestChallenge` (realm, nonce, opaque, domain, algorithm,
  the qop options, stale; false when it cannot be answered), `hexUrp`,
  `hexA1`, `digestResponse(method, uri, &a1, qopAuth, nonce, cnonce, nc,
  &out)` (RFC 2617's request-digest), `clientNonce`, `nonceCountHex` and
  `nonceCountOf`, `writeDigest(sink, &credentials)` (the Authorization
  value as libsoup writes it: `username`, `realm`, `nonce`, `uri`,
  `algorithm`, `response`, `opaque`, `cnonce`, `nc`, `qop`),
  `nextDomainItem` and `withoutTrailingSlash` for a challenge's `domain`.
  A server checks a response with the same `digestResponse`.

`tests/376_http_auth_runtime.mlx` checks RFC 2617's example (Mufasa's
response `6629fae49393a05397450978507c4ef1`), RFC 7617's token, the
challenge splitting and the written values; mlx-soup's `SoupAuthBasic`,
`SoupAuthDigest`, auth manager and auth domains (compiler/libc/soup) are
over it.

### Cookies (`std.http.cookie`, `std.http.cookiejar`)

`std/src/http/cookie.mlx` is a cookie (RFC 6265) as libsoup reads,
writes and matches one. A `Cookie` has a name, value, domain and path
(the last two may be absent), an expiry in seconds since the Unix epoch
(none for a session cookie), Secure, HttpOnly and `SameSite` (none, lax,
strict); it owns its texts (one block of an allocator, `cookieFree`) or
borrows them (`view`). `parseSetCookie(allocator, text, hasOrigin, host,
path, now, &out)` reads a Set-Cookie value with libsoup's rules: Max-Age
and Expires in order, the last one winning; a domain needing a dot past
its first character and getting one before it unless it is an address;
the origin's host having to be in the domain; the default path from the
request's. `write`/`writeSetCookie`/`writeCookieHeader` write it back
(`expires=` in the cookie date form, `path=`, `domain=`, `SameSite=`,
`secure`, `HttpOnly`). `hostMatches`, `pathMatches`, `defaultPath`,
`appliesTo(cookie, https, path, now)`, `equal` (name, value, path),
`isExpired`, `setMaxAge`, `isIpAddress`; `nameAndValue`, `nextAttribute`
and `value` are the syntax alone.

`std/src/http/cookiejar.mlx` is the jar: `init(&jar, allocator)`,
`setCookie(&jar, header, &origin, hasFirstParty, firstPartyHost, now)`
and `addCookie` take a cookie by the jar's `policy` (`Policy.always`,
`never`, `noThirdParty`, `grandfatheredThirdParty`: a third party is one
whose registrable domain, by the Public Suffix List, is not the first
party's; `baseDomain`) and libsoup's rules (no public suffixes, no
Secure cookie from http, SameSite=None only with Secure, the `__Secure-`
and `__Host-` prefixes, no control characters, 4096 bytes), replacing
one of the same name and path or dropping an expired one; `cookiesFor(&jar,
&request, now, &found)` gives those that go with a `Request` (host,
normalized path, https, for HTTP or a script, a safe method, a top-level
navigation, the top-level site and the site for cookies for SameSite),
longest paths first then the oldest, as `writeCookieHeader` lists them;
`deleteCookie`, `next` with an `Iterator`, `count`; `onChanged(&jar,
callback, context)` is told of each change with the cookie before and
after. The Mozilla cookies.txt form (`#HttpOnly_` lines, `TRUE`/`FALSE`
flags, expiry, SameSite last): `loadText`, `writeText`, `parseTextLine`,
`writeTextLine`, and `rewriteText` (what SoupCookieJarText writes after a
change).

`tests/377_http_cookies_runtime.mlx` feeds a jar what mlx-soup's
`tests/cookies.c` feeds one and expects the headers libsoup gives;
mlx-soup's SoupCookie, SoupCookieJar, SoupCookieJarText and
SoupCookieJarDB are over these modules.

### Strict Transport Security (`std.http.hsts`)

`std/src/http/hsts.mlx` is HSTS (RFC 6797) as libsoup's enforcer keeps
it. A `Policy` is a domain, its max-age, its expiry (none for a session
policy) and whether it covers subdomains (`policy(domain, maxAge,
includeSubdomains, now)`, `sessionPolicy`, `policyWithExpiry`,
`isSession`, `isExpired`, `equal`, `own`/`policyFree`);
`parseHeader(value, host, now, &out)` reads a Strict-Transport-Security
value strictly (max-age in digits, quoted or not; includeSubDomains
without a value; no parameter twice). An `Enforcer` (`init`, `deinit`,
`onChanged`) keeps host and session policies: `setPolicy` (an expired
one removes the domain's; a changed one is told), `setSessionPolicy`,
`hasValidPolicy`, `includesSubdomains`, `superDomain`, `mustEnforce`
(the host or a domain above it with a policy covering subdomains),
`policyAt`; its file is a line per host policy (`writeText`, `loadText`,
`parseTextLine`). `tests/378_http_hsts_runtime.mlx` checks the headers
of mlx-soup's `tests/hsts.c` and the enforcer's answers; mlx-soup's
SoupHSTSPolicy, SoupHSTSEnforcer and SoupHSTSEnforcerDB are over it.

### Multipart bodies (`std.http.multipart`)

`std/src/http/multipart.mlx` cuts and writes multipart bodies (RFC 2046)
as libsoup does, allocating nothing: `boundaryOf(contentType, out)` (the
boundary of a `multipart/...` Content-Type), `mediaType`, `paramOf`;
`parts(body, boundary)` and `nextPart(&it, &part)` give each part's
header lines (`headers`, and `raw` from the boundary line on) and bytes,
the preamble skipped, `findBoundary` looking as libsoup looks (a body
without a closing boundary is not `wellFormed`); `writePartStart`,
`writeHeader`, `writeHeadersEnd`, `writeClose`, `writeFormDisposition`
(`form-data; name="..."; filename="..."`), `writeContentType` and
`makeBoundary` (the SHA-256 of 8 random bytes, in hex) write one.
mlx-soup's SoupMultipart is over it (its part stream stays GIO's).

### WebSockets (`std.http.websocket`, `std.http.websocket.deflate`)

`std/src/http/websocket.mlx` has RFC 6455's pieces that do not depend on
how bytes move: `makeKey` (16 random bytes to a Sec-WebSocket-Key),
`validKey`, `acceptKey` and `acceptMatches` (the server's
Sec-WebSocket-Accept, compared case aside), `protocolAsked` and
`anyProtocolAsked` (a Sec-WebSocket-Protocol value split at commas and
spaces, as libsoup splits it), `nextExtension` (the elements of a
Sec-WebSocket-Extensions value: name and parameters), frame headers
(`parseHeader`, `writeHeader`), `applyMask` and `validUtf8`.

`std/src/http/websocket/deflate.mlx` is the permessage-deflate extension
(RFC 7692) as libsoup's: an `Extension` (`init`, `deinit`) configured
for a client or a server from an element's parameters
(`server_no_context_takeover`, `client_no_context_takeover`,
`server_max_window_bits`, `client_max_window_bits`; `parseParams`,
`configure`, the `Error` naming what was wrong and `badName` which
parameter), `REQUEST_PARAMS` and `writeResponseParams`; `outgoing`
compresses a message (a sync flush, its 00 00 ff ff taken off, RSV1 set
on the frame's first byte; control frames and empty payloads pass) and
`incoming` inflates a frame (the tail put back at the message's end,
fragments carried, context takeover or not), into `Bytes` of the
extension's allocator (`freeBytes`).

`tests/379_http_multipart_websocket_runtime.mlx` checks the bodies,
keys (RFC 6455's example), headers and compressed messages; mlx-soup's
SoupMultipart, handshake and SoupWebsocketExtensionDeflate are over
them (SoupWebsocketConnection itself stays GIO's).

## Networking (`std.net`, `std.net.dns`)

**Normative source:** none yet (see the gap below)

`std/src/net.mlx` is TCP over IPv4 and IPv6 on std.linux's socket calls:
an `Address` (family, bytes, port, an IPv6 scope) read from a literal
(`parseAddress`, `parseIpv4`, `parseIpv6` with brackets, an IPv4 tail
and a `%scope`) and written back as inet_ntop writes it
(`writeAddress`), `ipv4(a, b, c, d, port)`, the `sockaddr` both ways;
`connect(&address, timeoutMs)` (non-blocking until connected, then
blocking, TCP_NODELAY set), `listen`, `accept`, `localAddress` (the port
the system picked), `send` (all of it), `receive`, `readable` (with a
timeout), `shutdownWrite`, `close`. `resolve(name, port, found, max,
timeoutMs)` is `std/src/net/dns.mlx`'s: an address literal and
"localhost" answered at once, then `/etc/hosts`, then the name servers
of `/etc/resolv.conf` asked over UDP for A and AAAA records (RFC 1035
messages written and read, compression pointers and CNAMEs followed),
IPv4 addresses first. `tests/381_net_runtime.mlx` checks the addresses,
a connection on the loopback and the DNS messages.

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

`std.net` above is the first such module, without a normative contract
yet (no event-loop API, no UDP beyond the resolver's own). Programs
needing more go directly through `std.os.linux.socket`/`std.os.linux.event`
(Unix-domain sockets, raw `AF_INET`/`AF_INET6` constants, `epoll`, and a
raw `io_uring` setup/enter/register gateway), as
`tests/181_linux_socket_epoll_runtime.mlx` and
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
a `std.testing` module. Some library checks compare an Mlx program
with a system reference instead: `tools/check_crypt.sh` feeds settings
to `tests/support/crypt_hashes.mlx` and compares its hashes with
crypt(3)'s. This chapter does not describe an `expect*` API
surface beyond the four names `testing.xml` gives, since none of them is
grounded in source or a runtime fixture.
