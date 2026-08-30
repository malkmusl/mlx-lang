const std = @import("std");
const Token = @import("token.zig").Token;

pub const Ast = struct {
    tokens: []const Token,
    nodes: std.MultiArrayList(Node),
    extra_data: []const u32,

    pub fn deinit(self: *Ast, allocator: std.mem.Allocator) void {
        allocator.free(self.tokens);
        self.nodes.deinit(allocator);
        allocator.free(self.extra_data);
    }
};

pub const Node = struct {
    tag: Tag,
    main_token: u32,
    data: Data,
    decl_flags: DeclFlags = .{},
    extern_name_token: u32 = std.math.maxInt(u32),

    pub const Index = u32;

    pub const DeclFlags = packed struct(u8) {
        public: bool = false,
        exported: bool = false,
        inline_hint: bool = false,
        noinline_hint: bool = false,
        extern_decl: bool = false,
        comptime_param: bool = false,
        _padding: u2 = 0,
    };

    pub const Data = struct {
        lhs: Index,
        rhs: Index,
    };

    /// Node layout documentation:
    ///
    /// `const_decl` / `var_decl`:
    ///   main_token = token index of `const` / `var` keyword
    ///   lhs = ident token index
    ///   rhs = extra_data index where:
    ///     extra_data[rhs + 0] = type annotation node (0 = inferred)
    ///     extra_data[rhs + 1] = init expression node
    ///   (stored as extra_data entries so both type + expr can be stored)
    ///   When rhs == 0 (old layout fallback): lhs=ident_tok, rhs=init_node.
    ///
    /// `fn_decl`:
    ///   main_token = `fn` token
    ///   lhs = fn_proto node
    ///   rhs = block node
    ///
    /// `fn_proto`:
    ///   main_token = ident token (function name)
    ///   lhs = extra_start (extra_data index)
    ///   rhs = extra_len  (extra_data[lhs] = required ret_type_node, [lhs+1..] = param nodes)
    ///
    /// `param_decl`:
    ///   main_token = ident token (param name)
    ///   lhs = ident token index
    ///   rhs = type annotation node (0 = anytype)
    ///
    /// `block`:
    ///   main_token = `{` token
    ///   lhs = extra_start
    ///   rhs = extra_end   (exclusive) — extra_data[lhs..rhs] are child node indices
    ///
    /// `binary_op`:
    ///   main_token = operator token
    ///   lhs = left operand node
    ///   rhs = right operand node
    ///
    /// `call`:
    ///   main_token = `(` token
    ///   lhs = callee node
    ///   rhs = extra_start where:
    ///     extra_data[rhs + 0] = arg count N
    ///     extra_data[rhs + 1 .. rhs + 1 + N] = arg nodes
    ///
    /// `pointer_type`:
    ///   main_token = `*` token
    ///   lhs = child type node
    ///   rhs = flags: bit0 = is_const, bit1 = is_many ([*]), bit2 = is_slice ([])
    ///
    /// `optional_type`:
    ///   main_token = `?` token
    ///   lhs = child type node
    ///   rhs = 0
    ///
    /// `array_type`:
    ///   main_token = `[` token
    ///   lhs = extra_start where:
    ///     extra_data[lhs + 0] = size expression node
    ///     extra_data[lhs + 1] = element type node
    ///   rhs = 0
    ///
    /// `error_union_type`:
    ///   main_token = `!` token
    ///   lhs = error set node (0 = inferred error set)
    ///   rhs = payload type node
    ///
    /// `if_stmt`:
    ///   main_token = `if` token
    ///   lhs = extra_start
    ///   rhs = extra_end where:
    ///     extra_data[lhs + 0] = condition node
    ///     extra_data[lhs + 1] = then-block node
    ///     extra_data[lhs + 2] = else-block node (optional; only present if rhs > lhs + 2)
    ///
    /// `while_stmt`:
    ///   main_token = `while` token
    ///   lhs = extra_start where:
    ///     extra_data[lhs + 0] = label token, or maxInt(u32)
    ///     extra_data[lhs + 1] = condition node
    ///     extra_data[lhs + 2] = body block node
    ///   rhs = lhs + 3
    ///
    /// `for_stmt`:
    ///   main_token = `for` token
    ///   lhs = extra_start where:
    ///     extra_data[lhs + 0] = label token, or maxInt(u32)
    ///     extra_data[lhs + 1] = capture flags (bit0 = pointer capture)
    ///     extra_data[lhs + 2] = item capture token
    ///     extra_data[lhs + 3] = index capture token, or maxInt(u32)
    ///     extra_data[lhs + 4] = iterable/range node
    ///     extra_data[lhs + 5] = body block node
    ///   rhs = lhs + 6
    ///
    /// `range`:
    ///   main_token = `..` token
    ///   lhs = inclusive start expression
    ///   rhs = exclusive end expression
    ///
    /// `array_literal`:
    ///   main_token = `[` token
    ///   lhs = extra_start where:
    ///     extra_data[lhs + 0] = declared length node, or maxInt(u32) for `[_]`
    ///     extra_data[lhs + 1] = element type node
    ///     extra_data[lhs + 2] = element count
    ///     extra_data[lhs + 3..] = element expression nodes
    ///   rhs = lhs + 3 + element count
    ///
    /// `break_stmt` / `continue_stmt`:
    ///   main_token = keyword token
    ///   lhs = label token, or maxInt(u32)
    ///   rhs = break value node, or maxInt(u32)
    ///
    /// `return_stmt`:
    ///   main_token = `return` token
    ///   lhs = 0
    ///   rhs = expression node, or maxInt(u32) for a value-less return
    pub const Tag = enum {
        root,

        // Declarations
        fn_decl,
        fn_proto,
        param_decl,
        var_decl,
        const_decl,
        test_decl,

        // Statements
        block,
        if_stmt,
        while_stmt,
        for_stmt,
        match_stmt,
        return_stmt,
        break_stmt,
        continue_stmt,
        defer_stmt,
        errdefer_stmt,
        unsafe_block,

        // Expressions
        binary_op,
        unary_op,
        range,
        call,
        field_access,
        array_access,
        slice,

        // Literals & Primitives
        identifier,
        integer_literal,
        float_literal,
        string_literal,
        char_literal,
        bool_literal,
        null_literal,
        undefined_literal,
        tuple_literal,
        array_literal,

        // Type expressions (appear in type-annotation position)
        pointer_type, // *T, *const T, [*]T, [*]const T
        slice_type, // []T, []const T
        array_type, // [N]T
        optional_type, // ?T
        error_union_type, // E!T or !T

        // Aggregate type declarations
        struct_decl,
        enum_decl,
        union_decl,
        field_decl,
        enum_member,
        union_member,
        tuple_type,

        // Builtins & Intrinsics
        builtin_call,
    };
};
