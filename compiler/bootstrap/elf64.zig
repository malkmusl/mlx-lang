/// ELF64 executable writer for zin0.
///
/// Produces a minimal statically-linked Linux x86_64 ELF64 executable
/// directly from the machine-code buffer produced by x86_64_encoder.
///
/// Layout written to disk:
///   [0x000]  ELF64 file header         (64 bytes)
///   [0x040]  Program header – PT_LOAD  (56 bytes)
///   [0x078]  Padding to page offset
///   [0x1000] .text section             (machine code)
///   [after]  .shstrtab
///   [align8] Section header table
///
/// Virtual memory map (LOAD_VADDR = 0x400000):
///   0x400000       = file offset 0  (headers)
///   0x401000       = .text start

const std = @import("std");
const Encoder = @import("x86_64_encoder.zig").Encoder;

// ─────────────────────────────────────────────────────────────────────────────
//  ELF64 constants
// ─────────────────────────────────────────────────────────────────────────────

const ELFMAG0: u8 = 0x7f;
const ELFMAG1: u8 = 'E';
const ELFMAG2: u8 = 'L';
const ELFMAG3: u8 = 'F';
const ELFCLASS64:  u8 = 2;
const ELFDATA2LSB: u8 = 1;
const EV_CURRENT:  u8 = 1;
const ELFOSABI_NONE: u8 = 0;

const ET_EXEC:    u16 = 2;
const EM_X86_64:  u16 = 62;

const PT_LOAD: u32 = 1;
const PF_X: u32 = 1;
const PF_W: u32 = 2;
const PF_R: u32 = 4;

const SHT_NULL:     u32 = 0;
const SHT_PROGBITS: u32 = 1;
const SHT_STRTAB:   u32 = 3;
const SHF_ALLOC:    u64 = 0x2;
const SHF_EXECINSTR: u64 = 0x4;

const LOAD_VADDR:      u64 = 0x400000;
const TEXT_FILE_OFFSET: u64 = 0x1000;
const TEXT_VADDR:      u64 = LOAD_VADDR + TEXT_FILE_OFFSET;

// ─────────────────────────────────────────────────────────────────────────────
//  ELF64 struct layouts
// ─────────────────────────────────────────────────────────────────────────────

pub const Elf64Ehdr = extern struct {
    e_ident:     [16]u8,
    e_type:      u16,
    e_machine:   u16,
    e_version:   u32,
    e_entry:     u64,
    e_phoff:     u64,
    e_shoff:     u64,
    e_flags:     u32,
    e_ehsize:    u16,
    e_phentsize: u16,
    e_phnum:     u16,
    e_shentsize: u16,
    e_shnum:     u16,
    e_shstrndx:  u16,
};

pub const Elf64Phdr = extern struct {
    p_type:   u32,
    p_flags:  u32,
    p_offset: u64,
    p_vaddr:  u64,
    p_paddr:  u64,
    p_filesz: u64,
    p_memsz:  u64,
    p_align:  u64,
};

pub const Elf64Shdr = extern struct {
    sh_name:      u32,
    sh_type:      u32,
    sh_flags:     u64,
    sh_addr:      u64,
    sh_offset:    u64,
    sh_size:      u64,
    sh_link:      u32,
    sh_info:      u32,
    sh_addralign: u64,
    sh_entsize:   u64,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Binary builder
// ─────────────────────────────────────────────────────────────────────────────

/// Build the entire ELF64 binary into a byte buffer.
/// Caller owns the returned slice (free with allocator.free).
pub fn buildExecutable(
    allocator: std.mem.Allocator,
    encoder: *Encoder,
    entry_symbol: []const u8,
) ![]u8 {
    std.debug.print("[elf64] building ELF64 binary\n", .{});

    const entry_code_offset = encoder.symbols.get(entry_symbol) orelse {
        std.debug.print("[elf64] ERROR: entry symbol '{s}' not found\n", .{entry_symbol});
        return error.EntrySymbolNotFound;
    };
    const entry_vaddr: u64 = TEXT_VADDR + entry_code_offset;
    std.debug.print("[elf64] entry '{s}' @ code+0x{x} → vaddr 0x{x}\n", .{
        entry_symbol, entry_code_offset, entry_vaddr,
    });

    const code = encoder.buf.items;
    const code_size: u64 = code.len;

    // .shstrtab: \0 ".text\0" ".shstrtab\0"
    var shstrtab = std.ArrayList(u8).empty;
    defer shstrtab.deinit(allocator);
    try shstrtab.append(allocator, 0);
    const text_name_off: u32 = @as(u32, @intCast(shstrtab.items.len));
    try appendStr(&shstrtab, allocator, ".text");
    const shstrtab_name_off: u32 = @as(u32, @intCast(shstrtab.items.len));
    try appendStr(&shstrtab, allocator, ".shstrtab");

    // Layout
    const text_off:      u64 = TEXT_FILE_OFFSET;
    const shstrtab_off:  u64 = text_off + code_size;
    const shstrtab_size: u64 = shstrtab.items.len;
    const shdr_off:      u64 = alignUp(shstrtab_off + shstrtab_size, 8);

    const load_filesz: u64 = text_off + code_size;

    // ELF header
    const ehdr = Elf64Ehdr{
        .e_ident = .{
            ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
            ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_NONE,
            0, 0, 0, 0, 0, 0, 0, 0,
        },
        .e_type      = ET_EXEC,
        .e_machine   = EM_X86_64,
        .e_version   = 1,
        .e_entry     = entry_vaddr,
        .e_phoff     = @sizeOf(Elf64Ehdr),
        .e_shoff     = shdr_off,
        .e_flags     = 0,
        .e_ehsize    = @sizeOf(Elf64Ehdr),
        .e_phentsize = @sizeOf(Elf64Phdr),
        .e_phnum     = 1,
        .e_shentsize = @sizeOf(Elf64Shdr),
        .e_shnum     = 3,
        .e_shstrndx  = 2,
    };

    // Program header
    const phdr = Elf64Phdr{
        .p_type   = PT_LOAD,
        .p_flags  = PF_R | PF_X,
        .p_offset = 0,
        .p_vaddr  = LOAD_VADDR,
        .p_paddr  = LOAD_VADDR,
        .p_filesz = load_filesz,
        .p_memsz  = load_filesz,
        .p_align  = 0x1000,
    };

    // Section headers
    const shdrs = [3]Elf64Shdr{
        .{ // [0] NULL
            .sh_name = 0, .sh_type = SHT_NULL, .sh_flags = 0,
            .sh_addr = 0, .sh_offset = 0, .sh_size = 0,
            .sh_link = 0, .sh_info = 0, .sh_addralign = 0, .sh_entsize = 0,
        },
        .{ // [1] .text
            .sh_name      = text_name_off,
            .sh_type      = SHT_PROGBITS,
            .sh_flags     = SHF_ALLOC | SHF_EXECINSTR,
            .sh_addr      = TEXT_VADDR,
            .sh_offset    = text_off,
            .sh_size      = code_size,
            .sh_link      = 0, .sh_info = 0,
            .sh_addralign = 16, .sh_entsize = 0,
        },
        .{ // [2] .shstrtab
            .sh_name      = shstrtab_name_off,
            .sh_type      = SHT_STRTAB,
            .sh_flags     = 0,
            .sh_addr      = 0,
            .sh_offset    = shstrtab_off,
            .sh_size      = shstrtab_size,
            .sh_link      = 0, .sh_info = 0,
            .sh_addralign = 1, .sh_entsize = 0,
        },
    };

    // Assemble output buffer
    const total_size: usize = @as(usize, shdr_off) + 3 * @sizeOf(Elf64Shdr);
    var out = try allocator.alloc(u8, total_size);
    @memset(out, 0);

    // Write ELF header
    const ehdr_bytes = std.mem.asBytes(&ehdr);
    @memcpy(out[0..ehdr_bytes.len], ehdr_bytes);

    // Write program header
    const phdr_off = @sizeOf(Elf64Ehdr);
    const phdr_bytes = std.mem.asBytes(&phdr);
    @memcpy(out[phdr_off..phdr_off + phdr_bytes.len], phdr_bytes);

    // Write .text
    @memcpy(out[text_off..text_off + code.len], code);

    // Write .shstrtab
    @memcpy(out[shstrtab_off..shstrtab_off + shstrtab.items.len], shstrtab.items);

    // Write section headers
    for (shdrs, 0..) |shdr, i| {
        const off = @as(usize, shdr_off) + i * @sizeOf(Elf64Shdr);
        const shdr_bytes = std.mem.asBytes(&shdr);
        @memcpy(out[off..off + shdr_bytes.len], shdr_bytes);
    }

    std.debug.print("[elf64] built {d} bytes, entry=0x{x}\n", .{ total_size, entry_vaddr });
    return out;
}

/// Write the ELF64 binary to disk using std.Io.Dir.writeFile.
pub fn writeExecutable(
    allocator: std.mem.Allocator,
    io: std.Io,
    encoder: *Encoder,
    entry_symbol: []const u8,
    out_path: []const u8,
) !void {
    const bytes = try buildExecutable(allocator, encoder, entry_symbol);
    defer allocator.free(bytes);
    std.debug.print("[elf64] writing {d} bytes to '{s}'\n", .{ bytes.len, out_path });
    try std.Io.Dir.writeFile(.cwd(), io, .{
        .sub_path = out_path,
        .data = bytes,
    });
    // Make executable (0o755) via Linux chmod syscall
    const path_z = try std.mem.concatWithSentinel(allocator, u8, &.{out_path}, 0);
    defer allocator.free(path_z);
    _ = std.os.linux.chmod(path_z, 0o755);
    std.debug.print("[elf64] done\n", .{});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

fn appendStr(list: *std.ArrayList(u8), allocator: std.mem.Allocator, s: []const u8) !void {
    for (s) |c| try list.append(allocator, c);
    try list.append(allocator, 0);
}

pub fn alignUp(v: u64, a: u64) u64 {
    return (v + a - 1) & ~(a - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tests (struct sizes only — file I/O tests run through main.zig)
// ─────────────────────────────────────────────────────────────────────────────

test "ELF header size is 64 bytes" {
    try std.testing.expectEqual(@as(usize, 64), @sizeOf(Elf64Ehdr));
}

test "Program header size is 56 bytes" {
    try std.testing.expectEqual(@as(usize, 56), @sizeOf(Elf64Phdr));
}

test "Section header size is 64 bytes" {
    try std.testing.expectEqual(@as(usize, 64), @sizeOf(Elf64Shdr));
}

test "alignUp" {
    try std.testing.expectEqual(@as(u64, 8),      alignUp(1, 8));
    try std.testing.expectEqual(@as(u64, 8),      alignUp(8, 8));
    try std.testing.expectEqual(@as(u64, 16),     alignUp(9, 8));
    try std.testing.expectEqual(@as(u64, 0x1000), alignUp(0xFFF, 0x1000));
    try std.testing.expectEqual(@as(u64, 0x1000), alignUp(0x1000, 0x1000));
}

test "ELF magic bytes in built binary" {
    const enc_mod = @import("x86_64_encoder.zig");
    var enc = enc_mod.Encoder.init(std.testing.allocator);
    defer enc.deinit();

    try enc.defineSymbol("_start");
    try enc.emitMovRegImm64(.rax, 60);
    try enc.emitMovRegImm64(.rdi, 0);
    try enc.emitSyscall();
    try enc.applyFixups();

    const bytes = try buildExecutable(std.testing.allocator, &enc, "_start");
    defer std.testing.allocator.free(bytes);

    // Check ELF magic
    try std.testing.expectEqual(@as(u8, 0x7f), bytes[0]);
    try std.testing.expectEqual(@as(u8, 'E'),  bytes[1]);
    try std.testing.expectEqual(@as(u8, 'L'),  bytes[2]);
    try std.testing.expectEqual(@as(u8, 'F'),  bytes[3]);
    // e_machine = EM_X86_64 = 62 at offset 18 (little-endian)
    try std.testing.expectEqual(@as(u8, 62), bytes[18]);
    try std.testing.expectEqual(@as(u8, 0),  bytes[19]);
    // e_phnum = 1 at offset 56
    try std.testing.expectEqual(@as(u8, 1), bytes[56]);
}
