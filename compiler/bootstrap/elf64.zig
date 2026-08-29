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
const ELFCLASS64: u8 = 2;
const ELFDATA2LSB: u8 = 1;
const EV_CURRENT: u8 = 1;
const ELFOSABI_NONE: u8 = 0;

const ET_EXEC: u16 = 2;
const EM_X86_64: u16 = 62;

const PT_LOAD: u32 = 1;
const PF_X: u32 = 1;
const PF_W: u32 = 2;
const PF_R: u32 = 4;

const SHT_NULL: u32 = 0;
const SHT_PROGBITS: u32 = 1;
const SHT_STRTAB: u32 = 3;
const SHF_ALLOC: u64 = 0x2;
const SHF_EXECINSTR: u64 = 0x4;

const LOAD_VADDR: u64 = 0x400000;
const TEXT_FILE_OFFSET: u64 = 0x1000;
const TEXT_VADDR: u64 = LOAD_VADDR + TEXT_FILE_OFFSET;
// .rodata is placed right after .text (page aligned in virtual memory)
const RODATA_PAGE_ALIGN: u64 = 0x1000;
const SHF_WRITE: u64 = 0x1;

// ─────────────────────────────────────────────────────────────────────────────
//  ELF64 struct layouts
// ─────────────────────────────────────────────────────────────────────────────

pub const Elf64Ehdr = extern struct {
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

pub const Elf64Phdr = extern struct {
    p_type: u32,
    p_flags: u32,
    p_offset: u64,
    p_vaddr: u64,
    p_paddr: u64,
    p_filesz: u64,
    p_memsz: u64,
    p_align: u64,
};

pub const Elf64Shdr = extern struct {
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

// ─────────────────────────────────────────────────────────────────────────────
//  Binary builder
// ─────────────────────────────────────────────────────────────────────────────

/// Build the entire ELF64 binary into a byte buffer.
/// Caller owns the returned slice (free with allocator.free).
/// `rodata` may be empty — it is embedded as a .rodata section if non-empty.
pub fn buildExecutable(
    allocator: std.mem.Allocator,
    encoder: *Encoder,
    entry_symbol: []const u8,
    rodata: []const u8,
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

    const has_rodata = rodata.len > 0;

    // .shstrtab entries
    var shstrtab = std.ArrayList(u8).empty;
    defer shstrtab.deinit(allocator);
    try shstrtab.append(allocator, 0);
    const text_name_off: u32 = @as(u32, @intCast(shstrtab.items.len));
    try appendStr(&shstrtab, allocator, ".text");
    const rodata_name_off: u32 = @as(u32, @intCast(shstrtab.items.len));
    try appendStr(&shstrtab, allocator, ".rodata");
    const shstrtab_name_off: u32 = @as(u32, @intCast(shstrtab.items.len));
    try appendStr(&shstrtab, allocator, ".shstrtab");

    // Layout
    const text_off: u64 = TEXT_FILE_OFFSET;
    // rodata immediately follows text in the file (page-aligned in file too for simplicity)
    const rodata_file_off: u64 = alignUp(text_off + code_size, RODATA_PAGE_ALIGN);
    const rodata_file_end: u64 = if (has_rodata) rodata_file_off + rodata.len else rodata_file_off;
    const rodata_vaddr: u64 = TEXT_VADDR + (rodata_file_off - text_off);

    const shstrtab_off: u64 = if (has_rodata) alignUp(rodata_file_end, 8) else alignUp(text_off + code_size, 8);
    const shstrtab_size: u64 = shstrtab.items.len;
    const shdr_off: u64 = alignUp(shstrtab_off + shstrtab_size, 8);

    const num_phdrs: u16 = if (has_rodata) 2 else 1;
    const num_shdrs: u16 = if (has_rodata) 4 else 3;
    const shstrndx: u16 = num_shdrs - 1;

    const load_filesz: u64 = text_off + code_size;

    // ELF header
    const ehdr = Elf64Ehdr{
        .e_ident = .{
            ELFMAG0,    ELFMAG1,     ELFMAG2,    ELFMAG3,
            ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_NONE,
            0,          0,           0,          0,
            0,          0,           0,          0,
        },
        .e_type = ET_EXEC,
        .e_machine = EM_X86_64,
        .e_version = 1,
        .e_entry = entry_vaddr,
        .e_phoff = @sizeOf(Elf64Ehdr),
        .e_shoff = shdr_off,
        .e_flags = 0,
        .e_ehsize = @sizeOf(Elf64Ehdr),
        .e_phentsize = @sizeOf(Elf64Phdr),
        .e_phnum = num_phdrs,
        .e_shentsize = @sizeOf(Elf64Shdr),
        .e_shnum = num_shdrs,
        .e_shstrndx = shstrndx,
    };

    // Program header for .text (RX)
    const phdr_text = Elf64Phdr{
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = 0,
        .p_vaddr = LOAD_VADDR,
        .p_paddr = LOAD_VADDR,
        .p_filesz = load_filesz,
        .p_memsz = load_filesz,
        .p_align = 0x1000,
    };

    // Program header for .rodata (RO)
    const phdr_rodata = Elf64Phdr{
        .p_type = PT_LOAD,
        .p_flags = PF_R,
        .p_offset = rodata_file_off,
        .p_vaddr = rodata_vaddr,
        .p_paddr = rodata_vaddr,
        .p_filesz = rodata.len,
        .p_memsz = rodata.len,
        .p_align = 0x1000,
    };

    // Section headers
    const shdr_null = Elf64Shdr{
        .sh_name = 0,
        .sh_type = SHT_NULL,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = 0,
        .sh_size = 0,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 0,
        .sh_entsize = 0,
    };
    const shdr_text = Elf64Shdr{
        .sh_name = text_name_off,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = TEXT_VADDR,
        .sh_offset = text_off,
        .sh_size = code_size,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 16,
        .sh_entsize = 0,
    };
    const shdr_rodata = Elf64Shdr{
        .sh_name = rodata_name_off,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC,
        .sh_addr = rodata_vaddr,
        .sh_offset = rodata_file_off,
        .sh_size = rodata.len,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };
    const shdr_shstrtab = Elf64Shdr{
        .sh_name = shstrtab_name_off,
        .sh_type = SHT_STRTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = shstrtab_off,
        .sh_size = shstrtab_size,
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
    };

    // Total output size
    const total_size: usize = @as(usize, shdr_off) + @as(usize, num_shdrs) * @sizeOf(Elf64Shdr);
    var out = try allocator.alloc(u8, total_size);
    @memset(out, 0);

    // Write ELF header
    const ehdr_bytes = std.mem.asBytes(&ehdr);
    @memcpy(out[0..ehdr_bytes.len], ehdr_bytes);

    // Write program headers
    const phdr_base = @sizeOf(Elf64Ehdr);
    const phdr_text_bytes = std.mem.asBytes(&phdr_text);
    @memcpy(out[phdr_base .. phdr_base + phdr_text_bytes.len], phdr_text_bytes);
    if (has_rodata) {
        const phdr_ro_off = phdr_base + @sizeOf(Elf64Phdr);
        const phdr_rodata_bytes = std.mem.asBytes(&phdr_rodata);
        @memcpy(out[phdr_ro_off .. phdr_ro_off + phdr_rodata_bytes.len], phdr_rodata_bytes);
    }

    // Write .text
    @memcpy(out[text_off .. text_off + code.len], code);

    // Write .rodata
    if (has_rodata) {
        @memcpy(out[rodata_file_off .. rodata_file_off + rodata.len], rodata);
    }

    // Write .shstrtab
    @memcpy(out[shstrtab_off .. shstrtab_off + shstrtab.items.len], shstrtab.items);

    // Write section headers
    const shdrs_to_write: []const Elf64Shdr = if (has_rodata)
        &[_]Elf64Shdr{ shdr_null, shdr_text, shdr_rodata, shdr_shstrtab }
    else
        &[_]Elf64Shdr{ shdr_null, shdr_text, shdr_shstrtab };

    for (shdrs_to_write, 0..) |shdr, i| {
        const off = @as(usize, shdr_off) + i * @sizeOf(Elf64Shdr);
        const shdr_bytes = std.mem.asBytes(&shdr);
        @memcpy(out[off .. off + shdr_bytes.len], shdr_bytes);
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
    rodata: []const u8,
) !void {
    const bytes = try buildExecutable(allocator, encoder, entry_symbol, rodata);
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
    try std.testing.expectEqual(@as(u64, 8), alignUp(1, 8));
    try std.testing.expectEqual(@as(u64, 8), alignUp(8, 8));
    try std.testing.expectEqual(@as(u64, 16), alignUp(9, 8));
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

    const bytes = try buildExecutable(std.testing.allocator, &enc, "_start", &.{});
    defer std.testing.allocator.free(bytes);

    // Check ELF magic
    try std.testing.expectEqual(@as(u8, 0x7f), bytes[0]);
    try std.testing.expectEqual(@as(u8, 'E'), bytes[1]);
    try std.testing.expectEqual(@as(u8, 'L'), bytes[2]);
    try std.testing.expectEqual(@as(u8, 'F'), bytes[3]);
    // e_machine = EM_X86_64 = 62 at offset 18 (little-endian)
    try std.testing.expectEqual(@as(u8, 62), bytes[18]);
    try std.testing.expectEqual(@as(u8, 0), bytes[19]);
    // e_phnum = 1 at offset 56
    try std.testing.expectEqual(@as(u8, 1), bytes[56]);
}
