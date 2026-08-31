const std = @import("std");
const Node = @import("../../syntax/ast.zig").Node;
const Type = @import("../type.zig").Type;
const Scope = @import("../scope.zig").Scope;
const builtin = @import("../builtin.zig");

pub fn analyze(self: anytype, node_idx: Node.Index, scope: *Scope) std.mem.Allocator.Error!Type.Id {
    const node = self.ast_tree.nodes.get(node_idx);
    const source = self.diags.source_manager.getFile(self.source_id).?.content;
    const name_token = self.ast_tree.tokens[node.data.lhs];
    const name = source[name_token.start..name_token.end];
    const kind = builtin.lookup(name) orelse {
        try self.reportError(3001, .resolve, name_token.start, "Unknown builtin");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
    };
    const extra_start = node.data.rhs;
    const argument_count = self.ast_tree.extra_data[extra_start];
    const arguments = self.ast_tree.extra_data[extra_start + 1 .. extra_start + 1 + argument_count];
    if (builtin.arity(kind)) |expected| {
        if (!expected.accepts(arguments.len)) {
            try self.reportError(5005, .@"comptime", name_token.start, "Invalid builtin argument count");
            return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
        }
    }

    return switch (kind) {
        .typeOf => analyzeTypeOf(self, node_idx, arguments, scope),
        .nocopy => analyzeNoCopy(self, node_idx, arguments, scope, name_token.start),
        .move => analyzeMove(self, node_idx, arguments, scope, name_token.start, source),
        .sizeOf, .alignOf, .bitSizeOf => analyzeLayout(self, kind, node_idx, arguments, scope, name_token.start),
        .isInteger, .isFloat, .isStruct, .isEnum, .isUnion, .isPointer, .isSlice, .isArray, .isOptional, .isErrorUnion, .isCopyable => analyzePredicate(self, kind, node_idx, arguments, scope, name_token.start),
        .Vector => analyzeVector(self, node_idx, arguments, scope, name_token.start),
        .intCast, .floatCast, .floatFromInt, .intFromFloat, .ptrCast, .alignCast, .bitCast, .ptrFromInt, .enumFromInt => self.analyzeCastBuiltin(kind, node_idx, arguments, scope, name_token.start),
        .intFromPtr => analyzeIntFromPtr(self, node_idx, arguments, scope, name_token.start),
        .intFromEnum => analyzeIntFromEnum(self, node_idx, arguments, scope, name_token.start),
        .discardError => analyzeDiscardError(self, node_idx, arguments, scope, name_token.start),
        .compileError => analyzeCompileError(self, node_idx, arguments, node.main_token),
        .setEvalBranchQuota => analyzeQuota(self, node_idx, arguments, scope, name_token.start),
        .import => analyzeImport(self, node_idx, arguments, name_token.start),
        .fieldCount => analyzeFieldCount(self, node_idx, arguments, scope, name_token.start),
        .fieldType => analyzeFieldType(self, node_idx, arguments, scope, name_token.start),
        .hasField => analyzeHasField(self, node_idx, arguments, scope, name_token.start),
        .offsetOf => analyzeOffsetOf(self, node_idx, arguments, scope, name_token.start),
        .field => analyzeField(self, node_idx, arguments, scope, name_token.start),
        .tagOf => analyzeTagOf(self, node_idx, arguments, scope, name_token.start),
        .typeInfo, .hasDecl, .decl, .languageVersion => unsupportedReflection(self, node_idx, name_token.start),
        .atomicLoad, .atomicRmw, .atomicStore, .cmpxchgStrong, .cmpxchgWeak, .fence, .splat, .shuffle, .reduce, .select => unsupportedLowering(self, node_idx, arguments, scope, name_token.start),
    };
}

fn analyzeTypeOf(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope) !Type.Id {
    const value_type = try self.analyzeNode(arguments[0], scope);
    try self.type_values.put(node_idx, value_type);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
}

fn analyzeNoCopy(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const wrapped = try self.resolveBuiltinTypeArg(arguments[0], scope) orelse {
        try self.reportError(5005, .@"comptime", start, "@nocopy requires a type argument");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    };
    const nocopy_type = try self.type_pool.intern(self.type_pool.get(wrapped).data, .explicit_nocopy);
    try self.type_values.put(node_idx, nocopy_type);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
}

fn analyzeMove(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32, source: []const u8) !Type.Id {
    const argument_node = self.ast_tree.nodes.get(arguments[0]);
    const value_type = try self.analyzeNode(arguments[0], scope);
    if (argument_node.tag != .identifier and argument_node.tag != .field_access) {
        try self.reportError(6003, .sema, start, "@move source must be addressable storage");
        return self.putBuiltinResult(node_idx, value_type);
    }
    if (argument_node.tag == .identifier) {
        const token = self.ast_tree.tokens[argument_node.main_token];
        if (scope.get(source[token.start..token.end])) |symbol| try self.local_states.put(symbol.decl_node, .moved);
    }
    if (self.type_pool.get(value_type).isCopyable()) try self.reportWarningPublic(6001, .sema, start, "Redundant move of a copyable value");
    return self.putBuiltinResult(node_idx, value_type);
}

fn analyzeLayout(self: anytype, kind: builtin.Kind, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const type_value = try self.resolveBuiltinTypeArg(arguments[0], scope) orelse {
        try self.reportError(5005, .@"comptime", start, "Layout builtin requires a type argument");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
    };
    const value = switch (kind) {
        .sizeOf => self.type_pool.sizeOf(type_value) catch return layoutFailure(self, node_idx, start, "Type has no runtime size"),
        .alignOf => self.type_pool.alignOf(type_value) catch return layoutFailure(self, node_idx, start, "Type has no runtime alignment"),
        .bitSizeOf => self.type_pool.bitSizeOf(type_value) catch return layoutFailure(self, node_idx, start, "Type has no runtime bit size"),
        else => unreachable,
    };
    try self.const_values.put(node_idx, value);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
}

fn layoutFailure(self: anytype, node_idx: Node.Index, start: u32, message: []const u8) !Type.Id {
    try self.reportError(5005, .@"comptime", start, message);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
}

fn analyzePredicate(self: anytype, kind: builtin.Kind, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const type_value = try self.resolveBuiltinTypeArg(arguments[0], scope) orelse {
        try self.reportError(5005, .@"comptime", start, "Type predicate requires a type argument");
        return self.putBoolBuiltinResult(node_idx, false);
    };
    const value = self.type_pool.get(type_value);
    const result = switch (kind) {
        .isInteger => value.isInteger(),
        .isFloat => value.isFloat(),
        .isStruct => value.data == .@"struct",
        .isEnum => value.data == .@"enum",
        .isUnion => value.data == .@"union",
        .isPointer => value.data == .pointer and value.data.pointer.size != .Slice,
        .isSlice => value.data == .pointer and value.data.pointer.size == .Slice,
        .isArray => value.data == .array,
        .isOptional => value.data == .optional,
        .isErrorUnion => value.data == .error_union,
        .isCopyable => value.isCopyable(),
        else => unreachable,
    };
    return self.putBoolBuiltinResult(node_idx, result);
}

fn analyzeVector(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    _ = try self.analyzeNode(arguments[0], scope);
    const length = self.const_values.get(arguments[0]) orelse {
        try self.reportError(5001, .@"comptime", start, "@Vector length must be comptime-known");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    };
    if (length == 0 or length > std.math.maxInt(u32)) {
        try self.reportError(4002, .sema, start, "Invalid vector length");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    }
    const child_type = try self.resolveBuiltinTypeArg(arguments[1], scope) orelse {
        try self.reportError(5005, .@"comptime", start, "@Vector element must be a type");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    };
    const child = self.type_pool.get(child_type);
    if (!child.isInteger() and !child.isFloat() and !(child.data == .primitive and child.data.primitive == .bool_type)) {
        try self.reportError(4001, .sema, start, "Vector element type must be integer, float, or bool");
    }
    const vector_type = try self.type_pool.intern(.{ .vector = .{ .len = @intCast(length), .child_type = child_type } }, .copyable);
    try self.type_values.put(node_idx, vector_type);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
}

fn analyzeIntFromPtr(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const source_type = try self.analyzeNode(arguments[0], scope);
    if (self.type_pool.get(source_type).data != .pointer) try self.reportError(4003, .sema, start, "@intFromPtr requires a pointer");
    return self.putBuiltinResult(node_idx, try self.type_pool.internSizeInt(false));
}

fn analyzeIntFromEnum(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const source_type = try self.analyzeNode(arguments[0], scope);
    if (self.type_pool.get(source_type).data != .@"enum") {
        try self.reportError(4003, .sema, start, "@intFromEnum requires an enum value");
        return self.putBuiltinResult(node_idx, try self.type_pool.internSizeInt(false));
    }
    const backing = (self.type_pool.aggregateInfo(source_type) orelse unreachable).backing_type orelse unreachable;
    return self.putBuiltinResult(node_idx, backing);
}

fn analyzeDiscardError(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const value_type = try self.analyzeNode(arguments[0], scope);
    if (self.type_pool.get(value_type).data != .error_union) try self.reportError(4001, .sema, start, "@discardError requires an error union");
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
}

fn analyzeCompileError(self: anytype, node_idx: Node.Index, arguments: []const u32, main_token: u32) !Type.Id {
    const message = self.stringLiteralContent(arguments[0]) orelse "@compileError requires a string literal";
    try self.reportError(5003, .@"comptime", self.ast_tree.tokens[main_token].start, message);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.noreturn_type));
}

fn analyzeQuota(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    _ = try self.analyzeNode(arguments[0], scope);
    self.eval_branch_quota = self.const_values.get(arguments[0]) orelse {
        try self.reportError(5001, .@"comptime", start, "Evaluation quota must be comptime-known");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
    };
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
}

fn analyzeImport(self: anytype, node_idx: Node.Index, arguments: []const u32, start: u32) !Type.Id {
    if (self.stringLiteralContent(arguments[0]) == null) try self.reportError(5001, .@"comptime", start, "@import path must be a comptime string literal");
    if (self.import_ids) |imports| {
        if (imports.get(node_idx)) |module_id| {
            try self.module_values.put(node_idx, module_id);
        } else {
            try self.reportError(3004, .resolve, start, "Imported module was not loaded");
        }
    }
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.anyopaque_type));
}

fn analyzeFieldCount(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const aggregate_type = try requireAggregateType(self, node_idx, arguments[0], scope, start, "@fieldCount requires a type argument", .comptime_int_type) orelse return self.node_types.get(node_idx).?;
    const info = self.type_pool.aggregateInfo(aggregate_type) orelse {
        try self.reportError(5005, .@"comptime", start, "@fieldCount requires an aggregate type");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
    };
    try self.const_values.put(node_idx, info.fields_len);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
}

fn analyzeFieldType(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const aggregate_type = try requireAggregateType(self, node_idx, arguments[0], scope, start, "@fieldType requires an aggregate type", .type_type) orelse return self.node_types.get(node_idx).?;
    const field_name = self.stringLiteralContent(arguments[1]) orelse {
        try self.reportError(5001, .@"comptime", start, "@fieldType field name must be a comptime string");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    };
    const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
        try self.reportError(5005, .@"comptime", start, "Unknown aggregate field");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
    };
    try self.type_values.put(node_idx, field.type_id);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.type_type));
}

fn analyzeHasField(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const aggregate_type = try self.resolveBuiltinTypeArg(arguments[0], scope) orelse {
        try self.reportError(5005, .@"comptime", start, "@hasField requires an aggregate type");
        return self.putBoolBuiltinResult(node_idx, false);
    };
    const field_name = self.stringLiteralContent(arguments[1]) orelse {
        try self.reportError(5001, .@"comptime", start, "@hasField field name must be a comptime string");
        return self.putBoolBuiltinResult(node_idx, false);
    };
    return self.putBoolBuiltinResult(node_idx, self.type_pool.aggregateField(aggregate_type, field_name) != null);
}

fn analyzeOffsetOf(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const aggregate_type = try requireAggregateType(self, node_idx, arguments[0], scope, start, "@offsetOf requires an aggregate type", .comptime_int_type) orelse return self.node_types.get(node_idx).?;
    const field_name = self.stringLiteralContent(arguments[1]) orelse {
        try self.reportError(5001, .@"comptime", start, "@offsetOf field name must be a comptime string");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
    };
    const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
        try self.reportError(5005, .@"comptime", start, "Unknown aggregate field");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
    };
    try self.const_values.put(node_idx, field.offset);
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.comptime_int_type));
}

fn analyzeField(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const aggregate_type = try self.analyzeNode(arguments[0], scope);
    const field_name = self.stringLiteralContent(arguments[1]) orelse {
        try self.reportError(5001, .@"comptime", start, "@field field name must be a comptime string");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
    };
    const field = self.type_pool.aggregateField(aggregate_type, field_name) orelse {
        try self.reportError(5005, .@"comptime", start, "Unknown aggregate field");
        return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
    };
    try self.dynamic_fields.put(node_idx, .{ .base_node = arguments[0], .name = field_name });
    return self.putBuiltinResult(node_idx, field.type_id);
}

fn analyzeTagOf(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    const value_type = try self.analyzeNode(arguments[0], scope);
    const info = self.type_pool.aggregateInfo(value_type) orelse return tagFailure(self, node_idx, start);
    const backing = info.backing_type orelse return tagFailure(self, node_idx, start);
    return self.putBuiltinResult(node_idx, backing);
}

fn tagFailure(self: anytype, node_idx: Node.Index, start: u32) !Type.Id {
    try self.reportError(5005, .@"comptime", start, "@tagOf requires an enum or tagged union value");
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
}

fn requireAggregateType(self: anytype, node_idx: Node.Index, argument: u32, scope: *Scope, start: u32, message: []const u8, fallback: Type.Primitive) !?Type.Id {
    if (try self.resolveBuiltinTypeArg(argument, scope)) |resolved| return resolved;
    try self.reportError(5005, .@"comptime", start, message);
    _ = try self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(fallback));
    return null;
}

fn unsupportedReflection(self: anytype, node_idx: Node.Index, start: u32) !Type.Id {
    try self.reportError(5005, .@"comptime", start, "Reflection builtin requires aggregate/module metadata not implemented in Stage 0 yet");
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.anyopaque_type));
}

fn unsupportedLowering(self: anytype, node_idx: Node.Index, arguments: []const u32, scope: *Scope, start: u32) !Type.Id {
    for (arguments) |argument| _ = try self.analyzeNode(argument, scope);
    try self.reportError(9001, .lowering, start, "Builtin lowering is not available in the Stage-0 backend yet");
    return self.putBuiltinResult(node_idx, try self.type_pool.internPrimitive(.void_type));
}
