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

    pub const Index = u32;

    pub const Data = struct {
        lhs: Index,
        rhs: Index,
    };

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
        match_stmt,
        return_stmt,
        break_stmt,
        continue_stmt,
        defer_stmt,
        errdefer_stmt,
        
        // Expressions
        binary_op,
        unary_op,
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
        
        // Types & Aggregates
        pointer_type,
        slice_type,
        array_type,
        optional_type,
        error_union_type,
        struct_decl,
        enum_decl,
        union_decl,
        tuple_type,
        
        // Builtins & Intrinsic
        nocopy_builtin,
        move_builtin,
        typeof_builtin,
        sizeof_builtin,
        builtin_call,
    };
};
