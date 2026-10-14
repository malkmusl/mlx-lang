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
| C library | mlxlibc (`projects/libc`): 830 C names, libm and the dynamic loader | glibc, ld.so, libm | runs glibc-linked programs; `libm.so.6` in Mlx; iconv, `strtold` missing |
| C compiler | mlx-cc (`projects/cc`) | gcc, as, ld for C programs | whole programs of C89 to C23 (`-std=`) build and run, on glibc and on mlxlibc alone; zlib, Lua and SQLite pass their tests; separate compilation is next |
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
`projects/cc/README.md`.

1. **C subset, whole programs.** (done) The preprocessor (`#include`,
   object-like and function-like macros with `#` and `##`, conditionals),
   integer types, pointers, arrays, structs, unions, enums, typedefs, all
   statements, all operators, function pointers, calls into the C library
   (variadic ones too), globals with initializers, several `.c` files
   compiled into one program, our own headers for mlxlibc
   (`projects/cc/include`). Check: test programs whose output is known.
2. **The rest of C99/C11 for real code.** (done) Floating point (`float`,
   `double`, SSE), structs passed and returned by value, variadic
   functions defined in C (`<stdarg.h>`), bit fields, compound literals,
   designated initializers, `static` locals, `_Alignof`, `_Static_assert`,
   `inline`, wide characters. Check: a C test suite of our own, then
   suites of well-known small compilers' tests that we may use.
3. **Separate compilation.** Relocatable ELF objects (`-c`), an archiver
   (`mlx-ar`) and a linker (`mlx-ld`): static linking of `.o` and `.a`,
   shared objects (`-shared`, `-fPIC`), `--dynamic-linker` set to mlxlibc
   so a program starts on it without glibc's loader. Check: programs built
   from several objects and libraries, run under mlxlibc alone.
4. **Real projects.** What C programs actually use: the GNU extensions
   that are everywhere (`__attribute__`, `__builtin_*`, statement
   expressions, `typeof`), and `asm` for the few that need it. Targets in
   order: zlib, Lua, SQLite, then a small shell and editor written in C,
   then mlxlibc's own C tests. Check: their own test suites pass.
5. **aarch64.** The same on the aarch64 backend, for Android and ARM
   boards.

## Phase 2: the C library, complete

1. libm (done as far as Lua and SQLite ask: `projects/libc/libm.mlx`;
   `erf`, the gamma functions and the reduction of huge arguments are
   open), `iconv`, `strtold` (a way to return x87 values), locales as far
   as programs ask, wide character functions.
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

GPU drivers (Mesa) and some desktop programs are C++. Either mlx-cc
grows a C++ front end, or we write what we need in Mlx: a Vulkan driver
for one GPU family in Mlx (`docs/VULKAN.md` has the first steps) takes
Mesa out of the picture for that hardware.

## Phase 7: boot and bootstrap

1. Boot without GRUB: the kernel's EFI stub started directly, or an EFI
   loader written in Mlx (the PE32+ target of `docs/reference/formats.md`).
2. A bootstrap without Zig and GCC: today `mlx4` comes from a Zig-built
   seed (`docs/BOOTSTRAP.md`). The seed becomes a small, auditable binary
   (or a chain from one), and the system rebuilds itself on itself.
3. An installable image: MLX/Linux on a USB stick, installed from there.

## How we work

- Every step lands with a check under `tools/` and a README next to its
  code; a step is done when its check passes on a clean checkout.
- Compiler gaps met on the way are recorded as tasks and worked around in
  the meantime, never hidden.
- Nothing from GNU and no third-party library goes into our own code.
  Test tools may use what the machine has (python), and checks compare
  against GNU programs where that proves we behave the same.
