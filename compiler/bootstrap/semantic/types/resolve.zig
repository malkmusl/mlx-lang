const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;
const aggregate = @import("aggregate.zig");
const error_set = @import("error_set.zig");

/// Resolves an annotation-position AST node to an interned semantic type.
pub fn resolve(sema: anytype, node_index: Node.Index) std.mem.Allocator.Error!Type.Id {
    const node = sema.ast_tree.nodes.get(node_index);
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    return switch (node.tag) {
        .identifier => blk: {
            const token = sema.ast_tree.tokens[node.main_token];
            const name = source[token.start..token.end];
            if (sema.root_scope.get(name)) |symbol| {
                if (sema.type_values.get(symbol.decl_node)) |type_value| break :blk type_value;
            }
            break :blk resolveBuiltinName(sema, name, token.start);
        },
        .builtin_call => blk: {
            if (sema.type_values.get(node_index)) |type_value| break :blk type_value;
            try sema.reportError(5005, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "Builtin expression does not denote a type");
            break :blk sema.type_pool.internPrimitive(.void_type);
        },
        .struct_decl, .enum_decl, .union_decl => blk: {
            if (sema.type_values.get(node_index)) |type_value| break :blk type_value;
            _ = try aggregate.analyze(sema, node_index, sema.root_scope);
            break :blk sema.type_values.get(node_index) orelse sema.type_pool.internPrimitive(.void_type);
        },
        .error_set_decl => blk: {
            if (sema.type_values.get(node_index)) |type_value| break :blk type_value;
            _ = try error_set.analyze(sema, node_index, sema.root_scope);
            break :blk sema.type_values.get(node_index) orelse sema.type_pool.internPrimitive(.void_type);
        },
        .pointer_type => resolvePointer(sema, node),
        .slice_type => resolveSlice(sema, node),
        .optional_type => blk: {
            const child = try resolve(sema, node.data.lhs);
            break :blk sema.type_pool.intern(.{ .optional = .{ .child_type = child } }, .copyable);
        },
        .array_type => resolveArray(sema, node, source),
        .tuple_type => resolveTuple(sema, node),
        .fn_type => resolveFunction(sema, node),
        .error_union_type => blk: {
            const payload = try resolve(sema, node.data.rhs);
            const errors: Type.Id = if (node.data.lhs != 0)
                try resolve(sema, node.data.lhs)
            else
                try sema.type_pool.internPrimitive(.void_type);
            break :blk sema.type_pool.intern(.{ .error_union = .{ .err_set = errors, .payload = payload } }, .copyable);
        },
        else => sema.type_pool.internPrimitive(.void_type),
    };
}

fn resolveFunction(sema: anytype, node: Node) std.mem.Allocator.Error!Type.Id {
    const return_type = try resolve(sema, sema.ast_tree.extra_data[node.data.lhs]);
    const count = sema.ast_tree.extra_data[node.data.lhs + 1];
    var parameters = std.ArrayList(Type.Id).empty;
    defer parameters.deinit(sema.allocator);
    var index: u32 = 0;
    while (index < count) : (index += 1) {
        try parameters.append(sema.allocator, try resolve(sema, sema.ast_tree.extra_data[node.data.lhs + 2 + index]));
    }
    const params_start = try sema.type_pool.appendParams(parameters.items);
    return sema.type_pool.intern(.{ .function = .{
        .ret_type = return_type,
        .params_start = params_start,
        .params_len = count,
        .is_var_args = false,
    } }, .copyable);
}

fn resolveTuple(sema: anytype, node: Node) std.mem.Allocator.Error!Type.Id {
    const count = sema.ast_tree.extra_data[node.data.lhs];
    var fields = std.ArrayList(@import("../type.zig").TypePool.AggregateFieldInput).empty;
    defer fields.deinit(sema.allocator);
    var index: u32 = 0;
    while (index < count) : (index += 1) {
        try fields.append(sema.allocator, .{
            .name = "",
            .type_id = try resolve(sema, sema.ast_tree.extra_data[node.data.lhs + 1 + index]),
        });
    }
    return sema.type_pool.internAggregate(.tuple, fields.items, null, null, false) catch |err| switch (err) {
        error.OutOfMemory => error.OutOfMemory,
        else => {
            try sema.reportError(5005, .@"comptime", sema.ast_tree.tokens[node.main_token].start, "Tuple layout could not be computed");
            return sema.type_pool.internPrimitive(.void_type);
        },
    };
}

fn resolvePointer(sema: anytype, node: Node) std.mem.Allocator.Error!Type.Id {
    const child = try resolve(sema, node.data.lhs);
    var flags = node.data.rhs;
    var explicit_alignment: ?u64 = null;
    if ((flags & 0x8000_0000) != 0) {
        const qualifier_start = flags & 0x7fff_ffff;
        flags = sema.ast_tree.extra_data[qualifier_start];
        const alignment_node = sema.ast_tree.extra_data[qualifier_start + 1];
        explicit_alignment = comptimeInteger(sema, alignment_node);
        if (explicit_alignment == null or explicit_alignment.? == 0 or !std.math.isPowerOfTwo(explicit_alignment.?)) {
            try sema.reportError(7002, .sema, sema.ast_tree.tokens[node.main_token].start, "Pointer alignment must be a comptime power of two");
            explicit_alignment = null;
        }
    }
    return sema.type_pool.intern(.{ .pointer = .{
        .child_type = child,
        .is_const = (flags & 1) != 0,
        .is_volatile = (flags & 8) != 0,
        .is_allowzero = false,
        .is_optional = false,
        .alignment = explicit_alignment,
        .size = if ((flags & 2) != 0) .Many else .One,
        .sentinel = null,
    } }, .copyable);
}

fn resolveSlice(sema: anytype, node: Node) std.mem.Allocator.Error!Type.Id {
    const child = try resolve(sema, node.data.lhs);
    return sema.type_pool.intern(.{ .pointer = .{
        .child_type = child,
        .is_const = (node.data.rhs & 1) != 0,
        .is_volatile = false,
        .is_allowzero = false,
        .is_optional = false,
        .alignment = null,
        .size = .Slice,
        .sentinel = null,
    } }, .copyable);
}

fn resolveArray(sema: anytype, node: Node, source: []const u8) std.mem.Allocator.Error!Type.Id {
    const element_node = sema.ast_tree.extra_data[node.data.lhs + 1];
    const element_type = try resolve(sema, element_node);
    const size_node_index = sema.ast_tree.extra_data[node.data.lhs];
    const size_node = sema.ast_tree.nodes.get(size_node_index);
    var length: u64 = 0;
    if (size_node.tag == .integer_literal) {
        const token = sema.ast_tree.tokens[size_node.main_token];
        length = std.fmt.parseInt(u64, source[token.start..token.end], 10) catch 0;
    }
    return sema.type_pool.intern(.{ .array = .{
        .child_type = element_type,
        .len = length,
        .sentinel = null,
    } }, .copyable);
}

pub fn comptimeInteger(sema: anytype, node_index: Node.Index) ?u64 {
    if (sema.const_values.get(node_index)) |value| return value;
    const node = sema.ast_tree.nodes.get(node_index);
    if (node.tag != .integer_literal) return null;
    const token = sema.ast_tree.tokens[node.main_token];
    const source = sema.diags.source_manager.getFile(sema.source_id).?.content;
    return std.fmt.parseInt(u64, source[token.start..token.end], 0) catch null;
}

fn resolveBuiltinName(sema: anytype, name: []const u8, start_byte: u32) std.mem.Allocator.Error!Type.Id {
    if (std.mem.eql(u8, name, "void")) return sema.type_pool.internPrimitive(.void_type);
    if (std.mem.eql(u8, name, "noreturn")) return sema.type_pool.internPrimitive(.noreturn_type);
    if (std.mem.eql(u8, name, "bool")) return sema.type_pool.internPrimitive(.bool_type);
    if (std.mem.eql(u8, name, "type")) return sema.type_pool.internPrimitive(.type_type);
    if (std.mem.eql(u8, name, "anytype")) return sema.type_pool.internPrimitive(.anytype_type);
    if (std.mem.eql(u8, name, "anyopaque")) return sema.type_pool.internPrimitive(.anyopaque_type);
    if (std.mem.eql(u8, name, "comptime_int")) return sema.type_pool.internPrimitive(.comptime_int_type);
    if (std.mem.eql(u8, name, "comptime_float")) return sema.type_pool.internPrimitive(.comptime_float_type);
    if (std.mem.eql(u8, name, "f16")) return sema.type_pool.internPrimitive(.f16_type);
    if (std.mem.eql(u8, name, "f32")) return sema.type_pool.internPrimitive(.f32_type);
    if (std.mem.eql(u8, name, "f64")) return sema.type_pool.internPrimitive(.f64_type);
    if (std.mem.eql(u8, name, "f80")) return sema.type_pool.internPrimitive(.f80_type);
    if (std.mem.eql(u8, name, "f128")) return sema.type_pool.internPrimitive(.f128_type);
    if (std.mem.eql(u8, name, "usize")) return sema.type_pool.internSizeInt(false);
    if (std.mem.eql(u8, name, "isize")) return sema.type_pool.internSizeInt(true);
    if (name.len >= 2) {
        const signed = name[0] == 'i';
        const unsigned = name[0] == 'u';
        if ((signed or unsigned) and name.len <= 5) {
            const bits = std.fmt.parseInt(u16, name[1..], 10) catch 0;
            if (bits > 0 and bits <= 4096) return sema.type_pool.internInt(signed, bits);
        }
    }
    try sema.reportError(3001, .resolve, start_byte, "Unknown type name");
    return sema.type_pool.internPrimitive(.void_type);
}
