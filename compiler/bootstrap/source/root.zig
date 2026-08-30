pub const source_manager = @import("source_manager.zig");
pub const diagnostics = @import("diagnostics.zig");

pub const SourceManager = source_manager.SourceManager;
pub const SourceSpan = source_manager.SourceSpan;
pub const DiagnosticEngine = diagnostics.DiagnosticEngine;
pub const Phase = diagnostics.Phase;
