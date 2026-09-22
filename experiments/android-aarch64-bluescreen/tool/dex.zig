//! Hand-written minimal, empty `classes.dex`.
//!
//! This experiment's activity is a pure `android.app.NativeActivity` — no
//! app-specific JVM/Dalvik code at all — so the dex just needs to be a
//! *structurally valid, empty* dex file: zero strings/types/protos/fields
//! /methods/classes. Some install-time / package-parsing paths on real
//! devices still expect *a* `classes.dex` to be present (this is the belt
//! to the manifest's `android:hasCode="false"` suspenders); shipping a
//! correctly checksummed empty one costs 140 bytes and removes that risk.
//!
//! Format: `dalvik/libdex/DexFile.h` (unchanged in its base layout since
//! Android 1.0). Header is 0x70 (112) bytes; a valid dex must have a map
//! list containing at least a `TYPE_HEADER_ITEM` and a `TYPE_MAP_LIST`
//! entry pointing at itself.
const std = @import("std");

const HEADER_SIZE: u32 = 0x70;
const ENDIAN_CONSTANT: u32 = 0x12345678;
const TYPE_HEADER_ITEM: u16 = 0x0000;
const TYPE_MAP_LIST: u16 = 0x1000;

pub fn buildEmptyDex(allocator: std.mem.Allocator) ![]u8 {
    const map_off = HEADER_SIZE;
    const map_list_size: u32 = 4 + 2 * 12; // u4 count + 2 * MapItem(12 bytes)
    const file_size = HEADER_SIZE + map_list_size;

    var buf = try allocator.alloc(u8, file_size);
    @memset(buf, 0);

    @memcpy(buf[0..8], "dex\n035\x00");
    // checksum (bytes[8..12]) and signature (bytes[12..32]) patched at the end.
    writeU32(buf, 32, file_size);
    writeU32(buf, 36, HEADER_SIZE);
    writeU32(buf, 40, ENDIAN_CONSTANT);
    writeU32(buf, 44, 0); // link_size
    writeU32(buf, 48, 0); // link_off
    writeU32(buf, 52, map_off); // map_off
    // string/type/proto/field/method/class-def counts+offsets: all zero
    // (bytes 56..103 — left untouched by the @memset(0) above).
    writeU32(buf, 104, map_list_size); // data_size
    writeU32(buf, 108, map_off); // data_off

    var w = map_off;
    writeU32(buf, w, 2); // map list: 2 entries
    w += 4;
    writeMapItem(buf, w, TYPE_HEADER_ITEM, 1, 0);
    w += 12;
    writeMapItem(buf, w, TYPE_MAP_LIST, 1, map_off);
    w += 12;
    std.debug.assert(w == file_size);

    // signature = SHA-1(bytes[32..]), checksum = Adler-32(bytes[12..])
    var sha = std.crypto.hash.Sha1.init(.{});
    sha.update(buf[32..]);
    var digest: [20]u8 = undefined;
    sha.final(&digest);
    @memcpy(buf[12..32], &digest);

    const checksum = std.hash.Adler32.hash(buf[12..]);
    writeU32(buf, 8, checksum);

    return buf;
}

fn writeU32(buf: []u8, off: usize, v: u32) void {
    std.mem.writeInt(u32, buf[off..][0..4], v, .little);
}
fn writeU16(buf: []u8, off: usize, v: u16) void {
    std.mem.writeInt(u16, buf[off..][0..2], v, .little);
}
fn writeMapItem(buf: []u8, off: usize, item_type: u16, size: u32, item_off: u32) void {
    writeU16(buf, off, item_type);
    writeU16(buf, off + 2, 0); // unused
    writeU32(buf, off + 4, size);
    writeU32(buf, off + 8, item_off);
}

test "buildEmptyDex produces a self-consistent header" {
    const alloc = std.testing.allocator;
    const bytes = try buildEmptyDex(alloc);
    defer alloc.free(bytes);
    try std.testing.expectEqualSlices(u8, "dex\n035\x00", bytes[0..8]);
    const file_size = std.mem.readInt(u32, bytes[32..36], .little);
    try std.testing.expectEqual(@as(u32, @intCast(bytes.len)), file_size);

    var sha = std.crypto.hash.Sha1.init(.{});
    sha.update(bytes[32..]);
    var digest: [20]u8 = undefined;
    sha.final(&digest);
    try std.testing.expectEqualSlices(u8, &digest, bytes[12..32]);

    const checksum = std.hash.Adler32.hash(bytes[12..]);
    const stored = std.mem.readInt(u32, bytes[8..12], .little);
    try std.testing.expectEqual(checksum, stored);
}
