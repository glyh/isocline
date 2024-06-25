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

    const module = b.addModule("isocline", .{
        .root_source_file = b.path("wrapper/lib.zig"),
        .imports = &.{},
        .link_libc = true,
    });
    module.addIncludePath(b.path("include"));
    module.addCSourceFile(.{ .file = b.path("src/isocline.c") });

    const wrapper_test = b.addTest(.{
        .root_source_file = b.path("wrapper/lib.zig"),
        .target = target,
        .optimize = optimize,
    });

    const run_wrapper_test = b.addRunArtifact(wrapper_test);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_wrapper_test.step);

    var c_example = b.addExecutable(.{
        .name = "c-example",
        .target = target,
        .optimize = optimize,
    });
    c_example.root_module.addCSourceFile(.{ .file = b.path("test/example.c") });

    var c_example_run = b.addRunArtifact(c_example);

    const c_example_step = b.step("c-example", "Run C example");
    c_example_step.dependOn(&c_example_run.step);

    var c_test_colors = b.addExecutable(.{
        .name = "c-test-colors",
        .target = target,
        .optimize = optimize,
    });
    c_test_colors.root_module.addCSourceFile(.{ .file = b.path("test/test_colors.c") });

    var c_test_colors_run = b.addRunArtifact(c_test_colors);

    const c_test_colors_step = b.step("c-test-colors", "Run C test colors");
    c_test_colors_step.dependOn(&c_test_colors_run.step);

    inline for ([_]*std.Build.Step.Compile{ wrapper_test, c_example, c_test_colors }) |c| {
        c.linkLibC();
        c.addIncludePath(b.path("include"));
        c.root_module.addCSourceFile(.{ .file = b.path("src/isocline.c") });
    }
}
