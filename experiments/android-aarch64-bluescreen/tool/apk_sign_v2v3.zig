//! APK Signature Scheme v2 and v3 — the "APK Signing Block" that real
//! Android (API 24+ for v2, API 28+ for v3) verifies in addition to (not
//! instead of) the classic v1 JAR signature already produced by
//! jar_sign.zig. Straight port of
//! experiments/android-aarch64-bluescreen/mlx/apk_sign_v2v3.mlx — see that
//! file's header comment for the full byte-format derivation and the three
//! real bugs this format hid from a plain "do the offsets line up" review
//! (a missing per-entry length prefix on digest/signature pairs, chunking
//! the three content sections as one flat stream instead of independently,
//! and digesting the file's real post-insertion EOCD instead of the
//! pre-insertion one apksig's own verifier actually expects). All three
//! were caught only by the real `apksigner verify` tool, not by
//! disassembly or by this port's own bytes lining up internally.
const std = @import("std");
const Managed = std.math.big.int.Managed;
const bn = @import("bignum.zig");
const asn1 = @import("asn1.zig");
const jar_sign = @import("jar_sign.zig");

pub const SIGNING_BLOCK_ID_V2: u32 = 0x7109871a;
pub const SIGNING_BLOCK_ID_V3: u32 = 0xf05368c0;

const SIG_ALGORITHM_ID: u32 = 0x0103; // RSASSA-PKCS1-v1_5 with SHA2-256
const MAX_SDK: u32 = 0x7fffffff;
const CHUNK_SIZE: usize = 1048576; // 1 MiB

const OID_RSA_ENCRYPTION = "1.2.840.113549.1.1.1";

fn appendU32LE(list: *std.ArrayList(u8), v: u32) !void {
    try list.appendSlice(&std.mem.toBytes(std.mem.nativeToLittle(u32, v)));
}
fn appendU64LE(list: *std.ArrayList(u8), v: u64) !void {
    try list.appendSlice(&std.mem.toBytes(std.mem.nativeToLittle(u64, v)));
}
// 4-byte length prefix + payload — the format every v2/v3 sub-field uses.
fn appendLp(list: *std.ArrayList(u8), content: []const u8) !void {
    try appendU32LE(list, @intCast(content.len));
    try list.appendSlice(content);
}

fn readU32LE(data: []const u8, offset: usize) u32 {
    return std.mem.readInt(u32, data[offset..][0..4], .little);
}

// ── chunked content digest (APK Signing Block's own digest algorithm,
// distinct from a plain sha256(data)) ──────────────────────────────────

// Appends one SHA256(0xa5 ++ LE32(chunkLen) ++ chunk) digest per <=1MB chunk
// of `data` to `out_digests`, returns how many chunks it produced (0 for an
// empty section). Each of the three sections chunks *independently* — a
// fresh chunk boundary always starts at a section's first byte, never
// spanning two sections — confirmed against apksig's real
// `computeOneMbChunkContentDigests` (it loops `DataSource[] contents`,
// restarting hashing at the start of every element), not by chunking one
// flat concatenation of all three sections.
fn chunkDigestsForSection(allocator: std.mem.Allocator, data: []const u8, out_digests: *std.ArrayList(u8)) !usize {
    var count: usize = 0;
    var offset: usize = 0;
    while (offset < data.len) {
        const chunk_len = @min(CHUNK_SIZE, data.len - offset);

        var buf = std.ArrayList(u8).init(allocator);
        defer buf.deinit();
        try buf.append(0xa5); // per-chunk prefix
        try appendU32LE(&buf, @intCast(chunk_len));
        try buf.appendSlice(data[offset .. offset + chunk_len]);

        var digest: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(buf.items, &digest, .{});
        try out_digests.appendSlice(&digest);

        offset += chunk_len;
        count += 1;
    }
    return count;
}

// The three sections are fixed-arity parameters, matching the mlx port
// (which avoids arrays-of-slices for an unrelated backend-bug reason —
// not needed here, but kept identical for easy comparison between ports).
pub fn chunkedSha256DigestOverSections(allocator: std.mem.Allocator, section1: []const u8, section2: []const u8, section3: []const u8) ![32]u8 {
    var chunk_digests = std.ArrayList(u8).init(allocator);
    defer chunk_digests.deinit();
    var total_chunks: usize = 0;
    total_chunks += try chunkDigestsForSection(allocator, section1, &chunk_digests);
    total_chunks += try chunkDigestsForSection(allocator, section2, &chunk_digests);
    total_chunks += try chunkDigestsForSection(allocator, section3, &chunk_digests);

    var top = std.ArrayList(u8).init(allocator);
    defer top.deinit();
    try top.append(0x5a); // top-level combine prefix
    try appendU32LE(&top, @intCast(total_chunks));
    try top.appendSlice(chunk_digests.items);

    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(top.items, &digest, .{});
    return digest;
}

// ── SubjectPublicKeyInfo (DER) — duplicated from jar_sign.buildSelfSignedCert's
// inline construction rather than imported, since its helpers aren't `pub`
// (see that file's header comment for the same tradeoff elsewhere) ──────

pub fn buildSubjectPublicKeyInfo(allocator: std.mem.Allocator, key: *const jar_sign.RsaKeyPair) ![]u8 {
    const n_mag = try bn.toBytesBEMinimal(allocator, &key.n);
    defer allocator.free(n_mag);
    const n_int = try asn1.integerFromUnsignedBE(allocator, n_mag);
    defer allocator.free(n_int);
    const e_int = try asn1.integerFromU64(allocator, 65537);
    defer allocator.free(e_int);
    const rsa_pub = try asn1.sequence(allocator, &.{ n_int, e_int });
    defer allocator.free(rsa_pub);
    const oid_bytes = try asn1.oid(allocator, OID_RSA_ENCRYPTION);
    defer allocator.free(oid_bytes);
    const nul = try asn1.asnNull(allocator);
    defer allocator.free(nul);
    const rsa_alg = try asn1.sequence(allocator, &.{ oid_bytes, nul });
    defer allocator.free(rsa_alg);
    const pub_key_bits = try asn1.bitStringFromBytes(allocator, rsa_pub);
    defer allocator.free(pub_key_bits);
    return asn1.sequence(allocator, &.{ rsa_alg, pub_key_bits });
}

// ── signedData sub-fields (shared shape between v2 and v3) ─────────────

// A "digests"/"signatures" entry is NOT simply algId+lp(payload) — real
// apksig's `encodeAsSequenceOfLengthPrefixedPairsOfIntAndLengthPrefixedBytes`
// wraps each pair in its OWN outer length prefix too (length = 8 +
// payload.length, covering the algId word and the inner length word), so
// an entry is a *triply* nested structure: lp( algId(4) ++ lp(payload) ).
fn buildAlgIdPayloadEntry(allocator: std.mem.Allocator, alg_id: u32, payload: []const u8) ![]u8 {
    var inner = std.ArrayList(u8).init(allocator);
    defer inner.deinit();
    try appendU32LE(&inner, alg_id);
    try appendLp(&inner, payload);

    var entry = std.ArrayList(u8).init(allocator);
    errdefer entry.deinit();
    try appendLp(&entry, inner.items);
    return entry.toOwnedSlice();
}

fn buildDigestsField(allocator: std.mem.Allocator, digest: []const u8) ![]u8 {
    const entry = try buildAlgIdPayloadEntry(allocator, SIG_ALGORITHM_ID, digest);
    defer allocator.free(entry);
    var field = std.ArrayList(u8).init(allocator);
    errdefer field.deinit();
    try appendLp(&field, entry);
    return field.toOwnedSlice();
}

fn buildCertsField(allocator: std.mem.Allocator, cert_der: []const u8) ![]u8 {
    var seq = std.ArrayList(u8).init(allocator);
    defer seq.deinit();
    try appendLp(&seq, cert_der);
    var field = std.ArrayList(u8).init(allocator);
    errdefer field.deinit();
    try appendLp(&field, seq.items);
    return field.toOwnedSlice();
}

fn buildEmptyAttrsField(allocator: std.mem.Allocator) ![]u8 {
    var field = std.ArrayList(u8).init(allocator);
    errdefer field.deinit();
    try appendU32LE(&field, 0); // zero-length sequence of additional attributes
    return field.toOwnedSlice();
}

fn buildSignaturesField(allocator: std.mem.Allocator, sig: []const u8) ![]u8 {
    const entry = try buildAlgIdPayloadEntry(allocator, SIG_ALGORITHM_ID, sig);
    defer allocator.free(entry);
    var field = std.ArrayList(u8).init(allocator);
    errdefer field.deinit();
    try appendLp(&field, entry);
    return field.toOwnedSlice();
}

// One full `signer` record (signedData + signatures + publicKey), v2 shape
// when include_sdk is false, v3 shape (minSdk/maxSdk inside AND outside
// signedData) when true. `digest` is the already-computed chunked content
// digest.
fn buildSignerBytes(allocator: std.mem.Allocator, key: *const jar_sign.RsaKeyPair, cert_der: []const u8, spki_bytes: []const u8, digest: []const u8, min_sdk: u32, include_sdk: bool) ![]u8 {
    const digests_field = try buildDigestsField(allocator, digest);
    defer allocator.free(digests_field);
    const certs_field = try buildCertsField(allocator, cert_der);
    defer allocator.free(certs_field);
    const attrs_field = try buildEmptyAttrsField(allocator);
    defer allocator.free(attrs_field);

    var signed_data = std.ArrayList(u8).init(allocator);
    defer signed_data.deinit();
    try signed_data.appendSlice(digests_field);
    try signed_data.appendSlice(certs_field);
    if (include_sdk) {
        try appendU32LE(&signed_data, min_sdk);
        try appendU32LE(&signed_data, MAX_SDK);
    }
    try signed_data.appendSlice(attrs_field);

    var signed_data_digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(signed_data.items, &signed_data_digest, .{});
    const sig = try jar_sign.signSha256Pkcs1v15(allocator, key, signed_data_digest);
    defer allocator.free(sig);
    const sigs_field = try buildSignaturesField(allocator, sig);
    defer allocator.free(sigs_field);

    var pub_key_field = std.ArrayList(u8).init(allocator);
    defer pub_key_field.deinit();
    try appendLp(&pub_key_field, spki_bytes);

    var signer = std.ArrayList(u8).init(allocator);
    errdefer signer.deinit();
    try appendLp(&signer, signed_data.items);
    if (include_sdk) {
        try appendU32LE(&signer, min_sdk);
        try appendU32LE(&signer, MAX_SDK);
    }
    try signer.appendSlice(sigs_field);
    try signer.appendSlice(pub_key_field.items);
    return signer.toOwnedSlice();
}

fn buildPairEntry(allocator: std.mem.Allocator, id: u32, value: []const u8) ![]u8 {
    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();
    try appendU64LE(&out, 4 + value.len);
    try appendU32LE(&out, id);
    try out.appendSlice(value);
    return out.toOwnedSlice();
}

// Both ID-value pairs (v2 + v3), back to back — everything the Signing
// Block needs except its own 8-byte size fields and magic.
fn buildPairsBytes(allocator: std.mem.Allocator, key: *const jar_sign.RsaKeyPair, cert_der: []const u8, spki_bytes: []const u8, digest: []const u8, min_sdk: u32) ![]u8 {
    // Two nested length prefixes here, not one: the pair's value is
    // LP(signersSeq), and signersSeq is itself the concatenation of
    // LP(signer) for each signer (just one, here) — a verifier reads the
    // outer LP to find the whole sequence, then reads a per-signer LP from
    // inside it to find where each signer ends.
    const signer_v2 = try buildSignerBytes(allocator, key, cert_der, spki_bytes, digest, min_sdk, false);
    defer allocator.free(signer_v2);
    var signers_seq_v2 = std.ArrayList(u8).init(allocator);
    defer signers_seq_v2.deinit();
    try appendLp(&signers_seq_v2, signer_v2);
    var signers_value_v2 = std.ArrayList(u8).init(allocator);
    defer signers_value_v2.deinit();
    try appendLp(&signers_value_v2, signers_seq_v2.items);
    const pair_v2 = try buildPairEntry(allocator, SIGNING_BLOCK_ID_V2, signers_value_v2.items);
    defer allocator.free(pair_v2);

    const signer_v3 = try buildSignerBytes(allocator, key, cert_der, spki_bytes, digest, min_sdk, true);
    defer allocator.free(signer_v3);
    var signers_seq_v3 = std.ArrayList(u8).init(allocator);
    defer signers_seq_v3.deinit();
    try appendLp(&signers_seq_v3, signer_v3);
    var signers_value_v3 = std.ArrayList(u8).init(allocator);
    defer signers_value_v3.deinit();
    try appendLp(&signers_value_v3, signers_seq_v3.items);
    const pair_v3 = try buildPairEntry(allocator, SIGNING_BLOCK_ID_V3, signers_value_v3.items);
    defer allocator.free(pair_v3);

    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();
    try out.appendSlice(pair_v2);
    try out.appendSlice(pair_v3);
    return out.toOwnedSlice();
}

fn wrapSigningBlock(allocator: std.mem.Allocator, pairs_bytes: []const u8) ![]u8 {
    const size_val: u64 = pairs_bytes.len + 24; // + second size field (8) + magic (16)
    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();
    try appendU64LE(&out, size_val);
    try out.appendSlice(pairs_bytes);
    try appendU64LE(&out, size_val);
    try out.appendSlice("APK Sig Block 42");
    return out.toOwnedSlice();
}

/// Splice APK Signature Scheme v2 + v3 signing into `zip_bytes` (the
/// already v1(JAR)-signed, fully-built ZIP from zip.build() — v1 and
/// v2/v3 coexist, same as real apksigner's default behavior). Returns the
/// final APK bytes.
pub fn signV2V3(allocator: std.mem.Allocator, key: *const jar_sign.RsaKeyPair, cert_der: []const u8, zip_bytes: []const u8, min_sdk: u32) ![]u8 {
    const eocd_offset = zip_bytes.len - 22;
    const orig_cd_start = readU32LE(zip_bytes, eocd_offset + 16);

    const section1 = zip_bytes[0..orig_cd_start];
    const section2 = zip_bytes[orig_cd_start..eocd_offset];
    const orig_eocd = zip_bytes[eocd_offset..zip_bytes.len];

    const spki = try buildSubjectPublicKeyInfo(allocator, key);
    defer allocator.free(spki);

    // The content digest is computed treating EOCD's "start of central
    // directory" field as pointing to the START of the (about to be
    // inserted) APK Signing Block — i.e. unchanged from `orig_eocd`, since
    // that's exactly where the block goes. This is a real, deliberate
    // apksig quirk (its own comment: "For the purposes of verifying
    // integrity, ... EoCD must be treated as though its Central Directory
    // offset points to the start of APK Signing Block"), confirmed against
    // its actual verifier source (`ApkSigningBlockUtils.verifyIntegrity`
    // rewrites the EOCD it digests to `beforeApkSigningBlock.size()`,
    // which is exactly `orig_cd_start` here) — NOT the adjusted offset
    // that makes the final on-disk file a valid, readable ZIP. Since the
    // digest input never mentions the Signing Block's own size, there's
    // no circularity to resolve with a placeholder-then-rebuild pass —
    // the real digest, real signatures, and real block size can all be
    // computed in one straight pass.
    const content_digest = try chunkedSha256DigestOverSections(allocator, section1, section2, orig_eocd);

    const real_pairs = try buildPairsBytes(allocator, key, cert_der, spki, &content_digest, min_sdk);
    defer allocator.free(real_pairs);
    const signing_block = try wrapSigningBlock(allocator, real_pairs);
    defer allocator.free(signing_block);
    const block_size = signing_block.len;

    // The EOCD actually written to disk DOES need its cdStart adjusted —
    // unlike the digest input above, a real ZIP/Android reader must be
    // able to physically locate the Central Directory in the final file.
    var final_eocd: [22]u8 = undefined;
    @memcpy(&final_eocd, orig_eocd);
    const new_cd_start: u32 = orig_cd_start + @as(u32, @intCast(block_size));
    std.mem.writeInt(u32, final_eocd[16..20], new_cd_start, .little);

    var final_apk = std.ArrayList(u8).init(allocator);
    errdefer final_apk.deinit();
    try final_apk.appendSlice(section1);
    try final_apk.appendSlice(signing_block);
    try final_apk.appendSlice(section2);
    try final_apk.appendSlice(&final_eocd);
    return final_apk.toOwnedSlice();
}
