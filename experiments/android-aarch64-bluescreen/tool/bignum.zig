//! RSA's arithmetic, built on Zig's own `std.math.big.int` (part of the
//! standard library — the same arbitrary-precision integer code Zig's
//! std.crypto.Certificate uses to verify RSA signatures during TLS — not a
//! third-party crypto dependency). This module adds what std.math.big.int
//! doesn't itself provide: modular exponentiation, Miller-Rabin primality,
//! random prime search, and the extended-Euclid modular inverse RSA key
//! generation needs.
const std = @import("std");
const Managed = std.math.big.int.Managed;
const Const = std.math.big.int.Const;

pub fn initFromBytesBE(allocator: std.mem.Allocator, bytes: []const u8) !Managed {
    var r = try Managed.initSet(allocator, @as(u32, 0));
    errdefer r.deinit();
    var byte256 = try Managed.initSet(allocator, @as(u32, 256));
    defer byte256.deinit();
    var b = try Managed.init(allocator);
    defer b.deinit();
    for (bytes) |byte| {
        try r.mul(&r, &byte256);
        try b.set(byte);
        try r.add(&r, &b);
    }
    return r;
}

/// Big-endian bytes, sized exactly to the value's bit length (no leading
/// zero byte) — i.e. minimal encoding, which is what DER INTEGER content
/// needs (the caller adds the sign-disambiguation 0x00 byte if required).
pub fn toBytesBEMinimal(allocator: std.mem.Allocator, x: *const Managed) ![]u8 {
    const bits = x.toConst().bitCountAbs();
    const nbytes = (bits + 7) / 8;
    const buf = try allocator.alloc(u8, @max(nbytes, 1));
    x.toConst().writeTwosComplement(buf, .big);
    return buf;
}

/// r = (base ^ exp) mod modulus, via left-to-right square-and-multiply.
pub fn modPow(allocator: std.mem.Allocator, base: *const Managed, exp: *const Managed, modulus: *const Managed) !Managed {
    var result = try Managed.initSet(allocator, @as(u32, 1));
    errdefer result.deinit();

    var b = try Managed.init(allocator);
    defer b.deinit();
    {
        var q = try Managed.init(allocator);
        defer q.deinit();
        try q.divTrunc(&b, base, modulus);
    }

    var e = try Managed.init(allocator);
    try e.copy(exp.toConst());
    defer e.deinit();

    var tmp = try Managed.init(allocator);
    defer tmp.deinit();
    var qdump = try Managed.init(allocator);
    defer qdump.deinit();

    while (!e.eqlZero()) {
        if (e.isOdd()) {
            try tmp.mul(&result, &b);
            try qdump.divTrunc(&result, &tmp, modulus);
        }
        try tmp.mul(&b, &b);
        try qdump.divTrunc(&b, &tmp, modulus);
        try e.shiftRight(&e, 1);
    }
    return result;
}

/// Extended Euclidean algorithm: returns x such that (a * x) mod m == 1.
/// Assumes gcd(a, m) == 1 (true for RSA's e vs. phi(n) by construction).
pub fn modInverse(allocator: std.mem.Allocator, a_in: *const Managed, m_in: *const Managed) !Managed {
    var old_r = try Managed.init(allocator);
    defer old_r.deinit();
    try old_r.copy(a_in.toConst());
    var r = try Managed.init(allocator);
    defer r.deinit();
    try r.copy(m_in.toConst());

    var old_s = try Managed.initSet(allocator, @as(i32, 1));
    defer old_s.deinit();
    var s = try Managed.initSet(allocator, @as(i32, 0));
    defer s.deinit();

    var quotient = try Managed.init(allocator);
    defer quotient.deinit();
    var remainder = try Managed.init(allocator);
    defer remainder.deinit();
    var tmp = try Managed.init(allocator);
    defer tmp.deinit();

    while (!r.eqlZero()) {
        try quotient.divTrunc(&remainder, &old_r, &r);

        old_r.swap(&r);
        r.swap(&remainder);

        try tmp.mul(&quotient, &s);
        try remainder.sub(&old_s, &tmp); // reuse `remainder` as scratch
        old_s.swap(&s);
        s.swap(&remainder);
    }

    // old_s may be negative; normalize into [0, m).
    if (!old_s.isPositive()) {
        var mcopy = try Managed.init(allocator);
        defer mcopy.deinit();
        try mcopy.copy(m_in.toConst());
        try old_s.add(&old_s, &mcopy);
    }
    var result = try Managed.init(allocator);
    try result.copy(old_s.toConst());
    return result;
}

/// Miller-Rabin primality test (probabilistic; `rounds` independent bases).
pub fn isProbablePrime(allocator: std.mem.Allocator, n: *const Managed, rounds: u32) !bool {
    var two = try Managed.initSet(allocator, @as(u32, 2));
    defer two.deinit();
    var three = try Managed.initSet(allocator, @as(u32, 3));
    defer three.deinit();
    if (n.toConst().order(two.toConst()) == .lt) return false;
    if (n.toConst().order(two.toConst()) == .eq) return true;
    if (n.toConst().order(three.toConst()) == .eq) return true;
    if (n.isEven()) return false;

    // n - 1 = 2^s * d
    var one = try Managed.initSet(allocator, @as(u32, 1));
    defer one.deinit();
    var n_minus_1 = try Managed.init(allocator);
    defer n_minus_1.deinit();
    try n_minus_1.sub(n, &one);
    var d = try Managed.init(allocator);
    defer d.deinit();
    try d.copy(n_minus_1.toConst());
    var s: u32 = 0;
    while (d.isEven()) {
        try d.shiftRight(&d, 1);
        s += 1;
    }

    var n_minus_2 = try Managed.init(allocator);
    defer n_minus_2.deinit();
    try n_minus_2.sub(n, &two);

    var round: u32 = 0;
    while (round < rounds) : (round += 1) {
        var a = try randomInRange(allocator, &two, &n_minus_2);
        defer a.deinit();

        var x = try modPow(allocator, &a, &d, n);
        defer x.deinit();

        if (x.toConst().orderAgainstScalar(1) == .eq or x.eql(n_minus_1)) {
            continue;
        }

        var composite = true;
        var r: u32 = 1;
        while (r < s) : (r += 1) {
            var sq = try Managed.init(allocator);
            var qtmp = try Managed.init(allocator);
            try sq.mul(&x, &x);
            try qtmp.divTrunc(&x, &sq, n);
            sq.deinit();
            qtmp.deinit();
            if (x.eql(n_minus_1)) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

fn randomInRange(allocator: std.mem.Allocator, lo: *const Managed, hi: *const Managed) !Managed {
    const bits = hi.toConst().bitCountAbs();
    const nbytes = (bits + 7) / 8;
    const buf = try allocator.alloc(u8, nbytes);
    defer allocator.free(buf);
    while (true) {
        std.crypto.random.bytes(buf);
        var cand = try initFromBytesBE(allocator, buf);
        errdefer cand.deinit();
        if (cand.toConst().order(lo.toConst()) != .lt and cand.toConst().order(hi.toConst()) != .gt) {
            return cand;
        }
        cand.deinit();
    }
}

/// Generate a random probable prime with the top two bits set (guarantees
/// the product of two such primes has exactly `2*bits` bits) and the low
/// bit set (odd).
pub fn randomPrime(allocator: std.mem.Allocator, bits: usize) !Managed {
    const nbytes = (bits + 7) / 8;
    var buf = try allocator.alloc(u8, nbytes);
    defer allocator.free(buf);
    while (true) {
        std.crypto.random.bytes(buf);
        buf[0] |= 0xc0; // top two bits set
        buf[buf.len - 1] |= 0x01; // odd
        var cand = try initFromBytesBE(allocator, buf);
        if (try isProbablePrime(allocator, &cand, 32)) return cand;
        cand.deinit();
    }
}

test "modPow matches schoolbook exponentiation for small values" {
    const alloc = std.testing.allocator;
    var base = try Managed.initSet(alloc, @as(u32, 4));
    defer base.deinit();
    var exp = try Managed.initSet(alloc, @as(u32, 13));
    defer exp.deinit();
    var m = try Managed.initSet(alloc, @as(u32, 497));
    defer m.deinit();
    var r = try modPow(alloc, &base, &exp, &m);
    defer r.deinit();
    // 4^13 mod 497 = 445 (textbook RSA example, RFC-adjacent test vector)
    try std.testing.expectEqual(@as(i64, 445), try r.toConst().toInt(i64));
}

test "modInverse: e * d = 1 mod phi" {
    const alloc = std.testing.allocator;
    var e = try Managed.initSet(alloc, @as(u32, 17));
    defer e.deinit();
    var phi = try Managed.initSet(alloc, @as(u32, 3120));
    defer phi.deinit();
    var d = try modInverse(alloc, &e, &phi);
    defer d.deinit();
    var check = try Managed.init(alloc);
    defer check.deinit();
    try check.mul(&e, &d);
    var q = try Managed.init(alloc);
    defer q.deinit();
    var r = try Managed.init(alloc);
    defer r.deinit();
    try q.divTrunc(&r, &check, &phi);
    try std.testing.expectEqual(@as(i64, 1), try r.toConst().toInt(i64));
}

test "isProbablePrime agrees on small known primes and composites" {
    const alloc = std.testing.allocator;
    const primes = [_]u32{ 2, 3, 5, 7, 97, 7919 };
    for (primes) |p| {
        var n = try Managed.initSet(alloc, p);
        defer n.deinit();
        try std.testing.expect(try isProbablePrime(alloc, &n, 20));
    }
    const composites = [_]u32{ 4, 9, 15, 100, 7921 };
    for (composites) |c| {
        var n = try Managed.initSet(alloc, c);
        defer n.deinit();
        try std.testing.expect(!try isProbablePrime(alloc, &n, 20));
    }
}

test "byte round-trip" {
    const alloc = std.testing.allocator;
    const bytes = [_]u8{ 0x01, 0x23, 0x45, 0x67, 0x89 };
    var n = try initFromBytesBE(alloc, &bytes);
    defer n.deinit();
    const out = try toBytesBEMinimal(alloc, &n);
    defer alloc.free(out);
    try std.testing.expectEqualSlices(u8, &bytes, out);
}
