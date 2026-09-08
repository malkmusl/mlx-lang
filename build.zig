const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "mlx0",
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    b.installArtifact(exe);

    const lsp = b.addExecutable(.{
        .name = "mlx-lsp",
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/lsp.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(lsp);

    const lsp_run = b.addRunArtifact(lsp);
    const lsp_step = b.step("lsp", "Run the Mlx language server");
    lsp_step.dependOn(&lsp_run.step);

    const run_cmd = b.addRunArtifact(exe);
    if (b.args) |args| run_cmd.addArgs(args);
    const run_step = b.step("mlx0", "Run the stage-0 bootstrap compiler scaffold");
    run_step.dependOn(&run_cmd.step);

    const build_mlx1 = b.addRunArtifact(exe);
    // mlx0 resolves the self-hosted compiler's imports itself, so Zig cannot
    // infer those transitive inputs from main.mlx. Always rerun this cheap
    // bootstrap step to avoid installing a stale mlx1 from the build cache.
    build_mlx1.has_side_effects = true;
    build_mlx1.addFileArg(b.path("compiler/selfhost/main.mlx"));
    const mlx1_output = build_mlx1.addPrefixedOutputFileArg("-o", "mlx1");
    const install_mlx1 = b.addInstallBinFile(mlx1_output, "mlx1");
    const mlx1_step = b.step("mlx1", "Build the canonical compiler scaffold with mlx0");
    mlx1_step.dependOn(&install_mlx1.step);

    const test_exe = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    const run_test = b.addRunArtifact(test_exe);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_test.step);

    const lsp_test_exe = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/lsp.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    const run_lsp_test = b.addRunArtifact(lsp_test_exe);
    test_step.dependOn(&run_lsp_test.step);
}
