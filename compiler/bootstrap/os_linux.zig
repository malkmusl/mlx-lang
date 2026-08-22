//! Linux x86_64 raw OS layer for zin0 bootstrap.
//! Direct syscall wrappers with no libc dependency.

const std = @import("std");

pub const Fd = i32;
pub const Pid = i32;
pub const SyscallResult = i64;

// Syscall numbers (x86_64)
pub const SYS_read = 0;
pub const SYS_write = 1;
pub const SYS_open = 2;
pub const SYS_close = 3;
pub const SYS_stat = 4;
pub const SYS_fstat = 5;
pub const SYS_poll = 7;
pub const SYS_lseek = 8;
pub const SYS_mmap = 9;
pub const SYS_munmap = 11;
pub const SYS_ioctl = 16;
pub const SYS_pipe = 22;
pub const SYS_dup = 32;
pub const SYS_dup2 = 33;
pub const SYS_nanosleep = 35;
pub const SYS_socket = 41;
pub const SYS_connect = 42;
pub const SYS_sendmsg = 46;
pub const SYS_recvmsg = 47;
pub const SYS_exit = 60;
pub const SYS_fcntl = 72;
pub const SYS_ftruncate = 77;
pub const SYS_getcwd = 79;
pub const SYS_mkdir = 83;
pub const SYS_unlink = 87;
pub const SYS_getdents64 = 217;
pub const SYS_clock_gettime = 228;
pub const SYS_memfd_create = 319;

// Error codes
pub const OsError = error{
    EPERM,
    ENOENT,
    ESRCH,
    EINTR,
    EIO,
    ENXIO,
    EBADF,
    ECHILD,
    EAGAIN,
    ENOMEM,
    EACCES,
    EFAULT,
    EBUSY,
    EEXIST,
    ENODEV,
    ENOTDIR,
    EISDIR,
    EINVAL,
    ENFILE,
    EMFILE,
    ENOSYS,
    ENOSPC,
    EROFS,
    EPIPE,
    ENOTSUP,
    ERANGE,
    Unexpected,
};

pub fn check(res: SyscallResult) OsError!usize {
    if (res >= 0) return @as(usize, @intCast(res));
    const err = -res;
    return switch (err) {
        1 => error.EPERM,
        2 => error.ENOENT,
        3 => error.ESRCH,
        4 => error.EINTR,
        5 => error.EIO,
        6 => error.ENXIO,
        9 => error.EBADF,
        10 => error.ECHILD,
        11 => error.EAGAIN,
        12 => error.ENOMEM,
        13 => error.EACCES,
        14 => error.EFAULT,
        16 => error.EBUSY,
        17 => error.EEXIST,
        19 => error.ENODEV,
        20 => error.ENOTDIR,
        21 => error.EISDIR,
        22 => error.EINVAL,
        23 => error.ENFILE,
        24 => error.EMFILE,
        28 => error.ENOSPC,
        30 => error.EROFS,
        32 => error.EPIPE,
        34 => error.ERANGE,
        38 => error.ENOSYS,
        95 => error.ENOTSUP,
        else => error.Unexpected,
    };
}

// Syscall wrappers

pub inline fn syscall0(nr: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall1(nr: usize, a1: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall2(nr: usize, a1: usize, a2: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
          [a2] "{rsi}" (a2),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall3(nr: usize, a1: usize, a2: usize, a3: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
          [a2] "{rsi}" (a2),
          [a3] "{rdx}" (a3),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall4(nr: usize, a1: usize, a2: usize, a3: usize, a4: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
          [a2] "{rsi}" (a2),
          [a3] "{rdx}" (a3),
          [a4] "{r10}" (a4),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall5(nr: usize, a1: usize, a2: usize, a3: usize, a4: usize, a5: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
          [a2] "{rsi}" (a2),
          [a3] "{rdx}" (a3),
          [a4] "{r10}" (a4),
          [a5] "{r8}" (a5),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

pub inline fn syscall6(nr: usize, a1: usize, a2: usize, a3: usize, a4: usize, a5: usize, a6: usize) SyscallResult {
    return asm volatile ("syscall"
        : [ret] "={rax}" (-> SyscallResult),
        : [nr] "{rax}" (nr),
          [a1] "{rdi}" (a1),
          [a2] "{rsi}" (a2),
          [a3] "{rdx}" (a3),
          [a4] "{r10}" (a4),
          [a5] "{r8}" (a5),
          [a6] "{r9}" (a6),
        : .{ .rcx = true, .r11 = true, .memory = true }
    );
}

test "syscall getpid" {
    // getpid = 39
    const pid = syscall0(39);
    try std.testing.expect(pid > 0);
}
