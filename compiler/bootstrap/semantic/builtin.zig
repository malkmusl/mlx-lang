const std = @import("std");

/// Language builtins named by the Zin 1.0 normative specification.
/// `protocol` and `importSchema` intentionally do not appear here: protocol
/// materialization belongs to the standard-library bootstrap, not the compiler.
pub const Kind = enum {
    Vector,
    alignCast,
    alignOf,
    atomicLoad,
    atomicRmw,
    atomicStore,
    bitCast,
    bitSizeOf,
    cmpxchgStrong,
    cmpxchgWeak,
    compileError,
    decl,
    discardError,
    enumFromInt,
    fence,
    field,
    fieldCount,
    fieldType,
    floatCast,
    floatFromInt,
    hasDecl,
    hasField,
    import,
    intCast,
    intFromEnum,
    intFromFloat,
    intFromPtr,
    isArray,
    isCopyable,
    isEnum,
    isErrorUnion,
    isFloat,
    isInteger,
    isOptional,
    isPointer,
    isSlice,
    isStruct,
    isUnion,
    languageVersion,
    move,
    nocopy,
    offsetOf,
    ptrCast,
    ptrFromInt,
    reduce,
    select,
    setEvalBranchQuota,
    shuffle,
    sizeOf,
    splat,
    tagOf,
    typeInfo,
    typeOf,
};

pub const Arity = struct {
    min: u8,
    max: u8,

    pub fn exact(count: u8) Arity {
        return .{ .min = count, .max = count };
    }

    pub fn accepts(self: Arity, count: usize) bool {
        return count >= self.min and count <= self.max;
    }
};

const names = std.StaticStringMap(Kind).initComptime(.{
    .{ "Vector", .Vector },
    .{ "alignCast", .alignCast },
    .{ "alignOf", .alignOf },
    .{ "atomicLoad", .atomicLoad },
    .{ "atomicRmw", .atomicRmw },
    .{ "atomicStore", .atomicStore },
    .{ "bitCast", .bitCast },
    .{ "bitSizeOf", .bitSizeOf },
    .{ "cmpxchgStrong", .cmpxchgStrong },
    .{ "cmpxchgWeak", .cmpxchgWeak },
    .{ "compileError", .compileError },
    .{ "decl", .decl },
    .{ "discardError", .discardError },
    .{ "enumFromInt", .enumFromInt },
    .{ "fence", .fence },
    .{ "field", .field },
    .{ "fieldCount", .fieldCount },
    .{ "fieldType", .fieldType },
    .{ "floatCast", .floatCast },
    .{ "floatFromInt", .floatFromInt },
    .{ "hasDecl", .hasDecl },
    .{ "hasField", .hasField },
    .{ "import", .import },
    .{ "intCast", .intCast },
    .{ "intFromEnum", .intFromEnum },
    .{ "intFromFloat", .intFromFloat },
    .{ "intFromPtr", .intFromPtr },
    .{ "isArray", .isArray },
    .{ "isCopyable", .isCopyable },
    .{ "isEnum", .isEnum },
    .{ "isErrorUnion", .isErrorUnion },
    .{ "isFloat", .isFloat },
    .{ "isInteger", .isInteger },
    .{ "isOptional", .isOptional },
    .{ "isPointer", .isPointer },
    .{ "isSlice", .isSlice },
    .{ "isStruct", .isStruct },
    .{ "isUnion", .isUnion },
    .{ "languageVersion", .languageVersion },
    .{ "move", .move },
    .{ "nocopy", .nocopy },
    .{ "offsetOf", .offsetOf },
    .{ "ptrCast", .ptrCast },
    .{ "ptrFromInt", .ptrFromInt },
    .{ "reduce", .reduce },
    .{ "select", .select },
    .{ "setEvalBranchQuota", .setEvalBranchQuota },
    .{ "shuffle", .shuffle },
    .{ "sizeOf", .sizeOf },
    .{ "splat", .splat },
    .{ "tagOf", .tagOf },
    .{ "typeInfo", .typeInfo },
    .{ "typeOf", .typeOf },
});

pub fn lookup(name: []const u8) ?Kind {
    return names.get(name);
}

/// Arity is specified here only where the normative text fixes the call shape
/// or provides an unambiguous example. Vector operations and atomics are left
/// unconstrained until their source signatures are made normative.
pub fn arity(kind: Kind) ?Arity {
    return switch (kind) {
        .languageVersion => Arity.exact(0),
        .typeOf,
        .typeInfo,
        .sizeOf,
        .alignOf,
        .bitSizeOf,
        .fieldCount,
        .tagOf,
        .isInteger,
        .isFloat,
        .isStruct,
        .isEnum,
        .isUnion,
        .isPointer,
        .isSlice,
        .isArray,
        .isOptional,
        .isErrorUnion,
        .isCopyable,
        .intFromPtr,
        .intFromEnum,
        .compileError,
        .discardError,
        .import,
        .move,
        .nocopy,
        .setEvalBranchQuota,
        => Arity.exact(1),
        .Vector,
        .offsetOf,
        .field,
        .fieldType,
        .hasField,
        .hasDecl,
        .decl,
        .intCast,
        .floatCast,
        .floatFromInt,
        .intFromFloat,
        .ptrCast,
        .alignCast,
        .bitCast,
        .ptrFromInt,
        .enumFromInt,
        => Arity.exact(2),
        .atomicLoad,
        .atomicRmw,
        .atomicStore,
        .cmpxchgStrong,
        .cmpxchgWeak,
        .fence,
        .splat,
        .shuffle,
        .reduce,
        .select,
        => null,
    };
}

test "all normative builtin names resolve" {
    inline for (@typeInfo(Kind).@"enum".fields) |field| {
        try std.testing.expect(lookup(field.name) != null);
    }
    try std.testing.expect(lookup("print") == null);
    try std.testing.expect(lookup("protocol") == null);
    try std.testing.expect(lookup("importSchema") == null);
}
