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
    cc_bin_dir = "external/toolchains_llvm~~llvm~llvm_toolchain_llvm/bin"
    patchelf_dir = ctx.executable._patchelf.dirname

    # Get the Python toolchain information.
    py_toolchain = ctx.toolchains["@rules_python//python:toolchain_type"].py3_runtime
    py_interpreter = py_toolchain.interpreter.path
    py_bin_dir = py_toolchain.interpreter.dirname
    py_lib_dir = py_toolchain.interpreter.dirname.rsplit("/", 1)[0] + "/lib"

    # Get the Nuitka environment executable.
    nuitka = ctx.executable.nuitka_environment

    # Create a directory exposing POSIX tools required by Nuitka.
    posix_tools_dir = ctx.actions.declare_directory("external/posix_tools")
    ctx.actions.run_shell(
        outputs = [posix_tools_dir],
        command = "ln -s /usr/bin/sh /usr/bin/ld /usr/bin/ldd /usr/bin/readelf " + posix_tools_dir.path,
    )

    # Start building the command to run Nuitka.
    nuitka_cmd = [
        nuitka.path,
        src.path,
        "--clang",  # Use clang as the compiler. In this case, Nuitka generates .c files.
        "--disable-cache=all",  # Disable ccache as the Bazel hermerticity does not allow it.
        "--output-dir={}".format(output_dir.dirname),
        "--output-filename={}".format(ctx.attr.output_name or ctx.attr.name),
        "--show-scons",
        "--standalone",
    ]

    # Add optional flags, if specified.
    if flags:
        nuitka_cmd.extend(flags)

    # Add action tools to the env
    env = {
        "PATH": ctx.configuration.host_path_separator.join([
            py_bin_dir,
            cc_bin_dir,
            patchelf_dir,
            posix_tools_dir.path,
        ]),
        "LIBRARY_PATH": py_lib_dir,
        "LD_LIBRARY_PATH": py_lib_dir,
        # WORKAROUND: cc_toolchain.compiler_executable does not work as the generated
        # wrapper script attempts to unwrap @"file" into command line arguments with
        # incorrect quoting. Instead, we use the path to the compiler executable directly
        # here. Until Bazel is fixed, this is the only way to make the compiler work.
        "CC": cc_bin_dir + "/clang",
    }

    # Define a custom action to run the Nuitka command.
    ctx.actions.run(
        outputs = [output_dir],
        inputs = [src] + cc_toolchain.all_files.to_list() + py_toolchain.files.to_list(),
        tools = [nuitka, py_toolchain.interpreter, ctx.executable._patchelf, posix_tools_dir],
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
        "_patchelf": attr.label(
            doc = "The patchelf executable.",
            default = Label("@patchelf"),
            executable = True,
            cfg = "exec",
        ),
    },
    toolchains = use_cpp_toolchain() + [
        "@@rules_python~//python:toolchain_type",
    ],
    fragments = ["cpp"],
)
