//! Minimal hand-written ASN.1 DER encoder — just the handful of primitives
//! X.509 and PKCS#7/CMS need (SEQUENCE, SET, INTEGER, OBJECT IDENTIFIER,
//! BIT STRING, OCTET STRING, NULL, UTCTime, UTF8String, and the
//! context-specific tags certificates use for `version [0]` etc).
const std = @import("std");

pub const TAG_INTEGER: u8 = 0x02;
pub const TAG_BIT_STRING: u8 = 0x03;
pub const TAG_OCTET_STRING: u8 = 0x04;
pub const TAG_NULL: u8 = 0x05;
pub const TAG_OID: u8 = 0x06;
pub const TAG_UTF8_STRING: u8 = 0x0c;
pub const TAG_SEQUENCE: u8 = 0x30; // SEQUENCE | constructed
pub const TAG_SET: u8 = 0x31; // SET | constructed
pub const TAG_PRINTABLE_STRING: u8 = 0x13;
pub const TAG_UTC_TIME: u8 = 0x17;

fn appendLen(out: *std.ArrayList(u8), len: usize) !void {
    if (len < 0x80) {
        try out.append(@intCast(len));
        return;
    }
    var tmp: [8]u8 = undefined;
    var n: usize = 0;
    var v = len;
    while (v > 0) : (n += 1) {
        tmp[n] = @intCast(v & 0xff);
        v >>= 8;
    }
    try out.append(0x80 | @as(u8, @intCast(n)));
    var i: usize = n;
    while (i > 0) {
        i -= 1;
        try out.append(tmp[i]);
    }
}

/// Tag + DER length + raw content (caller supplies fully-formed content).
pub fn tlv(allocator: std.mem.Allocator, tag: u8, content: []const u8) ![]u8 {
    var out = std.ArrayList(u8).init(allocator);
    defer out.deinit();
    try out.append(tag);
    try appendLen(&out, content.len);
    try out.appendSlice(content);
    return out.toOwnedSlice();
}

/// SEQUENCE wrapping the concatenation of already-encoded child TLVs.
pub fn sequence(allocator: std.mem.Allocator, parts: []const []const u8) ![]u8 {
    return joinedTlv(allocator, TAG_SEQUENCE, parts);
}
pub fn set(allocator: std.mem.Allocator, parts: []const []const u8) ![]u8 {
    return joinedTlv(allocator, TAG_SET, parts);
}
/// Context-specific constructed tag, e.g. `[0] EXPLICIT ...` = 0xA0.
pub fn explicitTag(allocator: std.mem.Allocator, n: u8, parts: []const []const u8) ![]u8 {
    return joinedTlv(allocator, 0xa0 | n, parts);
}
/// Context-specific IMPLICIT constructed tag (no inner universal tag+len).
pub fn implicitConstructedTag(allocator: std.mem.Allocator, n: u8, parts: []const []const u8) ![]u8 {
    return joinedTlv(allocator, 0xa0 | n, parts);
}

fn joinedTlv(allocator: std.mem.Allocator, tag: u8, parts: []const []const u8) ![]u8 {
    var content = std.ArrayList(u8).init(allocator);
    defer content.deinit();
    for (parts) |p| try content.appendSlice(p);
    return tlv(allocator, tag, content.items);
}

/// INTEGER from a minimal big-endian unsigned magnitude (as produced by
/// bignum.toBytesBEMinimal): DER requires a leading 0x00 if the value would
/// otherwise be read as negative (top bit of the first byte set).
pub fn integerFromUnsignedBE(allocator: std.mem.Allocator, mag: []const u8) ![]u8 {
    var m = mag;
    while (m.len > 1 and m[0] == 0) m = m[1..]; // shouldn't happen (minimal already), defensive
    const needs_pad = m.len == 0 or (m[0] & 0x80) != 0;
    var content = try allocator.alloc(u8, m.len + @as(usize, if (needs_pad) 1 else 0));
    defer allocator.free(content);
    if (needs_pad) {
        content[0] = 0;
        @memcpy(content[1..], m);
    } else {
        if (m.len == 0) content[0] = 0 else @memcpy(content, m);
    }
    return tlv(allocator, TAG_INTEGER, content);
}

pub fn integerFromU64(allocator: std.mem.Allocator, v: u64) ![]u8 {
    var buf: [8]u8 = undefined;
    std.mem.writeInt(u64, &buf, v, .big);
    var i: usize = 0;
    while (i < 7 and buf[i] == 0) i += 1;
    return integerFromUnsignedBE(allocator, buf[i..]);
}

pub fn octetString(allocator: std.mem.Allocator, content: []const u8) ![]u8 {
    return tlv(allocator, TAG_OCTET_STRING, content);
}
pub fn bitStringFromBytes(allocator: std.mem.Allocator, content: []const u8) ![]u8 {
    var buf = try allocator.alloc(u8, content.len + 1);
    defer allocator.free(buf);
    buf[0] = 0; // 0 unused bits
    @memcpy(buf[1..], content);
    return tlv(allocator, TAG_BIT_STRING, buf);
}
pub fn asnNull(allocator: std.mem.Allocator) ![]u8 {
    return tlv(allocator, TAG_NULL, &[_]u8{});
}
pub fn utf8String(allocator: std.mem.Allocator, s: []const u8) ![]u8 {
    return tlv(allocator, TAG_UTF8_STRING, s);
}
pub fn printableString(allocator: std.mem.Allocator, s: []const u8) ![]u8 {
    return tlv(allocator, TAG_PRINTABLE_STRING, s);
}
pub fn utcTime(allocator: std.mem.Allocator, s: []const u8) ![]u8 {
    return tlv(allocator, TAG_UTC_TIME, s);
}

/// Object identifier from dotted-decimal text, e.g. "1.2.840.113549.1.1.1".
pub fn oid(allocator: std.mem.Allocator, dotted: []const u8) ![]u8 {
    var content = std.ArrayList(u8).init(allocator);
    defer content.deinit();

    var it = std.mem.splitScalar(u8, dotted, '.');
    const first = try std.fmt.parseInt(u32, it.next().?, 10);
    const second = try std.fmt.parseInt(u32, it.next().?, 10);
    try content.append(@intCast(first * 40 + second));

    while (it.next()) |part| {
        const v = try std.fmt.parseInt(u64, part, 10);
        try appendBase128(&content, v);
    }
    return tlv(allocator, TAG_OID, content.items);
}

fn appendBase128(out: *std.ArrayList(u8), v: u64) !void {
    var tmp: [10]u8 = undefined;
    var n: usize = 0;
    var x = v;
    tmp[n] = @intCast(x & 0x7f);
    n += 1;
    x >>= 7;
    while (x > 0) : (n += 1) {
        tmp[n] = @intCast((x & 0x7f) | 0x80);
        x >>= 7;
    }
    var i: usize = n;
    while (i > 0) {
        i -= 1;
        try out.append(tmp[i]);
    }
}

test "oid encodes well-known PKCS#1/X.509 identifiers correctly" {
    const alloc = std.testing.allocator;
    {
        const b = try oid(alloc, "1.2.840.113549.1.1.1"); // rsaEncryption
        defer alloc.free(b);
        try std.testing.expectEqualSlices(u8, &[_]u8{ 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 }, b);
    }
    {
        const b = try oid(alloc, "1.2.840.113549.1.1.11"); // sha256WithRSAEncryption
        defer alloc.free(b);
        try std.testing.expectEqualSlices(u8, &[_]u8{ 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b }, b);
    }
    {
        const b = try oid(alloc, "2.16.840.1.101.3.4.2.1"); // id-sha256
        defer alloc.free(b);
        try std.testing.expectEqualSlices(u8, &[_]u8{ 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01 }, b);
    }
    {
        const b = try oid(alloc, "2.5.4.3"); // commonName
        defer alloc.free(b);
        try std.testing.expectEqualSlices(u8, &[_]u8{ 0x06, 0x03, 0x55, 0x04, 0x03 }, b);
    }
}

test "integer DER-pads a high-bit-set magnitude" {
    const alloc = std.testing.allocator;
    const b = try integerFromUnsignedBE(alloc, &[_]u8{0xff});
    defer alloc.free(b);
    try std.testing.expectEqualSlices(u8, &[_]u8{ 0x02, 0x02, 0x00, 0xff }, b);
}

test "long-form length encoding" {
    const alloc = std.testing.allocator;
    const content = try alloc.alloc(u8, 200);
    defer alloc.free(content);
    @memset(content, 0xAB);
    const b = try tlv(alloc, TAG_OCTET_STRING, content);
    defer alloc.free(b);
    // 200 = 0xC8 needs one length-of-length byte: 0x81 0xC8
    try std.testing.expectEqualSlices(u8, &[_]u8{ TAG_OCTET_STRING, 0x81, 0xc8 }, b[0..3]);
}
