//! Builds `bluescreen.apk`: a minimal, real, *signed* Android package whose
//! `android.app.NativeActivity` fills its window solid blue — entirely from
//! hand-written bytes (AArch64 machine code, an ELF64 shared object, a
//! binary AndroidManifest.xml, a DEX header, a ZIP container, and an RSA/
//! X.509/PKCS#7 JAR signature). See the experiment README for what's
//! verified and what still needs a real device.
//!
//! Usage: `zig run main.zig -- [output.apk] [package.name] [--min-sdk=N] [--target-sdk=N]`
//! The two flags override MIN_SDK_VERSION/TARGET_SDK_VERSION below without
//! editing source -- see those constants' comments for what each bound
//! means and why TARGET_SDK_VERSION defaults to 30, not higher.
const std = @import("std");
const Managed = std.math.big.int.Managed;
const elf_so = @import("elf_so.zig");
const axml = @import("axml.zig");
const dex = @import("dex.zig");
const zip = @import("zip.zig");
const jar_sign = @import("jar_sign.zig");
const apk_sign_v2v3 = @import("apk_sign_v2v3.zig");
const bn = @import("bignum.zig");

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

// Kept low for broad device compatibility -- devices below this still
// install and run the app fine (NativeActivity itself needs nothing
// newer). Threaded into both the manifest's `<uses-sdk>` and (as the
// floor apk_sign_v2v3.signV2V3 assumes) the v3 Signing Block's own
// per-signer minSdk field. Override at build time with `--min-sdk=N` --
// this is a `DEFAULT_*` rather than a hardcoded value for exactly that
// reason (unlike mlx/main.mlx, which has no CLI arg access yet and so
// stays a plain hardcoded function -- see its own minSdkVersion() doc).
const DEFAULT_MIN_SDK_VERSION: u32 = 21;

// Declares how current this build has actually been tested/updated for.
// Real-device feedback: a low targetSdkVersion (this was 29 originally)
// makes current Android show "This app was built for an older version of
// Android and may not work properly" on install/launch, independent of
// signing -- Android surfaces that warning purely off the gap between
// targetSdkVersion and the device's own platform version.
//
// 37 (Android 17): the highest value real-device confirmed so far (31, 35,
// 36 and 37 all installed and ran once the attribute-order bug behind the
// old >= 31 block was fixed -- see the README's twenty-eighth and
// twenty-ninth reports). Override with `--target-sdk=N`.
//
// There's deliberately no "maxSdkVersion" to pair with the min:
// `<uses-sdk android:maxSdkVersion>` has been ignored at install time since
// API 4. Android installs on any device >= min_sdk_version regardless of
// target, so this single value is the real "how current" lever.
const DEFAULT_TARGET_SDK_VERSION: u32 = 37;

// Absolute path (not repo-relative) to a small persisted RSA keypair,
// reused across builds instead of generating a fresh random one every
// time -- shared with mlx/main.mlx's identical `keyPath()`/on-disk format
// so mlx- and zig-built APKs carry the SAME signing identity and stay
// installable as updates over each other. This is a real fix for a real
// bug: every build up through the twenty-first round called
// jar_sign.generateKeyPair fresh, so every shipped APK had a DIFFERENT
// self-signed certificate -- and Android refuses to install an "update"
// whose signing certificate doesn't match whatever's already installed
// under the same package name, confirmed on a real device as a "You
// can't install this app on your device" toast after several
// differently-keyed builds had been installed in a row. Mirrors the
// purpose (not the file format) of Android Studio's own debug.keystore.
const KEY_PATH = "/home/user/mlx-lang/experiments/android-aarch64-bluescreen/debug_signing_key.bin";

// On-disk format: [4 bytes LE n_bytes][n_bytes bytes n, big-endian]
// [n_bytes bytes d, big-endian] -- not a real keystore format, just
// enough to keep this experiment's signing identity stable across
// rebuilds (and across which port built it). `e` isn't persisted since
// it's always the fixed public exponent 65537 by construction. Returns
// null if the file doesn't exist yet or is short/corrupt (caller then
// falls back to generating a fresh key).
fn tryLoadKeyPair(allocator: std.mem.Allocator) !?jar_sign.RsaKeyPair {
    const file = std.fs.openFileAbsolute(KEY_PATH, .{}) catch return null;
    defer file.close();

    var header: [4]u8 = undefined;
    const header_read = try file.readAll(&header);
    if (header_read != 4) return null;
    const n_bytes: usize = std.mem.readInt(u32, &header, .little);

    const body = try allocator.alloc(u8, n_bytes * 2);
    defer allocator.free(body);
    const body_read = try file.readAll(body);
    if (body_read != n_bytes * 2) return null;

    var n = try bn.initFromBytesBE(allocator, body[0..n_bytes]);
    errdefer n.deinit();
    var d = try bn.initFromBytesBE(allocator, body[n_bytes .. n_bytes * 2]);
    errdefer d.deinit();
    var e = try Managed.initSet(allocator, @as(u32, 65537));
    errdefer e.deinit();

    return jar_sign.RsaKeyPair{ .allocator = allocator, .n = n, .e = e, .d = d, .n_bytes = n_bytes };
}

fn saveKeyPair(key: *const jar_sign.RsaKeyPair) !void {
    const file = try std.fs.createFileAbsolute(KEY_PATH, .{});
    defer file.close();

    var header: [4]u8 = undefined;
    std.mem.writeInt(u32, &header, @as(u32, @intCast(key.n_bytes)), .little);
    try file.writeAll(&header);

    const allocator = key.allocator;

    const n_padded = try allocator.alloc(u8, key.n_bytes);
    defer allocator.free(n_padded);
    @memset(n_padded, 0);
    const n_min = try bn.toBytesBEMinimal(allocator, &key.n);
    defer allocator.free(n_min);
    @memcpy(n_padded[key.n_bytes - n_min.len ..], n_min);
    try file.writeAll(n_padded);

    const d_padded = try allocator.alloc(u8, key.n_bytes);
    defer allocator.free(d_padded);
    @memset(d_padded, 0);
    const d_min = try bn.toBytesBEMinimal(allocator, &key.d);
    defer allocator.free(d_min);
    @memcpy(d_padded[key.n_bytes - d_min.len ..], d_min);
    try file.writeAll(d_padded);
}

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

    var min_sdk_version: u32 = DEFAULT_MIN_SDK_VERSION;
    var target_sdk_version: u32 = DEFAULT_TARGET_SDK_VERSION;
    while (args.next()) |arg| {
        if (std.mem.startsWith(u8, arg, "--min-sdk=")) {
            min_sdk_version = std.fmt.parseInt(u32, arg["--min-sdk=".len..], 10) catch {
                try out.print("error: invalid --min-sdk value: {s}\n", .{arg});
                return error.InvalidArgument;
            };
        } else if (std.mem.startsWith(u8, arg, "--target-sdk=")) {
            target_sdk_version = std.fmt.parseInt(u32, arg["--target-sdk=".len..], 10) catch {
                try out.print("error: invalid --target-sdk value: {s}\n", .{arg});
                return error.InvalidArgument;
            };
        } else {
            try out.print("error: unrecognized argument: {s}\n", .{arg});
            return error.InvalidArgument;
        }
    }
    if (target_sdk_version < min_sdk_version) {
        try out.print("error: --target-sdk ({d}) cannot be below --min-sdk ({d})\n", .{ target_sdk_version, min_sdk_version });
        return error.InvalidSdkRange;
    }

    try out.print("mlx android-aarch64-bluescreen experiment — building {s} (package {s})\n", .{ out_path, package });
    try out.print("      min-sdk={d} target-sdk={d}\n", .{ min_sdk_version, target_sdk_version });

    try out.print("[1/7] generating AArch64 machine code + ELF64 shared object...\n", .{});
    const so_bytes = try elf_so.build(allocator, "libmain.so", "libandroid.so", "libc.so", "liblog.so");
    defer allocator.free(so_bytes);
    try out.print("      {s}: {d} bytes\n", .{ SO_PATH, so_bytes.len });

    try out.print("[2/7] generating binary AndroidManifest.xml...\n", .{});
    const manifest_bytes = try axml.buildManifest(allocator, package, FULLSCREEN_ENABLED, SYSTEM_STATUS_BAR_COLOR_ENABLED, min_sdk_version, target_sdk_version);
    defer allocator.free(manifest_bytes);
    try out.print("      {s}: {d} bytes\n", .{ MANIFEST_PATH, manifest_bytes.len });

    try out.print("[3/7] generating classes.dex (empty — pure native activity)...\n", .{});
    const dex_bytes = try dex.buildEmptyDex(allocator);
    defer allocator.free(dex_bytes);
    try out.print("      {s}: {d} bytes\n", .{ DEX_PATH, dex_bytes.len });

    try out.print("[4/7] loading or generating RSA signing key + self-signed certificate...\n", .{});
    var key: jar_sign.RsaKeyPair = undefined;
    if (try tryLoadKeyPair(allocator)) |loaded| {
        key = loaded;
        try out.print("      reusing saved signing key: {s}\n", .{KEY_PATH});
    } else {
        try out.print("      no saved key found -- generating a new one (this takes a few seconds)...\n", .{});
        var timer = try std.time.Timer.start();
        key = try jar_sign.generateKeyPair(allocator, 2048);
        try out.print("      keygen: {d}ms\n", .{timer.lap() / 1_000_000});
        try saveKeyPair(&key);
        try out.print("      saved signing key: {s}\n", .{KEY_PATH});
    }
    defer key.deinit();
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

    try out.print("[7/7] signing (APK Signature Scheme v2/v3, auto-picked from targetSdkVersion)...\n", .{});
    // v1 is never dropped by schemesForTargetSdk (see its comment) -- the
    // META-INF/* entries above are unconditional for exactly that reason.
    const schemes = apk_sign_v2v3.schemesForTargetSdk(target_sdk_version);
    try out.print("      v2: {}, v3: {}\n", .{ schemes.v2, schemes.v3 });
    const apk_bytes = try apk_sign_v2v3.signV2V3(allocator, &key, cert_der, v1_apk_bytes, 28, schemes.v2, schemes.v3);
    defer allocator.free(apk_bytes);
    try out.print("      APK Signing Block: {d} bytes\n", .{apk_bytes.len - v1_apk_bytes.len});

    const f = try std.fs.cwd().createFile(out_path, .{});
    defer f.close();
    try f.writeAll(apk_bytes);

    try out.print("done: {s} ({d} bytes)\n", .{ out_path, apk_bytes.len });
}
