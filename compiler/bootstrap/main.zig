const std = @import("std");

pub fn main() !void {
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_writer = std.fs.File.stdout().writer(&stdout_buffer);
    const out = &stdout_writer.interface;
    try out.writeAll(
        "zin0 bootstrap scaffold\n" ++
        "Implement according to AGENT_IMPLEMENTATION.md and spec/.\n",
    );
    try out.flush();
}
