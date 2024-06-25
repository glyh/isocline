const std = @import("std");

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard optimization options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall. Here we do not
    // set a preferred release mode, allowing the user to decide how to optimize.
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "isocline",
        // In this case the main source file is merely a path, however, in more
        // complicated build scripts, this could be a generated file.
        .root_source_file = b.path("wrapper/lib.zig"),
        .target = target,
        .optimize = optimize,
    });

    lib.linkLibC();
    lib.root_module.addCSourceFile(.{ .file = b.path("src/isocline.c") });

    // This declares intent for the library to be installed into the standard
    // location when the user invokes the "install" step (the default step when
    // running `zig build`).
    b.installArtifact(lib);

    var c_example = b.addExecutable(.{
        .name = "c-example",
        .target = target,
        .optimize = optimize,
    });
    c_example.root_module.addCSourceFile(.{ .file = b.path("test/example.c") });
    c_example.addIncludePath(b.path("include"));
    c_example.linkLibC();
    c_example.linkLibrary(lib);

    var c_example_run = b.addRunArtifact(c_example);

    const c_example_step = b.step("c-example", "Run C example");
    c_example_step.dependOn(&c_example_run.step);
    c_example_step.dependOn(&lib.step);

    var c_test_colors = b.addExecutable(.{
        .name = "c-test-colors",
        .target = target,
        .optimize = optimize,
    });
    c_test_colors.root_module.addCSourceFile(.{ .file = b.path("test/test_colors.c") });
    c_test_colors.addIncludePath(b.path("include"));
    c_test_colors.linkLibC();
    c_test_colors.linkLibrary(lib);

    var c_test_colors_run = b.addRunArtifact(c_test_colors);

    const c_test_colors_step = b.step("c-test-colors", "Run C test colors");
    c_test_colors_step.dependOn(&c_test_colors_run.step);
    c_test_colors_step.dependOn(&lib.step);
}
