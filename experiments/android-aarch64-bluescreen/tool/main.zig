//! Builds `bluescreen.apk`: a minimal, real, *signed* Android package whose
//! `android.app.NativeActivity` fills its window solid blue — entirely from
//! hand-written bytes (AArch64 machine code, an ELF64 shared object, a
//! binary AndroidManifest.xml, a DEX header, a ZIP container, and an RSA/
//! X.509/PKCS#7 JAR signature). See the experiment README for what's
//! verified and what still needs a real device.
//!
//! Usage: `zig run main.zig -- [output.apk] [package.name]`
const std = @import("std");
const elf_so = @import("elf_so.zig");
const axml = @import("axml.zig");
const dex = @import("dex.zig");
const zip = @import("zip.zig");
const jar_sign = @import("jar_sign.zig");
const apk_sign_v2v3 = @import("apk_sign_v2v3.zig");

const SO_PATH = "lib/arm64-v8a/libmain.so";
const MANIFEST_PATH = "AndroidManifest.xml";
const DEX_PATH = "classes.dex";

// Toggle: true hides the system status bar entirely (a `*_FULLSCREEN`
// theme); false keeps the normal system status bar visible (the blue fill
// renders underneath it), which is what real-device testing showed was
// actually wanted -- see axml.zig's THEME_* constants for all four themes'
// resource IDs.
const FULLSCREEN_ENABLED = false;

// Toggle: true styles the status bar to match the device's actual default
// look (`Theme.DeviceDefault.NoActionBar[.Fullscreen]`); false forces a
// hardcoded black bar (`Theme.Black.NoTitleBar[.Fullscreen]`), which was
// this experiment's original look before real-device feedback asked for
// the status bar to "adapt to the system color" instead. Independent of
// FULLSCREEN_ENABLED above -- the two combine into all four themes.
const SYSTEM_STATUS_BAR_COLOR_ENABLED = true;

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var args = try std.process.argsWithAllocator(allocator);
    defer args.deinit();
    _ = args.next();
    const out_path = args.next() orelse "bluescreen.apk";
    const package = args.next() orelse "dev.mlxlang.experiments.bluescreen";

    const out = std.io.getStdOut().writer();

    try out.print("mlx android-aarch64-bluescreen experiment — building {s} (package {s})\n", .{ out_path, package });

    try out.print("[1/7] generating AArch64 machine code + ELF64 shared object...\n", .{});
    const so_bytes = try elf_so.build(allocator, "libmain.so", "libandroid.so", "libc.so");
    defer allocator.free(so_bytes);
    try out.print("      {s}: {d} bytes\n", .{ SO_PATH, so_bytes.len });

    try out.print("[2/7] generating binary AndroidManifest.xml...\n", .{});
    const manifest_bytes = try axml.buildManifest(allocator, package, FULLSCREEN_ENABLED, SYSTEM_STATUS_BAR_COLOR_ENABLED);
    defer allocator.free(manifest_bytes);
    try out.print("      {s}: {d} bytes\n", .{ MANIFEST_PATH, manifest_bytes.len });

    try out.print("[3/7] generating classes.dex (empty — pure native activity)...\n", .{});
    const dex_bytes = try dex.buildEmptyDex(allocator);
    defer allocator.free(dex_bytes);
    try out.print("      {s}: {d} bytes\n", .{ DEX_PATH, dex_bytes.len });

    try out.print("[4/7] generating RSA-2048 signing key + self-signed certificate (this takes a few seconds)...\n", .{});
    var timer = try std.time.Timer.start();
    var key = try jar_sign.generateKeyPair(allocator, 2048);
    defer key.deinit();
    try out.print("      keygen: {d}ms\n", .{timer.lap() / 1_000_000});
    const cert_der = try jar_sign.buildSelfSignedCert(allocator, &key, "mlx-android-aarch64-bluescreen", 1);
    defer allocator.free(cert_der);
    try out.print("      certificate: {d} bytes\n", .{cert_der.len});

    try out.print("[5/7] signing (MANIFEST.MF / CERT.SF / CERT.RSA)...\n", .{});
    const entries = [_]jar_sign.NamedEntry{
        .{ .name = MANIFEST_PATH, .data = manifest_bytes },
        .{ .name = DEX_PATH, .data = dex_bytes },
        .{ .name = SO_PATH, .data = so_bytes },
    };
    const signed = try jar_sign.signJar(allocator, &key, cert_der, "mlx-android-aarch64-bluescreen", 1, &entries);
    defer allocator.free(signed.manifest_mf);
    defer allocator.free(signed.cert_sf);
    defer allocator.free(signed.cert_rsa);
    try out.print("      META-INF/CERT.RSA: {d} bytes\n", .{signed.cert_rsa.len});

    try out.print("[6/7] packaging APK (ZIP)...\n", .{});
    const zip_entries = [_]zip.Entry{
        .{ .name = MANIFEST_PATH, .data = manifest_bytes },
        .{ .name = DEX_PATH, .data = dex_bytes },
        .{ .name = SO_PATH, .data = so_bytes },
        .{ .name = "META-INF/MANIFEST.MF", .data = signed.manifest_mf },
        .{ .name = "META-INF/CERT.SF", .data = signed.cert_sf },
        .{ .name = "META-INF/CERT.RSA", .data = signed.cert_rsa },
    };
    const v1_apk_bytes = try zip.build(allocator, &zip_entries);
    defer allocator.free(v1_apk_bytes);

    try out.print("[7/7] signing (APK Signature Scheme v2 + v3)...\n", .{});
    const apk_bytes = try apk_sign_v2v3.signV2V3(allocator, &key, cert_der, v1_apk_bytes, 28);
    defer allocator.free(apk_bytes);
    try out.print("      APK Signing Block: {d} bytes\n", .{apk_bytes.len - v1_apk_bytes.len});

    const f = try std.fs.cwd().createFile(out_path, .{});
    defer f.close();
    try f.writeAll(apk_bytes);

    try out.print("done: {s} ({d} bytes)\n", .{ out_path, apk_bytes.len });
}
