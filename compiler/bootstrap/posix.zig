//! POSIX-compatible API surface over os_linux.zig.

const std = @import("std");
const os = @import("os_linux.zig");

pub const Fd = os.Fd;
pub const OsError = os.OsError;

pub const Stat = extern struct {
    st_dev: u64,
    st_ino: u64,
    st_nlink: u64,
    st_mode: u32,
    st_uid: u32,
    st_gid: u32,
    _pad0: u32,
    st_rdev: u64,
    st_size: i64,
    st_blksize: i64,
    st_blocks: i64,
    st_atim: Timespec,
    st_mtim: Timespec,
    st_ctim: Timespec,
    _unused: [3]i64,
};

pub const Timespec = extern struct {
    tv_sec: i64,
    tv_nsec: i64,
};

pub const Dirent64 = extern struct {
    d_ino: u64,
    d_off: i64,
    d_reclen: u16,
    d_type: u8,
    d_name: [256]u8,
};

pub const PROT = struct {
    pub const NONE = 0;
    pub const READ = 1;
    pub const WRITE = 2;
    pub const EXEC = 4;
};

pub const MAP = struct {
    pub const SHARED = 1;
    pub const PRIVATE = 2;
    pub const ANONYMOUS = 0x20;
};

pub const O = struct {
    pub const RDONLY = 0;
    pub const WRONLY = 1;
    pub const RDWR = 2;
    pub const CREAT = 0o100;
    pub const TRUNC = 0o1000;
    pub const APPEND = 0o2000;
    pub const CLOEXEC = 0o2000000;
};

pub const SEEK = struct {
    pub const SET = 0;
    pub const CUR = 1;
    pub const END = 2;
};

pub fn exit(code: u8) noreturn {
    _ = os.syscall1(os.SYS_exit, code);
    unreachable;
}

pub fn write(fd: Fd, buf: []const u8) OsError!usize {
    return os.check(os.syscall3(os.SYS_write, @as(usize, @bitCast(@as(isize, fd))), @intFromPtr(buf.ptr), buf.len));
}

pub fn read(fd: Fd, buf: []u8) OsError!usize {
    return os.check(os.syscall3(os.SYS_read, @as(usize, @bitCast(@as(isize, fd))), @intFromPtr(buf.ptr), buf.len));
}

pub fn open(path: [*:0]const u8, flags: u32, mode: u32) OsError!Fd {
    const res = try os.check(os.syscall3(os.SYS_open, @intFromPtr(path), flags, mode));
    return @as(Fd, @intCast(res));
}

pub fn openZ(allocator: std.mem.Allocator, path: []const u8, flags: u32, mode: u32) OsError!Fd {
    const path_z = std.mem.concatWithSentinel(allocator, u8, &.{path}, 0) catch return error.ENOMEM;
    defer allocator.free(path_z);
    return open(path_z, flags, mode);
}

pub fn close(fd: Fd) OsError!void {
    _ = try os.check(os.syscall1(os.SYS_close, @as(usize, @bitCast(@as(isize, fd)))));
}

pub fn lseek(fd: Fd, offset: i64, whence: u32) OsError!u64 {
    const res = try os.check(os.syscall3(os.SYS_lseek, @as(usize, @bitCast(@as(isize, fd))), @as(usize, @bitCast(offset)), whence));
    return @as(u64, @intCast(res));
}

pub fn fstat(fd: Fd, out: *Stat) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_fstat, @as(usize, @bitCast(@as(isize, fd))), @intFromPtr(out)));
}

pub fn stat(path: [*:0]const u8, out: *Stat) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_stat, @intFromPtr(path), @intFromPtr(out)));
}

pub fn mmap(addr: ?[*]u8, len: usize, prot: u32, flags: u32, fd: Fd, offset: u64) OsError![]u8 {
    const res = try os.check(os.syscall6(
        os.SYS_mmap,
        @intFromPtr(addr),
        len,
        prot,
        flags,
        @as(usize, @bitCast(@as(isize, fd))),
        @as(usize, @bitCast(offset)),
    ));
    return @as([*]u8, @ptrFromInt(res))[0..len];
}

pub fn munmap(buf: []u8) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_munmap, @intFromPtr(buf.ptr), buf.len));
}

pub fn getcwd(buf: []u8) OsError![]u8 {
    const res = try os.check(os.syscall2(os.SYS_getcwd, @intFromPtr(buf.ptr), buf.len));
    return buf[0..res];
}

pub fn mkdir(path: [*:0]const u8, mode: u32) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_mkdir, @intFromPtr(path), mode));
}

pub fn unlink(path: [*:0]const u8) OsError!void {
    _ = try os.check(os.syscall1(os.SYS_unlink, @intFromPtr(path)));
}

pub fn pipe(fds: *[2]Fd) OsError!void {
    _ = try os.check(os.syscall1(os.SYS_pipe, @intFromPtr(fds)));
}

pub fn dup(fd: Fd) OsError!Fd {
    const res = try os.check(os.syscall1(os.SYS_dup, @as(usize, @bitCast(@as(isize, fd)))));
    return @as(Fd, @intCast(res));
}

pub fn dup2(old_fd: Fd, new_fd: Fd) OsError!Fd {
    const res = try os.check(os.syscall2(os.SYS_dup2, @as(usize, @bitCast(@as(isize, old_fd))), @as(usize, @bitCast(@as(isize, new_fd)))));
    return @as(Fd, @intCast(res));
}

pub fn nanosleep(req: *const Timespec, rem: ?*Timespec) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_nanosleep, @intFromPtr(req), @intFromPtr(rem)));
}

pub fn clock_gettime(clk_id: u32, out: *Timespec) OsError!void {
    _ = try os.check(os.syscall2(os.SYS_clock_gettime, clk_id, @intFromPtr(out)));
}

pub fn readdir(fd: Fd, buf: []u8) OsError!usize {
    return os.check(os.syscall3(os.SYS_getdents64, @as(usize, @bitCast(@as(isize, fd))), @intFromPtr(buf.ptr), buf.len));
}

test "posix runtime" {
    // write
    const msg = "hello zin posix\n";
    _ = try write(1, msg);

    // stat
    var st: Stat = undefined;
    try stat("/", &st);
    try std.testing.expect(st.st_ino != 0);

    // mmap anonymous
    const mem = try mmap(null, 4096, PROT.READ | PROT.WRITE, MAP.PRIVATE | MAP.ANONYMOUS, -1, 0);
    mem[0] = 42;
    try std.testing.expect(mem[0] == 42);
    try munmap(mem);

    // getcwd
    var cwd_buf: [1024]u8 = undefined;
    const cwd = try getcwd(&cwd_buf);
    try std.testing.expect(cwd.len > 0);
    try std.testing.expect(cwd[0] == '/');

    // clock_gettime
    var ts: Timespec = undefined;
    try clock_gettime(1, &ts); // CLOCK_MONOTONIC = 1
    try std.testing.expect(ts.tv_sec > 0);
}
