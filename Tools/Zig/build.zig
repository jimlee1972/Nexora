const std = @import("std");

pub fn build(builder: *std.Build) void {
    const script = builder.option([]const u8, "script", "Absolute game script path") orelse @panic("Pass -Dscript=<absolute path>");
    const name = builder.option([]const u8, "name", "Game module name") orelse @panic("Pass -Dname=<module name>");
    const target = builder.standardTargetOptions(.{});
    const optimize = builder.standardOptimizeOption(.{});
    const nexora = builder.createModule(.{
        .root_source_file = builder.path("../../Bindings/Zig/nexora.zig"),
        .target = target,
        .optimize = optimize,
    });
    const game = builder.createModule(.{
        .root_source_file = .{ .cwd_relative = script },
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "nexora", .module = nexora }},
    });
    const root = builder.createModule(.{
        .root_source_file = builder.path("game_entry.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{ .{ .name = "nexora", .module = nexora }, .{ .name = "game", .module = game } },
    });
    const library = builder.addLibrary(.{ .name = name, .linkage = .dynamic, .root_module = root });
    library.bundle_compiler_rt = true;
    builder.installArtifact(library);
    const tests = builder.addTest(.{ .root_module = root });
    const script_tests = builder.addTest(.{ .root_module = game });
    const test_step = builder.step("test", "Run SDK adapter and game script tests");
    test_step.dependOn(&builder.addRunArtifact(tests).step);
    test_step.dependOn(&builder.addRunArtifact(script_tests).step);
}
