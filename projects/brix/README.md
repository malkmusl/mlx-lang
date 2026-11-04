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
archive (the coreutils) as processes of their own, on a file system in
memory with brixOS's own layout and copy on write:

    ./mlx build brix           # also mlx-out/bin/brix/disk.img (64 MiB)
    qemu-system-x86_64 -m 512M -cpu max -display none -serial stdio \
        -kernel mlx-out/bin/brix/brix.img \
        -drive file=mlx-out/bin/brix/disk.img,format=raw,if=virtio
    tools/check_brix.sh        # the same, typed at, checked

With `-smp N`, the kernel finds the processors in ACPI's MADT and starts
the others through the local APIC (INIT and startup IPIs naming the
trampoline's page, image/ap.mlx, kernel/smp.mlx): they reach long mode on
the boot page tables, take a number each (the trampoline's counter), go
on in the blob's code64 (the kernel's GDT and IDT, a TSS and a block of
their own, layout.PerCpu, as the kernel's GS base: trap stack, arena,
number) and come into the kernel at brix_ap ("brixOS: 4 processors, 3
more started", "brixOS: processor 1 runs"; tools/check_brix_smp.sh). Up
to Memory.MAX_CPUS (8) run the kernel; more stay halted. Each
processor's arena (mlx4's aggregate arena, 4 MiB of the kernel's 32) is
loaded into r14 and r15 at each entry to the kernel; an idle processor,
which stays in the kernel, releases each round of its loop's
(Scheduler.switchTo: smp.arenaMark, arenaRelease).

The kernel runs on one processor at a time: every entry (brix_trap,
brix_ap) takes the kernel lock (smp.mlx; an interrupt while the kernel
waits on the same processor takes it again) and lets it go on the way
back to user mode or while the processor idles (on the boot page
tables, halted until an interrupt). The processes run beside each
other: a ready process is taken by whichever processor looks first (a
running one by none other: State.running), with its registers, SSE
state, page tables and FS base; a processor keeps its own current
process. The first processor's time slices come from the PIT, the
others' from their local APIC timers (vector 0xF0, calibrated against
the clock at boot); the PIC's interrupts (the serial line, the disks)
reach the first one only, so another processor polls a disk request.
On the way out of the kernel a processor that leaves a process runnable
(ready, or a blocked one whose wait is over) while another idles wakes
that one with an IPI (Vector.WAKE); an idle processor also looks again
at its timer's tick. A process ending or the shell restarting on one
processor leaves the processes running on others alone (they end as
usual).

or from UEFI (OVMF's firmware in QEMU; on a machine, the same file as
EFI/BOOT/BOOTX64.EFI on a FAT partition):

    qemu-system-x86_64 -m 512M -cpu max -display none -serial stdio \
        -bios /usr/share/ovmf/OVMF.fd \
        -drive format=raw,file=fat:rw:mlx-out/bin/brix/esp
    tools/check_brix_efi.sh    # the same, checked

brix-image writes an EFI application when its output ends in .efi
(image/efi.mlx): a PE32+ file with the Multiboot image in it and a
loader in front that does a Multiboot loader's part (the memory map's
end below 4 GiB as mem_upper, ExitBootServices, out of long mode, the
image to 1 MiB, the blob's entry). The console stays the serial line
(no GOP framebuffer yet).

    brixOS: kernel at 0x200000, 292 KiB
    brixOS: 511 MiB (415 free), 2076 MHz
    root@brix:~# seq 3 | sort -r | tr "\n" " "; echo
    3 2 1

The programs in the image are built for brixOS (`--target=x86_64-brixos`,
into mlx-out/bin/brix): `@import("builtin").os` is `brixos` in them, so a
program asks at compile time which system it is for (`uname -o` says
brixOS), and std picks its `X.brixos.mlx` files (std.platform). They
still call the kernel by Linux's system call numbers.
    root@brix:~# ls /
    devices  system  temporary  users
    root@brix:~# cd /temporary; cat /system/programs/mlx-sh > a
    root@brix:/temporary# stat -f -c "%f free" .; cp a b; stat -f -c "%f free" .
    37627 free
    37617 free

## The system on the disk

The boot image holds the kernel and an archive (mlx-sh, the tools, the
coreutils) that the first boot runs from. `install` in brixOS puts the
system onto a brixfs volume (`brixfs format /devices/disk0; install`:
/system, /users and /temporary copied to /volumes/disk0, committed), and
from then on the kernel boots from the disk: a volume with
system/programs/mlx-sh on it is the system, its /system, /users and
/temporary are the tree's (vfs.graft: the same nodes as
/volumes/disk0's; the memory-only ones go), programs run from the disk
and what is written stays. The boot archive's /system is at
/volumes/boot then (`install` installs from there again). "brixOS: /
from /volumes/disk0 (the boot archive at /volumes/boot)" says so;
tools/check_brix_system.sh does it, boots again twice and reads what it
wrote. /devices and /volumes stay the memory-only tree's.

## The layout

brixOS does not use Unix's directory layout (the FHS); its directories
are named for what they hold:

    /system/programs   the programs (the boot archive; read-only bytes,
                       copied into pages of their own when written)
    /system/settings   the system's settings: users, groups
    /devices           console, tty, null, zero, disk0, disk1, ... (the
                       virtio disks)
    /users/root        the user's home (the shell starts there)
    /temporary         scratch space
    /volumes           each disk's volume (diskN: brixfs, or ext2
                       read-only) and the brixfs volumes' snapshots

Linux programs (all the repository's programs are, for now) expect some
of Unix's paths. The system call layer, not the tree, maps them:
`/tmp` → /temporary, `/dev` → /devices, `/etc/passwd` and `/etc/group`
→ /system/settings/users and groups, `/proc/self/exe` → the program the
process runs. `ls /` shows only brixOS's own directories.

Besides files and directories the tree holds symbolic links (a link's
target is its data; `Vfs.walk` follows links in the middle of a path,
and the last one unless lstat, readlink, O_NOFOLLOW or O_CREAT|O_EXCL
ask for the link; 40 at most, then ELOOP) and FIFOs (mkfifo, mknod: a
pipe while one is open; a read waits for the first writer, a write for
the first reader; O_RDWR waits for no one). Both are stored on brixfs too.

All 73 programs of projects/coreutils are in the boot archive;
tools/check_brix.sh runs each, about 180 invocations with their options
(tools/brix_programs.sh), in brixOS and on the host and compares what
they print.
It also runs projects/brix/tools/floats (a long computation in the SSE
registers) twice at once: both must print the host's result, so each
process keeps its registers across the timer's preemptions; and
projects/brix/tools/signals, a line for each step of what signals do
(handlers, masks, sigtimedwait, EINTR and SA_RESTART, sigsuspend, a
fault on the alternate stack), which must print what it prints on the
host.

## Copy on write

A physical page counts who holds it (`Frames.share`, `Frames.free`):

- fork shares every page of the parent with the child, read-only and
  marked copy-on-write in both; the first write (by the program, or by
  the kernel for a system call: CR0.WP is on) copies the page, or takes
  it back writable when it is the last holder (`Space.copyOnWrite`).
- Files share pages: copy_file_range and the FICLONE ioctl share whole
  pages between files (`Vfs.copyRange`), and a write to a shared page
  copies it first (`writableBlock`). std.fs.copy (cp, and every copy of
  std) tries copy_file_range first, which on Linux gives reflinks on
  btrfs and XFS. A copy of a 570 KB file costs a few pages instead of
  139.
- The archive's programs are pages of the boot image until written.

## Snapshots

`snapshot SOURCE DESTINATION` (projects/brix/tools/snapshot, in
/system/programs) makes a copy of a tree at once, whatever its size: new
directories, files that share every page with the originals
(`Vfs.cloneTree`); a write to either side copies the page. `-r` makes it
read-only (changes fail with EROFS), `-d SNAPSHOT` takes one out. The
copy is built before anything names it, so a snapshot may go inside its
source: `snapshot -r / /users/root/before`. Restoring is a snapshot back.
The tool calls brixOS's own system calls 1000 (SNAPSHOT) and 1001
(REMOVE_SNAPSHOT), past Linux's numbers. (Hard links inside a tree become
separate files in its snapshot.)

## The disks

The virtio block devices (QEMU's `-drive ...,if=virtio`, up to four) are
found on PCI bus 0 at boot (`Pci`, pci.mlx), in the bus's order, and are
/devices/disk0, /devices/disk1, ..., block devices of their sizes:
programs read and write them at any byte offset (`head -c 9
/devices/disk0`, `echo text > /devices/disk1`); what is written is on the
disk image. Each disk's volume is mounted at /volumes/diskN (a brixfs
volume, read and written, or an ext2 one read-only); the first with a
system on it becomes / (below). A volume on another disk is formatted
and used alike (`brixfs format /devices/disk1`, then /volumes/disk1;
`sync` commits every volume); disks may share an interrupt line. A
disk with a GPT (block.mlx reads it: the header's and the entries'
CRC32s checked) is /devices/diskN with its partitions /devices/diskNpM
(from 1), each a volume of its own at /volumes/diskNpM (the disk itself
is then no volume); `brixfs partition /devices/disk3 2 1` writes a GPT
with partitions of those sizes (MiB) on a device or an image (a boot
later sees them).
tools/check_brix_disks.sh boots with three disks (a brixfs system, an
empty one formatted and written in brixOS, an ext2 image). The driver (`VirtioBlock`, virtio.mlx)
uses the legacy PCI transport (registers in an I/O BAR), one virtqueue in
pages of its own, one request at a time through a bounce page; the
processor halts until the device's interrupt (its PCI interrupt line, the
PIC's IRQ 11 in QEMU, taken back through the ISR register) says it is
done ("brixOS: disk0 answers by interrupt" at boot). The process that
asked still waits inside the kernel meanwhile: no other one runs until
the request is done.
While a volume is mounted from it, a /devices/diskN is not writable.

## The network

A virtio network device (QEMU's `-netdev user,id=n0 -device
virtio-net-pci,netdev=n0`) is /devices/net0 (`VirtioNet`,
virtionet.mlx: the legacy transport, a receive queue with buffers posted
ahead and a transmit queue, on a virtqueue of their own, virtqueue.mlx);
"brixOS: /devices/net0, 52:54:00:12:34:56 (virtio, IRQ 11)" at boot.
The kernel's stack (kernel/net) runs on it:

- stack.mlx: Ethernet, ARP (a table of the neighbours, a request when
  one is missing: the packet is dropped and the sender tries again),
  IPv4 (one address, netmask and gateway; no fragments) and ICMP (echo
  answered). The addresses are QEMU's user network's (10.0.2.15/24,
  the gateway 10.0.2.2, the name server 10.0.2.3) unless
  /system/settings/network says otherwise (`address`, `netmask`,
  `gateway`, `nameserver` lines); the kernel writes
  /system/settings/resolv.conf for programs that look names up
  ("brixOS: net0 is 10.0.2.15, gateway 10.0.2.2, name server
  10.0.2.3").
- udp.mlx, tcp.mlx: datagrams; connections (active and passive open,
  segments of MSS bytes within the peer's window from a send ring,
  sent again after a timeout that doubles, data received in order into
  the socket's ring and acknowledged at once, FIN both ways, RST; no
  selective acknowledgement, no slow start).
- sockets.mlx, api.mlx: the sockets a process's descriptors name
  (files.Kind.socket; 32 at most, a 16 KiB ring each), and what a call
  does to one. A call that has to wait leaves the process waiting for
  the socket to change (WaitKind.socket), as reads of pipes do.
- linux/socketcalls.mlx: Linux's socket calls (socket, connect
  (EINPROGRESS and SO_ERROR for a non-blocking one, as std.net makes
  them), bind, listen, accept, sendto, recvfrom, shutdown, the names and
  options) on AF_INET: std.net, std.net.dns (resolv.conf and hosts at
  std.platform's paths) and std.http.client run unchanged, so does
  wget (projects/wget: `wget URL [-O FILE]`, http and https).

- dhcp.mlx: at boot, unless the settings give an address, DHCP asks
  QEMU's user network (DISCOVER, REQUEST; the ACK's address, netmask,
  router and name server; "(DHCP)" on the boot line; the lease is not
  renewed). settings.mlx reads the network file and writes resolv.conf.
- native/sockets.mlx: brixOS's own socket calls (spec calls 39 to 46:
  socket_open of a stream, datagram or ping socket, bind, connect,
  listen, accept, send, receive, info; std.os.brix's socketOpen and
  the rest). A call never waits: it says would_block and the program
  waits on the handle (READABLE, WRITABLE, PEER_GONE, ENDED). A packet
  to our own address goes through the stack at once (a loopback), so a
  program may talk to itself.
- ping (projects/ping): `ping HOST [COUNT]` on a datagram socket of
  IPPROTO_ICMP (the kernel sets the identifier; Linux's ping sockets
  work the same).

tools/check_brix_net.sh boots with the user network: DHCP, ping to the
gateway, wget fetching files (a small one, a 400 KiB one, a missing one)
from an HTTP server on the host, and the native calls (a stream to
ourselves, a datagram, an echo). Not yet: IPv6, a lease's renewal.

## brixfs: the disk's format

brixfs (projects/brix/brixfs.mlx, the format; kernel/volume.mlx, the
kernel's side; tools/brixfs, the tool) is copy on write: nothing in use
is written over.

- Blocks of 4 KiB. Blocks 0 and 1 are superblocks taking turns; the
  valid one (its checksum right) with the higher generation is the
  volume. It names the inode table's root block, the next inode number
  and the snapshot table.
- The inode table: a root block of 512 inode block numbers, 16 inodes of
  256 bytes each (format version 2). An inode names its owner and group.
  A file's data is named by extents (a run of blocks: where in the file,
  how many, where on the disk): 12 in the inode, else in a block of 256
  (depth 1) or in up to 512 such blocks one block names (depth 2), so a
  file written in one go is one extent; a directory's data: 64-byte
  entries (inode, name); a symbolic link's: its target; a FIFO has none.
- A commit writes what changed into free blocks: of a changed file the
  pages written since the last commit (each page of the tree carries a
  changed mark), the rest keeps its blocks, and its extents are made
  again (`Store.writeFile`: an extent goes on while the next block
  follows the last on the disk); a changed directory whole; the inode blocks with changed
  inodes, a new table root; then, between two flushes of the device
  (virtio's FLUSH when it offers it), the other superblock with the next
  generation. Until that one block is written the disk is the last
  commit, whole; after it, the new one.
- Free blocks are those no root reaches (the current one, the
  snapshots'): marked again at mount and after each commit. The same
  sweep notes the current table's free inode numbers; new files take the
  lowest of them before a number never used.
- A snapshot is the current root kept in the snapshot table:
  `snapshot -r /volumes/disk0 /volumes/NAME` commits, keeps the root and
  mounts it read-only at /volumes/NAME; `snapshot -d /volumes/NAME`
  takes it out (its blocks are free unless the volume still uses them).

In the tree, /volumes/disk0 is the current state. Nodes are read from
the disk when first used (`Vfs.ready`: a file's data at once, a
directory's entries as nodes not read yet); a change marks a node
(`Vfs.touch`); a node made in the tmpfs and linked into the volume
becomes the volume's (`Vfs.adopt`); moves between the volume and the
rest of the tree are EXDEV. Commits happen when the system is idle (every
process waits, e.g. at the shell's prompt) and at sync, fsync, syncfs.
The tree's nodes come in page-sized chunks made as they are needed (no
fixed count; `Vfs.grow`), and the tree is a cache: after an idle commit
(`Store.trim`), while more nodes are in use than its budget (one for
every 256 KiB of memory, 512 at least), clean directories read from the
disk are unloaded (the nodes they name freed, subdirectories first;
nothing open, changed or linked twice goes), and while less than a
quarter of memory is free, clean files' pages are dropped; both are read
again when used. stat
gives a volume node's inode number and the volume as its device.

    brixfs format /devices/disk0     on brixOS (it is mounted at once), or
    brixfs format disk.img           on the host (mlx-out/bin/brixfs)
    brixfs create disk.img 64        a formatted 64 MiB image (one that
                                     is there is kept; ./mlx build brix
                                     makes mlx-out/bin/brix/disk.img so)
    ext2 make disk.img 8 DIRECTORY   an ext2 image of a host directory
                                     (mlx-out/bin/ext2): brixOS mounts
                                     an ext2 disk read-only at its
                                     /volumes/diskN (kernel/ext2.mlx;
                                     Linux's images too, 1-4 KiB blocks)
    brixfs list disk.img             every path and size, the snapshots
    brixfs cat disk.img docs/a       a file
    brixfs blocks disk.img docs/a    a file's block numbers (0: a hole)
    brixfs check disk.img            the volume checked (the table, every
                                     inode's extents in range and no
                                     block in two files of one root,
                                     entries naming inodes that are
                                     there, link counts; the snapshots'
                                     roots too); also on brixOS, on
                                     /devices/diskN while mounted
    brixfs repair disk.img           checked, link counts mended in a
                                     new commit

A directory of 64 entry slots or more is looked up through an index
made at its first lookup (`Vfs.buildIndex`: hash slots in frames, two
for each entry slot, probed in order), kept up as entries are added,
and dropped with the directory's entries (unloaded, or when a chain
grows long: made again at the next lookup). The entries on the disk
stay a list.

Users: a process runs as a user and a group (0, the system's, at boot;
setuid and setgid, which only the system's user may use to become
another: `as USER[:GROUP] PROGRAM` of the tools does), new nodes are
theirs, chown changes that (an owner only the group), and the kernel
checks modes (`Vfs.permits`: read, write, execute for the owner, the
group, the rest; the system's user may everything but run what no bit
lets anyone run) when a path is walked (execute on each directory), a
node is opened, made, removed, renamed, run, or its mode and times are
changed (the owner's); the Linux layer says EACCES or EPERM, brixOS's
own calls denied. Owner and group are on the disk with the inode.

Not there yet: a directory is still written whole when an entry changed;
nodes held open or with more than one link stay in memory; no journal of
small writes between commits (a crash loses what came after the last
one, never more).

The pieces:

- **mlx4 `--target=x86_64-freestanding`**: the program's `_start` takes
  its value arena from rdi (base) and rsi (end) instead of mapping one,
  calls `main` and halts; no system call is made by the generated code.
  The code is position independent, so the kernel runs where the boot
  maps it (the higher half).
- **brix-image** (projects/brix/image): one file QEMU (or any Multiboot
  loader) boots. The first MiB is the boot blob: its code is AT&T
  assembly (blob.mlx, ap.mlx, efi.mlx) assembled by mlx-cc's assembler
  (compiler/cc/asm.mlx, through assemble.mlx: the layout's numbers as
  `.set` constants, the parts at their offsets by `.org`, the kernel's
  _start, brix_trap and brix_ap by relocation), its tables written by
  brix-image: the Multiboot header, the boot info the kernel reads
  (layout.mlx `Info`), GDT, TSSs, IDT, the 32-bit entry (page tables for
  the first GiB at 0 and at 0xFFFF800000000000, long mode), the 64-bit
  start (higher half, IDT, TSS, SYSCALL MSRs, SSE), the SYSCALL entry
  and a stub per vector. Then
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
  - serial.mlx, console.mlx: `Uart` (COM1: output polled, input by its
    receive interrupt) and `Console` (`console.out`).
  - interrupts.mlx: `Irq`, the two 8259 PICs moved to vectors 32 to 47
    with only the timer (the PIT's channel 0, 100 Hz) and the serial
    line unmasked; end of interrupt, spurious IRQ 7 and 15. The kernel
    runs with interrupts off (trap gates and SYSCALL clear IF): they
    come in user mode, or while the idle loop halts.
  - memory.mlx: `Page`, `Frames` (physical frames from the Multiboot
    memory size, with the count of their holders), `Space` (an address
    space sharing the kernel's half: map, translate, back, release,
    write, clone sharing its pages, copyOnWrite, destroy).
  - vfs.mlx: `Vfs`, the file system: a tree of `Node`s in memory (a
    tmpfs). A file's data is in pages (12 direct, an indirect page, a
    double indirect one; shared between files, copied on write), a
    directory's in 64-byte entries; a node lives while a directory names
    it or a descriptor holds it. The tree at boot is the layout above.
  - linux/filecalls.mlx: `FileCalls`, the calls on paths (relative to the
    working directory or a directory's descriptor): open/creat with
    O_CREAT, O_EXCL, O_TRUNC, O_APPEND, stat/statx, access, getcwd,
    chdir, mkdir, rmdir, unlink, rename, link, truncate, chmod, utimensat,
    statfs, getdents64, copy_file_range; `Compat`, Linux's paths onto
    brixOS's; /proc/self/exe (a shell runs scripts by it).
  - archive.mlx, elf.mlx: the boot archive's files; a static ELF
    program in any file of the file system, its segments copied into an
    address space.
  - process.mlx: `Process`: a program from the archive (a static ELF, as
    mlx4 and mlx-ld write them) in its own `Space`, its stack (argv,
    envp, auxv), break, anonymous mappings and stack as `Region`s of
    pages made on first touch (page faults, or the kernel preparing a
    range it is handed), its `Descriptors`, its pid, parent and group,
    what it waits for, and its registers while it does not run.
  - scheduler.mlx: `Scheduler`, the table of processes: fork (the
    pages shared copy-on-write, `Space.clone`), the end of a process (a zombie
    until its parent's wait4; SIGCHLD to it), signals sent (kill, Ctrl-C,
    Ctrl-\\ and Ctrl-Z to the terminal's foreground group: pending until
    the process runs), job control (a process stopped by SIGSTOP, SIGTSTP,
    SIGTTIN or SIGTTOU waits until SIGCONT; its parent sees it with
    wait4's WUNTRACED and WCONTINUED; a group behind that reads the
    terminal gets SIGTTIN), and the switch to the next process: when it waits, ends or yields, or when
    the timer's interrupt comes in user mode (its time slice, 10 ms, is
    over). A system call that has to wait
    (wait4, a pipe, the terminal, poll, nanosleep, sigtimedwait) leaves
    the process at its SYSCALL instruction to make the call again when
    what it waits for (`Wait`) holds, so no process needs a kernel stack
    of its own; while none can run, the processor halts until an
    interrupt. Each process keeps its SSE registers (FXSAVE's image)
    while it does not run; a new program starts with the default ones.
  - pipe.mlx: `Pipe`s (4 KiB ring buffers, their readers and writers
    counted by the descriptors); files.mlx: `Descriptors` (of nodes or
    pipe ends, with their access mode, O_APPEND, close-on-exec).
  - linux/system.mlx, files.mlx, terminal.mlx, clock.mlx: `System` answers
    Linux's system call numbers (`Number`) and structures for what a
    shell on a terminal needs: read, write, the termios of the serial
    line (`Terminal`: its input queued as the interrupt brings it, Ctrl-C,
    Ctrl-\\ and Ctrl-Z taken at once, so they reach a program that never
    reads; canonical
    mode edited as input comes, with echo, erase and EOF, raw mode,
    ONLCR, ICRNL; the foreground group),
    the window size, fork, execve, wait4, exit, kill, pipe, setpgid,
    umask, mmap/munmap/brk, read/write/lseek on files, poll, clocks
    (`Clock`: the TSC, its rate measured against the `Pit`, the `Cmos`
    clock), uname, getrandom, signals. A call it lacks is said once on the console (`[brixOS: system call N
    not there]`) and gets ENOSYS.
  - signals.mlx: `Signals`, a process's actions (rt_sigaction), mask
    (rt_sigprocmask, rt_sigsuspend), pending signals with their senders,
    alternate stack (sigaltstack). Delivered on the way back to user
    mode: the default action (ignore, stop, or end the process; the shell
    is ended only by its faults and never stops), or the handler on a frame as Linux builds
    it (rt_sigframe: the return to the restorer, the ucontext with the
    registers and the mask, the siginfo, the SSE registers), which
    rt_sigreturn takes back. A system call the process waited in ends
    with EINTR, or is made again with SA_RESTART; pause and sigsuspend
    end with a handler. A fault raises its signal (SIGSEGV, SIGILL,
    SIGFPE, ...), which a handler can take (std.crash's do), else the
    program ends with a report.
  - pci.mlx, virtio.mlx: PCI's configuration space, the virtio disk;
    volume.mlx: `Store`, the brixfs volume (mount, read, commit, sweep,
    snapshots).
  - frame.mlx, trap.mlx, shell.mlx: the trap `Frame` (registers saved,
    SSE state kept by FXSAVE in the blob below it: `Frame.sse`, `Sse`),
    `brix_trap` (system calls and the switches they bring, the timer's
    and the serial line's interrupts, page faults of a process's regions, a crash
    report for the rest: a faulting program ends with SIGSEGV), and
    mlx-sh as the first process, started again (every other process
    ending) when it exits or faults.

Not there yet: orphaned process groups (a stopped one whose parent
ended stays stopped), TOSTOP's SIGTTOU, other processes running while
the disk works for one, the APIC (the legacy PICs do), groups a user is
in beyond its own, the kernel on more than one processor (the others start
and wait halted).

## brixOS's own interface

brixOS is a Unix in what a person sees, but its kernel interface is its
own (spec/04-stdlib/os/brix.xml): handles with rights instead of
descriptor numbers, one wait over the states of handles with a deadline,
reads and writes that name their position (no cursor in the kernel),
spawn and branch (a copy-on-write copy of a process, as snapshots copy
trees) instead of fork alone, a queue of events and one upcall per
process instead of signal numbers, handlers and masks, groups and
sessions as objects, its own status codes. Its calls are SYSCALL with
NATIVE_BASE (0x4000) + n; Linux's numbers below that are a translation
layer (kernel/linux: system.mlx, filecalls.mlx, errno.mlx) for the
programs built for Linux, and std.posix will be a facade over the native
calls.

A program starts with a start block (spec call 20, 22 and 47): its
arguments and environment (on its stack, as Linux lays them out) and its
handles by role, records of a role and a handle: input 1, output 2,
errors 3, directory 4 (a directory handle: the working directory at the
start), session 5 (the terminal; the first shell's 0 is one), a
spawner's own from 16. process_spawn takes the block and the handles the
child gets (a role names one of them); process_replace takes a block
naming the caller's own; a branch copies the roles; process_start gives
a program its roles as (role, handle) records (std.os.brix: `Start`,
`RoleRecord`, `Role`, `startRoles`, `handleFor`). std.spawn and mlx-sh
pass input, output, errors and their session on; the native tool prints
what it got (tools/check_brix.sh).

What there is: kernel/native (calls.mlx, objects.mlx, processes.mlx,
nodes.mlx, streams.mlx): handles (close, duplicate, info), wait, read and
write (at a position or streaming), nodes (open with read, write,
create, exclusive, truncate; info; make a directory, a FIFO or a link;
remove; list), streams (a pipe's two ends), channels (messages of bytes
and handles, which move from the sender's table to the receiver's;
kernel/channel.mlx), memory (map, unmap), processes (spawn a program
with exactly the handles given, branch, replace, exit, take a child's
change, send an event; a process handle's state says when it ended or
changed), events (claim, take, the upcall), the clocks, rename and
snapshots of a tree by its handle (kernel/names.mlx, the core's, which
the Linux layer's rename and SNAPSHOT use too), timers (kernel/timer.mlx:
readable when expired, a read takes the count; native/timers.mlx),
memory objects (kernel/memoryobjects.mlx: pages mapped shared, an entry
bit keeps them shared through a branch), files mapped (node_map: the
file's own pages, copy on write, so every view and the file share them
until one writes), groups (group_join; a group
handle in front of the session, an event to all of it). Native programs
see brixOS's own layout (/temporary; /tmp is the Linux layer's).
std.os.brix for programs; projects/brix/tools/native, events, objects
and kinds show them (tools/check_brix.sh compares their lines). A handle names a slot of the process's table and
the generation the slot had ((generation << 8) | (slot + 1)): a handle
kept past a close stays invalid when the slot is used again. The Linux
layer's descriptors are the slots' numbers, its view of the same table
(handle_of_descriptor gives a descriptor's handle while std moves over).
The core answers in its own Status (status.mlx); the Linux layer turns
it into errno at its edge (linux/errno.mlx).

Events are the core's (events.mlx): Ctrl-C at the session is an
interrupt, a child that changed a child event, a fault a fault event,
with a detail; a kind the program has not claimed takes its default
(end, stop, go on, nothing), a claimed one waits in the process's queue
(events_take) or runs the one upcall (upcall_set; the registers and SSE
registers in a context record on its stack, upcall_return goes back; a
wait it interrupts ends with interrupted). How a process ended or
stopped is an exit code or an event. The Linux layer's signals are a
view of these (linux/signals.mlx): a signal number is an event with the
number as its detail, so it comes back as sent; handlers, masks and
rt_sigframes are the Linux layer's, and wait4's status words its
encoding. A process that has claimed nothing and set no upcall sees its
events that way. projects/brix/tools/events shows the native side.

std on brixOS: a program built with --target=x86_64-brixos reads and
writes its files by brixOS's own calls (std/bootstrap/fs.mlx and
time.mlx go through ./os/host.mlx, which is host.brixos.mlx there: a
platform's file, not `if comptime`, so the compiler's own sources stay
buildable by older compilers and mlx1): std keeps descriptor numbers,
remembers each one's handle and asks again when a close or dup2
made it stale; the clocks are clock_read; _start ends the program by
process_exit (the compiler's x86_64 backend). The native statuses reach
std's callers as the errno values they check, until std's own error
sets replace them. A streaming read of the session from a group behind
gets READ_BEHIND, as the Linux layer's read gets SIGTTIN. Built for
Linux, the same programs keep Linux's calls (the code is the same byte
for byte; tools/check_brix.sh runs the host's side with those builds).
Opening (node_open, its handle's slot the descriptor; flags it has no
word for go to the Linux layer) and reading directories (node_list's
entries as getdents64 records, "." and ".." first, a cookie per
descriptor) are native too; fork/exec still go through the Linux
layer.

## The compiler in brixOS

brixOS builds its own compiler. mlx4 built for brixOS
(`--target=x86_64-brixos`, mlx-out/bin/brix/mlx4) is in the boot
archive, as is tar (projects/tar: tar archives made, listed and
unpacked, plain or gzip, on std.tar and std.flate.gzip). The compiler's
sources come in as an archive: on the host

    mlx-out/bin/tar -czf mlx.tar.gz compiler/selfhost std/src std/bootstrap mlx-out/version.mlx

(the compiler needs std/src beside its working directory and the version
module), served by any HTTP server; in brixOS

    cd /volumes/disk0
    wget http://10.0.2.2:8000/mlx.tar.gz -O mlx.tar.gz
    tar -xzf mlx.tar.gz
    mlx4 -o hello hello.mlx && ./hello
    mlx4 -o mlx4-2 compiler/selfhost/main.mlx && ./mlx4-2 --version

mlx4's target without --target is the system it runs on (brixOS there),
so what it builds runs in brixOS. The compiler it builds is byte for
byte the one the host builds for brixOS. tools/check_brix_selfhost.sh
does this with QEMU (1 GiB, two processors, a 256 MiB brixfs disk; the
self-build takes a few minutes without KVM) and compares the archive's
members, a file's bytes, the program's output and the compilers' bytes.

`mlx build` runs in brixOS too: mlx4's build command compiles
mlx-out/build/runner.mlx (build.mlx on std.build) and runs it, and the
runner starts its jobs as processes (std.spawn's native path, by node
handles) and waits for them by their process handles; without
/proc/meminfo the jobs are limited by -j alone, and without a home
directory the result cache is off. build.mlx leaves WebKit's steps out
where projects/webkit/upstream is missing, so the part of the repository
the brix group needs (build.mlx, compiler/selfhost, the assembler's
asm.mlx and base.mlx, std, projects/brix and the programs in the image,
projects/webkit/*.mlx, tools/build-number) is enough:

    cd /volumes/disk0 && tar -xzf mlx.tar.gz && mlx4 build brix -j 2

builds the kernel, brix-image, the tools, the coreutils and the boot
image there. tools/check_brix_build.sh does it (87 steps, 3 to 4 minutes
in QEMU without KVM) and compares the kernel and the boot image with the
host's build from the same archive: byte for byte the same.

## Next

1. Programs loaded through node_map (their text shared);
   files mapped shared and writable (the page cache telling a view's
   holds from a snapshot's); a framebuffer console
   (GOP's) for EFI boots; the kernel on the
   other processors (per-processor stacks, TSS and GDT, a lock, the local
   APIC's timer, IPIs); ext2 written too.
2. std.posix as the facade for what programs still ask the Linux layer
   (stat, fcntl, signals of their own), once the native ground holds.

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
