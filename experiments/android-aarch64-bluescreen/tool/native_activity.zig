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
//!   // `callbacks` is the first field, at offset 0. By the time
//!   // ANativeActivity_onCreate runs, the framework has already allocated
//!   // an ANativeActivityCallbacks struct and pointed `callbacks` at it --
//!   // real-device testing (see buildText's comment) confirmed the
//!   // contract is to write INTO that existing struct through the pointer
//!   // (`activity->callbacks->onNativeWindowCreated = ...`), not to
//!   // replace the pointer with one of our own.
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

pub const CB_OFFSET_ON_SAVE_INSTANCE_STATE: u16 = 16;
pub const CB_OFFSET_ON_NATIVE_WINDOW_CREATED: u16 = 56;
pub const CB_OFFSET_ON_NATIVE_WINDOW_RESIZED: u16 = 64;
pub const CB_OFFSET_ON_NATIVE_WINDOW_REDRAW_NEEDED: u16 = 72;
pub const CB_OFFSET_ON_INPUT_QUEUE_CREATED: u16 = 88;
pub const CB_OFFSET_ON_INPUT_QUEUE_DESTROYED: u16 = 96;
pub const ACTIVITY_OFFSET_CALLBACKS: u16 = 0;
/// `struct ANativeActivity { void* callbacks; JavaVM* vm; JNIEnv* env;
/// jobject clazz; ... }` (android/native_activity.h) -- needed now that
/// fixDisplayCutoutMode below makes this experiment's first-ever JNI calls.
pub const ACTIVITY_OFFSET_ENV: u16 = 16;
pub const ACTIVITY_OFFSET_CLAZZ: u16 = 24;

pub const BLUE_RGBA8888_LE: u32 = 0xFFFF0000;
pub const RED_RGBA8888_LE: u32 = 0xFF0000FF;
pub const GREEN_RGBA8888_LE: u32 = 0xFF00FF00;
pub const YELLOW_RGBA8888_LE: u32 = 0xFF00FFFF;
pub const WINDOW_FORMAT_RGBA_8888: u32 = 1;

/// The ALooper "ident" this experiment's input queue is attached under.
/// Irrelevant here beyond being >= 0 -- since a non-null callback is
/// supplied to AInputQueue_attachLooper, the looper invokes that callback
/// directly instead of ever returning this ident from ALooper_pollOnce.
pub const LOOPER_IDENT_INPUT: u32 = 1;

/// AInputEvent_getType's result for a touch/motion event (android/input.h).
pub const AINPUT_EVENT_TYPE_MOTION: u32 = 2;
/// AMotionEvent_getAction's low byte for a finger just touching down
/// (android/input.h's AMOTION_EVENT_ACTION_DOWN). Compared against the
/// *unmasked* action int below rather than `action & AMOTION_EVENT_ACTION_MASK`
/// first, since this encoder has no bitwise AND -- correct for this
/// experiment's single-finger taps (pointer index 0 contributes nothing to
/// the masked-off upper bits), but a multi-touch gesture with a nonzero
/// pointer index could in principle slip past this check unrecognized.
pub const AMOTION_EVENT_ACTION_DOWN: u32 = 0;

/// JNI function-table (`JNINativeInterface`) byte offsets -- ground-truthed
/// against the real JNI spec (stable since JNI 1.2, unchanged since; cross-
/// checked against both Oracle's own JNI Functions reference and a fetched
/// real AOSP jni.h, not trusted from memory alone). Every `JNIEnv*` is a
/// `JNINativeInterface**`, so calling any JNI function is
/// `functable = *env; fn = functable[INDEX]; fn(env, ...)` -- see
/// emitJniCall. Each index below is its position in the table (0-based)
/// times 8 (pointer size): FindClass=6, GetMethodID=33,
/// CallObjectMethod=34, CallVoidMethod=61, GetFieldID=94, SetIntField=109.
pub const JNI_FIND_CLASS: u32 = 48;
pub const JNI_GET_METHOD_ID: u32 = 264;
pub const JNI_CALL_OBJECT_METHOD: u32 = 272;
pub const JNI_CALL_VOID_METHOD: u32 = 488;
pub const JNI_GET_FIELD_ID: u32 = 752;
pub const JNI_SET_INT_FIELD: u32 = 872;
/// ExceptionClear=17 (index*8=136). A NULL result from FindClass/
/// GetMethodID/GetFieldID always means an exception is now pending on this
/// JNIEnv -- and per the JNI spec, calling almost any other JNI function
/// while an exception is pending is undefined behavior; ART's real,
/// observed behavior for that is a fatal abort of the whole process, not a
/// contained failure. fixDisplayCutoutMode's first version below didn't
/// check any of this and likely explains a real-device instant crash: no
/// class/method/field name typo needs to exist for a legitimate reason to
/// fail (a stricter classloader, a hidden-API policy change on some OS
/// build) and the very next JNI call would then abort the app. Every
/// handle this function depends on later is now null-checked immediately,
/// bailing out to a shared epilogue that clears any pending exception
/// before returning, so a failure here degrades to "cutout fix skipped"
/// instead of "app doesn't start".
pub const JNI_EXCEPTION_CLEAR: u32 = 136;

/// WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
/// ground-truthed against the real Android developer docs (not memorized):
/// "the window is always allowed to extend into the DisplayCutout areas on
/// the short edges of the screen" -- in every orientation, which is exactly
/// what fixDisplayCutoutMode below needs.
pub const LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES: u32 = 1;

/// Byte offsets within this experiment's own small writable `.data` block
/// (see elf_so.zig's `data_vaddr`) for its mutable app state -- never
/// touched by the dynamic linker (unlike `.got`), only read/written by our
/// own code below.
pub const DATA_OFFSET_CURRENT_COLOR: u64 = 0; // u32: the paint handler's fill color
pub const DATA_OFFSET_COLOR_INDEX: u64 = 4; // u32: which palette entry is current
pub const DATA_OFFSET_WINDOW: u64 = 8; // u64: most recently painted ANativeWindow*
pub const DATA_OFFSET_ACTIVITY: u64 = 16; // u64: the ANativeActivity* from onCreate

pub const GotLayout = struct {
    set_buffers_geometry: u64,
    lock: u64,
    unlock_and_post: u64,
    looper_for_thread: u64,
    input_queue_attach_looper: u64,
    input_queue_get_event: u64,
    input_queue_finish_event: u64,
    input_event_get_type: u64,
    motion_event_get_action: u64,
    input_queue_detach_looper: u64,
    malloc: u64,
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
    data_vaddr: u64,
) !std.ArrayList(u32) {
    var out = try std.ArrayList(u32).initCapacity(allocator, 40);
    errdefer out.deinit();

    // ── ANativeActivity_onCreate(x0=activity, x1=savedState, x2=savedStateSize) ──
    //
    // Root cause, found after an 8-round real-device diagnostic campaign
    // (crash-address leaks, an AOSP source cross-check of every struct
    // offset used here -- all correct -- and finally fetching the actual
    // JNI glue, frameworks/base/core/jni/android_app_NativeActivity.cpp):
    // `onSurfaceCreated_native` there checks
    // `code->callbacks.onNativeWindowCreated != NULL`, reading through
    // `activity->callbacks` as a pointer the FRAMEWORK already allocated
    // and pre-populated (as part of its own internal NativeCode object)
    // *before* calling our onCreate. The contract is to write INTO the
    // ANativeActivityCallbacks struct the framework already points us at
    // (`activity->callbacks->onNativeWindowCreated = ...`), not to
    // allocate our own separate struct and replace the pointer
    // (`activity->callbacks = &ourOwnStruct`) -- the latter (what this
    // code used to do) leaves the framework's own internal storage
    // untouched and all-NULL, so its guard silently never fires the
    // callback. No crash, no error, no visible effect at all -- which
    // matches everything observed on-device across every earlier round.
    // Now non-leaf (it calls fixDisplayCutoutMode via BLR near the end), so
    // -- per the same LR-preservation rule the paint handler below
    // documents, and got wrong once before there -- it must save its own
    // incoming LR before that call and restore it before its own `ret`,
    // even though nothing before this round ever touched LR here.
    const on_create_start = out.items.len;
    std.debug.assert(on_create_start == 0);
    try out.append(a64.subImm64(a64.sp, a64.sp, 16));
    try out.append(a64.strX(a64.lr, a64.sp, 0)); // save LR

    // Cache `activity` itself into this experiment's small `.data` block
    // (see elf_so.zig's `data_vaddr`): g_activity lets fixDisplayCutoutMode
    // (called at the end of this function) and a later touch event
    // (drainInputEvents) reach the JNI env/clazz and repaint without
    // either being threaded through as an extra parameter.
    try emitAdrpAdd(&out, text_vaddr, 10, data_vaddr + DATA_OFFSET_ACTIVITY);
    try out.append(a64.strX(0, 10, 0)); // g_activity = activity (x0, untouched so far)

    // Restore g_colorIndex/g_currentColor across a rotation instead of
    // always resetting to blue -- real-device feedback that a rotation
    // (which, per axml.zig's configChanges comment, now goes through a
    // full destroy-and-recreate of this activity, not an in-place resize)
    // was silently discarding whatever color a touch had already picked.
    // `saved_state`/`saved_state_size` (x1/x2, both still exactly as
    // handed to us -- unused until now) are Android's own standard
    // mechanism for this: `onSaveInstanceState` below hands the framework
    // a small malloc'd buffer before the old instance is destroyed, and
    // the framework passes that same buffer straight back here as
    // (savedState, savedStateSize) on the new instance's onCreate. A
    // fresh process launch (no prior instance to have saved anything) has
    // savedStateSize == 0, in which case this still defaults to blue/index
    // 0 exactly as before.
    const cbz_no_saved_state_idx = out.items.len;
    try out.append(0); // if savedStateSize(x2) == 0, goto use_default_index (patched below)
    try out.append(a64.ldrW(4, 1, 0)); // w4 = *savedState (the index onSaveInstanceState wrote)
    const b_index_ready_idx = out.items.len;
    try out.append(0); // branch to index_ready (patched below)

    const use_default_index_pos = out.items.len;
    out.items[cbz_no_saved_state_idx] = a64.cbzX(2, @as(i32, @intCast(use_default_index_pos)) * 4 - @as(i32, @intCast(cbz_no_saved_state_idx)) * 4);
    try out.append(a64.movz32(4, 0, 0)); // w4 = 0 (default index: blue)

    const index_ready = out.items.len;
    out.items[b_index_ready_idx] = a64.b(@as(i32, @intCast(index_ready)) * 4 - @as(i32, @intCast(b_index_ready_idx)) * 4);

    try emitAdrpAdd(&out, text_vaddr, 10, data_vaddr + DATA_OFFSET_COLOR_INDEX);
    try out.append(a64.strW(4, 10, 0)); // g_colorIndex = w4

    // color = colorForIndex(w4) -- its address is only known once this
    // whole function's length is counted, patched in after the fact like
    // every other forward reference in this function.
    try out.append(a64.movReg64(0, 4)); // x0 = index (colorForIndex's one arg)
    const adrp_color_for_index_idx = out.items.len;
    try out.append(0); // placeholder ADRP x3, colorForIndex
    try out.append(0); // placeholder ADD  x3, x3, #lo12
    try out.append(a64.blr(3)); // w0 = color
    try emitAdrpAdd(&out, text_vaddr, 10, data_vaddr + DATA_OFFSET_CURRENT_COLOR);
    try out.append(a64.strW(0, 10, 0)); // g_currentColor = color

    // Reload `activity` from g_activity rather than trusting x0: the
    // colorForIndex call above was free to clobber it (colorForIndex
    // itself never touches x0 beyond its own arg/return, but nothing
    // upstream of this point should keep relying on x0 staying live
    // across an intervening call regardless -- the same discipline this
    // file already uses everywhere else).
    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_ACTIVITY);
    try out.append(a64.ldrX(9, 9, 0)); // x9 = g_activity (== activity)
    try out.append(a64.ldrX(9, 9, ACTIVITY_OFFSET_CALLBACKS)); // x9 = activity->callbacks (framework-owned, already valid -- do not overwrite this pointer)

    // Register onSaveInstanceState -- see its own comment below for what
    // it hands the framework and why (the other half of the
    // savedState/savedStateSize restore above).
    const adrp_save_instance_state_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onSaveInstanceState
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_SAVE_INSTANCE_STATE)); // activity->callbacks->onSaveInstanceState = x10

    // (function 2's address is only known once we've counted this function's
    // own length below — patched in after the fact, see `windowCreatedAddr`)
    const adrp_window_created_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onNativeWindowCreated
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    // Register the handler for onNativeWindowCreated,
    // onNativeWindowResized, and onNativeWindowRedrawNeeded — all three
    // share the exact same `(activity, window) -> void` signature, and
    // this handler already re-reads the buffer's current
    // width/height/stride from ANativeWindow_lock's own out-param on every
    // call rather than caching them, so the same address serves all three
    // without any change.
    //
    // Both used to be left unregistered here: removed as a broad
    // diagnostic step while chasing an ANR ("Mlx Blue Screen reagiert
    // nicht") on the back gesture/home/idle-timeout, on the theory that
    // they might be firing repeatedly during transition animations and
    // each firing's synchronous Binder IPC
    // (ANativeWindow_lock/_unlockAndPost) was enough main-thread blocking
    // to trip the ANR watchdog. That theory was never actually confirmed —
    // the ANR *persisted* even after removing both, and was eventually
    // root-caused to something entirely unrelated: an unconsumed
    // AInputQueue (see onInputQueueCreated below), now fixed
    // independently. So there's no remaining evidence either callback
    // caused anything.
    //
    // Re-registered after real-device feedback that rotating the device
    // left a black bar on one edge. The primary fix for that turned out to
    // be elsewhere (axml.zig no longer declares `orientation|screenSize`
    // in `configChanges`, so a rotation now goes through Android's normal,
    // far-better-tested destroy-and-recreate path instead of resizing this
    // window in place — see that file's comment for why, including a real
    // NDK issue confirming even Google's own reference
    // android_native_app_glue.c never wires up either of these callbacks).
    // Both stay registered anyway as a harmless fallback for any resize
    // Android does dispatch to a still-live window without a restart (e.g.
    // multi-window drag-resize) — registering costs two extra
    // instructions and this handler is already proven idempotent and safe
    // to call repeatedly.
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_CREATED)); // activity->callbacks->onNativeWindowCreated = x10
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_RESIZED)); // activity->callbacks->onNativeWindowResized = x10 (same handler, x10 unchanged since the store above)
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_NATIVE_WINDOW_REDRAW_NEEDED)); // activity->callbacks->onNativeWindowRedrawNeeded = x10 (same handler, x10 still unchanged)
    // Also register onInputQueueCreated. `android.app.NativeActivity`
    // unconditionally calls `getWindow().takeInputQueue(this)` in its own
    // onCreate, which hands this app an input event queue -- Android then
    // expects native code to actively drain it (this is exactly what
    // android_native_app_glue.c's own internal plumbing does for every app
    // built on it). Found necessary after the callback-registration and
    // ANR-avoidance fixes above still left an ANR ("Mlx Blue Screen
    // reagiert nicht") on the back gesture, home, and idle screen-timeout
    // -- all of which involve input events (the back gesture is
    // fundamentally a touch/drag gesture) -- even with *no* window
    // callback left registered at all, proving the ANR wasn't about the
    // paint path anymore. An unconsumed input event sitting in the queue
    // forever is exactly what trips Android's "Input dispatching timed
    // out" watchdog. onInputQueueCreated below sets up a lightweight
    // drain via the existing main-thread ALooper (no new thread needed).
    //
    // x10 is free to reuse here: its previous value (the paint handler's
    // address) was already stored above, so this placeholder pair
    // overwrites it with onInputQueueCreated's own address instead.
    const adrp_input_queue_created_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onInputQueueCreated
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_INPUT_QUEUE_CREATED)); // activity->callbacks->onInputQueueCreated = x10

    // Also register onInputQueueDestroyed, its counterpart -- see its own
    // comment below for why: real-device feedback that combining rotation
    // with touch input broke, root-caused to never detaching the old
    // instance's input queue from its looper before that queue (and the
    // instance it belonged to) is torn down.
    const adrp_input_queue_destroyed_idx = out.items.len;
    try out.append(0); // placeholder ADRP x10, onInputQueueDestroyed
    try out.append(0); // placeholder ADD  x10, x10, #lo12
    try out.append(a64.strX(10, 9, CB_OFFSET_ON_INPUT_QUEUE_DESTROYED)); // activity->callbacks->onInputQueueDestroyed = x10

    // Fix the landscape display-cutout black bar (real-device feedback):
    // see fixDisplayCutoutMode's own comment, at the end of this file, for
    // the full mechanism. Its address is only known once this whole
    // function's length is counted -- patched in after the fact, the same
    // way onNativeWindowCreated's and onInputQueueCreated's addresses are
    // above.
    const adrp_fix_cutout_idx = out.items.len;
    try out.append(0); // placeholder ADRP x3, fixDisplayCutoutMode
    try out.append(0); // placeholder ADD  x3, x3, #lo12
    try out.append(a64.blr(3));

    try out.append(a64.ldrX(a64.lr, a64.sp, 0)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 16));
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
    // Android framework.
    //
    // The real reason this handler was never even reached, across many
    // rounds of real-device diagnosis, turned out to be entirely upstream
    // of this function -- see ANativeActivity_onCreate's comment above.
    // The lock/unlockAndPost failure traps that used to be here predate
    // that finding (added as a diagnostic while this handler's own
    // invocation was still in question) -- and confirmed real: with the
    // actual bug fixed and a real blue screen finally rendering, the very
    // next real-device test (the Android back gesture) crashed.
    // `ANativeWindow_lock` can and does legitimately fail while a window
    // is mid-teardown (exactly what the back gesture's dismiss animation
    // triggers), and that's supposed to be a normal, silent "skip this
    // frame" case for any well-behaved app, not a crash -- so the traps
    // were downgraded back to a plain skip once they'd done their job of
    // confirming the real bug was fixed.
    try out.append(a64.subImm64(a64.sp, a64.sp, 64));
    try out.append(a64.strX(a64.lr, a64.sp, 48)); // save LR
    try out.append(a64.strX(1, a64.sp, 56)); // save window

    // Keep g_window fresh across every paint (create/resize/redraw, and
    // now a touch-triggered repaint too), so drainInputEvents always has
    // an up-to-date window to hand back to this same function.
    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_WINDOW);
    try out.append(a64.strX(1, 9, 0)); // g_window = window

    // ANativeWindow_setBuffersGeometry(window, 0, 0, RGBA_8888) -- the
    // window's actual default surface format is PixelFormat.RGB_565 (16-bit,
    // per NativeActivity.java's onCreate, which calls
    // getWindow().setFormat(PixelFormat.RGB_565) before ever loading our
    // native code), not RGBA_8888, so this call is required, not optional:
    // without it, ANativeWindow_lock would hand back a 16-bit buffer that
    // the 32-bit fill loop below would misinterpret.
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
    try emitAdrpAdd(&out, text_vaddr, 13, data_vaddr + DATA_OFFSET_CURRENT_COLOR);
    try out.append(a64.ldrW(13, 13, 0)); // w13 = g_currentColor (touch-cycled fill color)

    // if (total_pixels == 0) goto skip_fill;  (forward branch, patched below)
    const cbz_zero_pixels_idx = out.items.len;
    try out.append(0);

    const loop_start = out.items.len;
    try out.append(a64.strwPostIndex(13, 12, 4)); // *bits++ = blue
    try out.append(a64.subImm64(11, 11, 1));
    try out.append(a64.cbnzX(11, @as(i32, @intCast(loop_start)) * 4 - @as(i32, @intCast(out.items.len)) * 4));

    const skip_fill = out.items.len;
    out.items[cbz_zero_pixels_idx] = a64.cbzX(11, @as(i32, @intCast(skip_fill)) * 4 - @as(i32, @intCast(cbz_zero_pixels_idx)) * 4);

    // ANativeWindow_unlockAndPost(window) -- its result isn't checked:
    // it's the last thing this handler does before the shared epilogue
    // either way, so there's nothing left to skip on failure.
    try out.append(a64.ldrX(0, a64.sp, 56)); // x0 = window
    try emitGotCall(&out, text_vaddr, 9, got.unlock_and_post);

    const epilogue = out.items.len;
    out.items[cbnz_lock_failed_idx] = a64.cbnzW(0, @as(i32, @intCast(epilogue)) * 4 - @as(i32, @intCast(cbnz_lock_failed_idx)) * 4);
    try out.append(a64.ldrX(a64.lr, a64.sp, 48)); // restore LR (both the lock-failed and normal paths land here)
    try out.append(a64.addImm64(a64.sp, a64.sp, 64));
    try out.append(a64.ret(a64.lr));

    const on_input_queue_created_start = out.items.len;
    const on_input_queue_created_vaddr = text_vaddr + on_input_queue_created_start * 4;
    {
        const pc = text_vaddr + adrp_input_queue_created_idx * 4;
        out.items[adrp_input_queue_created_idx] = a64.adrp(10, pc, on_input_queue_created_vaddr);
        out.items[adrp_input_queue_created_idx + 1] = a64.addImm64(10, 10, @truncate(on_input_queue_created_vaddr & 0xfff));
    }

    // ── onInputQueueCreated(x0=activity, x1=queue) ──
    // Stack frame: [16..23] = saved queue, [24..31] = saved LR.
    // Attaches `queue` to this thread's already-running ALooper (the main
    // thread's -- onInputQueueCreated runs on it, same as every other
    // ANativeActivityCallbacks callback, so ALooper_forThread() here finds
    // the looper Android's own runtime already prepared for it) with
    // drainInputEvents (below) as the per-event callback. This mirrors
    // exactly what android_native_app_glue.c does, minus its extra thread
    // — ALooper_pollOnce's own caller is the main thread's existing
    // message loop, so no new thread is needed here, just this one-time
    // registration.
    try out.append(a64.subImm64(a64.sp, a64.sp, 32));
    try out.append(a64.strX(a64.lr, a64.sp, 24)); // save LR (this is a non-leaf function; see the LR-preservation note above)
    try out.append(a64.strX(1, a64.sp, 16)); // save queue

    try emitGotCall(&out, text_vaddr, 9, got.looper_for_thread); // x0 = ALooper_forThread()
    try out.append(a64.movReg64(1, 0)); // x1 = looper
    try out.append(a64.ldrX(0, a64.sp, 16)); // x0 = queue
    try out.append(a64.movReg64(4, 0)); // x4 = data = queue (threaded through to drainInputEvents's 3rd arg)
    try out.append(a64.movz32(2, LOOPER_IDENT_INPUT, 0)); // w2 = ident

    const adrp_drain_callback_idx = out.items.len;
    try out.append(0); // placeholder ADRP x3, drainInputEvents
    try out.append(0); // placeholder ADD  x3, x3, #lo12

    // AInputQueue_attachLooper(queue=x0, looper=x1, ident=w2, callback=x3, data=x4)
    try emitGotCall(&out, text_vaddr, 9, got.input_queue_attach_looper);

    try out.append(a64.ldrX(a64.lr, a64.sp, 24)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 32));
    try out.append(a64.ret(a64.lr));

    const drain_callback_start = out.items.len;
    const drain_callback_vaddr = text_vaddr + drain_callback_start * 4;
    {
        const pc = text_vaddr + adrp_drain_callback_idx * 4;
        out.items[adrp_drain_callback_idx] = a64.adrp(3, pc, drain_callback_vaddr);
        out.items[adrp_drain_callback_idx + 1] = a64.addImm64(3, 3, @truncate(drain_callback_vaddr & 0xfff));
    }

    // ── drainInputEvents(w0=fd, w1=events, x2=data=queue) ──
    // The ALooper_callbackFunc registered above. Called by the main
    // thread's own message loop whenever the input queue's fd has data.
    // Does nothing with the events themselves (this experiment has no use
    // for input) beyond immediately finishing each one -- the point is
    // purely to keep Android's "Input dispatching timed out" watchdog from
    // ever finding an unconsumed event sitting in the queue. Returns 1
    // (w0) to keep receiving future callbacks, per ALooper_callbackFunc's
    // documented contract.
    // Stack frame: [0..7] = AInputEvent* out-param for getEvent,
    // [16..23] = saved queue, [24..31] = saved LR.
    try out.append(a64.subImm64(a64.sp, a64.sp, 32));
    try out.append(a64.strX(a64.lr, a64.sp, 24)); // save LR (non-leaf: calls getEvent/finishEvent via BLR)
    try out.append(a64.strX(2, a64.sp, 16)); // save queue (arrives in x2, the callback's "data" param)

    const drain_loop_top = out.items.len;
    try out.append(a64.ldrX(0, a64.sp, 16)); // x0 = queue
    try out.append(a64.addImm64(1, a64.sp, 0)); // x1 = &outEvent
    try emitGotCall(&out, text_vaddr, 9, got.input_queue_get_event); // w0 = result (negative once the queue is drained)

    // if (result < 0) goto drain_done;  (forward branch, patched below)
    const tbnz_drain_done_idx = out.items.len;
    try out.append(0);

    // Touch-to-cycle-colors: on a finger touching down, advance through a
    // small color palette and repaint immediately -- added after
    // real-device feedback asking to test touch this way. See
    // AMOTION_EVENT_ACTION_DOWN's comment above for the one known
    // limitation (no bitwise AND here, so the action int isn't masked).
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = event
    try emitGotCall(&out, text_vaddr, 9, got.input_event_get_type); // w0 = AInputEvent_getType(event)
    try out.append(a64.subImm64(9, 0, AINPUT_EVENT_TYPE_MOTION));
    const cbnz_not_motion_idx = out.items.len;
    try out.append(0); // if type != MOTION, skip touch handling (forward branch, patched below)

    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = event (reload: getType above clobbered it)
    try emitGotCall(&out, text_vaddr, 9, got.motion_event_get_action); // w0 = AMotionEvent_getAction(event)
    try out.append(a64.subImm64(9, 0, AMOTION_EVENT_ACTION_DOWN));
    const cbnz_not_down_idx = out.items.len;
    try out.append(0); // if action != DOWN, skip touch handling (forward branch, patched below)

    // Advance g_colorIndex (0->1->2->3->0) and g_currentColor to match --
    // written as a 4-case if/elif/elif/else chain rather than an
    // index-scaled table jump: this encoder has no register-offset
    // addressing mode or bitwise AND for a mod-4, and four cases is cheap
    // to just write out directly.
    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_COLOR_INDEX);
    try out.append(a64.ldrW(10, 9, 0)); // w10 = g_colorIndex; x9 = &g_colorIndex, kept live so every case below can store its new index through it directly

    try out.append(a64.subImm64(11, 10, 0)); // index - 0
    const skip_case0_idx = out.items.len;
    try out.append(0); // if index != 0, try the next case (patched below)
    try out.append(a64.movz32(12, 1, 0));
    try out.append(a64.strW(12, 9, 0)); // g_colorIndex = 1
    try out.append(a64.movz32(13, @truncate(RED_RGBA8888_LE), 0));
    try out.append(a64.movk32(13, @truncate(RED_RGBA8888_LE >> 16), 1));
    const done_case0_idx = out.items.len;
    try out.append(0); // branch to color_chosen (patched below)

    const case1_start = out.items.len;
    out.items[skip_case0_idx] = a64.cbnzX(11, @as(i32, @intCast(case1_start)) * 4 - @as(i32, @intCast(skip_case0_idx)) * 4);
    try out.append(a64.subImm64(11, 10, 1)); // index - 1
    const skip_case1_idx = out.items.len;
    try out.append(0);
    try out.append(a64.movz32(12, 2, 0));
    try out.append(a64.strW(12, 9, 0)); // g_colorIndex = 2
    try out.append(a64.movz32(13, @truncate(GREEN_RGBA8888_LE), 0));
    try out.append(a64.movk32(13, @truncate(GREEN_RGBA8888_LE >> 16), 1));
    const done_case1_idx = out.items.len;
    try out.append(0);

    const case2_start = out.items.len;
    out.items[skip_case1_idx] = a64.cbnzX(11, @as(i32, @intCast(case2_start)) * 4 - @as(i32, @intCast(skip_case1_idx)) * 4);
    try out.append(a64.subImm64(11, 10, 2)); // index - 2
    const skip_case2_idx = out.items.len;
    try out.append(0);
    try out.append(a64.movz32(12, 3, 0));
    try out.append(a64.strW(12, 9, 0)); // g_colorIndex = 3
    try out.append(a64.movz32(13, @truncate(YELLOW_RGBA8888_LE), 0));
    try out.append(a64.movk32(13, @truncate(YELLOW_RGBA8888_LE >> 16), 1));
    const done_case2_idx = out.items.len;
    try out.append(0);

    const case3_start = out.items.len;
    out.items[skip_case2_idx] = a64.cbnzX(11, @as(i32, @intCast(case3_start)) * 4 - @as(i32, @intCast(skip_case2_idx)) * 4);
    // else (index == 3, or any unexpected value): wrap back to blue.
    try out.append(a64.movz32(12, 0, 0));
    try out.append(a64.strW(12, 9, 0)); // g_colorIndex = 0
    try out.append(a64.movz32(13, @truncate(BLUE_RGBA8888_LE), 0));
    try out.append(a64.movk32(13, @truncate(BLUE_RGBA8888_LE >> 16), 1));
    // falls straight through to color_chosen, no branch needed

    const color_chosen = out.items.len;
    out.items[done_case0_idx] = a64.b(@as(i32, @intCast(color_chosen)) * 4 - @as(i32, @intCast(done_case0_idx)) * 4);
    out.items[done_case1_idx] = a64.b(@as(i32, @intCast(color_chosen)) * 4 - @as(i32, @intCast(done_case1_idx)) * 4);
    out.items[done_case2_idx] = a64.b(@as(i32, @intCast(color_chosen)) * 4 - @as(i32, @intCast(done_case2_idx)) * 4);

    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_CURRENT_COLOR);
    try out.append(a64.strW(13, 9, 0)); // g_currentColor = the chosen color

    // Repaint now with the new color: call the shared paint handler
    // directly (x0=activity is unused by it, x1=window from g_window --
    // see its own comment). Its address (on_window_created_vaddr) is
    // already known at this point in the emission order, unlike the other
    // ADRP+ADD sites in this file that reference addresses not yet known
    // when emitted, so no forward-patch is needed here.
    try emitAdrpAdd(&out, text_vaddr, 1, data_vaddr + DATA_OFFSET_WINDOW);
    try out.append(a64.ldrX(1, 1, 0)); // x1 = g_window
    try out.append(a64.movz32(0, 0, 0)); // x0 = 0 (unused by the paint handler)
    try emitAdrpAdd(&out, text_vaddr, 3, on_window_created_vaddr);
    try out.append(a64.blr(3));

    const touch_handled = out.items.len;
    out.items[cbnz_not_motion_idx] = a64.cbnzX(9, @as(i32, @intCast(touch_handled)) * 4 - @as(i32, @intCast(cbnz_not_motion_idx)) * 4);
    out.items[cbnz_not_down_idx] = a64.cbnzX(9, @as(i32, @intCast(touch_handled)) * 4 - @as(i32, @intCast(cbnz_not_down_idx)) * 4);

    try out.append(a64.ldrX(0, a64.sp, 16)); // x0 = queue
    try out.append(a64.ldrX(1, a64.sp, 0)); // x1 = event (written by getEvent above)
    try out.append(a64.movz32(2, 0, 0)); // w2 = handled = 0
    try emitGotCall(&out, text_vaddr, 9, got.input_queue_finish_event);

    const branch_back_idx = out.items.len;
    try out.append(a64.b(@as(i32, @intCast(drain_loop_top)) * 4 - @as(i32, @intCast(branch_back_idx)) * 4));

    const drain_done = out.items.len;
    out.items[tbnz_drain_done_idx] = a64.tbnz(0, 31, @as(i32, @intCast(drain_done)) * 4 - @as(i32, @intCast(tbnz_drain_done_idx)) * 4);

    try out.append(a64.ldrX(a64.lr, a64.sp, 24)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 32));
    try out.append(a64.movz32(0, 1, 0)); // w0 = 1 (keep receiving future callbacks)
    try out.append(a64.ret(a64.lr));

    const fix_cutout_start = out.items.len;
    const fix_cutout_vaddr = text_vaddr + fix_cutout_start * 4;
    {
        const pc = text_vaddr + adrp_fix_cutout_idx * 4;
        out.items[adrp_fix_cutout_idx] = a64.adrp(3, pc, fix_cutout_vaddr);
        out.items[adrp_fix_cutout_idx + 1] = a64.addImm64(3, 3, @truncate(fix_cutout_vaddr & 0xfff));
    }

    // ── fixDisplayCutoutMode() -- reads g_activity, takes no other args ──
    //
    // Real-device feedback: rotating to landscape left a black bar over
    // the area next to the display cutout (the front camera). Confirmed
    // via Android's own documentation rather than guessed at: "The
    // platform default allows a window under the cutout in portrait, but
    // avoids it in landscape, leaving a black bar over the cutout area"
    // -- exactly this symptom. The fix is to set
    // `WindowManager.LayoutParams.layoutInDisplayCutoutMode` to
    // LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES (1, see its own comment
    // above), which lets the window extend into the cutout area on the
    // screen's short edges in every orientation. That field can only be
    // set via a custom theme (a resources.arsc/style writer this
    // experiment doesn't have) or, as here, directly through JNI -- this
    // project's first use of it. Equivalent to the Java:
    //   Window w = activity.getWindow();
    //   WindowManager.LayoutParams lp = w.getAttributes();
    //   lp.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    //   w.setAttributes(lp);
    // Every class/method/field name below is a real, public Android API
    // (android.app.Activity.getWindow, android.view.Window.
    // getAttributes/setAttributes, android.view.WindowManager.
    // LayoutParams.layoutInDisplayCutoutMode). getAttributes/setAttributes
    // are declared directly on the public abstract Window class, so
    // FindClass on that (rather than the actual hidden PhoneWindow runtime
    // subclass getWindow() returns) is enough -- JNI resolves
    // inherited/overridden methods correctly starting from either the
    // declaring class or a subclass.
    //
    // A real-device crash (SIGILL, ILL_ILLOPC, fault PC landing squarely on
    // the raw bytes of "android/app/NativeActivity" read as an
    // instruction) confirmed the very first version of this function had a
    // real bug: each emitCString call site emitted its string's bytes
    // directly in the middle of straight-line code, immediately after a
    // normal instruction and before another one, with nothing to jump
    // over it. Ordinary sequential execution doesn't know those bytes
    // aren't code -- it just fell straight from the last real instruction
    // into the string data and tried to execute it. Fixed by packing every
    // C string this function needs into one block, jumped over with a
    // single unconditional branch before any of it is reached, immediately
    // after this function's own entry point -- every string's address is
    // still just an ordinary ADRP+ADD against an address already known by
    // the time it's used below, same as before.
    const skip_strings_idx = out.items.len;
    try out.append(0); // placeholder B real_start (patched below)

    const str_native_activity = try emitCString(&out, text_vaddr, "android/app/NativeActivity");
    const str_get_window = try emitCString(&out, text_vaddr, "getWindow");
    const str_get_window_sig = try emitCString(&out, text_vaddr, "()Landroid/view/Window;");
    const str_window = try emitCString(&out, text_vaddr, "android/view/Window");
    const str_get_attrs = try emitCString(&out, text_vaddr, "getAttributes");
    const str_get_attrs_sig = try emitCString(&out, text_vaddr, "()Landroid/view/WindowManager$LayoutParams;");
    const str_set_attrs = try emitCString(&out, text_vaddr, "setAttributes");
    const str_set_attrs_sig = try emitCString(&out, text_vaddr, "(Landroid/view/WindowManager$LayoutParams;)V");
    const str_layout_params = try emitCString(&out, text_vaddr, "android/view/WindowManager$LayoutParams");
    const str_cutout_field = try emitCString(&out, text_vaddr, "layoutInDisplayCutoutMode");
    const str_int_sig = try emitCString(&out, text_vaddr, "I");

    const real_start = out.items.len;
    out.items[skip_strings_idx] = a64.b(@as(i32, @intCast(real_start)) * 4 - @as(i32, @intCast(skip_strings_idx)) * 4);

    // Stack frame (96 bytes, 16-aligned): [0]=env [8]=activityObj
    // [16]=windowClass [24]=layoutParamsClass [32]=windowObj [40]=attrsObj
    // [48]=getWindowMid [56]=getAttrsMid [64]=setAttrsMid
    // [72]=cutoutModeFid [80]=saved LR. Every JNI call below reloads its
    // arguments from this frame rather than trusting a register to still
    // hold an earlier result, since emitJniCall's own ADRP+LDR sequence
    // (and every JNI call itself) is free to clobber any caller-saved
    // register -- the same discipline drainInputEvents already uses
    // around its own GOT calls.
    //
    // Every handle a later call depends on is null-checked immediately
    // after it's produced, bailing out to a shared epilogue (cutout_bail)
    // that clears any pending JNI exception before returning, rather than
    // pressing on: a NULL from FindClass/GetMethodID/GetFieldID always
    // means an exception is now pending on this JNIEnv, and calling
    // almost any other JNI function while one is pending is undefined
    // behavior -- ART's real, observed response to that is a fatal abort
    // of the whole process, not a contained failure. This wasn't what
    // caused the crash above (that was the string-data bug), but it's
    // still a real gap this function's first version had, worth closing
    // now rather than leaving as a second latent way to take the app down
    // if any class/method/field lookup ever legitimately fails on some
    // other OS build.
    try out.append(a64.subImm64(a64.sp, a64.sp, 96));
    try out.append(a64.strX(a64.lr, a64.sp, 80)); // save LR (non-leaf: many JNI calls via BLR)

    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_ACTIVITY);
    try out.append(a64.ldrX(9, 9, 0)); // x9 = g_activity
    try out.append(a64.ldrX(0, 9, ACTIVITY_OFFSET_ENV)); // x0 = activity->env (JNIEnv*)
    try out.append(a64.strX(0, a64.sp, 0)); // save env
    try out.append(a64.ldrX(1, 9, ACTIVITY_OFFSET_CLAZZ)); // x1 = activity->clazz
    try out.append(a64.strX(1, a64.sp, 8)); // save activityObj

    // activityClass = FindClass(env, "android/app/NativeActivity")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try emitAdrpAdd(&out, text_vaddr, 1, str_native_activity);
    try emitJniCall(&out, JNI_FIND_CLASS); // x0 = activityClass
    const bail_activity_class_idx = out.items.len;
    try out.append(0); // if activityClass == NULL, bail (patched below)
    try out.append(a64.movReg64(9, 0)); // x9 = activityClass (kept live for the very next call only)

    // getWindowMid = GetMethodID(env, activityClass, "getWindow", "()Landroid/view/Window;")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.movReg64(1, 9)); // x1 = activityClass
    try emitAdrpAdd(&out, text_vaddr, 2, str_get_window);
    try emitAdrpAdd(&out, text_vaddr, 3, str_get_window_sig);
    try emitJniCall(&out, JNI_GET_METHOD_ID); // x0 = getWindowMid
    const bail_get_window_mid_idx = out.items.len;
    try out.append(0); // if getWindowMid == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 48)); // save getWindowMid

    // windowClass = FindClass(env, "android/view/Window")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try emitAdrpAdd(&out, text_vaddr, 1, str_window);
    try emitJniCall(&out, JNI_FIND_CLASS); // x0 = windowClass
    const bail_window_class_idx = out.items.len;
    try out.append(0); // if windowClass == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 16)); // save windowClass
    try out.append(a64.movReg64(9, 0)); // x9 = windowClass (kept live for the next call only)

    // getAttrsMid = GetMethodID(env, windowClass, "getAttributes", "()Landroid/view/WindowManager$LayoutParams;")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.movReg64(1, 9)); // x1 = windowClass
    try emitAdrpAdd(&out, text_vaddr, 2, str_get_attrs);
    try emitAdrpAdd(&out, text_vaddr, 3, str_get_attrs_sig);
    try emitJniCall(&out, JNI_GET_METHOD_ID); // x0 = getAttrsMid
    const bail_get_attrs_mid_idx = out.items.len;
    try out.append(0); // if getAttrsMid == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 56)); // save getAttrsMid

    // setAttrsMid = GetMethodID(env, windowClass, "setAttributes", "(Landroid/view/WindowManager$LayoutParams;)V")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.ldrX(1, a64.sp, 16)); // x1 = windowClass (reload: the previous call clobbered x9)
    try emitAdrpAdd(&out, text_vaddr, 2, str_set_attrs);
    try emitAdrpAdd(&out, text_vaddr, 3, str_set_attrs_sig);
    try emitJniCall(&out, JNI_GET_METHOD_ID); // x0 = setAttrsMid
    const bail_set_attrs_mid_idx = out.items.len;
    try out.append(0); // if setAttrsMid == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 64)); // save setAttrsMid

    // layoutParamsClass = FindClass(env, "android/view/WindowManager$LayoutParams")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try emitAdrpAdd(&out, text_vaddr, 1, str_layout_params);
    try emitJniCall(&out, JNI_FIND_CLASS); // x0 = layoutParamsClass
    const bail_layout_params_class_idx = out.items.len;
    try out.append(0); // if layoutParamsClass == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 24)); // save layoutParamsClass
    try out.append(a64.movReg64(9, 0)); // x9 = layoutParamsClass (kept live for the next call only)

    // cutoutModeFid = GetFieldID(env, layoutParamsClass, "layoutInDisplayCutoutMode", "I")
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.movReg64(1, 9)); // x1 = layoutParamsClass
    try emitAdrpAdd(&out, text_vaddr, 2, str_cutout_field);
    try emitAdrpAdd(&out, text_vaddr, 3, str_int_sig);
    try emitJniCall(&out, JNI_GET_FIELD_ID); // x0 = cutoutModeFid
    const bail_cutout_mode_fid_idx = out.items.len;
    try out.append(0); // if cutoutModeFid == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 72)); // save cutoutModeFid

    // windowObj = CallObjectMethod(env, activityObj, getWindowMid)
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.ldrX(1, a64.sp, 8)); // x1 = activityObj
    try out.append(a64.ldrX(2, a64.sp, 48)); // x2 = getWindowMid
    try emitJniCall(&out, JNI_CALL_OBJECT_METHOD); // x0 = windowObj
    const bail_window_obj_idx = out.items.len;
    try out.append(0); // if windowObj == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 32)); // save windowObj

    // attrsObj = CallObjectMethod(env, windowObj, getAttrsMid)
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.ldrX(1, a64.sp, 32)); // x1 = windowObj
    try out.append(a64.ldrX(2, a64.sp, 56)); // x2 = getAttrsMid
    try emitJniCall(&out, JNI_CALL_OBJECT_METHOD); // x0 = attrsObj
    const bail_attrs_obj_idx = out.items.len;
    try out.append(0); // if attrsObj == NULL, bail (patched below)
    try out.append(a64.strX(0, a64.sp, 40)); // save attrsObj

    // attrsObj.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.ldrX(1, a64.sp, 40)); // x1 = attrsObj
    try out.append(a64.ldrX(2, a64.sp, 72)); // x2 = cutoutModeFid
    try out.append(a64.movz32(3, @truncate(LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES), 0)); // w3 = 1
    try emitJniCall(&out, JNI_SET_INT_FIELD);

    // window.setAttributes(attrsObj)
    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try out.append(a64.ldrX(1, a64.sp, 32)); // x1 = windowObj
    try out.append(a64.ldrX(2, a64.sp, 64)); // x2 = setAttrsMid
    try out.append(a64.ldrX(3, a64.sp, 40)); // x3 = attrsObj
    try emitJniCall(&out, JNI_CALL_VOID_METHOD);

    // cutout_bail: every early-out above lands here too. Clearing any
    // pending exception is harmless when none is pending (ExceptionClear
    // is documented as a no-op in that case) and required when one is, so
    // it's unconditional rather than guarded.
    const cutout_bail = out.items.len;
    out.items[bail_activity_class_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_activity_class_idx)) * 4);
    out.items[bail_get_window_mid_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_get_window_mid_idx)) * 4);
    out.items[bail_window_class_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_window_class_idx)) * 4);
    out.items[bail_get_attrs_mid_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_get_attrs_mid_idx)) * 4);
    out.items[bail_set_attrs_mid_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_set_attrs_mid_idx)) * 4);
    out.items[bail_layout_params_class_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_layout_params_class_idx)) * 4);
    out.items[bail_cutout_mode_fid_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_cutout_mode_fid_idx)) * 4);
    out.items[bail_window_obj_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_window_obj_idx)) * 4);
    out.items[bail_attrs_obj_idx] = a64.cbzX(0, @as(i32, @intCast(cutout_bail)) * 4 - @as(i32, @intCast(bail_attrs_obj_idx)) * 4);

    try out.append(a64.ldrX(0, a64.sp, 0)); // x0 = env
    try emitJniCall(&out, JNI_EXCEPTION_CLEAR);

    try out.append(a64.ldrX(a64.lr, a64.sp, 80)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 96));
    try out.append(a64.ret(a64.lr));

    const color_for_index_start = out.items.len;
    const color_for_index_vaddr = text_vaddr + color_for_index_start * 4;
    {
        const pc = text_vaddr + adrp_color_for_index_idx * 4;
        out.items[adrp_color_for_index_idx] = a64.adrp(3, pc, color_for_index_vaddr);
        out.items[adrp_color_for_index_idx + 1] = a64.addImm64(3, 3, @truncate(color_for_index_vaddr & 0xfff));
    }

    // ── colorForIndex(w0=index) -> w0=color ── a pure leaf function, no
    // calls, no data of any kind embedded in it (deliberately, after the
    // real-device crash the rest of this file's comments document: never
    // again mix data into a straight-line instruction stream without an
    // unconditional jump over it). Mirrors drainInputEvents' own
    // touch-cycle palette (index 0/1/2/3 -> blue/red/green/yellow) by
    // deliberate duplication rather than a shared helper: this function
    // exists solely so onCreate's savedState-restore path (above) can
    // recompute g_currentColor for a restored g_colorIndex without
    // touching drainInputEvents' already real-device-verified logic at
    // all -- any unexpected index (should never happen; only ever written
    // by our own onSaveInstanceState) safely falls back to blue, same as
    // index 0.
    try out.append(a64.subImm64(9, 0, 0)); // index - 0
    const cfi_skip0_idx = out.items.len;
    try out.append(0); // if index != 0, try next (patched below)
    try out.append(a64.movz32(0, @truncate(BLUE_RGBA8888_LE), 0));
    try out.append(a64.movk32(0, @truncate(BLUE_RGBA8888_LE >> 16), 1));
    try out.append(a64.ret(a64.lr));

    const cfi_try1 = out.items.len;
    out.items[cfi_skip0_idx] = a64.cbnzX(9, @as(i32, @intCast(cfi_try1)) * 4 - @as(i32, @intCast(cfi_skip0_idx)) * 4);
    try out.append(a64.subImm64(9, 0, 1)); // index - 1
    const cfi_skip1_idx = out.items.len;
    try out.append(0);
    try out.append(a64.movz32(0, @truncate(RED_RGBA8888_LE), 0));
    try out.append(a64.movk32(0, @truncate(RED_RGBA8888_LE >> 16), 1));
    try out.append(a64.ret(a64.lr));

    const cfi_try2 = out.items.len;
    out.items[cfi_skip1_idx] = a64.cbnzX(9, @as(i32, @intCast(cfi_try2)) * 4 - @as(i32, @intCast(cfi_skip1_idx)) * 4);
    try out.append(a64.subImm64(9, 0, 2)); // index - 2
    const cfi_skip2_idx = out.items.len;
    try out.append(0);
    try out.append(a64.movz32(0, @truncate(GREEN_RGBA8888_LE), 0));
    try out.append(a64.movk32(0, @truncate(GREEN_RGBA8888_LE >> 16), 1));
    try out.append(a64.ret(a64.lr));

    const cfi_else = out.items.len;
    out.items[cfi_skip2_idx] = a64.cbnzX(9, @as(i32, @intCast(cfi_else)) * 4 - @as(i32, @intCast(cfi_skip2_idx)) * 4);
    // index == 3 (the only remaining case this function is ever actually
    // called with).
    try out.append(a64.movz32(0, @truncate(YELLOW_RGBA8888_LE), 0));
    try out.append(a64.movk32(0, @truncate(YELLOW_RGBA8888_LE >> 16), 1));
    try out.append(a64.ret(a64.lr));

    const save_instance_state_start = out.items.len;
    const save_instance_state_vaddr = text_vaddr + save_instance_state_start * 4;
    {
        const pc = text_vaddr + adrp_save_instance_state_idx * 4;
        out.items[adrp_save_instance_state_idx] = a64.adrp(10, pc, save_instance_state_vaddr);
        out.items[adrp_save_instance_state_idx + 1] = a64.addImm64(10, 10, @truncate(save_instance_state_vaddr & 0xfff));
    }

    // ── onSaveInstanceState(x0=activity[unused], x1=outLen) -> x0=void* ──
    // Called on the OLD activity instance before it's destroyed (e.g. by
    // the destroy-and-recreate a rotation now triggers) -- the other half
    // of onCreate's savedState-restore path above. Per the real NDK
    // contract: "the returned data will be freed by the caller using
    // free(), so this data should be allocated using malloc()" -- this
    // project's first use of libc's allocator (elf_so.zig's second
    // DT_NEEDED, `libc.so`, added just for this). `*outLen` (a `size_t*`,
    // 8 bytes on this LP64 ABI, not 4) must be filled in with however many
    // bytes were allocated.
    // Stack frame (32 bytes): [0]=allocated ptr, [8]=saved outLen(x1),
    // [16]=saved LR.
    try out.append(a64.subImm64(a64.sp, a64.sp, 32));
    try out.append(a64.strX(a64.lr, a64.sp, 16)); // save LR (non-leaf: calls malloc via BLR)
    try out.append(a64.strX(1, a64.sp, 8)); // save outLen ptr

    try out.append(a64.movz32(0, 4, 0)); // x0 = 4 (bytes to allocate: one u32 color index)
    try emitGotCall(&out, text_vaddr, 9, got.malloc); // x0 = malloc(4)
    try out.append(a64.strX(0, a64.sp, 0)); // save allocated ptr

    try emitAdrpAdd(&out, text_vaddr, 9, data_vaddr + DATA_OFFSET_COLOR_INDEX);
    try out.append(a64.ldrW(10, 9, 0)); // w10 = g_colorIndex
    try out.append(a64.ldrX(0, a64.sp, 0)); // reload allocated ptr
    try out.append(a64.strW(10, 0, 0)); // *ptr = g_colorIndex

    try out.append(a64.ldrX(1, a64.sp, 8)); // reload outLen ptr
    try out.append(a64.movz32(9, 4, 0)); // x9 = 4 (movz32 zero-extends into the full 64-bit x9, so this is a correct 8-byte size_t value, not just the low 32 bits)
    try out.append(a64.strX(9, 1, 0)); // *outLen = 4

    try out.append(a64.ldrX(0, a64.sp, 0)); // return value = allocated ptr
    try out.append(a64.ldrX(a64.lr, a64.sp, 16)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 32));
    try out.append(a64.ret(a64.lr));

    const input_queue_destroyed_start = out.items.len;
    const input_queue_destroyed_vaddr = text_vaddr + input_queue_destroyed_start * 4;
    {
        const pc = text_vaddr + adrp_input_queue_destroyed_idx * 4;
        out.items[adrp_input_queue_destroyed_idx] = a64.adrp(10, pc, input_queue_destroyed_vaddr);
        out.items[adrp_input_queue_destroyed_idx + 1] = a64.addImm64(10, 10, @truncate(input_queue_destroyed_vaddr & 0xfff));
    }

    // ── onInputQueueDestroyed(x0=activity[unused], x1=queue) ──
    // Real-device feedback: combining rotation with touch input broke.
    // Root cause: this activity's input queue was never detached from its
    // ALooper before being torn down. A rotation now destroys and
    // recreates the whole activity (see axml.zig's configChanges
    // comment), which destroys the OLD instance's input queue too -- and
    // per the real NDK docs, `AInputQueue_detachLooper` must be called
    // before that happens, exactly what `onInputQueueDestroyed` exists
    // for (android_native_app_glue.c's own internal plumbing does this
    // same call in its own onInputQueueDestroyed handler). Without it, the
    // stale attachment from the old, now-destroyed queue/looper pairing
    // was left dangling right as the new instance's onInputQueueCreated
    // (above) set up a fresh one -- consistent with input only misbehaving
    // specifically around a rotation, not otherwise.
    try out.append(a64.subImm64(a64.sp, a64.sp, 16));
    try out.append(a64.strX(a64.lr, a64.sp, 0)); // save LR (non-leaf: calls AInputQueue_detachLooper via BLR)
    try out.append(a64.movReg64(0, 1)); // x0 = queue
    try emitGotCall(&out, text_vaddr, 9, got.input_queue_detach_looper);
    try out.append(a64.ldrX(a64.lr, a64.sp, 0)); // restore LR
    try out.append(a64.addImm64(a64.sp, a64.sp, 16));
    try out.append(a64.ret(a64.lr));

    return out;
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

/// ADRP + ADD idiom: put the absolute address of `target` (anywhere in our
/// own image) into `reg`, PC-relative, no relocation needed.
fn emitAdrpAdd(out: *std.ArrayList(u32), text_vaddr: u64, reg: a64.Reg, target: u64) !void {
    const pc = text_vaddr + out.items.len * 4;
    try out.append(a64.adrp(reg, pc, target));
    try out.append(a64.addImm64(reg, reg, @truncate(target & 0xfff)));
}

/// JNI calling idiom, used only by fixDisplayCutoutMode: given `env`
/// (JNIEnv*) already loaded into x0 and any other real arguments already
/// loaded into x1.. per the target JNI function's signature, calls through
/// the JNI function table. `env` is a `JNINativeInterface**`, so reaching
/// any JNI function needs two dereferences: one to get the table, one --
/// at `offset`, one of the JNI_* byte offsets above -- to get the actual
/// function pointer. Uses x9 as scratch (never x0/x1/x2/x3, which may hold
/// live call arguments) so it never clobbers the arguments the caller just
/// set up; every JNI function's first parameter is `env` itself, so x0
/// must already hold it going in and this helper leaves it untouched.
fn emitJniCall(out: *std.ArrayList(u32), offset: u32) !void {
    try out.append(a64.ldrX(9, 0, 0)); // x9 = *env (the JNINativeInterface* function table)
    try out.append(a64.ldrX(9, 9, @truncate(offset))); // x9 = functable[offset] (the actual function pointer)
    try out.append(a64.blr(9));
}

/// Packs `s` plus a NUL terminator into `out` as raw data words -- never
/// executed, just readable bytes living in the same R+X `.text` segment as
/// every instruction here (harmless: nothing ever branches into it, and
/// AArch64 doesn't care that non-instruction bytes sit in an executable
/// page unless something actually tries to run them as code). Returns the
/// vaddr the string starts at, for the caller to hand to emitAdrpAdd the
/// same way any function address in this file already is.
fn emitCString(out: *std.ArrayList(u32), text_vaddr: u64, s: []const u8) !u64 {
    const start_vaddr = text_vaddr + out.items.len * 4;
    var i: usize = 0;
    while (i < s.len) : (i += 4) {
        var word: u32 = 0;
        var j: u5 = 0;
        while (j < 4) : (j += 1) {
            const byte_val: u32 = if (i + j < s.len) @intCast(s[i + j]) else 0;
            word |= byte_val << (j * 8);
        }
        try out.append(word);
    }
    // The loop above already zero-pads (hence NUL-terminates) any partial
    // final word, but a length that's an exact multiple of 4 needs one
    // more all-zero word for the terminator.
    if (s.len % 4 == 0) {
        try out.append(0);
    }
    return start_vaddr;
}

test "buildText is deterministic in length across the two-pass call" {
    const alloc = std.testing.allocator;
    var pass1 = try buildText(alloc, 0, .{
        .set_buffers_geometry = 0,
        .lock = 0,
        .unlock_and_post = 0,
        .looper_for_thread = 0,
        .input_queue_attach_looper = 0,
        .input_queue_get_event = 0,
        .input_queue_finish_event = 0,
        .input_event_get_type = 0,
        .motion_event_get_action = 0,
        .input_queue_detach_looper = 0,
        .malloc = 0,
    }, 0);
    defer pass1.deinit();
    var pass2 = try buildText(alloc, 0x2000, .{
        .set_buffers_geometry = 0x30c0,
        .lock = 0x30c8,
        .unlock_and_post = 0x30d0,
        .looper_for_thread = 0x30d8,
        .input_queue_attach_looper = 0x30e0,
        .input_queue_get_event = 0x30e8,
        .input_queue_finish_event = 0x30f0,
        .input_event_get_type = 0x30f8,
        .motion_event_get_action = 0x3100,
        .input_queue_detach_looper = 0x3108,
        .malloc = 0x3110,
    }, 0x3118);
    defer pass2.deinit();
    try std.testing.expectEqual(pass1.items.len, pass2.items.len);
    try std.testing.expect(pass1.items.len > 300);
}
