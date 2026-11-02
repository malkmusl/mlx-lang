# brixOS kernel

The start of brixOS's kernel, written in Mlx and built by the repository's
own toolchain (mlx4, mlx-ld; no third-party compiler, linker or library).
brixOS is the Stage 4 target of AGENT_IMPLEMENTATION.md; its normative
requirements are spec/04-stdlib/os/brix.xml:

- native first: its own handles, waits, memory maps, processes and IPC;
- std.posix translates the POSIX semantics programs need onto them;
- a Wayland-capable byte-stream transport with handle passing, shared
  memory export/import and poll/wait integration, so that std.wayland's
  public API stays the same on Linux, BSD and brixOS.

## Where it starts

Nothing of the kernel exists yet. What the toolchain has and lacks:

- mlx4 writes x86_64 ELF executables for Linux (and aarch64, APK); there
  is no `x86_64-freestanding` or `x86_64-brixos` target yet, though
  docs/reference/build-system.md lists both.
- The PE32+ writer of docs/reference/formats.md is the way to an EFI
  loader (ROADMAP.md, phase 7).
- The value arena (mlx4's aggregates in r14/r15) and the bootstrap std
  assume Linux system calls; a kernel needs a freestanding runtime
  without them.

## First steps

1. A freestanding target in mlx4 and std.build: no libc, no system calls,
   the arena set up by the program, a linker script equivalent in
   mlx-ld (load address, sections), an ELF the loader can place.
2. Boot: an EFI application in Mlx (PE32+) that gets the memory map and
   the framebuffer (GOP), loads the kernel ELF and jumps to it; QEMU with
   OVMF as the test machine, a check script under tools/.
3. The kernel's first stage: serial and framebuffer console, GDT/IDT,
   exceptions with a crash report (as std.crash gives one), the physical
   page allocator from the EFI memory map, paging with a higher-half
   kernel.
4. Then: timer and scheduling, user space and system calls, the native
   handles and IPC of spec/04-stdlib/os/brix.xml, std.posix on top.

Each step ends with a check script under tools/, as everything in this
repository does.
