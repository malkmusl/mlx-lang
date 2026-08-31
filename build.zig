const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "zin0",
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    b.installArtifact(exe);

    const lsp = b.addExecutable(.{
        .name = "zin-lsp",
        .root_module = b.createModule(.{
            .root_source_file = b.path("compiler/bootstrap/lsp.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    b.installArtifact(lsp);

    const lsp_run = b.addRunArtifact(lsp);
    const lsp_step = b.step("lsp", "Run the Zin language server");
    lsp_step.dependOn(&lsp_run.step);

    const run_cmd = b.addRunArtifact(exe);
    if (b.args) |args| run_cmd.addArgs(args);
    const run_step = b.step("zin0", "Run the stage-0 bootstrap compiler scaffold");
    run_step.dependOn(&run_cmd.step);

    const build_zin1 = b.addRunArtifact(exe);
    build_zin1.addFileArg(b.path("compiler/selfhost/main.zin"));
    const zin1_output = build_zin1.addPrefixedOutputFileArg("-o", "zin1");
    const install_zin1 = b.addInstallBinFile(zin1_output, "zin1");
    const zin1_step = b.step("zin1", "Build the canonical compiler scaffold with zin0");
    zin1_step.dependOn(&install_zin1.step);

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
