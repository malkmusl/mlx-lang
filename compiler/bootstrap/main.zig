const std = @import("std");
const sm = @import("source/source_manager.zig");
const diag = @import("source/diagnostics.zig");

pub fn main(init: std.process.Init) !u8 {
    const allocator = init.gpa;
    const io = init.io;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: std.Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const out = &stdout_file_writer.interface;

    std.debug.print("zin0 bootstrap compiler\n", .{});

    const args = try init.minimal.args.toSlice(allocator);
    defer allocator.free(args);

    if (args.len < 2) {
        std.debug.print("Usage: zin0 <source.zin> [--emit=asm]\n", .{});
        return 1;
    }
    const path = args[1];

    // Parse flags
    var emit_asm = false;
    var out_path: []const u8 = "out";
    for (args[2..]) |arg| {
        if (std.mem.eql(u8, arg, "--emit=asm")) emit_asm = true;
        if (std.mem.startsWith(u8, arg, "-o")) out_path = arg[2..];
    }

    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    var engine = diag.DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    // Stages 1-4: recursively load, lex and parse the complete module graph.
    const modules = @import("modules/root.zig");
    var module_loader = modules.loader.Loader.init(allocator, io, &source_manager, &engine, .{ .std_root = "std/src" });
    defer module_loader.deinit();
    const root_module_id = module_loader.loadRoot(path) catch |err| {
        std.debug.print("Fatal parse error: {}\n", .{err});
        engine.renderDebug();
        return 1;
    };
    const root_module = module_loader.get(root_module_id);
    if (root_module.ast == null) {
        engine.renderDebug();
        return 1;
    }
    const ast = root_module.ast.?;

    engine.renderDebug();

    if (engine.error_count > 0) {
        std.debug.print("Compilation failed with {d} error(s).\n", .{engine.error_count});
        return 1;
    }

    // Stage 5/6/7: Type + Sema
    const type_mod = @import("semantic/type.zig");
    var type_pool = type_mod.TypePool.init(allocator);
    defer type_pool.deinit();

    var module_registry = try modules.namespace.Registry.init(allocator, module_loader.modules.items.len);
    defer module_registry.deinit();

    // Dependencies were appended while recursively walking imports. Analyze in
    // reverse discovery order so ordinary acyclic imports expose their public
    // namespace before their importer is checked.
    const sema_mod = @import("semantic/sema.zig");
    const export_collector = @import("semantic/exports.zig");
    const scope_mod = @import("semantic/scope.zig");
    const ImportedAnalysis = struct { scope: *scope_mod.Scope, sema: *sema_mod.Sema };
    var imported_analyses = std.ArrayList(ImportedAnalysis).empty;
    defer {
        for (imported_analyses.items) |analysis| {
            analysis.sema.deinit();
            allocator.destroy(analysis.sema);
            analysis.scope.deinit();
            allocator.destroy(analysis.scope);
        }
        imported_analyses.deinit(allocator);
    }
    var module_index = module_loader.modules.items.len;
    while (module_index > 1) {
        module_index -= 1;
        const imported = module_loader.get(@intCast(module_index));
        if (imported.ast == null) continue;
        const imported_scope = try allocator.create(scope_mod.Scope);
        errdefer allocator.destroy(imported_scope);
        imported_scope.* = scope_mod.Scope.init(allocator, null);
        errdefer imported_scope.deinit();
        const imported_sema = try allocator.create(sema_mod.Sema);
        errdefer allocator.destroy(imported_sema);
        imported_sema.* = sema_mod.Sema.init(
            allocator,
            imported.ast.?,
            imported.source_id,
            &engine,
            &type_pool,
            imported_scope,
        );
        errdefer imported_sema.deinit();
        imported_sema.configureModules(@intCast(module_index), &imported.imports, &module_registry);
        imported_sema.analyze() catch |err| {
            std.debug.print("Imported module sema failed: {}\n", .{err});
            return 1;
        };
        try export_collector.collect(imported_sema, &module_registry, @intCast(module_index));
        try imported_analyses.append(allocator, .{ .scope = imported_scope, .sema = imported_sema });
    }

    var root_scope = scope_mod.Scope.init(allocator, null);
    defer root_scope.deinit();

    var sema = sema_mod.Sema.init(allocator, ast, root_module.source_id, &engine, &type_pool, &root_scope);
    defer sema.deinit();
    sema.configureModules(root_module_id, &root_module.imports, &module_registry);
    sema.analyze() catch |err| {
        std.debug.print("Sema failed: {}\n", .{err});
        return 1;
    };

    engine.renderDebug();

    if (engine.error_count > 0) {
        std.debug.print("Compilation failed with {d} error(s).\n", .{engine.error_count});
        return 1;
    }

    // Stage 9: LIR
    const lir_gen_mod = @import("ir/lower.zig");
    var lir_builder = lir_gen_mod.LirBuilder.init(allocator, &sema);
    defer lir_builder.deinit();
    lir_builder.generate() catch |err| {
        std.debug.print("LIR gen failed: {}\n", .{err});
        return 1;
    };
    for (imported_analyses.items) |analysis| {
        lir_builder.generateModule(analysis.sema) catch |err| {
            std.debug.print("Imported module LIR generation failed: {}\n", .{err});
            return 1;
        };
    }
    lir_builder.printLir();

    std.debug.print("AST has {d} nodes\n", .{ast.nodes.len});

    var x86_gen = @import("backend/x86_64/codegen.zig").X86Gen.init(
        allocator,
        &lir_builder.lir,
        &type_pool,
        ast,
        source_manager.getFile(root_module.source_id).?.content,
    );
    defer x86_gen.deinit();

    if (emit_asm) {
        // ── Stage 10: NASM text output (legacy / debug) ──────────────────────
        std.debug.print("[zin0] --emit=asm: writing NASM text\n", .{});
        try x86_gen.generate(out);
        try stdout_file_writer.flush();
    } else {
        // ── Stage 12: Binary ELF64 output ────────────────────────────────────
        std.debug.print("[zin0] emitting ELF64 binary → '{s}'\n", .{out_path});
        var enc = @import("backend/x86_64/encoder.zig").Encoder.init(allocator);
        defer enc.deinit();

        // Phase 1: generate binary to discover code size and collect rodata strings.
        // rodata_vaddr is 0 here — string addresses will be wrong, but we need code size.
        x86_gen.generateBinary(&enc) catch |err| {
            std.debug.print("Binary code generation failed: {}\n", .{err});
            return 1;
        };

        // Compute rodata_vaddr from ELF layout:
        //   TEXT_FILE_OFFSET = 0x1000, TEXT_VADDR = 0x401000
        //   rodata starts at page-aligned offset after .text
        const elf64_mod = @import("object/elf64.zig");
        const text_size: u64 = @as(u64, @intCast(enc.buf.items.len));
        const rodata_file_off: u64 = elf64_mod.alignUp(0x1000 + text_size, 0x1000);
        const rodata_vaddr: u64 = 0x401000 + (rodata_file_off - 0x1000);
        std.debug.print("[zin0] rodata_vaddr = 0x{x} (text_size={d})\n", .{ rodata_vaddr, text_size });

        if (x86_gen.rodata.items.len > 0) {
            // Phase 2: set rodata_vaddr and regenerate with correct string addresses.
            x86_gen.rodata_vaddr = rodata_vaddr;
            // Reset encoder and virtual register allocator for clean re-generation.
            enc.buf.clearRetainingCapacity();
            enc.fixups.clearRetainingCapacity();
            enc.symbols.clearRetainingCapacity();
            x86_gen.vreg_to_op.clearRetainingCapacity();
            x86_gen.addr_to_slot.clearRetainingCapacity();
            x86_gen.error_tag_slots.clearRetainingCapacity();
            x86_gen.error_payload_extra_slots.clearRetainingCapacity();
            x86_gen.next_gp_reg = 0;
            x86_gen.next_stack_slot = 8;
            x86_gen.current_function_return_type = null;
            x86_gen.current_hidden_payload_slot = null;
            // Clear rodata so strings don't accumulate across two generateBinary calls
            x86_gen.rodata.clearRetainingCapacity();
            x86_gen.string_offsets.clearRetainingCapacity();

            x86_gen.generateBinary(&enc) catch |err| {
                std.debug.print("Binary code generation (phase 2) failed: {}\n", .{err});
                return 1;
            };
        }

        // writeExecutable with rodata slice
        elf64_mod.writeExecutable(allocator, io, &enc, "_start", out_path, x86_gen.rodata.items) catch |err| {
            std.debug.print("ELF64 write failed: {}\n", .{err});
            return 1;
        };
        std.debug.print("[zin0] wrote '{s}' — done.\n", .{out_path});
    }

    try stdout_file_writer.flush();
    std.debug.print("Done.\n", .{});
    return 0;
}

test {
    _ = @import("source/source_manager.zig");
    _ = @import("source/diagnostics.zig");
    _ = @import("syntax/lexer.zig");
    _ = @import("syntax/ast.zig");
    _ = @import("semantic/builtin.zig");
    _ = @import("syntax/parser.zig");
    _ = @import("semantic/type.zig");
    _ = @import("semantic/scope.zig");
    _ = @import("semantic/sema.zig");
    _ = @import("ir/lir.zig");
    _ = @import("ir/lower.zig");
    _ = @import("backend/x86_64/abi.zig");
    _ = @import("backend/x86_64/encoder.zig");
    _ = @import("backend/x86_64/codegen.zig");
    _ = @import("object/elf64.zig");
    _ = @import("platform/linux/raw.zig");
    _ = @import("platform/linux/posix.zig");
    _ = @import("modules/root.zig");
}
