#!/usr/bin/env python3
"""Runs a static Linux AArch64 executable under Unicorn (user-mode emulation).

This is the test vehicle for mlx1's AArch64 backend on x86_64 hosts without
qemu-user: it loads the ELF's PT_LOAD segments, builds the initial process
stack (argc, argv, envp, auxv), and services `svc #0` system calls from the
host. Only the syscalls mlx programs use are implemented; anything else
returns -ENOSYS and is reported on stderr with --trace-syscalls.

Exit status follows the shell convention: the program's exit code, 128 +
signal for SIGILL (undefined instruction: 132, like x86_64 ud2), SIGTRAP
(133) and SIGSEGV (139).

Usage: python3 tools/aarch64_linux_emulator.py [--trace-syscalls] prog [args...]
"""
import ctypes
import errno
import os
import stat
import struct
import sys
import time

from unicorn import (Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_INTR,
                     UC_PROT_ALL)
from unicorn.arm64_const import (UC_ARM64_REG_CPACR_EL1, UC_ARM64_REG_PC,
                                 UC_ARM64_REG_SP, UC_ARM64_REG_X0,
                                 UC_ARM64_REG_X8)

PAGE = 0x1000
STACK_TOP = 0x7FFF_F000_0000
STACK_SIZE = 8 << 20
MMAP_BASE = 0x10_0000_0000
AT_FDCWD = -100
EXCP_UDEF = 1
EXCP_SWI = 2
EXCP_BKPT = 7

LIBC = ctypes.CDLL(None, use_errno=True)
LIBC.syscall.restype = ctypes.c_long
HOST_SYS_GETDENTS64 = 217
HOST_SYS_STATX = 332
HOST_SYS_SPLICE = 275
HOST_SYS_EPOLL_CTL = 233
HOST_SYS_EPOLL_WAIT = 232
HOST_SYS_EPOLL_CREATE1 = 291
HOST_SYS_SOCKETPAIR = 53
HOST_SYS_PPOLL = 271

# aarch64 O_* bits that differ from the x86_64 host.
ARM_O_DIRECTORY, ARM_O_NOFOLLOW, ARM_O_DIRECT, ARM_O_LARGEFILE = 0x4000, 0x8000, 0x10000, 0x20000
HOST_O_DIRECTORY, HOST_O_NOFOLLOW, HOST_O_DIRECT, HOST_O_LARGEFILE = 0x10000, 0x20000, 0x4000, 0x8000


def align_up(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def signed64(value):
    return value - (1 << 64) if value & (1 << 63) else value


class Exit(Exception):
    def __init__(self, status):
        super().__init__(status)
        self.status = status


class Process:
    def __init__(self, path, argv, stdin=b"", trace=False, timeout_seconds=60, environment=None):
        self.path = path
        self.argv = argv
        self.environment = os.environ if environment is None else environment
        self.stdin = bytearray(stdin)
        self.stdout = bytearray()
        self.stderr = bytearray()
        self.trace = trace
        self.timeout = timeout_seconds
        self.status = None
        self.mapped = set()
        self.next_mmap = MMAP_BASE
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.uc.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)  # enable FP/SIMD
        self.entry = self._load(path)
        self._setup_stack()
        self.uc.hook_add(UC_HOOK_INTR, self._on_interrupt)

    # ── Loading ──────────────────────────────────────────────────────────
    def _map(self, address, size):
        """Maps [address, address + size), skipping pages already mapped, with
        one mem_map per run of new pages (Unicorn regions are costly)."""
        start = address & ~(PAGE - 1)
        end = align_up(address + size, PAGE)
        page = start
        while page < end:
            if page in self.mapped:
                page += PAGE
                continue
            run = page
            while run < end and run not in self.mapped:
                run += PAGE
            self.uc.mem_map(page, run - page, UC_PROT_ALL)
            self.mapped.update(range(page, run, PAGE))
            page = run

    def _load(self, path):
        with open(path, "rb") as handle:
            image = handle.read()
        if image[:4] != b"\x7fELF" or image[4] != 2 or struct.unpack_from("<H", image, 18)[0] != 183:
            raise ValueError(f"{path}: not an ELF64 AArch64 file")
        entry, phoff = struct.unpack_from("<QQ", image, 24)
        phentsize, phnum = struct.unpack_from("<HH", image, 54)
        for index in range(phnum):
            p_type, _flags, offset, vaddr, _paddr, filesz, memsz, _align = struct.unpack_from(
                "<IIQQQQQQ", image, phoff + index * phentsize)
            if p_type != 1:
                continue
            self._map(vaddr, memsz)
            self.uc.mem_write(vaddr, image[offset:offset + filesz])
        return entry

    def _setup_stack(self):
        self._map(STACK_TOP - STACK_SIZE, STACK_SIZE)
        cursor = STACK_TOP - 16

        def push_string(text):
            nonlocal cursor
            data = text.encode("utf-8", "surrogateescape") + b"\0"
            cursor -= len(data)
            self.uc.mem_write(cursor, data)
            return cursor

        arguments = [push_string(a) for a in self.argv]
        environment = [push_string(f"{k}={v}") for k, v in self.environment.items()]
        cursor &= ~15
        # argc, argv..., NULL, envp..., NULL, auxv: AT_PAGESZ, AT_NULL
        words = [len(arguments)] + arguments + [0] + environment + [0] + [6, PAGE, 0, 0]
        cursor -= len(words) * 8
        cursor &= ~15
        self.uc.mem_write(cursor, struct.pack(f"<{len(words)}Q", *words))
        self.uc.reg_write(UC_ARM64_REG_SP, cursor)

    # ── Running ──────────────────────────────────────────────────────────
    def run(self):
        try:
            self.uc.emu_start(self.entry, 0, timeout=self.timeout * 1_000_000)
            if self.status is None:
                self.status = 124  # timed out, like `timeout`
        except Exit as done:
            self.status = done.status
        except UcError as error:
            if self.status is None:
                pc = self.uc.reg_read(UC_ARM64_REG_PC)
                if self.trace:
                    sys.stderr.write(f"[emulator] {error} at pc={pc:#x}\n")
                self.status = 139
        return self.status

    def _stop(self, status):
        self.status = status
        self.uc.emu_stop()

    def _on_interrupt(self, uc, intno, _user_data):
        if intno == EXCP_SWI:
            number = uc.reg_read(UC_ARM64_REG_X8)
            args = [uc.reg_read(UC_ARM64_REG_X0 + i) for i in range(6)]
            try:
                result = self._syscall(number, args)
            except Exit as done:
                self._stop(done.status)
                return
            except OSError as error:
                result = -error.errno
            if self.trace:
                sys.stderr.write(f"[syscall] {number}({', '.join(hex(a) for a in args)}) = {result}\n")
            uc.reg_write(UC_ARM64_REG_X0, result & 0xFFFFFFFFFFFFFFFF)
        elif intno == EXCP_UDEF:
            self._stop(132)
        elif intno == EXCP_BKPT:
            self._stop(133)
        else:
            self._stop(128 + 6)

    # ── Memory helpers ───────────────────────────────────────────────────
    def read(self, address, size):
        return bytes(self.uc.mem_read(address, size)) if size else b""

    def write(self, address, data):
        if data:
            self.uc.mem_write(address, bytes(data))

    def cstring(self, address):
        out = bytearray()
        while True:
            chunk = self.read(address + len(out), 64)
            end = chunk.find(b"\0")
            if end >= 0:
                out += chunk[:end]
                return out.decode("utf-8", "surrogateescape")
            out += chunk

    # ── System calls (aarch64 numbering) ─────────────────────────────────
    def _syscall(self, number, args):
        handler = getattr(self, f"sys_{number}", None)
        if handler is None:
            sys.stderr.write(f"[emulator] unhandled syscall {number}\n")
            return -errno.ENOSYS
        return handler(*args)

    @staticmethod
    def _host_flags(flags):
        result = flags & ~(ARM_O_DIRECTORY | ARM_O_NOFOLLOW | ARM_O_DIRECT | ARM_O_LARGEFILE)
        for arm, host in ((ARM_O_DIRECTORY, HOST_O_DIRECTORY), (ARM_O_NOFOLLOW, HOST_O_NOFOLLOW),
                          (ARM_O_DIRECT, HOST_O_DIRECT), (ARM_O_LARGEFILE, HOST_O_LARGEFILE)):
            if flags & arm:
                result |= host
        return result

    @staticmethod
    def _dir_fd(dirfd):
        dirfd = signed64(dirfd)
        return None if dirfd == AT_FDCWD else dirfd

    def _pack_stat(self, st):
        # struct stat for aarch64 (asm-generic layout, 128 bytes).
        return struct.pack("<QQIIIIQQqiiqqQqQqQII",
                           st.st_dev, st.st_ino, st.st_mode, st.st_nlink, st.st_uid, st.st_gid,
                           st.st_rdev, 0, st.st_size, st.st_blksize, 0, st.st_blocks,
                           int(st.st_atime), st.st_atime_ns % 1_000_000_000,
                           int(st.st_mtime), st.st_mtime_ns % 1_000_000_000,
                           int(st.st_ctime), st.st_ctime_ns % 1_000_000_000, 0, 0)

    def sys_63(self, fd, buf, count, *_):  # read
        if fd == 0:
            data = bytes(self.stdin[:count])
            del self.stdin[:count]
        else:
            data = os.read(fd, count)
        self.write(buf, data)
        return len(data)

    def sys_64(self, fd, buf, count, *_):  # write
        data = self.read(buf, count)
        if fd == 1:
            self.stdout += data
            return count
        if fd == 2:
            self.stderr += data
            return count
        return os.write(fd, data)

    def sys_65(self, fd, iov, count, *_):  # readv
        total = 0
        for index in range(count):
            base, length = struct.unpack("<QQ", self.read(iov + index * 16, 16))
            got = self.sys_63(fd, base, length)
            total += got
            if got < length:
                break
        return total

    def sys_66(self, fd, iov, count, *_):  # writev
        total = 0
        for index in range(count):
            base, length = struct.unpack("<QQ", self.read(iov + index * 16, 16))
            total += self.sys_64(fd, base, length)
        return total

    def sys_56(self, dirfd, path, flags, mode, *_):  # openat
        return os.open(self.cstring(path), self._host_flags(flags), mode & 0o7777, dir_fd=self._dir_fd(dirfd))

    def sys_57(self, fd, *_):  # close
        if fd > 2:
            os.close(fd)
        return 0

    def sys_62(self, fd, offset, whence, *_):  # lseek
        return os.lseek(fd, signed64(offset), whence)

    def sys_67(self, fd, buf, count, offset, *_):  # pread64
        data = os.pread(fd, count, offset)
        self.write(buf, data)
        return len(data)

    def sys_68(self, fd, buf, count, offset, *_):  # pwrite64
        return os.pwrite(fd, self.read(buf, count), offset)

    def sys_79(self, dirfd, path, buf, flags, *_):  # newfstatat
        name = self.cstring(path)
        if name == "" and flags & 0x1000:  # AT_EMPTY_PATH
            st = os.fstat(signed64(dirfd))
        else:
            st = os.stat(name, dir_fd=self._dir_fd(dirfd), follow_symlinks=not flags & 0x100)
        self.write(buf, self._pack_stat(st))
        return 0

    def sys_80(self, fd, buf, *_):  # fstat
        self.write(buf, self._pack_stat(os.fstat(fd)))
        return 0

    def sys_291(self, dirfd, path, flags, mask, buf, *_):  # statx (arch-independent struct)
        host = ctypes.create_string_buffer(256)
        name = self.cstring(path).encode()
        result = LIBC.syscall(HOST_SYS_STATX, ctypes.c_int(signed64(dirfd)), name, ctypes.c_int(flags),
                              ctypes.c_uint(mask), host)
        if result < 0:
            return -ctypes.get_errno()
        self.write(buf, host.raw)
        return 0

    def sys_61(self, fd, buf, count, *_):  # getdents64 (arch-independent records)
        host = ctypes.create_string_buffer(count)
        result = LIBC.syscall(HOST_SYS_GETDENTS64, ctypes.c_int(fd), host, ctypes.c_uint(count))
        if result < 0:
            return -ctypes.get_errno()
        self.write(buf, host.raw[:result])
        return result

    def sys_17(self, buf, size, *_):  # getcwd
        data = os.getcwd().encode() + b"\0"
        if len(data) > size:
            return -errno.ERANGE
        self.write(buf, data)
        return len(data)

    def sys_49(self, path, *_):  # chdir
        os.chdir(self.cstring(path))
        return 0

    def sys_34(self, dirfd, path, mode, *_):  # mkdirat
        os.mkdir(self.cstring(path), mode, dir_fd=self._dir_fd(dirfd))
        return 0

    def sys_35(self, dirfd, path, flags, *_):  # unlinkat
        if flags & 0x200:
            os.rmdir(self.cstring(path), dir_fd=self._dir_fd(dirfd))
        else:
            os.unlink(self.cstring(path), dir_fd=self._dir_fd(dirfd))
        return 0

    def sys_38(self, olddir, old, newdir, new, *_):  # renameat
        os.rename(self.cstring(old), self.cstring(new), src_dir_fd=self._dir_fd(olddir),
                  dst_dir_fd=self._dir_fd(newdir))
        return 0

    def sys_276(self, olddir, old, newdir, new, flags, *_):  # renameat2
        if flags:
            return -errno.EINVAL
        return self.sys_38(olddir, old, newdir, new)

    def sys_37(self, olddir, old, newdir, new, flags, *_):  # linkat
        os.link(self.cstring(old), self.cstring(new), src_dir_fd=self._dir_fd(olddir),
                dst_dir_fd=self._dir_fd(newdir), follow_symlinks=bool(flags & 0x400))
        return 0

    def sys_36(self, target, newdir, path, *_):  # symlinkat
        os.symlink(self.cstring(target), self.cstring(path), dir_fd=self._dir_fd(newdir))
        return 0

    def sys_78(self, dirfd, path, buf, size, *_):  # readlinkat
        data = os.readlink(self.cstring(path), dir_fd=self._dir_fd(dirfd)).encode()[:size]
        self.write(buf, data)
        return len(data)

    def sys_53(self, dirfd, path, mode, *_):  # fchmodat
        os.chmod(self.cstring(path), mode, dir_fd=self._dir_fd(dirfd))
        return 0

    def sys_54(self, dirfd, path, uid, gid, flags, *_):  # fchownat
        os.chown(self.cstring(path), signed64(uid) if uid >> 63 else uid, signed64(gid) if gid >> 63 else gid,
                 dir_fd=self._dir_fd(dirfd), follow_symlinks=not flags & 0x100)
        return 0

    def sys_48(self, dirfd, path, mode, *_):  # faccessat
        return 0 if os.access(self.cstring(path), mode, dir_fd=self._dir_fd(dirfd)) else -errno.EACCES

    def sys_46(self, fd, length, *_):  # ftruncate
        os.ftruncate(fd, length)
        return 0

    def sys_82(self, fd, *_):  # fsync
        os.fsync(fd)
        return 0

    def sys_83(self, fd, *_):  # fdatasync
        os.fdatasync(fd)
        return 0

    def sys_81(self, *_):  # sync
        return 0

    def sys_25(self, fd, cmd, arg, *_):  # fcntl
        import fcntl
        return fcntl.fcntl(fd, cmd, arg)

    def sys_29(self, *_):  # ioctl
        return -errno.ENOTTY

    def sys_23(self, fd, *_):  # dup
        return os.dup(fd)

    def sys_24(self, old, new, flags, *_):  # dup3
        return os.dup2(old, new, inheritable=not flags)

    def sys_59(self, fds, flags, *_):  # pipe2
        read_end, write_end = os.pipe2(self._host_flags(flags))
        self.write(fds, struct.pack("<ii", read_end, write_end))
        return 0

    def sys_222(self, addr, length, prot, flags, fd, offset):  # mmap
        size = align_up(length, PAGE)
        if size == 0:
            return -errno.EINVAL
        address = self.next_mmap
        self.next_mmap += size + PAGE
        self.uc.mem_map(address, size, UC_PROT_ALL)
        if not flags & 0x20:  # file-backed: copy the contents in
            self.write(address, os.pread(fd, length, offset))
        return address

    def sys_215(self, *_):  # munmap (memory stays mapped; addresses are never reused)
        return 0

    def sys_226(self, *_):  # mprotect
        return 0

    def sys_233(self, *_):  # madvise
        return 0

    def sys_214(self, *_):  # brk: no heap break, callers fall back to mmap
        return 0

    def sys_113(self, clock, ts, *_):  # clock_gettime
        now = time.clock_gettime_ns(clock)
        self.write(ts, struct.pack("<qq", now // 1_000_000_000, now % 1_000_000_000))
        return 0

    def sys_114(self, clock, ts, *_):  # clock_getres
        if ts:
            self.write(ts, struct.pack("<qq", 0, 1))
        return 0

    def sys_101(self, *_):  # nanosleep
        return 0

    def sys_115(self, *_):  # clock_nanosleep
        return 0

    def sys_124(self, *_):  # sched_yield
        return 0

    def sys_172(self, *_):  # getpid
        return os.getpid()

    def sys_173(self, *_):  # getppid
        return os.getppid()

    def sys_178(self, *_):  # gettid
        return os.getpid()

    def sys_174(self, *_):
        return os.getuid()

    def sys_175(self, *_):
        return os.geteuid()

    def sys_176(self, *_):
        return os.getgid()

    def sys_177(self, *_):
        return os.getegid()

    def sys_166(self, mask, *_):  # umask
        return os.umask(mask)

    def sys_160(self, buf, *_):  # uname
        info = os.uname()
        fields = [info.sysname, info.nodename, info.release, info.version, "aarch64", "(none)"]
        self.write(buf, b"".join(f.encode()[:64].ljust(65, b"\0") for f in fields))
        return 0

    def sys_278(self, buf, length, *_):  # getrandom
        self.write(buf, os.urandom(length))
        return length

    def sys_134(self, *_):  # rt_sigaction
        return 0

    def sys_135(self, *_):  # rt_sigprocmask
        return 0

    def sys_96(self, *_):  # set_tid_address
        return os.getpid()

    def sys_99(self, *_):  # set_robust_list
        return 0

    def sys_129(self, pid, sig, *_):  # kill
        if pid in (0, os.getpid()) and sig:
            raise Exit(128 + sig)
        return 0

    def _host(self, number, *args):
        result = LIBC.syscall(number, *[ctypes.c_long(a) if isinstance(a, int) else a for a in args])
        return -ctypes.get_errno() if result < 0 else result

    def sys_98(self, uaddr, op, value, *_):  # futex (single-threaded: nothing ever waits)
        command = op & 127
        if command in (0, 9):  # FUTEX_WAIT, FUTEX_WAIT_BITSET
            current = struct.unpack("<I", self.read(uaddr, 4))[0]
            return -errno.EAGAIN if current != value & 0xFFFFFFFF else -errno.ETIMEDOUT
        return 0  # wakes nobody

    def sys_199(self, domain, kind, protocol, fds, *_):  # socketpair
        pair = (ctypes.c_int * 2)()
        result = self._host(HOST_SYS_SOCKETPAIR, domain, kind, protocol, ctypes.addressof(pair))
        if result == 0:
            self.write(fds, struct.pack("<ii", pair[0], pair[1]))
        return result

    def sys_206(self, fd, buf, length, flags, addr, addrlen):  # sendto
        if addr:
            return -errno.EINVAL
        return os.write(fd, self.read(buf, length)) if not flags & ~0x4000 else self._send(fd, buf, length, flags)

    def _send(self, fd, buf, length, flags):
        data = ctypes.create_string_buffer(self.read(buf, length), length)
        return self._host(44, fd, ctypes.addressof(data), length, flags, 0, 0)

    def sys_207(self, fd, buf, length, flags, addr, addrlen):  # recvfrom
        data = ctypes.create_string_buffer(length)
        result = self._host(45, fd, ctypes.addressof(data), length, flags, 0, 0)
        if result > 0:
            self.write(buf, data.raw[:result])
        return result

    def sys_210(self, fd, how, *_):  # shutdown
        return self._host(48, fd, how)

    def sys_20(self, flags, *_):  # epoll_create1
        return self._host(HOST_SYS_EPOLL_CREATE1, flags)

    def sys_21(self, epfd, op, fd, event, *_):  # epoll_ctl: aarch64 16-byte event -> x86_64 packed 12
        host = ctypes.create_string_buffer(12)
        if event:
            events, data = struct.unpack("<IxxxxQ", self.read(event, 16))
            host.raw = struct.pack("<IQ", events, data)
        return self._host(HOST_SYS_EPOLL_CTL, epfd, op, fd, ctypes.addressof(host) if event else 0)

    def sys_22(self, epfd, events, maximum, timeout, sigmask, *_):  # epoll_pwait
        count = min(maximum, 1024)
        host = ctypes.create_string_buffer(12 * max(count, 1))
        result = self._host(HOST_SYS_EPOLL_WAIT, epfd, ctypes.addressof(host), count, signed64(timeout) & 0xFFFFFFFF)
        for index in range(max(result, 0)):
            flags, data = struct.unpack_from("<IQ", host.raw, index * 12)
            self.write(events + index * 16, struct.pack("<IIQ", flags, 0, data))
        return result

    def sys_73(self, fds, count, timeout, sigmask, *_):  # ppoll (struct pollfd is arch-independent)
        host = ctypes.create_string_buffer(self.read(fds, count * 8), max(count * 8, 1))
        spec = ctypes.create_string_buffer(self.read(timeout, 16), 16) if timeout else None
        result = self._host(HOST_SYS_PPOLL, ctypes.addressof(host), count, ctypes.addressof(spec) if spec else 0, 0, 8)
        if result >= 0:
            self.write(fds, host.raw[:count * 8])
        return result

    def sys_76(self, fd_in, off_in, fd_out, off_out, length, flags):  # splice
        offsets = []
        for pointer in (off_in, off_out):
            offsets.append(ctypes.create_string_buffer(self.read(pointer, 8), 8) if pointer else None)
        result = self._host(HOST_SYS_SPLICE, fd_in, ctypes.addressof(offsets[0]) if offsets[0] else 0, fd_out,
                            ctypes.addressof(offsets[1]) if offsets[1] else 0, length, flags)
        for pointer, buffer in zip((off_in, off_out), offsets):
            if pointer:
                self.write(pointer, buffer.raw)
        return result

    def sys_425(self, entries, parameters, *_):  # io_uring_setup: not emulated
        return -errno.EFAULT if parameters == 0 else -errno.ENOSYS

    def sys_93(self, status, *_):  # exit
        raise Exit(status & 0xFF)

    def sys_94(self, status, *_):  # exit_group
        raise Exit(status & 0xFF)


def main():
    arguments = sys.argv[1:]
    trace = False
    if arguments and arguments[0] == "--trace-syscalls":
        trace = True
        arguments = arguments[1:]
    if not arguments:
        sys.stderr.write(__doc__)
        return 2
    process = Process(arguments[0], arguments, stdin=b"" if sys.stdin.isatty() else sys.stdin.buffer.read(),
                      trace=trace)
    status = process.run()
    sys.stdout.buffer.write(process.stdout)
    sys.stderr.buffer.write(process.stderr)
    return status


if __name__ == "__main__":
    sys.exit(main())
