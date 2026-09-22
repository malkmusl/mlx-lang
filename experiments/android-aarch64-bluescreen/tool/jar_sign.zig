//! RSA key generation, a minimal self-signed X.509 certificate, and classic
//! (no signed-attributes) PKCS#7 `SignedData` — enough to produce the
//! `META-INF/MANIFEST.MF`, `META-INF/CERT.SF` and `META-INF/CERT.RSA`
//! trio that make an APK's ZIP a *signed* JAR (APK Signature Scheme v1 /
//! JAR signing — see the README for why v1 rather than v2/v3, and for how
//! this was cross-checked against `jarsigner -verify` and `openssl` as
//! development-time oracles, never as a build dependency).
const std = @import("std");
const Managed = std.math.big.int.Managed;
const bn = @import("bignum.zig");
const asn1 = @import("asn1.zig");

const OID_RSA_ENCRYPTION = "1.2.840.113549.1.1.1";
const OID_SHA256_WITH_RSA = "1.2.840.113549.1.1.11";
const OID_SHA256 = "2.16.840.1.101.3.4.2.1";
const OID_COMMON_NAME = "2.5.4.3";
const OID_PKCS7_DATA = "1.2.840.113549.1.7.1";
const OID_PKCS7_SIGNED_DATA = "1.2.840.113549.1.7.2";

pub const RsaKeyPair = struct {
    allocator: std.mem.Allocator,
    n: Managed,
    e: Managed,
    d: Managed,
    n_bytes: usize,

    pub fn deinit(self: *RsaKeyPair) void {
        self.n.deinit();
        self.e.deinit();
        self.d.deinit();
    }
};

/// Generate an RSA key pair with a `bits`-bit modulus (e.g. 2048) and
/// public exponent 65537. ~1-3 seconds at 2048 bits (see README timing
/// note) — this runs once per APK build, not per signature.
pub fn generateKeyPair(allocator: std.mem.Allocator, bits: usize) !RsaKeyPair {
    var p = try bn.randomPrime(allocator, bits / 2);
    defer p.deinit();
    var q = try bn.randomPrime(allocator, bits / 2);
    defer q.deinit();

    var n = try Managed.init(allocator);
    errdefer n.deinit();
    try n.mul(&p, &q);

    var one = try Managed.initSet(allocator, @as(u32, 1));
    defer one.deinit();
    var p1 = try Managed.init(allocator);
    defer p1.deinit();
    try p1.sub(&p, &one);
    var q1 = try Managed.init(allocator);
    defer q1.deinit();
    try q1.sub(&q, &one);
    var phi = try Managed.init(allocator);
    defer phi.deinit();
    try phi.mul(&p1, &q1);

    var e = try Managed.initSet(allocator, @as(u32, 65537));
    errdefer e.deinit();
    var d = try bn.modInverse(allocator, &e, &phi);
    errdefer d.deinit();

    return .{ .allocator = allocator, .n = n, .e = e, .d = d, .n_bytes = (bits + 7) / 8 };
}

/// RSASSA-PKCS1-v1_5 signature of a SHA-256 digest (RFC 8017 §8.2, §9.2):
/// EM = 0x00 0x01 [0xFF...] 0x00 DigestInfo(sha256, digest); signature =
/// EM^d mod n, left-padded to exactly `n_bytes`.
pub fn signSha256Pkcs1v15(allocator: std.mem.Allocator, key: *const RsaKeyPair, digest: [32]u8) ![]u8 {
    const alg_id = try algIdWithNull(allocator, OID_SHA256);
    defer allocator.free(alg_id);
    const digest_octets = try asn1.octetString(allocator, &digest);
    defer allocator.free(digest_octets);
    const digest_info = try asn1.sequence(allocator, &.{ alg_id, digest_octets });
    defer allocator.free(digest_info);

    if (digest_info.len + 11 > key.n_bytes) return error.KeyTooSmall;
    const em = try allocator.alloc(u8, key.n_bytes);
    defer allocator.free(em);
    em[0] = 0x00;
    em[1] = 0x01;
    const ps_len = key.n_bytes - digest_info.len - 3;
    @memset(em[2..][0..ps_len], 0xff);
    em[2 + ps_len] = 0x00;
    @memcpy(em[key.n_bytes - digest_info.len ..], digest_info);

    var em_int = try bn.initFromBytesBE(allocator, em);
    defer em_int.deinit();
    var sig_int = try bn.modPow(allocator, &em_int, &key.d, &key.n);
    defer sig_int.deinit();

    const sig = try allocator.alloc(u8, key.n_bytes);
    sig_int.toConst().writeTwosComplement(sig, .big);
    return sig;
}

fn rdnCommonName(allocator: std.mem.Allocator, cn: []const u8) ![]u8 {
    const cn_oid = try asn1.oid(allocator, OID_COMMON_NAME);
    defer allocator.free(cn_oid);
    const cn_val = try asn1.utf8String(allocator, cn);
    defer allocator.free(cn_val);
    const ava = try asn1.sequence(allocator, &.{ cn_oid, cn_val });
    defer allocator.free(ava);
    const rdn = try asn1.set(allocator, &.{ava});
    defer allocator.free(rdn);
    return asn1.sequence(allocator, &.{rdn});
}

fn algIdWithNull(allocator: std.mem.Allocator, oid_dotted: []const u8) ![]u8 {
    const o = try asn1.oid(allocator, oid_dotted);
    defer allocator.free(o);
    const n = try asn1.asnNull(allocator);
    defer allocator.free(n);
    return asn1.sequence(allocator, &.{ o, n });
}

/// A minimal self-signed v1 X.509 certificate over `key`, subject/issuer
/// `CN=<cn>`, valid 1980-01-01..2049-12-31 (kept inside the UTCTime range
/// so no GeneralizedTime handling is needed).
pub fn buildSelfSignedCert(allocator: std.mem.Allocator, key: *const RsaKeyPair, cn: []const u8, serial: u64) ![]u8 {
    const name = try rdnCommonName(allocator, cn);
    defer allocator.free(name);

    const n_mag = try bn.toBytesBEMinimal(allocator, &key.n);
    defer allocator.free(n_mag);
    const n_int = try asn1.integerFromUnsignedBE(allocator, n_mag);
    defer allocator.free(n_int);
    const e_int = try asn1.integerFromU64(allocator, 65537);
    defer allocator.free(e_int);
    const rsa_pub = try asn1.sequence(allocator, &.{ n_int, e_int });
    defer allocator.free(rsa_pub);
    const rsa_alg = try algIdWithNull(allocator, OID_RSA_ENCRYPTION);
    defer allocator.free(rsa_alg);
    const pub_key_bits = try asn1.bitStringFromBytes(allocator, rsa_pub);
    defer allocator.free(pub_key_bits);
    const spki = try asn1.sequence(allocator, &.{ rsa_alg, pub_key_bits });
    defer allocator.free(spki);

    const not_before = try asn1.utcTime(allocator, "700101000000Z");
    defer allocator.free(not_before);
    const not_after = try asn1.utcTime(allocator, "491231235959Z");
    defer allocator.free(not_after);
    const validity = try asn1.sequence(allocator, &.{ not_before, not_after });
    defer allocator.free(validity);

    const serial_int = try asn1.integerFromU64(allocator, serial);
    defer allocator.free(serial_int);
    const sig_alg = try algIdWithNull(allocator, OID_SHA256_WITH_RSA);
    defer allocator.free(sig_alg);

    const tbs = try asn1.sequence(allocator, &.{ serial_int, sig_alg, name, validity, name, spki });
    defer allocator.free(tbs);

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(tbs, &digest, .{});
    const sig = try signSha256Pkcs1v15(allocator, key, digest);
    defer allocator.free(sig);
    const sig_bits = try asn1.bitStringFromBytes(allocator, sig);
    defer allocator.free(sig_bits);

    return asn1.sequence(allocator, &.{ tbs, sig_alg, sig_bits });
}

/// PKCS#7 `SignedData` (detached, no signed attributes — the classic JAR
/// `CERT.RSA` shape) over `content` (the CERT.SF bytes), embedding `cert`.
pub fn buildPkcs7SignedData(allocator: std.mem.Allocator, key: *const RsaKeyPair, cert_der: []const u8, cn: []const u8, serial: u64, content: []const u8) ![]u8 {
    const digest_alg = try algIdWithNull(allocator, OID_SHA256);
    defer allocator.free(digest_alg);
    const digest_algs_set = try asn1.set(allocator, &.{digest_alg});
    defer allocator.free(digest_algs_set);

    const data_oid = try asn1.oid(allocator, OID_PKCS7_DATA);
    defer allocator.free(data_oid);
    const inner_content_info = try asn1.sequence(allocator, &.{data_oid});
    defer allocator.free(inner_content_info);

    const certs_wrapper = try asn1.implicitConstructedTag(allocator, 0, &.{cert_der});
    defer allocator.free(certs_wrapper);

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(content, &digest, .{});
    const sig = try signSha256Pkcs1v15(allocator, key, digest);
    defer allocator.free(sig);
    const enc_digest = try asn1.octetString(allocator, sig);
    defer allocator.free(enc_digest);

    const name = try rdnCommonName(allocator, cn);
    defer allocator.free(name);
    const serial_int = try asn1.integerFromU64(allocator, serial);
    defer allocator.free(serial_int);
    const issuer_and_serial = try asn1.sequence(allocator, &.{ name, serial_int });
    defer allocator.free(issuer_and_serial);

    const rsa_alg = try algIdWithNull(allocator, OID_RSA_ENCRYPTION);
    defer allocator.free(rsa_alg);
    const version1 = try asn1.integerFromU64(allocator, 1);
    defer allocator.free(version1);

    const signer_info = try asn1.sequence(allocator, &.{ version1, issuer_and_serial, digest_alg, rsa_alg, enc_digest });
    defer allocator.free(signer_info);
    const signer_infos_set = try asn1.set(allocator, &.{signer_info});
    defer allocator.free(signer_infos_set);

    const version1b = try asn1.integerFromU64(allocator, 1);
    defer allocator.free(version1b);
    const signed_data = try asn1.sequence(allocator, &.{ version1b, digest_algs_set, inner_content_info, certs_wrapper, signer_infos_set });
    defer allocator.free(signed_data);

    const sd_oid = try asn1.oid(allocator, OID_PKCS7_SIGNED_DATA);
    defer allocator.free(sd_oid);
    const explicit_sd = try asn1.explicitTag(allocator, 0, &.{signed_data});
    defer allocator.free(explicit_sd);

    return asn1.sequence(allocator, &.{ sd_oid, explicit_sd });
}

// ── JAR manifest / signature-file text generation ──────────────────────

const b64 = std.base64.standard.Encoder;

fn sha256Base64(allocator: std.mem.Allocator, data: []const u8) ![]u8 {
    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(data, &digest, .{});
    const out = try allocator.alloc(u8, b64.calcSize(32));
    _ = b64.encode(out, &digest);
    return out;
}

pub const NamedEntry = struct { name: []const u8, data: []const u8 };

pub const SignedJarFiles = struct {
    manifest_mf: []u8,
    cert_sf: []u8,
    cert_rsa: []u8,
};

/// Produce MANIFEST.MF, CERT.SF and CERT.RSA for `entries` (the APK's
/// content files other than the META-INF/ signing trio itself).
pub fn signJar(allocator: std.mem.Allocator, key: *const RsaKeyPair, cert_der: []const u8, cn: []const u8, serial: u64, entries: []const NamedEntry) !SignedJarFiles {
    var mf = std.ArrayList(u8).init(allocator);
    defer mf.deinit();
    try mf.appendSlice("Manifest-Version: 1.0\r\nCreated-By: mlx android-aarch64-bluescreen experiment\r\n\r\n");

    var sections = try allocator.alloc([]u8, entries.len);
    defer {
        for (sections) |s| allocator.free(s);
        allocator.free(sections);
    }
    for (entries, 0..) |ent, i| {
        const digest_b64 = try sha256Base64(allocator, ent.data);
        defer allocator.free(digest_b64);
        var section = std.ArrayList(u8).init(allocator);
        errdefer section.deinit();
        const w = section.writer();
        try w.print("Name: {s}\r\nSHA-256-Digest: {s}\r\n\r\n", .{ ent.name, digest_b64 });
        sections[i] = try section.toOwnedSlice();
        try mf.appendSlice(sections[i]);
    }

    var sf = std.ArrayList(u8).init(allocator);
    defer sf.deinit();
    const mf_digest_b64 = try sha256Base64(allocator, mf.items);
    defer allocator.free(mf_digest_b64);
    try sf.writer().print("Signature-Version: 1.0\r\nCreated-By: mlx android-aarch64-bluescreen experiment\r\nSHA-256-Digest-Manifest: {s}\r\n\r\n", .{mf_digest_b64});
    for (entries, 0..) |ent, i| {
        const section_digest_b64 = try sha256Base64(allocator, sections[i]);
        defer allocator.free(section_digest_b64);
        try sf.writer().print("Name: {s}\r\nSHA-256-Digest: {s}\r\n\r\n", .{ ent.name, section_digest_b64 });
    }

    const cert_rsa = try buildPkcs7SignedData(allocator, key, cert_der, cn, serial, sf.items);

    return .{
        .manifest_mf = try mf.toOwnedSlice(),
        .cert_sf = try sf.toOwnedSlice(),
        .cert_rsa = cert_rsa,
    };
}

test "signSha256Pkcs1v15 produces a signature RSA verification accepts" {
    const alloc = std.testing.allocator;
    var key = try generateKeyPair(alloc, 512); // small: unit test speed only
    defer key.deinit();

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash("hello world", &digest, .{});
    const sig = try signSha256Pkcs1v15(alloc, &key, digest);
    defer alloc.free(sig);

    // Verify by hand: sig^e mod n should decode to the same EMSA-PKCS1 block.
    var sig_int = try bn.initFromBytesBE(alloc, sig);
    defer sig_int.deinit();
    var recovered = try bn.modPow(alloc, &sig_int, &key.e, &key.n);
    defer recovered.deinit();
    const em = try alloc.alloc(u8, key.n_bytes);
    defer alloc.free(em);
    recovered.toConst().writeTwosComplement(em, .big);

    try std.testing.expectEqual(@as(u8, 0x00), em[0]);
    try std.testing.expectEqual(@as(u8, 0x01), em[1]);
    // last 32 bytes of the recovered EM must be the digest itself
    try std.testing.expectEqualSlices(u8, &digest, em[em.len - 32 ..]);
}

test "buildSelfSignedCert produces a DER SEQUENCE" {
    const alloc = std.testing.allocator;
    var key = try generateKeyPair(alloc, 512);
    defer key.deinit();
    const cert = try buildSelfSignedCert(alloc, &key, "mlx-experiment", 1);
    defer alloc.free(cert);
    try std.testing.expectEqual(@as(u8, asn1.TAG_SEQUENCE), cert[0]);
}
