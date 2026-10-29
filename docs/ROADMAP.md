# Roadmap: MLX/Linux without GNU

The goal: a Linux system whose userland is ours. Every program we write is
written in Mlx and needs no C library and no third-party library; the
programs that stay in C run on our C library and are built by our C
compiler; and the system can rebuild itself from source with nothing from
GNU (glibc, gcc, binutils, make, coreutils, bash, grub) on the way. The
Linux kernel stays: it is not GNU, and replacing it is not the goal.

This page says where we are and what comes next, in order. Each step ends
with a check script under `tools/` that proves it, as everything in this
repository does.

## Where we are

| Area | Ours | Stands in for | State |
| --- | --- | --- | --- |
| Language and compiler | Mlx, `mlx4` (self-hosted, x86_64 and aarch64, ELF, APK) | gcc for our own code | done; a few compiler gaps are recorded as tasks |
| Standard library | `std` (system calls, files, formats, crypto, Wayland, Vulkan, UI) | glibc for our own code | done for what the programs use |
| C library | mlxlibc (`compiler/libc`): 830 C names, libm and the dynamic loader | glibc, ld.so, libm | runs glibc-linked programs; `libm.so.6` in Mlx with the long double functions as x87 code; `strtold` to 64 bits; iconv missing |
| C compiler | mlx-cc (`compiler/cc`) | gcc, as, ld for C programs | programs of C89 to C23 (`-std=`, `_BitInt`, `#embed`, x87 `long double`) build and run, on glibc and on mlxlibc alone; `-c` objects, `mlx-ar` archives and the compiler's own linker (gcc's objects link too); zlib, Lua and SQLite pass their tests; shared objects are next |
| Init | mlx-init (`projects/init`) | systemd as PID 1, sysvinit | supervises, mounts, shuts down; no users per service, no device events |
| Shell | mlx-sh (`projects/shell`) | bash, dash | POSIX scripts, line editor |
| Core utilities | `projects/coreutils` (about 70 programs) | GNU coreutils | the common ones; the rest of coreutils is open |
| System bus | MLXIPC (`projects/desktop/ipc`) | dbus-daemon | session and system bus, per-app permissions |
| Network | mlx-netd (`projects/network`) | NetworkManager, wpa_supplicant, dhclient | cables, DHCPv4, WPA2-Personal |
| Sound | MLX Audio (`projects/desktop/audio`) | PipeWire, PulseAudio | own libpulse and libpipewire for C programs |
| Desktop | compositor, dock, launcher, top bar, settings, files, terminal | GNOME, KDE | done; greeter and lock screen open |
| Login | mlx-login (`projects/desktop/login`) | GDM, SDDM, systemd-logind | bus and accounts done; the privileged steps are left to the owner |

What a running system still takes from GNU today: glibc under the
programs we did not write (Mesa's Vulkan drivers, Xwayland, browsers),
gcc and binutils to build those programs and the kernel, make, bash for
the build scripts, GNU coreutils beyond ours, and GRUB to boot.

## Phase 1: the C compiler (mlx-cc)

C is how the rest of the world's programs come to us. mlx-cc is a C
compiler written in Mlx that writes ELF itself: it uses the x86_64
encoder and the ELF writer of the Mlx compiler
(`compiler/selfhost/backend/x86_64/encoder.mlx`,
`compiler/selfhost/object/elf64_dynamic.mlx`), so no assembler and no
linker are needed. Programs it builds link against `libc.so.6`, which
is mlxlibc on our system and glibc elsewhere.

State: steps 1 and 2 are done (`tools/check_cc.sh`), and the C variants
from C89 to C23 with GNU's (`-std=`, C23's keywords, attributes, `auto`,
`constexpr`, fixed enum types); of step 3, `--dynamic-linker` (a program
starts on mlxlibc by itself); of step 4, zlib, Lua and SQLite build and
pass their own tests (`tools/check_cc_projects.sh`), on glibc and on
mlxlibc with its own libm. The gaps left are listed in
`compiler/cc/README.md`.

1. **C subset, whole programs.** (done) The preprocessor (`#include`,
   object-like and function-like macros with `#` and `##`, conditionals),
   integer types, pointers, arrays, structs, unions, enums, typedefs, all
   statements, all operators, function pointers, calls into the C library
   (variadic ones too), globals with initializers, several `.c` files
   compiled into one program, our own headers for mlxlibc
   (`compiler/cc/include`). Check: test programs whose output is known.
2. **The rest of C99/C11 for real code.** (done) Floating point (`float`,
   `double`, SSE), structs passed and returned by value, variadic
   functions defined in C (`<stdarg.h>`), bit fields, compound literals,
   designated initializers, `static` locals, `_Alignof`, `_Static_assert`,
   `inline`, wide characters. Check: a C test suite of our own, then
   suites of well-known small compilers' tests that we may use.
3. **Separate compilation.** (done but for shared objects) Relocatable
   ELF objects (`-c`), an archiver (`mlx-ar`) and the linker, the
   compiler's own (`compiler/selfhost/object/link.mlx`, which links
   gcc's `-fPIE` objects as well): static linking of `.o` and `.a`,
   `--dynamic-linker` set to mlxlibc so a program starts on it without
   glibc's loader. Open: shared objects (`-shared`, `-fPIC`). Check:
   `tools/check_cc.sh` (the test programs from objects and archives,
   under mlxlibc alone), `tools/check_cc_projects.sh` (Lua from
   objects), `tools/check_link.sh`.
4. **Real projects.** What C programs actually use: the GNU extensions
   that are everywhere (`__attribute__`, `__builtin_*`, statement
   expressions, `typeof`), and `asm` for the few that need it. Targets in
   order: zlib, Lua, SQLite, then a small shell and editor written in C,
   then mlxlibc's own C tests. Check: their own test suites pass.
5. **aarch64.** The same on the aarch64 backend, for Android and ARM
   boards.

## Phase 2: the C library, complete

1. libm (done: `compiler/libc/libm.mlx`, from the elementary functions
   to `erf` and the gamma functions, and the long double variants as x87
   code in `x87_long.mlx`; the Bessel functions are open), `strtold` to
   64 bits (done), `iconv`, locales as far as programs ask, wide
   character functions.
2. Static linking: `libmlxc.a` from mlx-cc's archiver.
3. Run what the system needs from C: Mesa's Vulkan drivers and Xwayland
   on mlxlibc instead of glibc (they also need a C++ runtime: see Phase 6).

## Phase 3: build tools

1. `mlx-make`: make's language as projects use it (rules, pattern rules,
   variables, functions, includes, `-j`).
2. `pkg-config` and the small binary tools: `nm`, `strip`, `objdump`,
   `size`, `readelf`.
3. A build description for our own tree that needs no bash: the check
   scripts become mlx-sh scripts or Mlx programs.

## Phase 4: the userland, complete

1. The rest of coreutils, then findutils, grep, sed, awk, diff and patch,
   tar, gzip and xz, less.
2. util-linux's essentials: mount, umount, fdisk, mkfs for ext4 and vfat,
   fsck, losetup, dmesg, kill, su.
3. mlx-sh far enough to run the configure scripts real projects ship.

## Phase 5: the system, complete

1. mlx-init: services as users, ordered shutdown, logs that survive a
   reboot, device events (uevents, coldplug, `/dev` permissions), readiness
   by socket, fstab and fsck.
2. Login: the greeter, the lock screen, the seat (the part of logind the
   compositor uses), the installer for them.
3. Updates and packages: a ports-like tree of build descriptions that
   mlx-cc, mlx-make and mlx-sh build from source.

## Phase 6: C++ and the drivers

GPU drivers (Mesa) and some desktop programs are C++. mlx-cc grows a
C++ front end (`compiler/cc/README.md`, "C++"): namespaces, references,
overloading, classes with constructors, destructors, operators, new and
delete, single inheritance and virtual functions are there (steps 1 and
2), with tests run against g++ (`tools/check_cpp.sh`), and the first
slice of a C++ standard library of our own (`compiler/libcpp`:
`std::string`, `<iostream>`, new and delete, the `c*` headers, linked
in by itself), and templates with `<vector>`, `<map>`, `<set>`,
`<utility>` and `<algorithm>` over them, lambdas, explicit
specializations, member templates, const as part of a type, `<memory>`,
and a small interpreter (`tests/cpp/interp.cpp`) as the larger test;
tinyxml2 builds and passes its 528 tests; partial specializations,
alias templates, variadic templates with fold expressions, `constexpr`
functions and `if constexpr`, rvalue references with move semantics,
the declaration forms of `wtf/Forward.h`, the type trait builtins with
`<type_traits>`, SFINAE, concepts and requires clauses, the atomic
builtins with `<atomic>` and `<mutex>`, template template parameters,
GCC's bit builtins and operand-less `asm` statements, and the standard
headers WTF includes (`<concepts>`, `<bit>`, `<span>`, `<array>`,
`<tuple>`, `<optional>`, `<expected>`, `<functional>`, `<iterator>`,
`<ranges>`, `<compare>` with `<=>`) are read and run as g++'s builds
do. Of WTF's headers `Assertions.h`, `StdLibExtras.h`, `Vector.h`,
`RefPtr.h`, `Ref.h`, `HashSet.h`, `Deque.h`, `Function.h`, `Lock.h`,
`WeakPtr.h`, `ThreadSafeRefCounted.h`, `URL.h` and `text/WTFString.h`,
`text/StringBuilder.h`, `text/AtomString.h` compile (against ICU's
headers), and `HashMap.h`, `Int128.h`, `MathExtras.h`, `Seconds.h`,
`WallTime.h`, `text/StringImpl.h` and `text/StringView.h` with them
(user-defined literals, `__int128`). WTF's `.cpp` files are next:
`<charconv>`, `<system_error>`, `<chrono>`, `<thread>`,
`<condition_variable>` and `<filesystem>` are there for them (run as
g++'s builds in `tests/cpp/threads.cpp` and `filesystem.cpp`), and
the language gaps they showed are closed one by one (member aliases,
inline namespaces, generic callables, SFINAE through alias
templates). Pointers to members, structured bindings, class template
argument deduction, defaulted comparisons, `<string_view>`, a full
`<cmath>`, function pointers as template arguments, inline `asm` with
register operands (`cpuid`) and the C++17/20 forms WTF's headers use
followed: 40 of WTF's 174 `.cpp` files compile (`BitVector.cpp` among
them), with one WebKit patch (`compiler/cc/webkit/`: SIMDe's portable
paths without SSE2). Multiple inheritance (`StringImpl`, `WorkQueue`,
the weak-pointer mixins, with the empty base optimization) followed,
and with it what WTF's `Variant` (mpark's variant) and `Expected` ask:
pack expansions in base lists and braced lists, inherited
constructors, surrogate calls, `decltype`'s value categories, implicit
deduction guides, SFINAE through `void_t` (class and variable
templates), narrowing in braced conversions and constexpr lambdas;
`Variant` builds and runs, and `Assertions.cpp` compiles whole (GCC's
overflow builtins, constexpr calls returning references, the copy
deduction candidate, forwarding references in packs, overloaded member
templates defined outside their class). Errors inside templates name
the instances that led there. The rest of WTF followed: all 174 of its
`.cpp` files compile with mlx-cc (FastMalloc and Threading with
bmalloc's libpas headers among them), the forms each step asked for
kept as `tests/cpp/wtfforms*.cpp`, with two WebKit patches in
`compiler/cc/webkit/`. bmalloc followed: all 168 of its sources
(157 of them libpas's C) compile, which took GNU C's labels as values,
compound literals as static initializers, the typed overflow builtins
and Valgrind's client request. JavaScriptCore is under way (its build
without JIT, the C loop interpreter): its first unified source,
`UnifiedSource-API-1.cpp`, compiles, and a full pass over its 204
compile units is worked through error by error. It took an inline
assembler (`compiler/cc/asm.mlx`, AT&T x86-64 text for `asm` with
operands), guaranteed copy elision of temporaries, functions as
`template <auto f>` arguments, the forms of partial specializations'
members and per-instance member templates, and more of the library
(`move_iterator`, `mem_fn`, the uninitialized algorithms, `unique_ptr`
deleters, `optional`'s ordering). Each JavaScriptCore unit needs some
6 GB in mlx-cc, which bounds how many compile at once. Then come
linking and running `jsc`.
The target is WPEWebKit
(a browser engine of our own is out of reach; a C and C++ compiler of
our own over mlxlibc and mlxlibcpp keeps the whole stack ours), so the
next steps are what its base library WTF needs: `wtf/Vector.h`,
`RefPtr.h`, `HashMap.h` and `text/WTFString.h` compiling (class
template argument deduction, `<=>`, structured bindings, threads among
what they will ask); then what Mesa needs. Once WPEWebKit builds, the
compilers join the standard Mlx distribution with the headers as
builtins, and `importC` in Mlx (C and C++ code as Mlx code) follows. The other way stays open: a Vulkan
driver for one GPU family in Mlx (`docs/VULKAN.md` has the first steps)
takes Mesa out of the picture for that hardware.

## Phase 6b: mlx-cc's code to gcc -O2's speed

Voyager runs, and runs slowly: a profile of its main thread on Google
and YouTube (`mlx-profile`, 2026-10-28) is flat, the top of it bmalloc's
`ALWAYS_INLINE` allocation path, `LayoutUnit`'s accessors,
`OptionSet::containsAny` and `_mm_cmpeq_epi8` as real calls, and 128-bit
arithmetic through `__mlxcc_wide_*`: the compiler, not one hot spot.
JavaScriptCore's JIT tiers (LLInt as assembly, Baseline, DFG, FTL,
WebAssembly) are in, so the C++ is what is left to speed up. The LIR back
end (mlx4's own LIR, optimizer and register allocation, the default
since epoch 44) is the first step: SQLite's workload 2.8 s -> 1.43 s,
bench.js 1.05 s -> 0.75 s. The measure for the rest is a workload's time
against gcc -O2's, not an estimate: the goal is under 2x on native
WebKit work (webcore-dump laying out and painting tests/bench's pages,
jsc on bench.js, SQLite) before anything is called fast.

- [x] Inlining in the LIR lowering: `always_inline` and small functions
  (accessors, operators, intrinsics) of the unit at their call sites
  (epoch 45; `__OPTIMIZE__` defined for libpas's `PAS_ALWAYS_INLINE`,
  epoch 46). jsc's bench.js 0.74 s, webcore-dump bench.html 1.73 s (the
  LIR build before it was not timed on the same page). Checked by eye on
  a LayoutUnit loop (epoch 49): functions returning a reference
  (`operator+=`) are inlined too, `this` and reference parameters are
  aliases of the locals they name, a call that is a statement delivers
  no result, and a local of one scalar member becomes a slot the
  register pass may keep in a register: the loop's body went from 50
  instructions to 22 (gcc -O2: 7). What is left in it is the back
  end's: every intermediate value stored to its frame slot and read
  back, one working register (rax) and three callee-saved ones for
  locals.
- [x] The back end's register allocation (epoch 52): a block's
  intermediate values live in rsi, rdi, r8, r9 and r10 (State.cacheResult
  in `backend/x86_64/codegen/state.mlx`), spilled to their slots only
  before a call or the block's end when read after it; an operand read
  last is operated on in its own register and the result takes it
  over (`x * x`, `extend`, `copy` without code); loads and leas go
  straight into the result's register; the optimizer joins a block
  only fallen into to the block before it (the inliner left one per
  call), so the cache lives across what were block boundaries; mlx-cc's
  functions keep locals in five callee-saved registers (r14 and r15
  too: C code keeps no arena in them), `int`, `unsigned` and byte
  locals among them (the register holds the value extended; a store
  re-extends it, `x = x + 1` with its `extend` done in place); a
  discarded call at the end of a comma (an operator's, its argument
  evaluated before) delivers no result; a function's parameters compete
  with its locals for the registers (registers.parameterHomes, the
  value moved into one on entry), so `this` lives in a register; a
  call's arguments and its address (r11) are loaded as a parallel
  move from wherever they are, the cache's registers included (the
  call reads them before it writes them), and a shift by a constant
  is a shift by an immediate; `p[i]` is a gep the back end makes one
  indexed load, store or lea of ([base + index * 4], an index read
  last scaled in its own register otherwise), a pointer plus a
  constant (a vtable entry's) a gep folded into the access, and a
  load of what the block already loaded (the vtable pointer for two
  virtual calls, a member read twice) a copy of it. The LayoutUnit
  loop's body: 11 instructions (was 22; gcc -O2: 7), with no frame
  access at all; a virtual call: the vtable pointer and two entries
  loaded, two moves and the call. Found on the way: the encoder's rax
  slot cache
  was not reset by `setcc` and the memory-operand ALU forms, and mlx4
  miscompiles an assignment to a tuple's field (`held.0 = true` does
  nothing), so the back end uses separate variables.
- [x] The `extend` after an `int` operation (epoch 53): dropped when
  every reader takes the low 32 bits alone (an operator producing low
  bits from low bits, a store of 4 bytes or fewer, a call's argument,
  which the callee extends on entry); kept before a compare, a shift
  right, a division, an address, a whole-word store or a return.
- [ ] The vtable's adjustment entry read only for a class with more
  than one base; values live across blocks in registers; a function's
  prologue saving only the registers it uses (it does) and no frame
  for a function needing none.
- [x] SIMD intrinsics as their instructions (epoch 47): the SSE, SSE2,
  SSE3, SSSE3, SSE4.1 and SSE4.2 headers over `__builtin_mlxcc_asm`,
  the assembler knowing the packed-integer instructions
  (compiler/cc/README.md, "Inline assembly").
- [x] `__builtin_add_overflow`, `sub` and `mul` of one 4- or 8-byte
  type as the instruction and its flag (epoch 48): a function holding
  one no longer falls back to the stack machine with the wide runtime.
- [ ] 128-bit arithmetic as instructions where x86-64 has them (`mul`,
  add with carry, shifts), the helpers for the rest; `__int128` values
  in the LIR instead of the stack machine.
- [x] The benchmark: tools/bench_native.sh times jsc on bench.js and
  webcore-dump on bench.html (medians; gcc -O2's build of WebKit is not
  in the repository, so its numbers come from outside). Epoch 46: 0.74 s
  / 1.73 s / 0.89 s (without scripts); epoch 48 (SSE instructions, the
  overflow builtins): 0.77 s / 1.67 s / 0.92 s; epoch 51 (reference
  results inlined, parameter aliases, places promoted, struct
  parameters of any size): 0.65 s / 1.73 s / 0.97 s, the layout and
  paint checks green. JavaScriptCore gains, WebCore's layout does not:
  its time is the back end's frame traffic, not calls.
- [ ] Voyager profiled again under the same conditions (the same URLs
  and machine): the main thread, `MlxCompositor`, `MlxGPUPainting`,
  and JavaScript, layout, painting and compositing told apart. Seen on
  bench.js with `JSC_reportCompileTimes=true` (epoch 48): the tiers all
  run (34 compiles, FTL among them), but compiling is where the time
  goes: 958 ms of compile time in an 814 ms run (on its threads), one
  FTL compile of a 416-byte function 142 ms (DFG 56, B3 86), which a
  gcc-built JavaScriptCore does in a few milliseconds. The JIT's own
  C++ (DFG, B3, Air) is the code to make fast, not the JavaScript it
  emits.
- [ ] Zero-copy presentation on a real GPU: the window's buffer a dma-buf
  the compositor imports (it can), instead of wl_shm read back.

## Phase 7: boot and bootstrap

1. Boot without GRUB: the kernel's EFI stub started directly, or an EFI
   loader written in Mlx (the PE32+ target of `docs/reference/formats.md`).
2. A bootstrap without Zig and GCC: today `mlx4` comes from a Zig-built
   seed (`docs/BOOTSTRAP.md`). The seed becomes a small, auditable binary
   (or a chain from one), and the system rebuilds itself on itself.
3. An installable image: MLX/Linux on a USB stick, installed from there.

## After Lattice runs: the whole codebase cleaned up

Once Voyager runs on Lattice (projects/webkit/LATTICE.md), the whole
repository is untangled, not only the translated engine: the compilers
(mlx4, mlx-cc, mlx-ld), std, mlxlibc and mlxlibcpp, the desktop,
the tools and the projects. The code map is the guide (`mlx-codemap
complex`, `copies`, `unused`, `cycles`, `layers`, `workarounds`,
`untested`, `hot`), and the conversion to std's containers, std.heap and
`@noncopy` auto-drop (std → tools → projects → compiler/cc) is finished
on the way. Each cleanup is its own change with every check passing
before and after; workarounds for compiler gaps go once the gap is
fixed, and the shims of the bootstrap (the C libraries WebKit needed,
the GLib and WPE pieces) go once nothing uses them.

## How we work

- Every step lands with a check under `tools/` and a README next to its
  code; a step is done when its check passes on a clean checkout.
- Compiler gaps met on the way are recorded as tasks and worked around in
  the meantime, never hidden.
- Nothing from GNU and no third-party library goes into our own code.
  Test tools may use what the machine has (python), and checks compare
  against GNU programs where that proves we behave the same.
