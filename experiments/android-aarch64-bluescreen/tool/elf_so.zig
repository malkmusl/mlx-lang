//! Hand-written ELF64 *shared object* (`ET_DYN`) writer for `aarch64`,
//! targeting the Android/Bionic dynamic linker.
//!
//! This is the AArch64/Android sibling of `compiler/bootstrap/object/elf64.zig`
//! (mlx0's real x86_64/Linux *executable* writer): same idea — no external
//! assembler, no external linker, every byte placed by hand — extended to a
//! dynamically-linked shared library, which is what `ANativeActivity`
//! requires (Android loads it with `dlopen`).
//!
//! Binding strategy: rather than a lazy PLT (which needs `.plt`/`DT_PLTGOT`/
//! `DT_JMPREL` machinery), every imported symbol gets an eager
//! `R_AARCH64_GLOB_DAT` relocation in `.rela.dyn`. The dynamic linker
//! resolves these — writing real function addresses into `.got` — before
//! any code in this library runs, i.e. before `ANativeActivity_onCreate` is
//! ever called. Our machine code then just does ADRP+LDR+BLR through the
//! GOT slot. This needs no PLT stubs at all.
//!
//! File layout:
//!   0x0000  ELF header
//!   0x0040  Program headers (4: LOAD ro, LOAD rx, LOAD rw, DYNAMIC)
//!   0x1000  [LOAD #1, R]   .dynsym  .dynstr  .hash  .rela.dyn
//!   0x2000  [LOAD #2, R+X] .text
//!   0x3000  [LOAD #3, R+W] .dynamic  .got  .data
//!   ...     .shstrtab + section header table (not in any PT_LOAD — for
//!           `readelf`/`objdump` friendliness only; Bionic's linker never
//!           looks at section headers, only program headers + PT_DYNAMIC)
const std = @import("std");
const a64 = @import("aarch64.zig");
const na = @import("native_activity.zig");

const ELFMAG = "\x7fELF";
const ELFCLASS64: u8 = 2;
const ELFDATA2LSB: u8 = 1;
const EV_CURRENT: u8 = 1;
const ELFOSABI_NONE: u8 = 0;

const ET_DYN: u16 = 3;
const EM_AARCH64: u16 = 183;

const PT_LOAD: u32 = 1;
const PT_DYNAMIC: u32 = 2;
const PF_X: u32 = 1;
const PF_W: u32 = 2;
const PF_R: u32 = 4;

const SHT_NULL: u32 = 0;
const SHT_PROGBITS: u32 = 1;
const SHT_HASH: u32 = 5;
const SHT_DYNAMIC: u32 = 6;
const SHT_STRTAB: u32 = 3;
const SHT_DYNSYM: u32 = 11;
const SHT_RELA: u32 = 4;
const SHF_WRITE: u64 = 0x1;
const SHF_ALLOC: u64 = 0x2;
const SHF_EXECINSTR: u64 = 0x4;

const STB_GLOBAL: u8 = 1;
const STT_FUNC: u8 = 2;
const SHN_UNDEF: u16 = 0;

const DT_NULL: i64 = 0;
const DT_NEEDED: i64 = 1;
const DT_HASH: i64 = 4;
const DT_STRTAB: i64 = 5;
const DT_SYMTAB: i64 = 6;
const DT_RELA: i64 = 7;
const DT_RELASZ: i64 = 8;
const DT_RELAENT: i64 = 9;
const DT_STRSZ: i64 = 10;
const DT_SYMENT: i64 = 11;
const DT_SONAME: i64 = 14;

const R_AARCH64_GLOB_DAT: u32 = 1025;

const Elf64Ehdr = extern struct {
    e_ident: [16]u8,
    e_type: u16,
    e_machine: u16,
    e_version: u32,
    e_entry: u64,
    e_phoff: u64,
    e_shoff: u64,
    e_flags: u32,
    e_ehsize: u16,
    e_phentsize: u16,
    e_phnum: u16,
    e_shentsize: u16,
    e_shnum: u16,
    e_shstrndx: u16,
};
const Elf64Phdr = extern struct {
    p_type: u32,
    p_flags: u32,
    p_offset: u64,
    p_vaddr: u64,
    p_paddr: u64,
    p_filesz: u64,
    p_memsz: u64,
    p_align: u64,
};
const Elf64Shdr = extern struct {
    sh_name: u32,
    sh_type: u32,
    sh_flags: u64,
    sh_addr: u64,
    sh_offset: u64,
    sh_size: u64,
    sh_link: u32,
    sh_info: u32,
    sh_addralign: u64,
    sh_entsize: u64,
};
const Elf64Sym = extern struct {
    st_name: u32,
    st_info: u8,
    st_other: u8,
    st_shndx: u16,
    st_value: u64,
    st_size: u64,
};
const Elf64Rela = extern struct {
    r_offset: u64,
    r_info: u64,
    r_addend: i64,
};
const Elf64Dyn = extern struct {
    d_tag: i64,
    d_val: u64,
};

comptime {
    std.debug.assert(@sizeOf(Elf64Ehdr) == 64);
    std.debug.assert(@sizeOf(Elf64Phdr) == 56);
    std.debug.assert(@sizeOf(Elf64Shdr) == 64);
    std.debug.assert(@sizeOf(Elf64Sym) == 24);
    std.debug.assert(@sizeOf(Elf64Rela) == 24);
    std.debug.assert(@sizeOf(Elf64Dyn) == 16);
}

fn elfSymInfo(bind: u8, typ: u8) u8 {
    return (bind << 4) | (typ & 0xf);
}

/// SysV ELF hash (the classic algorithm from the System V ABI, unchanged
/// since the 1990s; Bionic accepts DT_HASH tables built with it).
fn elfHash(name: []const u8) u32 {
    var h: u32 = 0;
    for (name) |c| {
        h = (h << 4) +% c;
        const g = h & 0xf0000000;
        if (g != 0) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

const RO_VADDR: u64 = 0x1000;
const TEXT_VADDR: u64 = 0x2000;
const RW_VADDR: u64 = 0x3000;
const PAGE: u64 = 0x1000;

fn alignUp(x: u64, a: u64) u64 {
    return (x + a - 1) & ~(a - 1);
}

/// Build the complete `libmain.so` file bytes. `needed`/`needed2` become two
/// separate `DT_NEEDED` entries -- `libandroid.so` (every NDK/`libandroid`
/// import already used) and, as of this round, `libc.so` (for `malloc`,
/// needed by onSaveInstanceState's real, documented contract -- see
/// native_activity.zig's comment on it). A symbol's `.rela.dyn`/`.dynsym`
/// entry never has to say *which* of the two it resolves against: Bionic's
/// dynamic linker searches every `DT_NEEDED` dependency (in order) for each
/// undefined symbol, exactly how the existing single-library case already
/// works.
pub fn build(allocator: std.mem.Allocator, soname: []const u8, needed: []const u8, needed2: []const u8) ![]u8 {
    // ── .dynstr ─────────────────────────────────────────────────────────
    var dynstr = std.ArrayList(u8).init(allocator);
    defer dynstr.deinit();
    try dynstr.append(0); // index 0 = ""

    const Str = struct {
        fn add(list: *std.ArrayList(u8), s: []const u8) !u32 {
            const off: u32 = @intCast(list.items.len);
            try list.appendSlice(s);
            try list.append(0);
            return off;
        }
    };
    const name_set_buffers = try Str.add(&dynstr, "ANativeWindow_setBuffersGeometry");
    const name_lock = try Str.add(&dynstr, "ANativeWindow_lock");
    const name_unlock = try Str.add(&dynstr, "ANativeWindow_unlockAndPost");
    // These three (also part of the stable NDK libandroid.so, same as the
    // ANativeWindow_* imports above) back onInputQueueCreated's input-queue
    // drain, added to fix a real-device ANR -- see native_activity.zig's
    // buildText for the full story.
    const name_looper_for_thread = try Str.add(&dynstr, "ALooper_forThread");
    const name_input_queue_attach_looper = try Str.add(&dynstr, "AInputQueue_attachLooper");
    const name_input_queue_get_event = try Str.add(&dynstr, "AInputQueue_getEvent");
    const name_input_queue_finish_event = try Str.add(&dynstr, "AInputQueue_finishEvent");
    // Also part of the stable NDK libandroid.so (android/input.h), added for
    // the touch-to-cycle-colors feature: drainInputEvents inspects each
    // event's type/action instead of just finishing it unread.
    const name_input_event_get_type = try Str.add(&dynstr, "AInputEvent_getType");
    const name_motion_event_get_action = try Str.add(&dynstr, "AMotionEvent_getAction");
    // Also part of libandroid.so, added to fix real-device feedback that
    // combining rotation with touch input broke: onInputQueueDestroyed
    // (native_activity.zig) must detach the old, about-to-be-destroyed
    // input queue from its looper.
    const name_input_queue_detach_looper = try Str.add(&dynstr, "AInputQueue_detachLooper");
    // libc.so, not libandroid.so -- this project's first import from it
    // (hence the second DT_NEEDED, see build()'s own comment). Backs
    // onSaveInstanceState's real, documented contract: the buffer it
    // returns must be malloc'd, since the framework frees it with free().
    const name_malloc = try Str.add(&dynstr, "malloc");
    const name_on_create = try Str.add(&dynstr, "ANativeActivity_onCreate");
    const name_needed = try Str.add(&dynstr, needed);
    const name_needed2 = try Str.add(&dynstr, needed2);
    const name_soname = try Str.add(&dynstr, soname);

    // ── layout inside the RO segment ───────────────────────────────────
    const dynsym_count = 13; // null + 11 imports + 1 export
    const dynsym_off = RO_VADDR;
    const dynsym_size: u64 = dynsym_count * @sizeOf(Elf64Sym);

    const dynstr_off = dynsym_off + dynsym_size;
    const dynstr_size: u64 = dynstr.items.len;

    const hash_off = alignUp(dynstr_off + dynstr_size, 4);
    const nbucket: u32 = 1;
    const nchain: u32 = dynsym_count;
    const hash_size: u64 = (2 + nbucket + nchain) * 4;

    const rela_off = alignUp(hash_off + hash_size, 8);
    const rela_count = 11; // one GLOB_DAT per imported function
    const rela_size: u64 = rela_count * @sizeOf(Elf64Rela);

    const ro_end = rela_off + rela_size;
    std.debug.assert(ro_end <= TEXT_VADDR); // must fit in one page

    // ── .text (two-pass: discover size, then re-emit with final vaddrs) ──
    const got_vaddr = RW_VADDR + 192; // 192 = 12 Elf64Dyn entries * 16 bytes, see below
    const got = na.GotLayout{
        .set_buffers_geometry = got_vaddr,
        .lock = got_vaddr + 8,
        .unlock_and_post = got_vaddr + 16,
        .looper_for_thread = got_vaddr + 24,
        .input_queue_attach_looper = got_vaddr + 32,
        .input_queue_get_event = got_vaddr + 40,
        .input_queue_finish_event = got_vaddr + 48,
        .input_event_get_type = got_vaddr + 56,
        .motion_event_get_action = got_vaddr + 64,
        .input_queue_detach_looper = got_vaddr + 72,
        .malloc = got_vaddr + 80,
    };
    const got_size: u64 = 88; // 11 GOT slots * 8 bytes
    // `.data`: this experiment's own small, writable app-state block --
    // g_currentColor/g_colorIndex/g_window/g_activity, all written and read
    // only by our own code (never by the dynamic linker, unlike `.got`
    // above) -- see native_activity.zig's DATA_OFFSET_* constants for the
    // layout. Re-introduces a `.data` allocation after an earlier round
    // removed one (`g_callbacks`, a *different*, since-fixed mistake: that
    // one was replacing a framework-owned pointer instead of writing
    // through it -- see buildText's onCreate comment). This one is
    // legitimate mutable state with no such framework contract to violate.
    const data_vaddr = got_vaddr + got_size;
    const data_size: u64 = 24;
    var text = try na.buildText(allocator, TEXT_VADDR, got, data_vaddr);
    defer text.deinit();
    const text_size: u64 = text.items.len * 4;
    std.debug.assert(text_size <= PAGE);

    // ── layout inside the RW segment ───────────────────────────────────
    const dynamic_off = RW_VADDR;
    const dynamic_count = 12; // 2 DT_NEEDED (libandroid.so, libc.so) + 9 others + DT_NULL
    const dynamic_size: u64 = dynamic_count * @sizeOf(Elf64Dyn);
    std.debug.assert(RW_VADDR + dynamic_size == got_vaddr);
    const got_off = got_vaddr;
    const data_off = data_vaddr;
    const rw_end = data_off + data_size;
    std.debug.assert(rw_end <= RW_VADDR + PAGE);

    // ── .dynsym ─────────────────────────────────────────────────────────
    var dynsym: [dynsym_count]Elf64Sym = undefined;
    dynsym[0] = std.mem.zeroes(Elf64Sym);
    dynsym[1] = .{ .st_name = name_set_buffers, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[2] = .{ .st_name = name_lock, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[3] = .{ .st_name = name_unlock, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[4] = .{ .st_name = name_looper_for_thread, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[5] = .{ .st_name = name_input_queue_attach_looper, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[6] = .{ .st_name = name_input_queue_get_event, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[7] = .{ .st_name = name_input_queue_finish_event, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[8] = .{ .st_name = name_input_event_get_type, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[9] = .{ .st_name = name_motion_event_get_action, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[10] = .{ .st_name = name_input_queue_detach_looper, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[11] = .{ .st_name = name_malloc, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = SHN_UNDEF, .st_value = 0, .st_size = 0 };
    dynsym[12] = .{ .st_name = name_on_create, .st_info = elfSymInfo(STB_GLOBAL, STT_FUNC), .st_other = 0, .st_shndx = 6, .st_value = TEXT_VADDR, .st_size = text_size };

    // ── .hash (single bucket — O(n) lookup, fine for a handful of symbols) ──
    var hash_words = try std.ArrayList(u32).initCapacity(allocator, 2 + nbucket + nchain);
    defer hash_words.deinit();
    try hash_words.append(nbucket);
    try hash_words.append(nchain);
    try hash_words.append(1); // bucket[0] = symbol index 1
    try hash_words.append(0); // chain[0] (STN_UNDEF slot, unused)
    try hash_words.append(2); // chain[1] -> 2
    try hash_words.append(3); // chain[2] -> 3
    try hash_words.append(4); // chain[3] -> 4
    try hash_words.append(5); // chain[4] -> 5
    try hash_words.append(6); // chain[5] -> 6
    try hash_words.append(7); // chain[6] -> 7
    try hash_words.append(8); // chain[7] -> 8
    try hash_words.append(9); // chain[8] -> 9
    try hash_words.append(10); // chain[9] -> 10
    try hash_words.append(11); // chain[10] -> 11
    try hash_words.append(12); // chain[11] -> 12
    try hash_words.append(0); // chain[12] -> end
    std.debug.assert(hash_words.items.len == 2 + nbucket + nchain);
    // (elfHash is kept/exercised via the unit test below; a single-bucket
    // table doesn't need real hash values, only *a* consistent function.)
    _ = elfHash;

    // ── .rela.dyn — GLOB_DAT each GOT slot from its dynsym entry ─────────
    var relas: [rela_count]Elf64Rela = undefined;
    relas[0] = .{ .r_offset = got.set_buffers_geometry, .r_info = (@as(u64, 1) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[1] = .{ .r_offset = got.lock, .r_info = (@as(u64, 2) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[2] = .{ .r_offset = got.unlock_and_post, .r_info = (@as(u64, 3) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[3] = .{ .r_offset = got.looper_for_thread, .r_info = (@as(u64, 4) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[4] = .{ .r_offset = got.input_queue_attach_looper, .r_info = (@as(u64, 5) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[5] = .{ .r_offset = got.input_queue_get_event, .r_info = (@as(u64, 6) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[6] = .{ .r_offset = got.input_queue_finish_event, .r_info = (@as(u64, 7) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[7] = .{ .r_offset = got.input_event_get_type, .r_info = (@as(u64, 8) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[8] = .{ .r_offset = got.motion_event_get_action, .r_info = (@as(u64, 9) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[9] = .{ .r_offset = got.input_queue_detach_looper, .r_info = (@as(u64, 10) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };
    relas[10] = .{ .r_offset = got.malloc, .r_info = (@as(u64, 11) << 32) | R_AARCH64_GLOB_DAT, .r_addend = 0 };

    // ── .dynamic ───────────────────────────────────────────────────────
    var dyn: [dynamic_count]Elf64Dyn = undefined;
    dyn[0] = .{ .d_tag = DT_NEEDED, .d_val = name_needed };
    dyn[1] = .{ .d_tag = DT_NEEDED, .d_val = name_needed2 };
    dyn[2] = .{ .d_tag = DT_SONAME, .d_val = name_soname };
    dyn[3] = .{ .d_tag = DT_HASH, .d_val = hash_off };
    dyn[4] = .{ .d_tag = DT_STRTAB, .d_val = dynstr_off };
    dyn[5] = .{ .d_tag = DT_SYMTAB, .d_val = dynsym_off };
    dyn[6] = .{ .d_tag = DT_STRSZ, .d_val = dynstr_size };
    dyn[7] = .{ .d_tag = DT_SYMENT, .d_val = @sizeOf(Elf64Sym) };
    dyn[8] = .{ .d_tag = DT_RELA, .d_val = rela_off };
    dyn[9] = .{ .d_tag = DT_RELASZ, .d_val = rela_size };
    dyn[10] = .{ .d_tag = DT_RELAENT, .d_val = @sizeOf(Elf64Rela) };
    dyn[11] = .{ .d_tag = DT_NULL, .d_val = 0 };

    // ── section header string table (tooling only) ───────────────────────
    var shstrtab = std.ArrayList(u8).init(allocator);
    defer shstrtab.deinit();
    try shstrtab.append(0);
    const sh_dynsym = try Str.add(&shstrtab, ".dynsym");
    const sh_dynstr = try Str.add(&shstrtab, ".dynstr");
    const sh_hash = try Str.add(&shstrtab, ".hash");
    const sh_rela = try Str.add(&shstrtab, ".rela.dyn");
    const sh_dynamic = try Str.add(&shstrtab, ".dynamic");
    const sh_got = try Str.add(&shstrtab, ".got");
    const sh_text = try Str.add(&shstrtab, ".text");
    const sh_data = try Str.add(&shstrtab, ".data");
    const sh_shstrtab = try Str.add(&shstrtab, ".shstrtab");

    const shstrtab_off = alignUp(rw_end, 1);
    const shstrtab_size = shstrtab.items.len;
    const shdr_off = alignUp(shstrtab_off + shstrtab_size, 8);
    const shnum = 10; // null, dynsym, dynstr, hash, rela.dyn, dynamic, text, got, data, shstrtab
    const shdr_size = shnum * @sizeOf(Elf64Shdr);
    const file_size = shdr_off + shdr_size;

    // ── assemble the file ─────────────────────────────────────────────
    var buf = try allocator.alloc(u8, file_size);
    @memset(buf, 0);

    var ehdr: Elf64Ehdr = std.mem.zeroes(Elf64Ehdr);
    @memcpy(ehdr.e_ident[0..4], ELFMAG);
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_NONE;
    ehdr.e_type = ET_DYN;
    ehdr.e_machine = EM_AARCH64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0; // a library: no entry point, loaded via dlopen + dlsym
    ehdr.e_phoff = 0x40;
    ehdr.e_shoff = shdr_off;
    ehdr.e_ehsize = @sizeOf(Elf64Ehdr);
    ehdr.e_phentsize = @sizeOf(Elf64Phdr);
    ehdr.e_phnum = 4;
    ehdr.e_shentsize = @sizeOf(Elf64Shdr);
    ehdr.e_shnum = shnum;
    ehdr.e_shstrndx = 9; // index of .shstrtab among the 10 sections: null,dynsym,dynstr,hash,rela.dyn,dynamic,text,got,data,shstrtab
    writeStruct(buf, 0, Elf64Ehdr, ehdr);

    const phdrs = [4]Elf64Phdr{
        .{ .p_type = PT_LOAD, .p_flags = PF_R, .p_offset = 0, .p_vaddr = 0, .p_paddr = 0, .p_filesz = ro_end, .p_memsz = ro_end, .p_align = PAGE },
        .{ .p_type = PT_LOAD, .p_flags = PF_R | PF_X, .p_offset = TEXT_VADDR, .p_vaddr = TEXT_VADDR, .p_paddr = TEXT_VADDR, .p_filesz = text_size, .p_memsz = text_size, .p_align = PAGE },
        .{ .p_type = PT_LOAD, .p_flags = PF_R | PF_W, .p_offset = RW_VADDR, .p_vaddr = RW_VADDR, .p_paddr = RW_VADDR, .p_filesz = rw_end - RW_VADDR, .p_memsz = rw_end - RW_VADDR, .p_align = PAGE },
        .{ .p_type = PT_DYNAMIC, .p_flags = PF_R | PF_W, .p_offset = dynamic_off, .p_vaddr = dynamic_off, .p_paddr = dynamic_off, .p_filesz = dynamic_size, .p_memsz = dynamic_size, .p_align = 8 },
    };
    for (phdrs, 0..) |p, i| writeStruct(buf, 0x40 + i * @sizeOf(Elf64Phdr), Elf64Phdr, p);

    for (dynsym, 0..) |s, i| writeStruct(buf, dynsym_off + i * @sizeOf(Elf64Sym), Elf64Sym, s);
    @memcpy(buf[dynstr_off..][0..dynstr.items.len], dynstr.items);
    for (hash_words.items, 0..) |w, i| writeInt(buf, hash_off + i * 4, u32, w);
    for (relas, 0..) |r, i| writeStruct(buf, rela_off + i * @sizeOf(Elf64Rela), Elf64Rela, r);
    for (dyn, 0..) |d, i| writeStruct(buf, dynamic_off + i * @sizeOf(Elf64Dyn), Elf64Dyn, d);
    for (text.items, 0..) |w, i| writeInt(buf, TEXT_VADDR + i * 4, u32, w);
    // .got left zero — filled by the dynamic linker's GLOB_DAT relocations at load time.
    @memcpy(buf[shstrtab_off..][0..shstrtab.items.len], shstrtab.items);

    const shdrs = [shnum]Elf64Shdr{
        std.mem.zeroes(Elf64Shdr), // SHN_UNDEF
        .{ .sh_name = sh_dynsym, .sh_type = SHT_DYNSYM, .sh_flags = SHF_ALLOC, .sh_addr = dynsym_off, .sh_offset = dynsym_off, .sh_size = dynsym_size, .sh_link = 2, .sh_info = 1, .sh_addralign = 8, .sh_entsize = @sizeOf(Elf64Sym) },
        .{ .sh_name = sh_dynstr, .sh_type = SHT_STRTAB, .sh_flags = SHF_ALLOC, .sh_addr = dynstr_off, .sh_offset = dynstr_off, .sh_size = dynstr_size, .sh_link = 0, .sh_info = 0, .sh_addralign = 1, .sh_entsize = 0 },
        .{ .sh_name = sh_hash, .sh_type = SHT_HASH, .sh_flags = SHF_ALLOC, .sh_addr = hash_off, .sh_offset = hash_off, .sh_size = hash_size, .sh_link = 1, .sh_info = 0, .sh_addralign = 4, .sh_entsize = 4 },
        .{ .sh_name = sh_rela, .sh_type = SHT_RELA, .sh_flags = SHF_ALLOC, .sh_addr = rela_off, .sh_offset = rela_off, .sh_size = rela_size, .sh_link = 1, .sh_info = 0, .sh_addralign = 8, .sh_entsize = @sizeOf(Elf64Rela) },
        .{ .sh_name = sh_dynamic, .sh_type = SHT_DYNAMIC, .sh_flags = SHF_ALLOC | SHF_WRITE, .sh_addr = dynamic_off, .sh_offset = dynamic_off, .sh_size = dynamic_size, .sh_link = 2, .sh_info = 0, .sh_addralign = 8, .sh_entsize = @sizeOf(Elf64Dyn) },
        .{ .sh_name = sh_text, .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC | SHF_EXECINSTR, .sh_addr = TEXT_VADDR, .sh_offset = TEXT_VADDR, .sh_size = text_size, .sh_link = 0, .sh_info = 0, .sh_addralign = 4, .sh_entsize = 0 },
        .{ .sh_name = sh_got, .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC | SHF_WRITE, .sh_addr = got_off, .sh_offset = got_off, .sh_size = got_size, .sh_link = 0, .sh_info = 0, .sh_addralign = 8, .sh_entsize = 8 },
        .{ .sh_name = sh_data, .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC | SHF_WRITE, .sh_addr = data_off, .sh_offset = data_off, .sh_size = data_size, .sh_link = 0, .sh_info = 0, .sh_addralign = 8, .sh_entsize = 0 },
        .{ .sh_name = sh_shstrtab, .sh_type = SHT_STRTAB, .sh_flags = 0, .sh_addr = 0, .sh_offset = shstrtab_off, .sh_size = shstrtab_size, .sh_link = 0, .sh_info = 0, .sh_addralign = 1, .sh_entsize = 0 },
    };
    for (shdrs, 0..) |s, i| writeStruct(buf, shdr_off + i * @sizeOf(Elf64Shdr), Elf64Shdr, s);

    return buf;
}

fn writeStruct(buf: []u8, offset: u64, comptime T: type, value: T) void {
    const bytes = std.mem.asBytes(&value);
    @memcpy(buf[offset..][0..bytes.len], bytes);
}
fn writeInt(buf: []u8, offset: u64, comptime T: type, value: T) void {
    std.mem.writeInt(T, buf[offset..][0..@sizeOf(T)], value, .little);
}

test "elf_hash matches an independent reference implementation" {
    // Cross-checked against a from-scratch Python re-implementation of the
    // System V ABI's elf_hash() during development (see the README) rather
    // than against half-remembered published constants.
    try std.testing.expectEqual(@as(u32, 0x00000000), elfHash(""));
    try std.testing.expectEqual(@as(u32, 0x00660504), elfHash("_init"));
    try std.testing.expectEqual(@as(u32, 0x077905a6), elfHash("printf"));
}

test "build produces a well-formed ELF64 DYN aarch64 image" {
    const alloc = std.testing.allocator;
    const bytes = try build(alloc, "libmain.so", "libandroid.so", "libc.so");
    defer alloc.free(bytes);
    try std.testing.expect(bytes.len > @sizeOf(Elf64Ehdr));
    try std.testing.expectEqualSlices(u8, "\x7fELF", bytes[0..4]);
    try std.testing.expectEqual(@as(u8, ELFCLASS64), bytes[4]);
    const e_type = std.mem.readInt(u16, bytes[16..18], .little);
    const e_machine = std.mem.readInt(u16, bytes[18..20], .little);
    try std.testing.expectEqual(ET_DYN, e_type);
    try std.testing.expectEqual(EM_AARCH64, e_machine);
}
