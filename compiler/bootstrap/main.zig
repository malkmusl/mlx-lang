const std = @import("std");
const sm = @import("source/source_manager.zig");
const diag = @import("source/diagnostics.zig");

fn progress(enabled: bool, current: usize, total: usize, message: []const u8) void {
    if (!enabled) return;
    std.debug.print("[mlx0 {d}/{d}] {s}\n", .{ current, total, message });
}

fn traceValue(enabled: bool, label: []const u8, value: usize) void {
    if (!enabled) return;
    std.debug.print("[mlx0 trace] {s}{d}\n", .{ label, value });
}

fn traceHex(enabled: bool, label: []const u8, value: usize) void {
    if (!enabled) return;
    std.debug.print("[mlx0 trace] {s}0x{x}\n", .{ label, value });
}

fn traceText(enabled: bool, label: []const u8, value: []const u8) void {
    if (!enabled) return;
    std.debug.print("[mlx0 trace] {s}{s}\n", .{ label, value });
}

pub fn main(init: std.process.Init) !u8 {
    const allocator = init.gpa;
    const io = init.io;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: std.Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const out = &stdout_file_writer.interface;

    const args = try init.minimal.args.toSlice(allocator);
    defer allocator.free(args);

    if (args.len < 2) {
        std.debug.print("Usage: mlx0 <source.mlx> [-o output] [--progress|--quiet] [--trace|--verbose] [--safety=on|off] [--emit=asm]\n", .{});
        return 1;
    }
    const path = args[1];

    // Parse flags
    var emit_asm = false;
    var verbose = false;
    var trace_enabled = false;
    var show_progress = true;
    var runtime_safety = true;
    var dump_ast = false;
    var out_path: []const u8 = "out";
    var i: usize = 2;
    while (i < args.len) : (i += 1) {
        const arg = args[i];
        if (std.mem.eql(u8, arg, "--emit=asm")) {
            emit_asm = true;
        } else if (std.mem.eql(u8, arg, "--verbose")) {
            verbose = true;
            trace_enabled = true;
        } else if (std.mem.eql(u8, arg, "--trace")) {
            trace_enabled = true;
        } else if (std.mem.eql(u8, arg, "--progress")) {
            show_progress = true;
        } else if (std.mem.eql(u8, arg, "--quiet")) {
            show_progress = false;
        } else if (std.mem.eql(u8, arg, "--safety=on")) {
            runtime_safety = true;
        } else if (std.mem.eql(u8, arg, "--safety=off")) {
            runtime_safety = false;
        } else if (std.mem.eql(u8, arg, "--dump-ast")) {
            dump_ast = true;
        } else if (std.mem.startsWith(u8, arg, "-o")) {
            if (arg.len == 2) {
                i += 1;
                if (i < args.len) out_path = args[i];
            } else {
                out_path = arg[2..];
            }
        }
    }
    const progress_steps: usize = if (dump_ast) 1 else if (emit_asm) 5 else 7;
    traceText(trace_enabled, "input: ", path);
    traceText(trace_enabled, "output: ", out_path);
    traceText(trace_enabled, "runtime safety: ", if (runtime_safety) "on" else "off");

    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    var engine = diag.DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    // Stages 1-4: recursively load, lex and parse the complete module graph.
    progress(show_progress, 1, progress_steps, "load and parse module graph");
    const modules = @import("modules/root.zig");
    var module_loader = modules.loader.Loader.init(allocator, io, &source_manager, &engine, .{ .std_root = "std/src" });
    defer module_loader.deinit();
    const root_module_id = module_loader.loadRoot(path) catch |err| {
        std.debug.print("Fatal parse error: {}\n", .{err});
        engine.renderDebug();
        return 1;
    };
    traceValue(trace_enabled, "source files: ", source_manager.files.items.len);
    traceValue(trace_enabled, "modules: ", module_loader.modules.items.len);
    if (trace_enabled) {
        for (module_loader.graph.modules.items, 0..) |module, module_index| {
            const display_path = if (module_index == @as(usize, @intCast(root_module_id))) path else module.path;
            std.debug.print("[mlx0 trace] module[{d}]: {s}\n", .{ module_index, display_path });
        }
    }
    const root_module = module_loader.get(root_module_id);
    if (root_module.ast == null) {
        engine.renderDebug();
        return 1;
    }
    const ast = root_module.ast.?;
    traceValue(trace_enabled, "root AST nodes: ", ast.nodes.len);

    if (dump_ast) {
        ast.dump(source_manager.files.items[root_module.source_id].content, out) catch {};
        try stdout_file_writer.flush();
        return 0;
    }

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

    // Analyze imports topologically. Discovery order alone is insufficient:
    // a later sibling can depend on an already-discovered earlier sibling,
    // while a direct dependency discovered during recursion can also be later.
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
    const analyzed_modules = try allocator.alloc(bool, module_loader.modules.items.len);
    defer allocator.free(analyzed_modules);
    @memset(analyzed_modules, false);
    var remaining_modules: usize = 0;
    for (module_loader.modules.items[1..], 1..) |module, module_index| {
        if (module.ast == null) {
            analyzed_modules[module_index] = true;
        } else {
            remaining_modules += 1;
        }
    }
    progress(show_progress, 2, progress_steps, "analyze imported modules");
    while (remaining_modules > 0) {
        var made_progress = false;
        var module_index: usize = 1;
        while (module_index < module_loader.modules.items.len) : (module_index += 1) {
            if (analyzed_modules[module_index]) continue;
            const imported = module_loader.get(@intCast(module_index));
            var dependencies_ready = true;
            var dependencies = imported.imports.valueIterator();
            while (dependencies.next()) |dependency| {
                if (dependency.* >= analyzed_modules.len or !analyzed_modules[dependency.*]) {
                    dependencies_ready = false;
                    break;
                }
            }
            if (!dependencies_ready) continue;

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
            analyzed_modules[module_index] = true;
            remaining_modules -= 1;
            made_progress = true;
        }
        if (!made_progress) {
            std.debug.print("Imported module dependency cycle cannot be analyzed by Stage 0\n", .{});
            return 1;
        }
    }

    var root_scope = scope_mod.Scope.init(allocator, null);
    defer root_scope.deinit();

    var sema = sema_mod.Sema.init(allocator, ast, root_module.source_id, &engine, &type_pool, &root_scope);
    defer sema.deinit();
    sema.configureModules(root_module_id, &root_module.imports, &module_registry);
    progress(show_progress, 3, progress_steps, "analyze root module");
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
    progress(show_progress, 4, progress_steps, "lower typed AST to LIR");
    const lir_gen_mod = @import("ir/lower.zig");
    var lir_builder = lir_gen_mod.LirBuilder.init(allocator, &sema, verbose, runtime_safety);
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
    if (verbose) {
        lir_builder.printLir();
    }
    var lir_instruction_count: usize = 0;
    for (lir_builder.lir.blocks.items) |block| lir_instruction_count += block.insts.items.len;
    traceValue(trace_enabled, "LIR blocks: ", lir_builder.lir.blocks.items.len);
    traceValue(trace_enabled, "LIR instructions: ", lir_instruction_count);

    var x86_gen = @import("backend/x86_64/codegen.zig").X86Gen.init(
        allocator,
        &lir_builder.lir,
        &type_pool,
        verbose,
    );
    defer x86_gen.deinit();

    if (emit_asm) {
        // ── Stage 10: NASM text output (legacy / debug) ──────────────────────
        progress(show_progress, 5, progress_steps, "write NASM text");
        try x86_gen.generate(out);
        try stdout_file_writer.flush();
    } else {
        // ── Stage 12: Binary ELF64 output ────────────────────────────────────
        progress(show_progress, 5, progress_steps, "generate x86_64 machine code");
        var enc = @import("backend/x86_64/encoder.zig").Encoder.init(allocator, verbose);
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
        traceValue(trace_enabled, "first-pass text bytes: ", @intCast(text_size));
        traceHex(trace_enabled, "rodata virtual address: ", @intCast(rodata_vaddr));

        progress(show_progress, 6, progress_steps, "finalize layout and backend fixups");
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
        traceValue(trace_enabled, "final text bytes: ", enc.buf.items.len);
        traceValue(trace_enabled, "rodata bytes: ", x86_gen.rodata.items.len);

        // writeExecutable with rodata slice
        progress(show_progress, 7, progress_steps, "write ELF64 executable");
        elf64_mod.writeExecutable(allocator, io, &enc, "_start", out_path, x86_gen.rodata.items) catch |err| {
            std.debug.print("ELF64 write failed: {}\n", .{err});
            return 1;
        };
    }

    try stdout_file_writer.flush();
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
