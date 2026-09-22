//! Hand-written ZIP container writer — an APK *is* a ZIP file (with an
//! optional signing block appended, see jar_sign.zig). Every entry is
//! stored uncompressed (`method = 0`): no DEFLATE implementation needed,
//! and it happens to match how Android itself prefers native libraries to
//! be packed (uncompressed + page-aligned, so they can be mmap'd directly
//! instead of extracted) — see the README for the alignment follow-up.
//!
//! Format: PKWARE's APPNOTE.TXT, the same local/central/EOCD triad every
//! ZIP tool has used since 1993.
const std = @import("std");

pub const Entry = struct {
    name: []const u8,
    data: []const u8,
};

const LOCAL_SIG: u32 = 0x04034b50;
const CENTRAL_SIG: u32 = 0x02014b50;
const EOCD_SIG: u32 = 0x06054b50;
const DOS_TIME: u16 = 0x0000;
const DOS_DATE: u16 = 0x0021; // 1980-01-01 — the common fixed default

pub fn build(allocator: std.mem.Allocator, entries: []const Entry) ![]u8 {
    var out = std.ArrayList(u8).init(allocator);
    defer out.deinit();

    const LocalOffset = struct { offset: u32, crc: u32 };
    var offsets = try allocator.alloc(LocalOffset, entries.len);
    defer allocator.free(offsets);

    for (entries, 0..) |e, i| {
        const crc = std.hash.Crc32.hash(e.data);
        offsets[i] = .{ .offset = @intCast(out.items.len), .crc = crc };
        try appendU32(&out, LOCAL_SIG);
        try appendU16(&out, 20); // version needed
        try appendU16(&out, 0); // flags
        try appendU16(&out, 0); // method: stored
        try appendU16(&out, DOS_TIME);
        try appendU16(&out, DOS_DATE);
        try appendU32(&out, crc);
        try appendU32(&out, @intCast(e.data.len)); // compressed size
        try appendU32(&out, @intCast(e.data.len)); // uncompressed size
        try appendU16(&out, @intCast(e.name.len));
        try appendU16(&out, 0); // extra length
        try out.appendSlice(e.name);
        try out.appendSlice(e.data);
    }

    const cd_start: u32 = @intCast(out.items.len);
    for (entries, 0..) |e, i| {
        try appendU32(&out, CENTRAL_SIG);
        try appendU16(&out, 20); // version made by
        try appendU16(&out, 20); // version needed
        try appendU16(&out, 0); // flags
        try appendU16(&out, 0); // method
        try appendU16(&out, DOS_TIME);
        try appendU16(&out, DOS_DATE);
        try appendU32(&out, offsets[i].crc);
        try appendU32(&out, @intCast(e.data.len));
        try appendU32(&out, @intCast(e.data.len));
        try appendU16(&out, @intCast(e.name.len));
        try appendU16(&out, 0); // extra length
        try appendU16(&out, 0); // comment length
        try appendU16(&out, 0); // disk number start
        try appendU16(&out, 0); // internal attrs
        try appendU32(&out, 0); // external attrs
        try appendU32(&out, offsets[i].offset);
        try out.appendSlice(e.name);
    }
    const cd_size: u32 = @intCast(out.items.len - cd_start);

    try appendU32(&out, EOCD_SIG);
    try appendU16(&out, 0); // disk number
    try appendU16(&out, 0); // disk with cd
    try appendU16(&out, @intCast(entries.len));
    try appendU16(&out, @intCast(entries.len));
    try appendU32(&out, cd_size);
    try appendU32(&out, cd_start);
    try appendU16(&out, 0); // comment length

    return out.toOwnedSlice();
}

fn appendU16(out: *std.ArrayList(u8), v: u16) !void {
    var b: [2]u8 = undefined;
    std.mem.writeInt(u16, &b, v, .little);
    try out.appendSlice(&b);
}
fn appendU32(out: *std.ArrayList(u8), v: u32) !void {
    var b: [4]u8 = undefined;
    std.mem.writeInt(u32, &b, v, .little);
    try out.appendSlice(&b);
}

test "build produces a ZIP readable by its own EOCD pointers" {
    const alloc = std.testing.allocator;
    const bytes = try build(alloc, &[_]Entry{
        .{ .name = "a.txt", .data = "hello" },
        .{ .name = "dir/b.bin", .data = "\x00\x01\x02\x03" },
    });
    defer alloc.free(bytes);

    // Find EOCD at the tail (no comment, so it's the fixed-size final record).
    const eocd_off = bytes.len - 22;
    const sig = std.mem.readInt(u32, bytes[eocd_off..][0..4], .little);
    try std.testing.expectEqual(@as(u32, EOCD_SIG), sig);
    const n = std.mem.readInt(u16, bytes[eocd_off + 10 ..][0..2], .little);
    try std.testing.expectEqual(@as(u16, 2), n);
    const cd_off = std.mem.readInt(u32, bytes[eocd_off + 16 ..][0..4], .little);
    const cd_sig = std.mem.readInt(u32, bytes[cd_off..][0..4], .little);
    try std.testing.expectEqual(@as(u32, CENTRAL_SIG), cd_sig);
}
