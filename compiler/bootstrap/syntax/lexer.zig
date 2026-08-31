const std = @import("std");
const Token = @import("token.zig").Token;
const Tag = Token.Tag;

pub const Lexer = struct {
    buffer: [:0]const u8,
    index: u32,

    paren_level: u32,
    bracket_level: u32,
    last_tag: ?Tag,

    pub fn init(buffer: [:0]const u8) Lexer {
        return .{
            .buffer = buffer,
            .index = 0,
            .paren_level = 0,
            .bracket_level = 0,
            .last_tag = null,
        };
    }

    const State = enum {
        start,
        identifier,
        int_or_float,
        string_literal,
        char_literal,
        byte_string,
        line_comment,
        block_comment,
    };

    pub fn next(self: *Lexer) Token {
        var result = Token{
            .tag = .eof,
            .start = self.index,
            .end = self.index,
        };

        var state: State = .start;

        while (true) {
            const c = self.buffer[self.index];
            switch (state) {
                .start => switch (c) {
                    0 => {
                        if (self.index == self.buffer.len) {
                            result.tag = .eof;
                            result.start = self.index;
                            result.end = self.index;
                            return self.emit(result);
                        }
                        self.index += 1;
                        result.tag = .invalid;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    ' ', '\t', '\r' => {
                        self.index += 1;
                        result.start = self.index;
                    },
                    '\n' => {
                        self.index += 1;
                        if (self.shouldEmitStatementEnd()) {
                            result.tag = .statement_end;
                            result.end = self.index;
                            return self.emit(result);
                        } else {
                            result.start = self.index;
                        }
                    },
                    'a'...'z', 'A'...'Z', '_' => {
                        state = .identifier;
                        self.index += 1;
                    },
                    '0'...'9' => {
                        state = .int_or_float;
                        self.index += 1;
                    },
                    '"' => {
                        state = .string_literal;
                        self.index += 1;
                    },
                    '\'' => {
                        state = .char_literal;
                        self.index += 1;
                    },
                    '(' => {
                        result.tag = .l_paren;
                        self.index += 1;
                        result.end = self.index;
                        self.paren_level += 1;
                        return self.emit(result);
                    },
                    ')' => {
                        result.tag = .r_paren;
                        self.index += 1;
                        result.end = self.index;
                        if (self.paren_level > 0) self.paren_level -= 1;
                        return self.emit(result);
                    },
                    '[' => {
                        result.tag = .l_bracket;
                        self.index += 1;
                        result.end = self.index;
                        self.bracket_level += 1;
                        return self.emit(result);
                    },
                    ']' => {
                        result.tag = .r_bracket;
                        self.index += 1;
                        result.end = self.index;
                        if (self.bracket_level > 0) self.bracket_level -= 1;
                        return self.emit(result);
                    },
                    '{' => {
                        result.tag = .l_brace;
                        self.index += 1;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    '}' => {
                        result.tag = .r_brace;
                        self.index += 1;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    ',' => {
                        result.tag = .comma;
                        self.index += 1;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    ':' => {
                        result.tag = .colon;
                        self.index += 1;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    '@' => {
                        result.tag = .at;
                        self.index += 1;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    else => {
                        // Check if it's a comment
                        if (c == '/' and self.index + 1 < self.buffer.len) {
                            if (self.buffer[self.index + 1] == '/') {
                                state = .line_comment;
                                self.index += 2;
                                continue;
                            } else if (self.buffer[self.index + 1] == '*') {
                                state = .block_comment;
                                self.index += 2;
                                continue;
                            }
                        }

                        result.tag = self.lexOperator();
                        if (result.tag != .invalid) {
                            result.end = self.index;
                            return self.emit(result);
                        }

                        // Invalid char
                        self.index += 1;
                        result.tag = .invalid;
                        result.end = self.index;
                        return self.emit(result);
                    },
                },
                .identifier => switch (c) {
                    'a'...'z', 'A'...'Z', '0'...'9', '_' => {
                        self.index += 1;
                    },
                    else => {
                        result.tag = Tag.getKeyword(self.buffer[result.start..self.index]) orelse .ident;
                        result.end = self.index;
                        return self.emit(result);
                    },
                },
                .int_or_float => {
                    // `..` starts a range and is never part of a floating literal.
                    if (c == '.' and self.index + 1 < self.buffer.len and self.buffer[self.index + 1] == '.') {
                        const number = self.buffer[result.start..self.index];
                        const is_hex = std.mem.startsWith(u8, number, "0x") or std.mem.startsWith(u8, number, "0X");
                        const has_fraction = std.mem.indexOfScalar(u8, number, '.') != null;
                        const has_exponent = if (is_hex)
                            std.mem.indexOfAny(u8, number, "pP") != null
                        else
                            std.mem.indexOfAny(u8, number, "eE") != null;
                        result.tag = if (has_fraction or has_exponent) .float else .integer;
                        result.end = self.index;
                        return self.emit(result);
                    }
                    switch (c) {
                        'a'...'z', 'A'...'Z', '0'...'9', '.', '_' => {
                            self.index += 1;
                        },
                        else => {
                            const number = self.buffer[result.start..self.index];
                            const is_hex = std.mem.startsWith(u8, number, "0x") or std.mem.startsWith(u8, number, "0X");
                            const has_fraction = std.mem.indexOfScalar(u8, number, '.') != null;
                            const has_exponent = if (is_hex)
                                std.mem.indexOfAny(u8, number, "pP") != null
                            else
                                std.mem.indexOfAny(u8, number, "eE") != null;
                            result.tag = if (has_fraction or has_exponent) .float else .integer;
                            result.end = self.index;
                            return self.emit(result);
                        },
                    }
                },
                .string_literal => switch (c) {
                    0 => {
                        if (self.index == self.buffer.len) {
                            result.tag = .invalid;
                            result.end = self.index;
                            return self.emit(result);
                        }
                        self.index += 1;
                    },
                    '"' => {
                        self.index += 1;
                        result.tag = .string;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    '\\' => {
                        self.index += 2;
                    },
                    else => {
                        self.index += 1;
                    },
                },
                .char_literal => switch (c) {
                    0 => {
                        if (self.index == self.buffer.len) {
                            result.tag = .invalid;
                            result.end = self.index;
                            return self.emit(result);
                        }
                        self.index += 1;
                    },
                    '\'' => {
                        self.index += 1;
                        result.tag = .char;
                        result.end = self.index;
                        return self.emit(result);
                    },
                    '\\' => {
                        self.index += 2;
                    },
                    else => {
                        self.index += 1;
                    },
                },
                .line_comment => switch (c) {
                    0 => {
                        if (self.index == self.buffer.len) {
                            state = .start;
                        } else {
                            self.index += 1;
                        }
                    },
                    '\n' => {
                        state = .start;
                        // Do not consume newline, let the next start phase see it
                    },
                    else => {
                        self.index += 1;
                    },
                },
                .block_comment => {
                    // We need a nested level tracker. For simplicity here just flat block comment.
                    // To do nested block comments properly, we'll refine this later.
                    if (c == 0 and self.index == self.buffer.len) {
                        result.tag = .invalid;
                        result.end = self.index;
                        return self.emit(result);
                    }
                    if (c == '*' and self.buffer[self.index + 1] == '/') {
                        self.index += 2;
                        state = .start;
                        result.start = self.index; // reset start
                    } else {
                        self.index += 1;
                    }
                },
                else => unreachable,
            }
        }
    }

    fn match(self: *Lexer, expected: u8) bool {
        if (self.index < self.buffer.len and self.buffer[self.index] == expected) {
            self.index += 1;
            return true;
        }
        return false;
    }

    fn lexOperator(self: *Lexer) Tag {
        const c = self.buffer[self.index];
        self.index += 1;

        switch (c) {
            '=' => {
                if (self.match('=')) return .equal_equal;
                if (self.match('>')) return .arrow;
                return .equal;
            },
            '!' => {
                if (self.match('=')) return .bang_equal;
                return .bang;
            },
            '<' => {
                if (self.match('<')) {
                    if (self.match('%')) {
                        if (self.match('|')) {
                            if (self.match('=')) return .shl_percent_pipe_equal;
                            return .shl_percent_pipe;
                        }
                        if (self.match('=')) return .shl_percent_equal;
                        return .shl_percent;
                    }
                    if (self.match('|')) {
                        if (self.match('=')) return .shl_pipe_equal;
                        return .shl_pipe;
                    }
                    if (self.match('&')) {
                        if (self.match('=')) return .shl_ampersand_equal;
                        return .shl_ampersand;
                    }
                    if (self.match('^')) {
                        if (self.match('=')) return .shl_caret_equal;
                        return .shl_caret;
                    }
                    if (self.match('=')) return .shl_equal;
                    return .shl;
                }
                if (self.match('=')) return .angle_bracket_left_equal;
                return .angle_bracket_left;
            },
            '>' => {
                if (self.match('>')) {
                    if (self.match('%')) {
                        if (self.match('=')) return .shr_percent_equal;
                        return .shr_percent;
                    }
                    if (self.match('&')) {
                        if (self.match('=')) return .shr_ampersand_equal;
                        return .shr_ampersand;
                    }
                    if (self.match('|')) {
                        if (self.match('=')) return .shr_pipe_equal;
                        return .shr_pipe;
                    }
                    if (self.match('^')) {
                        if (self.match('=')) return .shr_caret_equal;
                        return .shr_caret;
                    }
                    if (self.match('=')) return .shr_equal;
                    return .shr;
                }
                if (self.match('=')) return .angle_bracket_right_equal;
                return .angle_bracket_right;
            },
            '.' => {
                if (self.match('.')) return .dot_dot;
                if (self.match('*')) return .dot_asterisk;
                if (self.match('?')) return .dot_question;
                return .dot;
            },
            '?' => return .question,
            '+' => return self.lexArithBase(.plus),
            '-' => return self.lexArithBase(.minus),
            '*' => return self.lexArithBase(.asterisk),
            '/' => {
                if (self.match('=')) return .slash_equal;
                return .slash;
            },
            '%' => {
                if (self.match('=')) return .percent_equal;
                return .percent;
            },
            '&' => {
                if (self.match('&')) return .ampersand_ampersand;
                if (self.match('<')) {
                    if (self.match('<')) {
                        if (self.match('=')) return .ampersand_shl_equal;
                        return .ampersand_shl;
                    }
                    self.index -= 1;
                }
                if (self.match('>')) {
                    if (self.match('>')) {
                        if (self.match('=')) return .ampersand_shr_equal;
                        return .ampersand_shr;
                    }
                    self.index -= 1;
                }
                if (self.match('=')) return .ampersand_equal;
                return .ampersand;
            },
            '|' => {
                if (self.match('|')) return .pipe_pipe;
                if (self.match('<')) {
                    if (self.match('<')) {
                        if (self.match('=')) return .pipe_shl_equal;
                        return .pipe_shl;
                    }
                    self.index -= 1;
                }
                if (self.match('>')) {
                    if (self.match('>')) {
                        if (self.match('=')) return .pipe_shr_equal;
                        return .pipe_shr;
                    }
                    self.index -= 1;
                }
                if (self.match('=')) return .pipe_equal;
                return .pipe;
            },
            '^' => {
                if (self.match('<')) {
                    if (self.match('<')) {
                        if (self.match('=')) return .caret_shl_equal;
                        return .caret_shl;
                    }
                    self.index -= 1;
                }
                if (self.match('>')) {
                    if (self.match('>')) {
                        if (self.match('=')) return .caret_shr_equal;
                        return .caret_shr;
                    }
                    self.index -= 1;
                }
                if (self.match('=')) return .caret_equal;
                return .caret;
            },
            '~' => return .tilde,
            else => return .invalid,
        }
    }

    fn lexArithBase(self: *Lexer, comptime base: Tag) Tag {
        if (self.match('%')) {
            if (self.match('<')) {
                if (self.match('<')) {
                    if (self.match('=')) return getCombined(base, .shl_percent_equal);
                    return getCombined(base, .shl_percent);
                }
                self.index -= 1;
            }
            if (self.match('>')) {
                if (self.match('>')) {
                    if (self.match('=')) return getCombined(base, .shr_percent_equal);
                    return getCombined(base, .shr_percent);
                }
                self.index -= 1;
            }
            if (self.match('=')) return getCombined(base, .percent_equal);
            return getCombined(base, .percent);
        }
        if (self.match('|')) {
            if (self.match('<')) {
                if (self.match('<')) {
                    if (self.match('=')) return getCombined(base, .shl_pipe_equal);
                    return getCombined(base, .shl_pipe);
                }
                self.index -= 1;
            }
            if (self.match('>')) {
                if (self.match('>')) {
                    if (self.match('=')) return getCombined(base, .shr_pipe_equal);
                    return getCombined(base, .shr_pipe);
                }
                self.index -= 1;
            }
            if (self.match('=')) return getCombined(base, .pipe_equal);
            return getCombined(base, .pipe);
        }
        if (self.match('<')) {
            if (self.match('<')) {
                if (self.match('=')) return getCombined(base, .shl_equal);
                return getCombined(base, .shl);
            }
            self.index -= 1;
        }
        if (self.match('>')) {
            if (self.match('>')) {
                if (self.match('=')) return getCombined(base, .shr_equal);
                return getCombined(base, .shr);
            }
            self.index -= 1;
        }
        if (self.match('=')) return getCombined(base, .equal);
        return base;
    }

    fn getCombined(base: Tag, mod: Tag) Tag {
        return switch (base) {
            .plus => switch (mod) {
                .percent => .plus_percent,
                .percent_equal => .plus_percent_equal,
                .pipe => .plus_pipe,
                .pipe_equal => .plus_pipe_equal,
                .shl => .plus_shl,
                .shl_equal => .plus_shl_equal,
                .shr => .plus_shr,
                .shr_equal => .plus_shr_equal,
                .shl_percent => .plus_percent_shl,
                .shl_percent_equal => .plus_percent_shl_equal,
                .shr_percent => .plus_percent_shr,
                .shr_percent_equal => .plus_percent_shr_equal,
                .shl_pipe => .plus_pipe_shl,
                .shl_pipe_equal => .plus_pipe_shl_equal,
                .shr_pipe => .plus_pipe_shr,
                .shr_pipe_equal => .plus_pipe_shr_equal,
                .equal => .plus_equal,
                else => unreachable,
            },
            .minus => switch (mod) {
                .percent => .minus_percent,
                .percent_equal => .minus_percent_equal,
                .pipe => .minus_pipe,
                .pipe_equal => .minus_pipe_equal,
                .shl => .minus_shl,
                .shl_equal => .minus_shl_equal,
                .shr => .minus_shr,
                .shr_equal => .minus_shr_equal,
                .shl_percent => .minus_percent_shl,
                .shl_percent_equal => .minus_percent_shl_equal,
                .shr_percent => .minus_percent_shr,
                .shr_percent_equal => .minus_percent_shr_equal,
                .shl_pipe => .minus_pipe_shl,
                .shl_pipe_equal => .minus_pipe_shl_equal,
                .shr_pipe => .minus_pipe_shr,
                .shr_pipe_equal => .minus_pipe_shr_equal,
                .equal => .minus_equal,
                else => unreachable,
            },
            .asterisk => switch (mod) {
                .percent => .asterisk_percent,
                .percent_equal => .asterisk_percent_equal,
                .pipe => .asterisk_pipe,
                .pipe_equal => .asterisk_pipe_equal,
                .shl => .asterisk_shl,
                .shl_equal => .asterisk_shl_equal,
                .shr => .asterisk_shr,
                .shr_equal => .asterisk_shr_equal,
                .shl_percent => .asterisk_percent_shl,
                .shl_percent_equal => .asterisk_percent_shl_equal,
                .shr_percent => .asterisk_percent_shr,
                .shr_percent_equal => .asterisk_percent_shr_equal,
                .shl_pipe => .asterisk_pipe_shl,
                .shl_pipe_equal => .asterisk_pipe_shl_equal,
                .shr_pipe => .asterisk_pipe_shr,
                .shr_pipe_equal => .asterisk_pipe_shr_equal,
                .equal => .asterisk_equal,
                else => unreachable,
            },
            else => unreachable,
        };
    }

    fn emit(self: *Lexer, token: Token) Token {
        if (token.tag != .invalid and token.tag != .eof) {
            self.last_tag = token.tag;
        }

        return token;
    }

    fn shouldEmitStatementEnd(self: *Lexer) bool {
        if (self.paren_level > 0 or self.bracket_level > 0) return false;

        const last = self.last_tag orelse return false;

        // RequiresContinuation prevents statement_end
        if (requiresContinuation(last)) return false;

        // CanTerminate allows statement_end
        return canTerminate(last);
    }
};

fn requiresContinuation(tag: Tag) bool {
    return switch (tag) {
        // Arithmetic
        .plus,
        .minus,
        .asterisk,
        .slash,
        .percent,
        .plus_percent,
        .minus_percent,
        .asterisk_percent,
        .plus_pipe,
        .minus_pipe,
        .asterisk_pipe,

        // Comparisons
        .equal_equal,
        .bang_equal,
        .angle_bracket_left,
        .angle_bracket_left_equal,
        .angle_bracket_right,
        .angle_bracket_right_equal,

        // Logical
        .ampersand_ampersand,
        .pipe_pipe,

        // Bitwise
        .ampersand,
        .pipe,
        .caret,

        // Shifts
        .shl,
        .shr,
        .shl_percent,
        .shr_percent,
        .shl_pipe,
        .shl_percent_pipe,

        // Assignments (including compound)
        .equal,
        .plus_equal,
        .minus_equal,
        .asterisk_equal,
        .slash_equal,
        .percent_equal,

        // Punctuation
        .comma,
        .dot,
        .dot_dot,
        .arrow,
        .colon,
        => true,
        else => false,
    };
}

fn canTerminate(tag: Tag) bool {
    return switch (tag) {
        .ident,
        .integer,
        .float,
        .string,
        .byte_string,
        .char,
        .keyword_true,
        .keyword_false,
        .keyword_null,
        .keyword_undefined,
        .r_paren,
        .r_bracket,
        .r_brace,
        .keyword_return,
        .keyword_break,
        .keyword_continue,
        => true,
        else => false,
    };
}

fn expectTokens(src: [:0]const u8, expected_tags: []const Tag) !void {
    var lexer = Lexer.init(src);
    for (expected_tags) |expected| {
        const token = lexer.next();
        try std.testing.expectEqual(expected, token.tag);
    }
    const final = lexer.next();
    try std.testing.expectEqual(Tag.eof, final.tag);
}

test "lexer: keywords and identifiers" {
    try expectTokens("const var fn comptime ident_123 pub", &.{ .keyword_const, .keyword_var, .keyword_fn, .keyword_comptime, .ident, .keyword_pub });
}

test "lexer: numbers and strings (simple)" {
    try expectTokens("123 3.14 \"hello\" 'a'", &.{ .integer, .float, .string, .char });
}

test "lexer: integer range is not a float" {
    try expectTokens("0..8", &.{ .integer, .dot_dot, .integer });
}

test "lexer: basic operators" {
    try expectTokens("+ - * / % == != <= >= << >>", &.{ .plus, .minus, .asterisk, .slash, .percent, .equal_equal, .bang_equal, .angle_bracket_left_equal, .angle_bracket_right_equal, .shl, .shr });
}

test "lexer: shift combine operators" {
    try expectTokens("+<< -<< +%<< +|<< +%<<= +|<<=", &.{ .plus_shl, .minus_shl, .plus_percent_shl, .plus_pipe_shl, .plus_percent_shl_equal, .plus_pipe_shl_equal });
    try expectTokens("&<< |>> ^<< <<& >>|", &.{ .ampersand_shl, .pipe_shr, .caret_shl, .shl_ampersand, .shr_pipe });
}

test "lexer: newline filtering" {
    // newline after ident emits statement_end
    try expectTokens("a\nb", &.{ .ident, .statement_end, .ident });

    // newline after + does NOT emit statement_end (RequiresContinuation)
    try expectTokens("a +\nb", &.{ .ident, .plus, .ident });

    // newline inside parens does NOT emit statement_end
    try expectTokens("(\na\n)", &.{ .l_paren, .ident, .r_paren });

    // newline inside brackets does NOT emit statement_end
    try expectTokens("[\na\n]", &.{ .l_bracket, .ident, .r_bracket });

    // newline after { does not emit (unless braces logic changes, but our logic only prevents inside paren/bracket)
    // Wait, `{` is not in CanTerminate, so it doesn't emit
    try expectTokens("{\na", &.{ .l_brace, .ident });
}

test "lexer: comments" {
    try expectTokens("a // this is a comment\nb /* block \n comment */ c", &.{ .ident, .statement_end, .ident, .ident });
}
