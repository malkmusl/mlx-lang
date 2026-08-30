const Tag = @import("../token.zig").Token.Tag;

pub const BindingPower = struct { left: u8, right: u8 };

pub fn bindingPower(tag: Tag) BindingPower {
    return switch (tag) {
        .equal,
        .plus_equal,
        .minus_equal,
        .asterisk_equal,
        .slash_equal,
        .percent_equal,
        .plus_percent_equal,
        .minus_percent_equal,
        .asterisk_percent_equal,
        .plus_pipe_equal,
        .minus_pipe_equal,
        .asterisk_pipe_equal,
        .ampersand_equal,
        .pipe_equal,
        .caret_equal,
        .shl_equal,
        .shr_equal,
        .shl_percent_equal,
        .shr_percent_equal,
        .shl_pipe_equal,
        .shl_percent_pipe_equal,
        .ampersand_shl_equal,
        .pipe_shl_equal,
        .caret_shl_equal,
        .ampersand_shr_equal,
        .pipe_shr_equal,
        .caret_shr_equal,
        .shl_ampersand_equal,
        .shl_caret_equal,
        .shr_ampersand_equal,
        .shr_pipe_equal,
        .shr_caret_equal,
        .plus_shl_equal,
        .minus_shl_equal,
        .asterisk_shl_equal,
        .plus_percent_shl_equal,
        .minus_percent_shl_equal,
        .asterisk_percent_shl_equal,
        .plus_pipe_shl_equal,
        .minus_pipe_shl_equal,
        .asterisk_pipe_shl_equal,
        .plus_shr_equal,
        .minus_shr_equal,
        .asterisk_shr_equal,
        .plus_percent_shr_equal,
        .minus_percent_shr_equal,
        .asterisk_percent_shr_equal,
        .plus_pipe_shr_equal,
        .minus_pipe_shr_equal,
        .asterisk_pipe_shr_equal,
        => .{ .left = 10, .right = 10 },

        .pipe_pipe => .{ .left = 20, .right = 21 },
        .ampersand_ampersand => .{ .left = 25, .right = 26 },
        .equal_equal, .bang_equal, .angle_bracket_left, .angle_bracket_left_equal, .angle_bracket_right, .angle_bracket_right_equal => .{ .left = 30, .right = 31 },
        .pipe => .{ .left = 34, .right = 35 },
        .caret => .{ .left = 36, .right = 37 },
        .ampersand => .{ .left = 38, .right = 39 },

        .shl, .shr, .shl_percent, .shr_percent, .shl_pipe, .shl_percent_pipe,
        .ampersand_shl, .pipe_shl, .caret_shl, .ampersand_shr, .pipe_shr, .caret_shr,
        .shl_ampersand, .shl_caret, .shr_ampersand, .shr_pipe, .shr_caret,
        .plus_shl, .minus_shl, .asterisk_shl,
        .plus_percent_shl, .minus_percent_shl, .asterisk_percent_shl,
        .plus_pipe_shl, .minus_pipe_shl, .asterisk_pipe_shl,
        .plus_shr, .minus_shr, .asterisk_shr,
        .plus_percent_shr, .minus_percent_shr, .asterisk_percent_shr,
        .plus_pipe_shr, .minus_pipe_shr, .asterisk_pipe_shr,
        => .{ .left = 40, .right = 41 },

        .plus, .minus, .plus_percent, .minus_percent, .plus_pipe, .minus_pipe => .{ .left = 50, .right = 51 },
        .asterisk, .slash, .percent, .asterisk_percent, .asterisk_pipe => .{ .left = 60, .right = 61 },
        else => .{ .left = 0, .right = 0 },
    };
}

pub fn isAssignment(tag: Tag) bool {
    const power = bindingPower(tag);
    return power.left == 10;
}

test "complete operator families have binding powers" {
    try @import("std").testing.expect(bindingPower(.caret_shl).left != 0);
    try @import("std").testing.expect(bindingPower(.shr_pipe_equal).left != 0);
    try @import("std").testing.expect(bindingPower(.asterisk_percent).left > bindingPower(.plus).left);
}
