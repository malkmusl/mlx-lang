const std = @import("std");
const sm = @import("source_manager.zig");

pub const Phase = enum { source, lexer, parser, resolve, sema, @"comptime", lowering, codegen, linker, stdlib };

pub const Severity = enum { @"error", warning, note, help };

pub const Family = enum {
    source_and_lexer, // ZIN-E1000..1999
    parser_and_grammar, // ZIN-E2000..2999
    name_module_resolve, // ZIN-E3000..3999
    types_sema_control, // ZIN-E4000..4999
    comptime_reflection, // ZIN-E5000..5999
    copyability_init, // ZIN-E6000..6999
    pointer_memory_safety, // ZIN-E7000..7999
    abi_extern_target, // ZIN-E8000..8999
    lowering_codegen_link, // ZIN-E9000..9999
};

pub const DiagnosticCode = enum(u32) {
    TooManyDiagnostics = 0,
    // Add more codes as needed. For now, we can represent them as u32 directly.
    _,
};

pub const Diagnostic = struct {
    code: u32,
    phase: Phase,
    severity: Severity,
    primary_span: sm.SourceSpan,
    message: []const u8,
    // Optional fields can be implemented as needed (notes, relatedSpans, etc.)
};

pub const DiagnosticEngine = struct {
    allocator: std.mem.Allocator,
    diagnostics: std.ArrayList(Diagnostic),
    source_manager: *const sm.SourceManager,
    error_count: u32 = 0,
    max_errors: u32 = 100,

    pub fn init(allocator: std.mem.Allocator, source_manager: *const sm.SourceManager) DiagnosticEngine {
        return .{
            .allocator = allocator,
            .diagnostics = std.ArrayList(Diagnostic).empty,
            .source_manager = source_manager,
        };
    }

    pub fn deinit(self: *DiagnosticEngine) void {
        self.diagnostics.deinit(self.allocator);
    }

    pub fn report(self: *DiagnosticEngine, diagnostic: Diagnostic) !void {
        if (diagnostic.severity == .@"error") {
            if (self.error_count >= self.max_errors) return;
            self.error_count += 1;
        }

        try self.diagnostics.append(self.allocator, diagnostic);

        if (self.error_count == self.max_errors) {
            try self.diagnostics.append(self.allocator, Diagnostic{
                .code = 0, // TooManyDiagnostics
                .phase = diagnostic.phase,
                .severity = .@"error",
                .primary_span = diagnostic.primary_span, // Reuse span for simplicity
                .message = "Too many errors emitted, stopping.",
            });
            self.error_count += 1; // Increment so we don't hit this again
        }
    }

    pub fn render(self: *const DiagnosticEngine, writer: anytype) !void {
        for (self.diagnostics.items) |diag| {
            const loc = self.source_manager.getLineCol(diag.primary_span.file_id, diag.primary_span.start_byte) orelse continue;
            const file = self.source_manager.getFile(diag.primary_span.file_id) orelse continue;

            const code_kind: u8 = if (diag.severity == .warning) 'W' else 'E';
            try writer.print("{s}:{d}:{d}: {s}: ZIN-{c}{d:0>4}: {s}\n", .{
                file.path,
                loc.line,
                loc.column,
                @tagName(diag.severity),
                code_kind,
                diag.code,
                diag.message,
            });
        }
    }

    /// Render diagnostics through the process debug stream. The compiler uses
    /// this path so diagnostics and progress messages share one stream and
    /// cannot overwrite each other when stdout/stderr are redirected together.
    pub fn renderDebug(self: *const DiagnosticEngine) void {
        for (self.diagnostics.items) |diag| {
            const loc = self.source_manager.getLineCol(diag.primary_span.file_id, diag.primary_span.start_byte) orelse continue;
            const file = self.source_manager.getFile(diag.primary_span.file_id) orelse continue;
            const code_kind: u8 = if (diag.severity == .warning) 'W' else 'E';
            std.debug.print("{s}:{d}:{d}: {s}: ZIN-{c}{d:0>4}: {s}\n", .{
                file.path,
                loc.line,
                loc.column,
                @tagName(diag.severity),
                code_kind,
                diag.code,
                diag.message,
            });
        }
    }
};

test "DiagnosticEngine" {
    const allocator = std.testing.allocator;
    var source_manager = sm.SourceManager.init(allocator);
    defer source_manager.deinit();

    const file_id = try source_manager.addFile("test.zin", "const a = 1;\n");

    var engine = DiagnosticEngine.init(allocator, &source_manager);
    defer engine.deinit();

    try engine.report(Diagnostic{
        .code = 1001,
        .phase = .lexer,
        .severity = .@"error",
        .primary_span = .{ .file_id = file_id, .start_byte = 6, .end_byte = 7 },
        .message = "Invalid character",
    });

    var buf: [1024]u8 = undefined;
    var w: std.Io.Writer = .fixed(&buf);
    try engine.render(&w);

    try std.testing.expectEqualStrings("test.zin:1:7: error: ZIN-E1001: Invalid character\n", w.buffered());
}
