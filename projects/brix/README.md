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
line and runs mlx-sh in user mode on it, and the programs of its boot
archive (the coreutils) as processes of their own:

    ./mlx build brix
    qemu-system-x86_64 -m 512M -cpu max -display none -serial stdio \
        -kernel mlx-out/bin/brix/brix.img
    tools/check_brix.sh        # the same, typed at, checked

    brixOS: kernel at 0x200000, 292 KiB
    brixOS: 511 MiB (415 free), 2076 MHz
    root@brix:~# seq 3 | sort -r | tr "\n" " "; echo
    3 2 1
    root@brix:~# ls -l /cat
    -rwxr-xr-x 1 0 0 233464 Nov  2 20:02 /cat

The pieces:

- **mlx4 `--target=x86_64-freestanding`**: the program's `_start` takes
  its value arena from rdi (base) and rsi (end) instead of mapping one,
  calls `main` and halts; no system call is made by the generated code.
  The code is position independent, so the kernel runs where the boot
  maps it (the higher half).
- **brix-image** (projects/brix/image): one file QEMU (or any Multiboot
  loader) boots. The first MiB is the boot blob, assembled by brix-image
  itself (asm.mlx's `Asm`: bytes, labels, fixups): the Multiboot header, the
  boot info the kernel reads (layout.mlx `Info`), GDT, TSS, IDT, the
  32-bit entry (page tables for the first GiB at 0 and at
  0xFFFF800000000000, long mode), the 64-bit start (higher half, IDT,
  TSS, SYSCALL MSRs, SSE), the SYSCALL entry and a stub per vector. Then
  the kernel's segments (at 2 MiB) and the archive of files
  (`BRIXFS01`: name, offset, size; `NAME=FILE` and `DIRECTORY/`
  arguments, sorted by name), up to the kernel's arena at 64 MiB.
- **The kernel** (projects/brix/kernel), Mlx without std, written as
  types that hold their functions and constants (`Space.map`,
  `Process.load`, `Terminal.read`, `layout.Op.OUTB`); the state is a few
  module variables of those types (the frames, the process, the
  terminal, the clock), reached through functions (`process.current()`):
  - cpu.mlx: mlx4 has no privileged instructions, so the kernel asks the
    blob for them by SYSCALL from ring 0 (`Port`, `Cpu`, `Msr`,
    `BootInfo`: port I/O, CR2/CR3, INVLPG, WRMSR, RDTSC, entering user
    mode); the entry tells the kernel's calls from the program's by the
    return address's half.
  - serial.mlx, console.mlx: `Uart` (COM1, polled) and `Console`
    (`console.out`).
  - memory.mlx: `Page`, `Frames` (physical frames from the Multiboot
    memory size), `Space` (an address space sharing the kernel's half:
    map, translate, back, release, write, destroy).
  - archive.mlx, elf.mlx: the boot archive's files; a static ELF
    program's segments.
  - process.mlx: `Process`: a program from the archive (a static ELF, as
    mlx4 and mlx-ld write them) in its own `Space`, its stack (argv,
    envp, auxv), break, anonymous mappings and stack as `Region`s of
    pages made on first touch (page faults, or the kernel preparing a
    range it is handed), its `Descriptors`, its pid, parent and group,
    what it waits for, and its registers while it does not run.
  - scheduler.mlx: `Scheduler`, the table of processes: fork (the
    touched pages copied, `Space.clone`), the end of a process (a zombie
    until its parent's wait4; SIGCHLD for sigtimedwait), signals that end
    processes (kill, Ctrl-C to the terminal's foreground group), and the
    switch to the next process. There is no timer yet: a process runs
    until it waits, ends or yields. A system call that has to wait
    (wait4, a pipe, the terminal, poll, nanosleep, sigtimedwait) leaves
    the process at its SYSCALL instruction to make the call again when
    what it waits for (`Wait`) holds, so no process needs a kernel stack
    of its own; while none can run, the serial line is polled.
  - pipe.mlx: `Pipe`s (4 KiB ring buffers, their readers and writers
    counted by the descriptors).
  - system.mlx, files.mlx, terminal.mlx, clock.mlx: `System` answers
    Linux's system call numbers (`Number`) and structures for what a
    shell on a terminal needs: read, write, the termios of the serial
    line (`Terminal`: canonical mode edited as input comes, with echo,
    erase, EOF and Ctrl-C, raw mode, ONLCR, ICRNL; the foreground group),
    the window size, fork, execve, wait4, exit, kill, pipe, setpgid,
    mmap/munmap/brk, the archive's files read-only in `/` (open, stat,
    statx, getdents64, lseek), poll, clocks
    (`Clock`: the TSC, its rate measured against the `Pit`, the `Cmos`
    clock), uname, getrandom; signals are accepted and never come. A
    call it lacks is said once on the console (`[brixOS: system call N
    not there]`) and gets ENOSYS.
  - frame.mlx, trap.mlx, shell.mlx: the trap `Frame` (registers saved,
    SSE state kept by FXSAVE in the blob), `brix_trap` (system calls and
    the switches they bring, page faults of a process's regions, a crash
    report for the rest: a faulting program ends with SIGSEGV), and
    mlx-sh as the first process, started again (every other process
    ending) when it exits or faults.

Not there yet: a timer (a program that computes without a system call
keeps the processor; Ctrl-C reaches only what reads the terminal or
waits), signal handlers (signals end a process or are dropped; job
control's stops do not stop), interrupts at all (the PICs are masked;
the serial line and the clocks are polled), a writable file system,
more than one CPU, an EFI loader.

## Next

1. The timer (the APIC's) and preemption; the serial line's interrupt;
   signal handlers (rt_sigaction, delivery, sigreturn) and job control
   (SIGTSTP, SIGCONT).
2. A file system (a RAM disk first, then a disk driver): directories,
   writable files, /proc/self/exe for scripts.
3. Copy-on-write fork (fork copies every touched page now).
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
