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
pub const CB_OFFSET_ON_NATIVE_WINDOW_RESIZED: u16 = 64;
pub const CB_OFFSET_ON_NATIVE_WINDOW_REDRAW_NEEDED: u16 = 72;
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
    //
    // Diagnostic history: an unconditional trap at the very start of the
    // window handler (below) proved the window-created/resized/redraw-
    // needed callback is never invoked at all on a real device, with no
    // crash. Moving that trap to this function's own first instruction
    // then proved ANativeActivity_onCreate itself *does* run (it crashed
    // immediately on launch). The ANativeActivityCallbacks/ANativeActivity
    // struct offsets used throughout this file were then cross-checked
    // against the real AOSP source (android.googlesource.com,
    // frameworks/native/include/android/native_activity.h) rather than
    // memory, and matched exactly (callbacks@0, onNativeWindowCreated@56,
    // onNativeWindowResized@64, onNativeWindowRedrawNeeded@72) -- ruling
    // out a struct-offset mistake as the explanation too.
    //
    // Next: verify the one thing that's still just an assumption -- that
    // `x0` genuinely points to a real ANativeActivity struct laid out the
    // way we expect, as opposed to something unexpected on this specific
    // real device/OS build. `activity->sdkVersion` (an int32_t at offset
    // 48) is read and then dereferenced as a pointer, which -- since a
    // real SDK version number (e.g. 34) is never a valid memory address --
    // is guaranteed to SIGSEGV. Android's crash report reliably includes
    // the exact faulting address, which is exactly the leaked sdkVersion
    // value: a real, plausible SDK level (confirms x0 and the struct
    // layout are right, and something else entirely is preventing the
    // window callback) vs. 0 or something implausible (means x0 or the
    // struct layout assumption is wrong after all). No adb needed -- just
    // the crash report's fault address. Remove once answered.
    const on_create_start = out.items.len;
    std.debug.assert(on_create_start == 0);
    try out.append(a64.ldrW(9, 0, 48)); // w9 = activity->sdkVersion
    try out.append(a64.ldrX(a64.xzr, 9, 0)); // deref w9/x9 as an address -- deliberate SIGSEGV, fault addr == sdkVersion

    try emitAdrpAdd(&out, text_vaddr, 9, data_callbacks_vaddr);
    // (function 2's address is only known once we've counted this function's
    // own length below — patched in after the fact, see `windowCreatedAddr`)
    const adrp_window_created_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onNativeWindowCreated
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    // Register the same handler for onNativeWindowCreated, onNativeWindowResized,
    // and onNativeWindowRedrawNeeded — all three share the identical
    // (ANativeActivity*, ANativeWindow*) signature, and our handler already
    // ignores the activity argument. A raw NativeActivity (no
    // android_native_app_glue) can have its very first onNativeWindowCreated
    // paint happen before the window is actually attached/composited and get
    // silently discarded with no further redraw ever requested from us;
    // registering onNativeWindowRedrawNeeded too (the system's explicit
    // "please draw now, it's safe" signal) is the standard fix, matching what
    // android_native_app_glue's own sample apps do by redrawing on more than
    // just window-created. Found via real-device testing (blue fill silently
    // not appearing on a Pixel 10 Pro / Android Canary build) — see the
    // mlx-native port's identical fix and the README for the diagnosis.
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_CREATED)); // g_callbacks.onNativeWindowCreated = x10
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_RESIZED)); // g_callbacks.onNativeWindowResized = x10
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_REDRAW_NEEDED)); // g_callbacks.onNativeWindowRedrawNeeded = x10
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
    // Stack frame: [0..47] = ANativeWindow_Buffer, [48..55] = saved LR,
    // [56..63] = saved window ptr.
    //
    // This function is NOT a leaf: it calls ANativeWindow_setBuffersGeometry/
    // _lock/_unlockAndPost via BLR, and BLR overwrites LR (x30) with the
    // return address *within this function* on every call. Per AAPCS64, a
    // non-leaf function must save its incoming LR before making any call and
    // restore it before its own `ret` -- otherwise the final `ret` uses
    // whatever the *last* BLR left in LR (pointing back into this function's
    // own body, not to our caller), so control never actually returns to the
    // Android framework. Missing this was the real cause of a real-device
    // black screen (the buffer was filled and posted, but the handler never
    // returned cleanly afterward) even after the onNativeWindowCreated-only
    // registration bug was fixed.
    //
    // Diagnostic history: after removing setBuffersGeometry and trapping on
    // a failing lock/unlockAndPost (see below), a real device was still
    // black with no crash -- ruling out either of those calls returning
    // failure. An unconditional trap as this handler's own first
    // instruction was then tried and also produced no crash, proving this
    // handler is never invoked by the framework at all -- see the
    // now-active trap at the very start of ANativeActivity_onCreate above
    // instead, which checks the more basic question of whether onCreate
    // itself even runs.
    try out.append(a64.subImm64(a64.sp, a64.sp, 64));
    try out.append(a64.strX(a64.lr, a64.sp, 48)); // save LR
    try out.append(a64.strX(1, a64.sp, 56)); // save window

    // ANativeWindow_lock(window, &buffer, NULL) -- deliberately no
    // ANativeWindow_setBuffersGeometry(window, 0, 0, RGBA_8888) call before
    // this (there used to be one): after fixing the callback-registration
    // and LR-preservation bugs, a real device still showed solid black
    // (confirmed independent of the window's theme/background -- a
    // SurfaceView shows black by default until a buffer is actually
    // posted, so this points at the lock/fill/post path itself). With no
    // adb/logcat access to see setBuffersGeometry's or lock's return codes
    // directly, this removes setBuffersGeometry as a variable (lock will
    // use the window's current/default size and format instead) and adds
    // explicit failure traps below so a crash report -- if lock or
    // unlockAndPost do fail -- pinpoints which one via its distinct PC.
    try out.append(a64.ldrX(0, a64.sp, 56)); // x0 = window
    try out.append(a64.addImm64(1, a64.sp, 0)); // x1 = &buffer
    try out.append(a64.movReg64(2, a64.xzr)); // x2 = NULL
    try emitGotCall(&out, text_vaddr, 9, got.lock);

    // if (result != 0) goto trap_lock_failed;  (forward branch, patched below)
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

    // if (result != 0) goto trap_post_failed;  (forward branch, patched below)
    const cbnz_post_failed_idx = out.items.len;
    try out.append(0);

    const epilogue = out.items.len;
    try out.append(a64.ldrX(a64.lr, a64.sp, 48)); // restore LR (only the success path reaches here)
    try out.append(a64.addImm64(a64.sp, a64.sp, 64));
    try out.append(a64.ret(a64.lr));

    const trap_lock_failed = out.items.len;
    try out.append(a64.udf(0));
    out.items[cbnz_lock_failed_idx] = a64.cbnzW(0, @as(i32, @intCast(trap_lock_failed)) * 4 - @as(i32, @intCast(cbnz_lock_failed_idx)) * 4);

    const trap_post_failed = out.items.len;
    try out.append(a64.udf(0));
    out.items[cbnz_post_failed_idx] = a64.cbnzW(0, @as(i32, @intCast(trap_post_failed)) * 4 - @as(i32, @intCast(cbnz_post_failed_idx)) * 4);

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
