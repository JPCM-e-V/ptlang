const std = @import("std");

const c_flags = [_][]const u8{ "-lasan", "-fsanitize=address" };

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

    const debug_bison = b.option(bool, "debug-bison", "Enable bison debugging") orelse false;
    const debug_flex = b.option(bool, "debug-flex", "Enable flex debugging") orelse false;

    // ast
    const ast = b.addObject(.{
        .name = "ptlang_ast",
        .target = target,
        .optimize = optimize,
    });

    ast.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_ast/ptlang_ast.c" },
        .flags = &c_flags,
    });

    ast.addIncludePath(.{ .path = "src/ptlang_utils" });
    ast.addIncludePath(.{ .path = "third-party/stb" });
    ast.linkLibC();

    // b.installArtifact(ast);

    // eval
    const eval = b.addObject(.{
        .name = "ptlang_eval",
        .target = target,
        .optimize = optimize,
    });

    eval.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_eval/ptlang_eval.c" },
        .flags = &c_flags,
    });

    eval.addIncludePath(.{ .path = "src/ptlang_ast" });
    eval.addIncludePath(.{ .path = "src/ptlang_ir_builder" });
    eval.addIncludePath(.{ .path = "src/ptlang_utils" });
    eval.linkLibC();

    // b.installArtifact(eval);

    // ir builder
    const ir_builder = b.addObject(.{
        .name = "ptlang_ir_builder",
        .target = target,
        .optimize = optimize,
    });

    ir_builder.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_ir_builder/ptlang_ir_builder.c" },
        .flags = &c_flags,
    });

    ir_builder.addIncludePath(.{ .path = "src/ptlang_ast" });
    ir_builder.addIncludePath(.{ .path = "src/ptlang_utils" });
    ir_builder.addIncludePath(.{ .path = "third-party/stb" });
    ir_builder.linkLibC();

    // b.installArtifact(ir_builder);

    // utils
    const utils = b.addObject(.{
        .name = "ptlang_utils",
        .target = target,
        .optimize = optimize,
    });

    utils.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_utils/ptlang_utils.c" },
        .flags = &c_flags,
    });

    utils.addIncludePath(.{ .path = "third-party/stb" });
    utils.linkLibC();

    // b.installArtifact(utils);

    // verify
    const verify = b.addObject(.{
        .name = "ptlang_verify",
        .target = target,
        .optimize = optimize,
    });

    verify.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_verify/ptlang_verify.c" },
        .flags = &c_flags,
    });

    verify.addIncludePath(.{ .path = "src/ptlang_ast" });
    verify.addIncludePath(.{ .path = "src/ptlang_eval" });
    verify.addIncludePath(.{ .path = "src/ptlang_utils" });
    verify.addIncludePath(.{ .path = "src/ptlang_main" });
    verify.addIncludePath(.{ .path = "third-party/stb" });
    verify.linkLibC();

    // b.installArtifact(verify);

    // parser
    const bison = b.addSystemCommand(&.{"bison"});
    const parser_h = bison.addPrefixedOutputFileArg("--header=", "parser.h");
    const parser_c = bison.addPrefixedOutputFileArg("--output=", "parser.c");
    bison.addFileArg(.{ .path = "src/ptlang_parser/ptlang.y" });

    if (debug_bison) {
        bison.addArg("--debug");
    }

    const flex = b.addSystemCommand(&.{"flex"});
    const lexer_h = flex.addPrefixedOutputFileArg("--header-file=", "lexer.h");
    const lexer_c = flex.addPrefixedOutputFileArg("--outfile=", "lexer.c");
    if (debug_flex) {
        flex.addArg("--debug");
    }
    flex.addFileArg(.{ .path = "src/ptlang_parser/ptlang.l" });

    const parser = b.addObject(.{
        .name = "ptlang_parser",
        .target = target,
        .optimize = optimize,
    });

    parser.defineCMacro("PTLANG_DEBUG_BISON", if (debug_bison) "1" else "0");
    parser.defineCMacro("PTLANG_DEBUG_FLEX", if (debug_flex) "1" else "0");

    parser.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_parser/ptlang_parser.c" },
        .flags = &c_flags,
    });

    parser.addCSourceFile(.{
        .file = parser_c,
        .flags = &c_flags,
    });

    parser.addCSourceFile(.{
        .file = lexer_c,
        .flags = &c_flags,
    });

    const include_dir = b.addWriteFiles();
    _ = include_dir.addCopyFile(parser_h, "parser.h");
    _ = include_dir.addCopyFile(lexer_h, "lexer.h");

    parser.addIncludePath(include_dir.getDirectory());
    parser.addIncludePath(.{ .path = "src/ptlang_parser" });
    parser.addIncludePath(.{ .path = "src/ptlang_ast" });
    parser.addIncludePath(.{ .path = "src/ptlang_utils" });
    parser.addIncludePath(.{ .path = "src/ptlang_main" });
    parser.addIncludePath(.{ .path = "third-party/stb" });
    parser.linkLibC();

    // b.installArtifact(parser);

    const exe = b.addExecutable(.{
        .name = "ptlang",
        // In this case the main source file is merely a path, however, in more
        // complicated build scripts, this could be a generated file.
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_main/main.c" },
        .flags = &c_flags,
    });

    exe.addCSourceFile(.{
        .file = .{ .path = "src/ptlang_main/ptlang_context.c" },
        .flags = &c_flags,
    });

    exe.addIncludePath(.{ .path = "src/ptlang_ast" });
    exe.addIncludePath(.{ .path = "src/ptlang_utils" });
    exe.addIncludePath(.{ .path = "src/ptlang_ir_builder" });
    exe.addIncludePath(.{ .path = "src/ptlang_parser" });
    exe.addIncludePath(.{ .path = "src/ptlang_main" });
    exe.addIncludePath(.{ .path = "src/ptlang_verify" });
    exe.addIncludePath(.{ .path = "third-party/stb" });
    exe.linkLibC();

    exe.addObject(parser);
    exe.addObject(ir_builder);
    exe.addObject(ast);
    exe.addObject(verify);
    exe.addObject(utils);
    exe.addObject(eval);
    exe.linkSystemLibrary2("LLVM", .{});

    // exe.linkLibrary(ast);

    // This declares intent for the executable to be installed into the
    // standard location when the user invokes the "install" step (the default
    // step when running `zig build`).
    b.installArtifact(exe);

    // This *creates* a Run step in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = b.addRunArtifact(exe);

    // By making the run step depend on the install step, it will be run from the
    // installation directory rather than directly from within the cache directory.
    // This is not necessary, however, if the application depends on other installed
    // files, this ensures they will be present and in the expected location.
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build run`
    // This will evaluate the `run` step rather than the default, which is "install".
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
