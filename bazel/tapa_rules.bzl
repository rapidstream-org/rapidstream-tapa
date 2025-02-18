"""Custom rule to add TAPA target to the target list."""

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Define the implementation function for the custom TAPA target rule.
def _tapa_xo_impl(ctx):
    # Retrieve the inputs and attributes from the rule invocation.
    tapa_cli = ctx.executable.tapa_cli
    src = ctx.file.src
    top_name = ctx.attr.top_name
    platform_name = ctx.attr.platform_name
    output_file = ctx.actions.declare_file(
        ctx.attr.output_file or "{}.{}.hw.xo".format(top_name, platform_name),
    )

    # Start building the command to run tapa-cli analyze.
    tapa_cmd = [tapa_cli.path, "analyze", "-f", src.path, "--top", top_name]

    # Add tapacc and tapa-clang executables.
    if ctx.file.tapacc:
        tapa_cmd.extend(["--tapacc", ctx.file.tapacc])
    if ctx.file.tapa_clang:
        tapa_cmd.extend(["--tapa-clang", ctx.file.tapa_clang])

    # Add optional flags, if specified.
    if ctx.attr.cflags:
        tapa_cmd.extend(["--cflags", ctx.attr.cflags])

    # Add include directory, if specified.
    if ctx.files.include:
        for include in ctx.files.include:
            tapa_cmd.extend(["--cflags", "-I" + include.path])

    # Build the command for tapa-cli synth.
    tapa_cmd.extend(["synth", "--platform", platform_name])

    # Limit the number of jobs to reduce excessive amount of processes.
    # Launch 2 workers to avoid wasting time on I/O.
    tapa_cmd.extend(["--jobs", "2"])

    # Add optional parameters to synth command.
    if ctx.attr.clock_period:
        tapa_cmd.extend(["--clock-period", ctx.attr.clock_period])
    if ctx.attr.part_num:
        tapa_cmd.extend(["--part-num", ctx.attr.part_num])
    if ctx.attr.enable_synth_util:
        tapa_cmd.extend(["--enable-synth-util"])

    # Build the command for tapa-cli link.
    tapa_cmd.extend(["link"])

    # Complete the command sequence with the pack command.
    tapa_cmd.extend(["pack", "--output", output_file.path])

    # Add custom rtl
    for rtl_file in ctx.files.custom_rtl_files:
        tapa_cmd.extend(["--custom-rtl", rtl_file.path])

    # Define a custom action to run the synthesized command.
    ctx.actions.run(
        outputs = [output_file],
        inputs = [src] + ctx.files.hdrs + ctx.files.custom_rtl_files,
        tools = [tapa_cli, ctx.executable.vitis_hls_env],
        executable = ctx.executable.vitis_hls_env,
        arguments = tapa_cmd,
    )

    # Return default information, including the output file.
    return [DefaultInfo(files = depset([output_file]))]

# Define the custom Bazel rule.
tapa_xo = rule(
    implementation = _tapa_xo_impl,
    # Define the attributes (inputs) for the rule.
    attrs = {
        "src": attr.label(allow_single_file = True, mandatory = True),
        "hdrs": attr.label_list(allow_files = True),
        "include": attr.label_list(allow_files = True),
        "top_name": attr.string(mandatory = True),
        "custom_rtl_files": attr.label_list(allow_files = True),
        "platform_name": attr.string(mandatory = True),
        "output_file": attr.string(),
        "tapa_cli": attr.label(
            cfg = "exec",
            default = Label("//tapa"),
            executable = True,
        ),
        "tapacc": attr.label(allow_single_file = True),
        "tapa_clang": attr.label(allow_single_file = True),
        "cflags": attr.string(),
        "clock_period": attr.string(),
        "part_num": attr.string(),
        "enable_synth_util": attr.bool(),
        "vitis_hls_env": attr.label(
            cfg = "exec",
            default = Label("//bazel:vitis_hls_env"),
            executable = True,
        ),
    },
)
