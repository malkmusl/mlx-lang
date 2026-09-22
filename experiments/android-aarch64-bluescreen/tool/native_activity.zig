//! Generates the machine code for a minimal Android `ANativeActivity` shared
//! library that fills its window with a solid blue color — the "hello
//! world" of this experiment.
//!
//! ABI notes (from `android/native_activity.h`, `android/native_window.h`,
//! `android/rect.h` — stable since NDK API level 9, hand-transcribed here
//! since no NDK headers are available in this build environment; this is
//! the biggest structural risk of the experiment and is flagged in the
//! README as needing on-device confirmation):
//!
//!   struct ANativeActivityCallbacks {           // 16 function pointers, 128 bytes
//!       void (*onStart)(ANativeActivity*);                   // +0
//!       void (*onResume)(ANativeActivity*);                  // +8
//!       void* (*onSaveInstanceState)(ANativeActivity*, size_t*); // +16
//!       void (*onPause)(ANativeActivity*);                   // +24
//!       void (*onStop)(ANativeActivity*);                    // +32
//!       void (*onDestroy)(ANativeActivity*);                 // +40
//!       void (*onWindowFocusChanged)(ANativeActivity*, int); // +48
//!       void (*onNativeWindowCreated)(ANativeActivity*, ANativeWindow*); // +56
//!       void (*onNativeWindowResized)(ANativeActivity*, ANativeWindow*); // +64
//!       void (*onNativeWindowRedrawNeeded)(ANativeActivity*, ANativeWindow*); // +72
//!       void (*onNativeWindowDestroyed)(ANativeActivity*, ANativeWindow*);   // +80
//!       void (*onInputQueueCreated)(ANativeActivity*, AInputQueue*);        // +88
//!       void (*onInputQueueDestroyed)(ANativeActivity*, AInputQueue*);      // +96
//!       void (*onContentRectChanged)(ANativeActivity*, const ARect*);       // +104
//!       void (*onConfigurationChanged)(ANativeActivity*);                  // +112
//!       void (*onLowMemory)(ANativeActivity*);                            // +120
//!   };
//!
//!   struct ANativeActivity { void* callbacks; /* ...JNI fields... */ };
//!   // `callbacks` is the first field, at offset 0 — this is the one field
//!   // this experiment relies on.
//!
//!   struct ANativeWindow_Buffer {
//!       int32_t width;    // +0
//!       int32_t height;   // +4
//!       int32_t stride;   // +8   (pixels per row, may be >= width)
//!       int32_t format;   // +12
//!       void*   bits;     // +16
//!       uint32_t reserved[6]; // +24..47
//!   };
//!
//!   int32_t ANativeWindow_setBuffersGeometry(ANativeWindow*, int32_t width,
//!       int32_t height, int32_t format);   // format 1 = WINDOW_FORMAT_RGBA_8888
//!   int32_t ANativeWindow_lock(ANativeWindow*, ANativeWindow_Buffer*, ARect*);
//!   int32_t ANativeWindow_unlockAndPost(ANativeWindow*);
//!
//! Pixel format: WINDOW_FORMAT_RGBA_8888 stores bytes R,G,B,A per pixel; read
//! as a little-endian u32 that is 0xAABBGGRR. Opaque blue (R=0,G=0,B=255,
//! A=255) is therefore the 32-bit word 0xFFFF0000.

const std = @import("std");
const a64 = @import("aarch64.zig");

pub const CALLBACKS_STRUCT_SIZE: u64 = 128;
pub const CB_OFFSET_ON_NATIVE_WINDOW_CREATED: u16 = 56;
pub const ACTIVITY_OFFSET_CALLBACKS: u16 = 0;

pub const BLUE_RGBA8888_LE: u32 = 0xFFFF0000;
pub const WINDOW_FORMAT_RGBA_8888: u32 = 1;

pub const GotLayout = struct {
    set_buffers_geometry: u64,
    lock: u64,
    unlock_and_post: u64,
};

/// Build the full .text machine code: `ANativeActivity_onCreate` immediately
/// followed by `onNativeWindowCreated`. Call once with all-zero addresses to
/// discover the size (`result.items.len * 4`), then call again with the
/// real, final virtual addresses once the surrounding ELF layout is known —
/// the instruction *sequence* is identical both times, only the embedded
/// address immediates differ, so the byte count is stable across both calls.
pub fn buildText(
    allocator: std.mem.Allocator,
    text_vaddr: u64,
    got: GotLayout,
    data_callbacks_vaddr: u64,
) !std.ArrayList(u32) {
    var out = try std.ArrayList(u32).initCapacity(allocator, 40);
    errdefer out.deinit();

    // ── ANativeActivity_onCreate(x0=activity, x1=savedState, x2=savedStateSize) ──
    const on_create_start = out.items.len;
    std.debug.assert(on_create_start == 0);

    try emitAdrpAdd(&out, text_vaddr, 9, data_callbacks_vaddr);
    // (function 2's address is only known once we've counted this function's
    // own length below — patched in after the fact, see `windowCreatedAddr`)
    const adrp_window_created_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onNativeWindowCreated
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_CREATED)); // g_callbacks.onNativeWindowCreated = x10
    try out.append(a64.strX(9, 0, ACTIVITY_OFFSET_CALLBACKS)); // activity->callbacks = &g_callbacks
    try out.append(a64.ret(a64.lr));

    const on_window_created_start = out.items.len;
    const on_window_created_vaddr = text_vaddr + on_window_created_start * 4;

    // Patch the two placeholders now that this function's address is known.
    {
        const pc = text_vaddr + adrp_window_created_idx * 4;
        out.items[adrp_window_created_idx] = a64.adrp(10, pc, on_window_created_vaddr);
        out.items[adrp_window_created_idx + 1] = a64.addImm64(10, 10, @truncate(on_window_created_vaddr & 0xfff));
    }

    // ── onNativeWindowCreated(x0=activity, x1=window) ──
    // Stack frame: [0..47] = ANativeWindow_Buffer, [56..63] = saved window ptr.
    try out.append(a64.subImm64(a64.sp, a64.sp, 64));
    try out.append(a64.strX(1, a64.sp, 56)); // save window

    // ANativeWindow_setBuffersGeometry(window, 0, 0, RGBA_8888)
    try out.append(a64.movReg64(0, 1)); // x0 = window
    try out.append(a64.movz32(1, 0, 0)); // w1 = 0 (width: keep current)
    try out.append(a64.movz32(2, 0, 0)); // w2 = 0 (height: keep current)
    try out.append(a64.movz32(3, @intCast(WINDOW_FORMAT_RGBA_8888), 0)); // w3 = format
    try emitGotCall(&out, text_vaddr, 9, got.set_buffers_geometry);

    // ANativeWindow_lock(window, &buffer, NULL)
    try out.append(a64.ldrX(0, a64.sp, 56)); // x0 = window
    try out.append(a64.addImm64(1, a64.sp, 0)); // x1 = &buffer
    try out.append(a64.movReg64(2, a64.xzr)); // x2 = NULL
    try emitGotCall(&out, text_vaddr, 9, got.lock);

    // if (result != 0) goto epilogue;  (forward branch, patched below)
    const cbnz_lock_failed_idx = out.items.len;
    try out.append(0);

    // Fill: total_pixels = buffer.height * buffer.stride
    try out.append(a64.ldrW(9, a64.sp, 4)); // w9 = height (zero-extends x9)
    try out.append(a64.ldrW(10, a64.sp, 8)); // w10 = stride (zero-extends x10)
    try out.append(a64.mul64(11, 9, 10)); // x11 = total pixel count
    try out.append(a64.ldrX(12, a64.sp, 16)); // x12 = buffer.bits
    try out.append(a64.movz32(13, @truncate(BLUE_RGBA8888_LE), 0));
    try out.append(a64.movk32(13, @truncate(BLUE_RGBA8888_LE >> 16), 1));

    // if (total_pixels == 0) goto skip_fill;  (forward branch, patched below)
    const cbz_zero_pixels_idx = out.items.len;
    try out.append(0);

    const loop_start = out.items.len;
    try out.append(a64.strwPostIndex(13, 12, 4)); // *bits++ = blue
    try out.append(a64.subImm64(11, 11, 1));
    try out.append(a64.cbnzX(11, @as(i32, @intCast(loop_start)) * 4 - @as(i32, @intCast(out.items.len)) * 4));

    const skip_fill = out.items.len;
    out.items[cbz_zero_pixels_idx] = a64.cbzX(11, @as(i32, @intCast(skip_fill)) * 4 - @as(i32, @intCast(cbz_zero_pixels_idx)) * 4);

    // ANativeWindow_unlockAndPost(window)
    try out.append(a64.ldrX(0, a64.sp, 56)); // x0 = window
    try emitGotCall(&out, text_vaddr, 9, got.unlock_and_post);

    const epilogue = out.items.len;
    out.items[cbnz_lock_failed_idx] = a64.cbnzW(0, @as(i32, @intCast(epilogue)) * 4 - @as(i32, @intCast(cbnz_lock_failed_idx)) * 4);

    try out.append(a64.addImm64(a64.sp, a64.sp, 64));
    try out.append(a64.ret(a64.lr));

    return out;
}

/// ADRP + ADD idiom: put the absolute address of `target` (anywhere in our
/// own image) into `reg`, PC-relative, no relocation needed.
fn emitAdrpAdd(out: *std.ArrayList(u32), text_vaddr: u64, reg: a64.Reg, target: u64) !void {
    const pc = text_vaddr + out.items.len * 4;
    try out.append(a64.adrp(reg, pc, target));
    try out.append(a64.addImm64(reg, reg, @truncate(target & 0xfff)));
}

/// ADRP + LDR + BLR idiom: call an imported function through its GOT slot
/// (filled in by the dynamic linker's R_AARCH64_GLOB_DAT relocations before
/// our code ever runs — see elf_so.zig).
fn emitGotCall(out: *std.ArrayList(u32), text_vaddr: u64, scratch: a64.Reg, got_slot_vaddr: u64) !void {
    const pc = text_vaddr + out.items.len * 4;
    try out.append(a64.adrp(scratch, pc, got_slot_vaddr));
    try out.append(a64.ldrX(scratch, scratch, @truncate(got_slot_vaddr & 0xfff)));
    try out.append(a64.blr(scratch));
}

test "buildText is deterministic in length across the two-pass call" {
    const alloc = std.testing.allocator;
    var pass1 = try buildText(alloc, 0, .{ .set_buffers_geometry = 0, .lock = 0, .unlock_and_post = 0 }, 0);
    defer pass1.deinit();
    var pass2 = try buildText(alloc, 0x2000, .{ .set_buffers_geometry = 0x3000, .lock = 0x3008, .unlock_and_post = 0x3010 }, 0x3020);
    defer pass2.deinit();
    try std.testing.expectEqual(pass1.items.len, pass2.items.len);
    try std.testing.expect(pass1.items.len > 30);
}
