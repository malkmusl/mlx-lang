const std = @import("std");

pub const FileId = u32;

pub const SourceLocation = struct {
    file_id: FileId,
    line: u32,
    column: u32,
};

pub const SourceSpan = struct {
    file_id: FileId,
    start_byte: u32,
    end_byte: u32,
};

pub const SourceFile = struct {
    id: FileId,
    path: []const u8,
    content: []const u8,
    line_offsets: std.ArrayList(u32),

    pub fn deinit(self: *SourceFile, allocator: std.mem.Allocator) void {
        self.line_offsets.deinit(allocator);
    }
};

pub const SourceManager = struct {
    allocator: std.mem.Allocator,
    files: std.ArrayList(SourceFile),

    pub fn init(allocator: std.mem.Allocator) SourceManager {
        return .{
            .allocator = allocator,
            .files = std.ArrayList(SourceFile).empty,
        };
    }

    pub fn deinit(self: *SourceManager) void {
        for (self.files.items) |*file| {
            file.deinit(self.allocator);
            self.allocator.free(file.path);
            self.allocator.free(file.content);
        }
        self.files.deinit(self.allocator);
    }

    pub fn addFile(self: *SourceManager, path: []const u8, content: []const u8) !FileId {
        const id: FileId = @intCast(self.files.items.len);
        
        var line_offsets = std.ArrayList(u32).empty;
        try line_offsets.append(self.allocator, 0);
        
        for (content, 0..) |byte, i| {
            if (byte == '\n') {
                try line_offsets.append(self.allocator, @intCast(i + 1));
            }
        }

        const path_dup = try self.allocator.dupe(u8, path);
        const content_dup = try self.allocator.dupe(u8, content);

        try self.files.append(self.allocator, SourceFile{
            .id = id,
            .path = path_dup,
            .content = content_dup,
            .line_offsets = line_offsets,
        });

        return id;
    }

    pub fn getFile(self: *const SourceManager, id: FileId) ?*const SourceFile {
        if (id >= self.files.items.len) return null;
        return &self.files.items[id];
    }

    pub fn getLineCol(self: *const SourceManager, file_id: FileId, offset: u32) ?SourceLocation {
        const file = self.getFile(file_id) orelse return null;
        
        // Binary search for the line
        const offsets = file.line_offsets.items;
        var low: usize = 0;
        var high: usize = offsets.len;

        while (low < high) {
            const mid = low + (high - low) / 2;
            if (offsets[mid] <= offset) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }

        const line = low - 1;
        const line_start = offsets[line];
        const column = offset - line_start;

        return SourceLocation{
            .file_id = file_id,
            .line = @intCast(line + 1), // 1-indexed
            .column = column + 1, // 1-indexed
        };
    }
};

test "SourceManager" {
    const allocator = std.testing.allocator;
    var sm = SourceManager.init(allocator);
    defer sm.deinit();

    const file_id = try sm.addFile("test.zin", "const a = 1;\nconst b = 2;\n");
    
    const loc1 = sm.getLineCol(file_id, 0).?;
    try std.testing.expectEqual(@as(u32, 1), loc1.line);
    try std.testing.expectEqual(@as(u32, 1), loc1.column);

    const loc2 = sm.getLineCol(file_id, 13).?;
    try std.testing.expectEqual(@as(u32, 2), loc2.line);
    try std.testing.expectEqual(@as(u32, 1), loc2.column);
}
