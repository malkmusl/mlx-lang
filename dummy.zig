const std = @import("std");
pub fn main(init: std.process.Init) !void {
    var file = try std.Io.Dir.openFile(.cwd(), init.io, "foo.txt", .{});
    const content = try file.reader().readAllAlloc(init.gpa, 10 * 1024 * 1024);
}
