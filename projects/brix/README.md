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

## What runs

brixOS boots in QEMU (`-kernel`, Multiboot), says itself on the serial
line and runs mlx-sh in user mode on it:

    ./mlx build brix
    qemu-system-x86_64 -m 512M -cpu max -display none -serial stdio \
        -kernel mlx-out/bin/brix/brix.img
    tools/check_brix.sh        # the same, typed at, checked

    brixOS: kernel at 0x200000, 52 KiB
    brixOS: 511 MiB, 2088 MHz
    root@brix:~# echo $((6*7))
    42

The pieces:

- **mlx4 `--target=x86_64-freestanding`**: the program's `_start` takes
  its value arena from rdi (base) and rsi (end) instead of mapping one,
  calls `main` and halts; no system call is made by the generated code.
  The code is position independent, so the kernel runs where the boot
  maps it (the higher half).
- **brix-image** (projects/brix/image): one file QEMU (or any Multiboot
  loader) boots. The first MiB is the boot blob, assembled by brix-image
  itself (asm.mlx: bytes, labels, fixups): the Multiboot header, the
  boot info the kernel reads (layout.mlx `INFO_*`), GDT, TSS, IDT, the
  32-bit entry (page tables for the first GiB at 0 and at
  0xFFFF800000000000, long mode), the 64-bit start (higher half, IDT,
  TSS, SYSCALL MSRs, SSE), the SYSCALL entry and a stub per vector. Then
  the kernel's segments (at 2 MiB) and the archive of files
  (`BRIXFS01`: name, offset, size; `NAME=FILE` arguments).
- **The kernel** (projects/brix/kernel), Mlx without std:
  - cpu.mlx: mlx4 has no privileged instructions, so the kernel asks the
    blob for them by SYSCALL from ring 0 (port I/O, CR2/CR3, INVLPG,
    WRMSR, RDTSC, entering user mode); the entry tells the kernel's calls
    from the program's by the return address's half.
  - serial.mlx, console.mlx: COM1, polled.
  - memory.mlx: physical frames from the Multiboot memory size, page
    tables, address spaces sharing the kernel's half.
  - process.mlx: the program from the archive (a static ELF, as mlx4 and
    mlx-ld write them) in its own address space, its stack (argv, envp,
    auxv), break, anonymous mappings and stack as pages made on first
    touch (page faults, or the kernel preparing a range it is handed).
  - system.mlx, files.mlx, tty.mlx, clock.mlx: Linux's system call
    numbers and structures for what a shell on a terminal needs: read,
    write, the termios of the serial line (canonical mode with echo and
    erase, raw mode, ONLCR, ICRNL), the window size, mmap/munmap/brk,
    the archive's files read-only in `/` (open, stat, getdents64, lseek),
    poll, clocks (TSC, its rate measured against the PIT, the CMOS
    clock), uname, getrandom; signals are accepted and never come.
    A call it lacks is said once on the console (`[brixOS: system call
    N not there]`) and gets ENOSYS.
  - trap.mlx: `brix_trap`, called by the blob's common trap path with
    the frame (registers saved, SSE state kept by FXSAVE): system calls,
    page faults of the program's regions, a crash report for the rest.
    When the shell exits or faults, the kernel starts it again.

Not there yet: processes of their own (fork, execve, wait: mlx-sh says
"cannot fork" for anything but its builtins), interrupts (the PICs are
masked; the serial line and the clocks are polled), a writable file
system, more than one CPU, an EFI loader.

## Next

1. Processes: a process table, fork (copying the address space, later
   copy on write), execve from the archive, wait4 and exit status,
   pipes; then mlx-sh runs programs and pipelines.
2. Interrupts: the APIC timer and the serial line's interrupt, a
   scheduler, sleeping instead of polling.
3. A file system (a RAM disk first, then a disk driver), the
   coreutils (projects/coreutils) in the image.
4. The native handles, waits and IPC of spec/04-stdlib/os/brix.xml, with
   std.posix translating onto them, instead of Linux's numbers.
5. EFI boot (PE32+, GOP framebuffer) beside Multiboot.

## The plan from the start

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
