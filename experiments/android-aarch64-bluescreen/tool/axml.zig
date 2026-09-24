//! Hand-written binary AndroidManifest.xml (AXML) encoder.
//!
//! Format ground-truthed against a *real* `aapt`-compiled manifest during
//! development: I don't have Android NDK headers memorized reliably enough
//! to trust the attribute resource-ID table from memory (getting even one
//! `android:*` attribute's numeric ID wrong would silently produce a manifest
//! the real platform parser misreads), so instead of guessing I compiled a
//! reference manifest with the actual Google `aapt` (installed from Ubuntu's
//! `google-android-build-tools-30.0.3-installer` purely as a development
//! oracle — not a dependency of this tool) and hand-parsed its output byte
//! by byte. See the experiment README for the full derivation. This encoder
//! reproduces that exact chunk format; it does not call aapt.
//!
//! Chunk format (Android's `ResourceTypes.h`, unchanged since Android 1.0):
//!
//!   ResChunk_header    { u16 type; u16 headerSize; u32 size; }
//!
//!   XML file  = ResChunk_header(type=RES_XML_TYPE=0x0003, headerSize=8)
//!             + StringPool chunk
//!             + XmlResourceMap chunk       (resource IDs for known attrs)
//!             + StartNamespace .. EndNamespace, containing:
//!                 StartElement / EndElement nodes (each 16-byte node header
//!                 = ResChunk_header + u32 lineNumber + u32 comment(=-1))
//!
//!   StringPool chunk (type=0x0001, headerSize=28): stringCount, styleCount,
//!   flags (0 = UTF-16), stringsStart, stylesStart, then a u32 offset table,
//!   then UTF-16LE strings as [u16 charLen][chars...][u16 0x0000], packed
//!   back-to-back with no inter-string padding (verified against the
//!   reference file: offsets are exactly cumulative byte lengths).
//!
//!   XmlResourceMap chunk (type=0x0180, headerSize=8): a u32 resource ID
//!   per string, for exactly the first N pool strings that are `android:`
//!   attribute names with a known ID — everything else in the pool (element
//!   names, string values, the plain `package` attribute) has no entry.
//!
//!   StartElement extra header (after the 16-byte node header): ns(i32),
//!   name(i32), attributeStart(u16)=20, attributeSize(u16)=20,
//!   attributeCount(u16), idIndex/classIndex/styleIndex(u16, =0 here); then
//!   attributeCount * 20-byte attribute records: ns(i32), name(i32),
//!   rawValue(i32, string index or -1), Res_value{size=8(u16), res0=0(u8),
//!   dataType(u8), data(u32)}.
//!
//!   EndElement extra header: ns(i32), name(i32).
//!   StartNamespace/EndNamespace extra header: prefix(i32), uri(i32).

const std = @import("std");

const RES_STRING_POOL_TYPE: u16 = 0x0001;
const RES_XML_TYPE: u16 = 0x0003;
const RES_XML_START_NAMESPACE_TYPE: u16 = 0x0100;
const RES_XML_END_NAMESPACE_TYPE: u16 = 0x0101;
const RES_XML_START_ELEMENT_TYPE: u16 = 0x0102;
const RES_XML_END_ELEMENT_TYPE: u16 = 0x0103;
const RES_XML_RESOURCE_MAP_TYPE: u16 = 0x0180;

const TYPE_REFERENCE: u8 = 0x01;
const TYPE_STRING: u8 = 0x03;
const TYPE_INT_DEC: u8 = 0x10;
const TYPE_INT_HEX: u8 = 0x11;
const TYPE_INT_BOOLEAN: u8 = 0x12;

const NO_VALUE: i32 = -1;

/// android:* attributes this manifest uses that carry a well-known resource
/// ID. Order matters: it must match first-use order in the tree below,
/// because the resource map only covers strings 0..N-1 of the pool and pool
/// index order is insertion order.
const AndroidAttr = struct { name: []const u8, resid: u32 };
const ANDROID_ATTRS = [_]AndroidAttr{
    .{ .name = "versionCode", .resid = 0x0101021b },
    .{ .name = "versionName", .resid = 0x0101021c },
    .{ .name = "minSdkVersion", .resid = 0x0101020c },
    .{ .name = "targetSdkVersion", .resid = 0x01010270 },
    .{ .name = "label", .resid = 0x01010001 },
    .{ .name = "hasCode", .resid = 0x0101000c },
    .{ .name = "name", .resid = 0x01010003 },
    .{ .name = "configChanges", .resid = 0x0101001f },
    .{ .name = "value", .resid = 0x01010024 },
    // android:theme -- ground-truthed the same way configChanges's 0x4a0
    // was: compiled a minimal reference manifest with an explicit
    // android:theme through the real `aapt` against
    // /usr/share/android-framework-res/framework-res.apk and read back
    // the resolved attribute ID from `aapt dump xmltree` -- see the
    // .reference attribute's use below for the resolved theme value.
    .{ .name = "theme", .resid = 0x01010000 },
    // android:exported, mandatory (not just recommended) on any
    // intent-filtered component once targetSdkVersion >= 31 -- omitting
    // it is a hard manifest-validation failure at install time on such
    // devices, not just a lint warning. This was invisible for this
    // project's entire life at targetSdkVersion=29 and only surfaced as
    // a real-device "You can't install this app on your device" block
    // once targetSdkVersion was bumped to 35 (see the twentieth report:
    // that bump fixed a real compatibility warning but silently exposed
    // this pre-existing gap). The install confirmation dialog still
    // renders fine beforehand (it reads only the package name/label/icon
    // via a lighter parse); this failure is in the deeper manifest
    // validation the actual install step performs, which is why it fails
    // near-instantly right after confirming, with no parse-level warning
    // visible anywhere else. Resource ID ground-truthed the same way as
    // every other attribute here: compiled a minimal reference manifest
    // with an explicit android:exported through the real `aapt` against
    // /usr/share/android-framework-res/framework-res.apk and read back
    // the resolved attribute ID from `aapt dump xmltree`.
    .{ .name = "exported", .resid = 0x01010010 },
};
const ANDROID_NS_URI = "http://schemas.android.com/apk/res/android";

const StringPool = struct {
    list: std.ArrayList([]const u8),
    map: std.StringHashMap(u32),

    fn init(allocator: std.mem.Allocator) StringPool {
        return .{ .list = std.ArrayList([]const u8).init(allocator), .map = std.StringHashMap(u32).init(allocator) };
    }
    fn deinit(self: *StringPool) void {
        self.list.deinit();
        self.map.deinit();
    }
    fn intern(self: *StringPool, s: []const u8) !u32 {
        if (self.map.get(s)) |idx| return idx;
        const idx: u32 = @intCast(self.list.items.len);
        try self.list.append(s);
        try self.map.put(s, idx);
        return idx;
    }
};

const AttrValue = union(enum) {
    str: []const u8,
    int_dec: i32,
    int_hex: u32,
    boolean: bool,
    reference: u32,
};
const Attr = struct { ns: bool, name: []const u8, value: AttrValue };

fn attrRecordBytes(pool: *StringPool, attr: Attr, out: *std.ArrayList(u8)) !void {
    const ns_idx: i32 = if (attr.ns) @intCast(try pool.intern(ANDROID_NS_URI)) else NO_VALUE;
    const name_idx: i32 = @intCast(try pool.intern(attr.name));
    var raw_idx: i32 = NO_VALUE;
    var dtype: u8 = undefined;
    var data: u32 = undefined;
    switch (attr.value) {
        .str => |s| {
            const idx = try pool.intern(s);
            raw_idx = @intCast(idx);
            dtype = TYPE_STRING;
            data = idx;
        },
        .int_dec => |v| {
            dtype = TYPE_INT_DEC;
            data = @bitCast(v);
        },
        .int_hex => |v| {
            dtype = TYPE_INT_HEX;
            data = v;
        },
        .boolean => |b| {
            dtype = TYPE_INT_BOOLEAN;
            data = if (b) 0xffffffff else 0;
        },
        .reference => |r| {
            dtype = TYPE_REFERENCE;
            data = r;
        },
    }
    try appendI32(out, ns_idx);
    try appendI32(out, name_idx);
    try appendI32(out, raw_idx);
    try appendU16(out, 8); // Res_value.size
    try out.append(0); // res0
    try out.append(dtype);
    try appendU32(out, data);
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
fn appendI32(out: *std.ArrayList(u8), v: i32) !void {
    try appendU32(out, @bitCast(v));
}

fn nodeHeader(out: *std.ArrayList(u8), chunk_type: u16, size: u32, line: u32) !void {
    try appendU16(out, chunk_type);
    try appendU16(out, 16); // headerSize
    try appendU32(out, size);
    try appendU32(out, line);
    try appendU32(out, 0xffffffff); // comment: none
}

const Element = struct {
    name: []const u8,
    attrs: []const Attr,
};

fn writeStartElement(out: *std.ArrayList(u8), pool: *StringPool, el: Element, line: u32) !void {
    var body = std.ArrayList(u8).init(out.allocator);
    defer body.deinit();
    const name_idx: i32 = @intCast(try pool.intern(el.name));
    try appendI32(&body, NO_VALUE); // ns
    try appendI32(&body, name_idx);
    try appendU16(&body, 20); // attributeStart
    try appendU16(&body, 20); // attributeSize
    try appendU16(&body, @intCast(el.attrs.len));
    try appendU16(&body, 0); // idIndex
    try appendU16(&body, 0); // classIndex
    try appendU16(&body, 0); // styleIndex
    for (el.attrs) |a| try attrRecordBytes(pool, a, &body);

    const size: u32 = 16 + @as(u32, @intCast(body.items.len));
    try nodeHeader(out, RES_XML_START_ELEMENT_TYPE, size, line);
    try out.appendSlice(body.items);
}

fn writeEndElement(out: *std.ArrayList(u8), pool: *StringPool, name: []const u8, line: u32) !void {
    const name_idx: i32 = @intCast(try pool.intern(name));
    try nodeHeader(out, RES_XML_END_ELEMENT_TYPE, 24, line);
    try appendI32(out, NO_VALUE); // ns
    try appendI32(out, name_idx);
}

// android:theme resource IDs, all four ground-truthed against the real
// `aapt` the same way as everything else in this file (compiled a minimal
// reference manifest with each theme through
// /usr/share/android-framework-res/framework-res.apk and read the resolved
// value back from `aapt dump xmltree`):
//   @android:style/Theme.Black.NoTitleBar.Fullscreen            -- hides
//     the status bar entirely, forced-black otherwise.
//   @android:style/Theme.Black.NoTitleBar                       -- keeps
//     the status bar, forced-black otherwise.
//   @android:style/Theme.DeviceDefault.NoActionBar.Fullscreen   -- hides
//     the status bar entirely, system-adaptive otherwise (moot: fullscreen
//     draws no status bar at all, so there's nothing to tint).
//   @android:style/Theme.DeviceDefault.NoActionBar.TranslucentDecor --
//     keeps the status bar (and nav bar) visible but translucent, per the
//     real style's own AOSP source (themes_material.xml, the base this
//     inherits from): `windowTranslucentStatus=true`,
//     `windowTranslucentNavigation=true`, `windowContentOverlay=@null`.
//     That's the actual mechanism behind the "adapt to the system color"
//     ask: a plain opaque `Theme.DeviceDefault.NoActionBar` (tried first,
//     0x01030129, still ground-truthed and correct, just not what was
//     wanted) still paints a solid status-bar background -- just whatever
//     unthemed color DeviceDefault happens to default to, not visibly
//     different from the old hardcoded-black theme. TranslucentDecor is
//     what actually lets this app's own blue fill show through the status
//     bar with the system's icons/clock drawn on top of it, since
//     ANativeWindow_lock's window then spans the full display including
//     the area behind the (now see-through) bar.
// None of the four is a memorized guess.
pub const THEME_FULLSCREEN: u32 = 0x0103000a;
pub const THEME_WITH_STATUS_BAR: u32 = 0x01030009;
pub const THEME_SYSTEM_FULLSCREEN: u32 = 0x0103012a;
pub const THEME_SYSTEM_WITH_STATUS_BAR: u32 = 0x010301e3;

/// Build the complete `dev.mlxlang.experiments.bluescreen` blue-screen
/// manifest: a single `android.app.NativeActivity` activity backed by
/// `main.so` (`android.app.lib_name` = "main"), MAIN/LAUNCHER intent filter.
/// `fullscreen` and `system_status_bar_color` each independently toggle
/// between the four themes above -- see main.zig's `FULLSCREEN_ENABLED` and
/// `SYSTEM_STATUS_BAR_COLOR_ENABLED` for the actual switches.
/// `min_sdk_version`/`target_sdk_version` are threaded in from main.zig
/// (rather than hardcoded here) so they're the same single source of
/// truth `apk_sign_v2v3.schemesForTargetSdk` uses to auto-pick which
/// signing schemes to add -- see main.zig's `MIN_SDK_VERSION`/
/// `TARGET_SDK_VERSION`.
pub fn buildManifest(allocator: std.mem.Allocator, package: []const u8, fullscreen: bool, system_status_bar_color: bool, min_sdk_version: u32, target_sdk_version: u32) ![]u8 {
    var pool = StringPool.init(allocator);
    defer pool.deinit();

    // Reserve pool indices 0..9 for the resource-mapped android: attrs, in
    // first-use order, *before* anything else is interned — this is what
    // makes the resource map (which only covers a string-pool prefix) line
    // up with the right attribute names.
    for (ANDROID_ATTRS) |a| _ = try pool.intern(a.name);

    var body = std.ArrayList(u8).init(allocator);
    defer body.deinit();
    var line: u32 = 2;

    // <manifest xmlns:android=... package="..." android:versionCode="1" android:versionName="...">
    {
        const ns_uri_idx = try pool.intern(ANDROID_NS_URI);
        const prefix_idx = try pool.intern("android");
        try nodeHeader(&body, RES_XML_START_NAMESPACE_TYPE, 24, line);
        try appendI32(&body, @intCast(prefix_idx));
        try appendI32(&body, @intCast(ns_uri_idx));
    }
    line += 1;
    try writeStartElement(&body, &pool, .{ .name = "manifest", .attrs = &[_]Attr{
        .{ .ns = true, .name = "versionCode", .value = .{ .int_dec = 1 } },
        .{ .ns = true, .name = "versionName", .value = .{ .str = "0.1-experiment" } },
        .{ .ns = false, .name = "package", .value = .{ .str = package } },
    } }, line);
    line += 1;

    try writeStartElement(&body, &pool, .{ .name = "uses-sdk", .attrs = &[_]Attr{
        .{ .ns = true, .name = "minSdkVersion", .value = .{ .int_dec = @intCast(min_sdk_version) } },
        .{ .ns = true, .name = "targetSdkVersion", .value = .{ .int_dec = @intCast(target_sdk_version) } },
    } }, line);
    line += 1;
    try writeEndElement(&body, &pool, "uses-sdk", line);
    line += 1;

    try writeStartElement(&body, &pool, .{ .name = "application", .attrs = &[_]Attr{
        .{ .ns = true, .name = "label", .value = .{ .str = "Mlx Blue Screen" } },
        .{ .ns = true, .name = "hasCode", .value = .{ .boolean = false } },
    } }, line);
    line += 1;

    // configChanges = keyboardHidden, resolved to 0x20 by the real aapt.
    // Used to also declare orientation|screenSize (0x4a0) — standard
    // NDK-sample boilerplate, copied without a specific reason to include
    // it — which opts the app INTO handling rotation itself by resizing
    // the existing window in place (via onNativeWindowResized) instead of
    // letting Android use its normal, far-better-tested path of destroying
    // and recreating the whole activity. Real-device testing showed that
    // in-place path is fragile: rotating left a black bar on one edge even
    // with onNativeWindowResized correctly registered and repainting. A
    // real NDK issue (android/ndk#1139) confirms why: even Google's own
    // reference `android_native_app_glue.c` never wires up
    // onNativeWindowResized/onNativeWindowRedrawNeeded at all — this
    // resize-in-place path is essentially unexercised in the wild. Since
    // this experiment has no state worth preserving across a restart,
    // dropping `orientation|screenSize` here routes rotation through the
    // ordinary, robust restart path instead: a fresh
    // ANativeActivity_onCreate -> onNativeWindowCreated with the new
    // orientation's correct dimensions from the start, the same path
    // already proven correct across every other lifecycle transition.
    // onNativeWindowResized/onNativeWindowRedrawNeeded (native_activity.zig)
    // stay registered regardless, as a harmless fallback for any resize
    // Android does dispatch to a still-live window (e.g. multi-window
    // drag-resize) rather than a restart.
    //
    // theme: `fullscreen` toggles hiding the status bar entirely vs.
    // keeping it visible; `system_status_bar_color` independently toggles
    // between a hardcoded black bar and one styled to match the device's
    // actual default look. Both added after real-device feedback -- see
    // the four THEME_* constants above for each resource ID's derivation.
    const theme_id: u32 = if (fullscreen)
        (if (system_status_bar_color) THEME_SYSTEM_FULLSCREEN else THEME_FULLSCREEN)
    else
        (if (system_status_bar_color) THEME_SYSTEM_WITH_STATUS_BAR else THEME_WITH_STATUS_BAR);
    try writeStartElement(&body, &pool, .{ .name = "activity", .attrs = &[_]Attr{
        .{ .ns = true, .name = "theme", .value = .{ .reference = theme_id } },
        .{ .ns = true, .name = "label", .value = .{ .str = "Mlx Blue Screen" } },
        .{ .ns = true, .name = "name", .value = .{ .str = "android.app.NativeActivity" } },
        .{ .ns = true, .name = "configChanges", .value = .{ .int_hex = 0x20 } },
        // Mandatory (not optional) once targetSdkVersion >= 31, since
        // this activity has an intent-filter below -- see ANDROID_ATTRS's
        // "exported" entry above for the full story.
        .{ .ns = true, .name = "exported", .value = .{ .boolean = true } },
    } }, line);
    line += 1;

    try writeStartElement(&body, &pool, .{ .name = "meta-data", .attrs = &[_]Attr{
        .{ .ns = true, .name = "name", .value = .{ .str = "android.app.lib_name" } },
        .{ .ns = true, .name = "value", .value = .{ .str = "main" } },
    } }, line);
    line += 1;
    try writeEndElement(&body, &pool, "meta-data", line);
    line += 1;

    try writeStartElement(&body, &pool, .{ .name = "intent-filter", .attrs = &[_]Attr{} }, line);
    line += 1;
    try writeStartElement(&body, &pool, .{ .name = "action", .attrs = &[_]Attr{
        .{ .ns = true, .name = "name", .value = .{ .str = "android.intent.action.MAIN" } },
    } }, line);
    line += 1;
    try writeEndElement(&body, &pool, "action", line);
    line += 1;
    try writeStartElement(&body, &pool, .{ .name = "category", .attrs = &[_]Attr{
        .{ .ns = true, .name = "name", .value = .{ .str = "android.intent.category.LAUNCHER" } },
    } }, line);
    line += 1;
    try writeEndElement(&body, &pool, "category", line);
    line += 1;
    try writeEndElement(&body, &pool, "intent-filter", line);
    line += 1;

    try writeEndElement(&body, &pool, "activity", line);
    line += 1;
    try writeEndElement(&body, &pool, "application", line);
    line += 1;
    try writeEndElement(&body, &pool, "manifest", line);
    {
        const ns_uri_idx = try pool.intern(ANDROID_NS_URI);
        const prefix_idx = try pool.intern("android");
        try nodeHeader(&body, RES_XML_END_NAMESPACE_TYPE, 24, line);
        try appendI32(&body, @intCast(prefix_idx));
        try appendI32(&body, @intCast(ns_uri_idx));
    }

    // ── serialize the string pool (UTF-16, matching the reference file) ──
    var str_data = std.ArrayList(u8).init(allocator);
    defer str_data.deinit();
    var str_offsets = std.ArrayList(u32).init(allocator);
    defer str_offsets.deinit();
    for (pool.list.items) |s| {
        try str_offsets.append(@intCast(str_data.items.len));
        std.debug.assert(s.len < 0x8000); // single-u16 length prefix only
        try appendU16(&str_data, @intCast(s.len));
        for (s) |c| try appendU16(&str_data, c); // ASCII-only strings in this manifest
        try appendU16(&str_data, 0);
    }
    const offsets_bytes: u32 = @intCast(str_offsets.items.len * 4);
    const strings_start: u32 = 28 + offsets_bytes;
    var pool_size: u32 = strings_start + @as(u32, @intCast(str_data.items.len));
    const pool_pad = (4 - (pool_size % 4)) % 4;
    pool_size += pool_pad;

    var out = std.ArrayList(u8).init(allocator);
    defer out.deinit();

    // XML root chunk header — size patched in once everything is known.
    try appendU16(&out, RES_XML_TYPE);
    try appendU16(&out, 8);
    try appendU32(&out, 0); // placeholder

    try appendU16(&out, RES_STRING_POOL_TYPE);
    try appendU16(&out, 28);
    try appendU32(&out, pool_size);
    try appendU32(&out, @intCast(pool.list.items.len)); // stringCount
    try appendU32(&out, 0); // styleCount
    try appendU32(&out, 0); // flags: UTF-16
    try appendU32(&out, strings_start);
    try appendU32(&out, 0); // stylesStart
    for (str_offsets.items) |o| try appendU32(&out, o);
    try out.appendSlice(str_data.items);
    var pad: u32 = 0;
    while (pad < pool_pad) : (pad += 1) try out.append(0);

    const resmap_size: u32 = 8 + ANDROID_ATTRS.len * 4;
    try appendU16(&out, RES_XML_RESOURCE_MAP_TYPE);
    try appendU16(&out, 8);
    try appendU32(&out, resmap_size);
    for (ANDROID_ATTRS) |a| try appendU32(&out, a.resid);

    try out.appendSlice(body.items);

    const total: u32 = @intCast(out.items.len);
    std.mem.writeInt(u32, out.items[4..8], total, .little);

    return out.toOwnedSlice();
}

test "buildManifest produces a well-formed AXML root chunk" {
    const alloc = std.testing.allocator;
    const bytes = try buildManifest(alloc, "dev.mlxlang.experiments.bluescreen", false, true, 21, 35);
    defer alloc.free(bytes);
    try std.testing.expect(bytes.len > 64);
    const root_type = std.mem.readInt(u16, bytes[0..2], .little);
    const root_size = std.mem.readInt(u32, bytes[4..8], .little);
    try std.testing.expectEqual(@as(u16, RES_XML_TYPE), root_type);
    try std.testing.expectEqual(@as(u32, @intCast(bytes.len)), root_size);
    // string pool chunk right after the 8-byte root header
    const sp_type = std.mem.readInt(u16, bytes[8..10], .little);
    try std.testing.expectEqual(@as(u16, RES_STRING_POOL_TYPE), sp_type);
}
