"""Custom rule to add Nuitka target to the target list."""

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")

# Define the implementation function for the custom Nuitka rule.
def _nuitka_binary_impl(ctx):
    # Retrieve the inputs and attributes from the rule invocation.
    src = ctx.file.src
    flags = ctx.attr.flags
    output_dir = ctx.actions.declare_directory(src.basename.split(".")[0] + ".dist")

    # Get the CC toolchain information.
    cc_toolchain = find_cpp_toolchain(ctx)

    # Get the Python toolchain information.
    py_toolchain = ctx.toolchains["@rules_python//python:toolchain_type"].py3_runtime
    py_interpreter = py_toolchain.interpreter.path

    # Get the Nuitka executable.
    nuitka = ctx.executable.nuitka_environment

    # Start building the command to run Nuitka.
    nuitka_cmd = [
        nuitka.path,
        src.path,
        "--clang",  # Use clang as the compiler. In this case, Nuitka generates .c files.
        "--disable-ccache",  # Disable ccache as the Bazel hermerticity does not allow it.
        "--follow-imports",
        "--output-dir={}".format(output_dir.dirname),
        "--output-filename={}".format(ctx.attr.output_name or ctx.attr.name),
        "--standalone",
    ]

    # Add optional flags, if specified.
    if flags:
        nuitka_cmd.extend(flags)

    # Add action tools to the env
    env = {
        "PATH": ctx.configuration.host_path_separator.join([
            ctx.bin_dir.path,
            "/usr/bin",  # Scons is hard to make hermetic, depending on system posix tools.
        ]),
        # WORKAROUND: cc_toolchain.compiler_executable does not work as the generated
        # wrapper script attempts to unwrap @"file" into command line arguments with
        # incorrect quoting. Instead, we use the path to the compiler executable directly
        # here. Until Bazel is fixed, this is the only way to make the compiler work.
        "CC": "external/toolchains_llvm~~llvm~llvm_toolchain_llvm/bin/clang",
    }

    # Define a custom action to run the Nuitka command.
    ctx.actions.run(
        outputs = [output_dir],
        inputs = [src] + cc_toolchain.all_files.to_list(),
        tools = [nuitka, py_toolchain.interpreter],
        executable = py_interpreter,
        arguments = nuitka_cmd,
        env = env,
    )

    # Create the list of all files generated under the output directory.
    # all_files = ctx.actions.glob([output_file.dirname + src.basename + ".dist/**"])

    return [DefaultInfo(files = depset([output_dir]))]

# Define the custom Bazel rule.
nuitka_binary = rule(
    implementation = _nuitka_binary_impl,
    # Define the attributes (inputs) for the rule.
    attrs = {
        "src": attr.label(
            doc = "The source file to compile.",
            mandatory = True,
            allow_single_file = True,
        ),
        "output_name": attr.string(
            doc = "The name of the output binary.",
            mandatory = True,
        ),
        "flags": attr.string_list(
            doc = "Flags to pass to Nuitka.",
            default = [],
        ),
        "nuitka_environment": attr.label(
            doc = "The Nuitka executable environment.",
            default = Label("//bazel:nuitka"),
            executable = True,
            cfg = "exec",
        ),
    },
    toolchains = use_cpp_toolchain() + [
        "@@rules_python~//python:toolchain_type",
    ],
    fragments = ["cpp"],
)
