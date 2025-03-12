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
    work_dir = ctx.actions.declare_directory(ctx.attr.name + ".tapa")

    output_file = ctx.outputs.output_file
    if output_file == None and ctx.attr.vitis_mode:
        output_file = ctx.actions.declare_file(ctx.attr.name + ".xo")

    outputs = [work_dir]

    # Start building the command to run tapa-cli.
    tapa_cmd = [tapa_cli.path, "--work-dir", work_dir.path]

    # Build the command for tapa-cli analyze.
    tapa_cmd.extend(["analyze", "--input", src.path, "--top", top_name])

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

    if ctx.attr.vitis_mode:
        tapa_cmd.extend(["--vitis-mode"])
    else:
        tapa_cmd.extend(["--no-vitis-mode"])

    # Add flatten hierarchy, if specified.
    if ctx.attr.flatten_hierarchy:
        tapa_cmd.extend(["--flatten-hierarchy"])

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
    ab_graph_file = None
    if ctx.attr.gen_ab_graph:
        tapa_cmd.extend(["--gen-ab-graph"])
        ab_graph_file = ctx.actions.declare_file(ctx.attr.name + ".json")

    # Build the command for tapa-cli link.
    tapa_cmd.extend(["link"])

    # Complete the command sequence with the pack command.
    if output_file != None:
        tapa_cmd.extend(["pack", "--output", output_file.path])
        outputs = [output_file] + outputs

    # Add custom rtl
    for rtl_file in ctx.files.custom_rtl_files:
        tapa_cmd.extend(["--custom-rtl", rtl_file.path])

    # Define a custom action to run the synthesized command.
    ctx.actions.run(
        outputs = outputs,
        inputs = [src] + ctx.files.hdrs + ctx.files.custom_rtl_files,
        tools = [tapa_cli, ctx.executable.vitis_hls_env],
        executable = ctx.executable.vitis_hls_env,
        arguments = tapa_cmd,
    )

    # Extract ab_graph
    ab_graph_return = []
    if ab_graph_file:
        ctx.actions.run_shell(
            inputs = [work_dir],
            outputs = [ab_graph_file],
            command = """
            cp {}/ab_graph.json {}
            """.format(work_dir.path, ab_graph_file.path),
        )
        ab_graph_return = [ab_graph_file]

    # Return default information, including the output file.
    return [DefaultInfo(files = depset([output_file or work_dir] + ab_graph_return))]

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
        "output_file": attr.output(),
        "tapa_cli": attr.label(
            cfg = "exec",
            default = Label("//tapa"),
            executable = True,
        ),
        "tapacc": attr.label(allow_single_file = True),
        "tapa_clang": attr.label(allow_single_file = True),
        "cflags": attr.string(),
        "vitis_mode": attr.bool(
            default = True,
            doc = "If true, generate XO as `output_file`. Otherwise, generate RTL in the work dir as `output_file`.",
        ),
        "clock_period": attr.string(),
        "part_num": attr.string(),
        "enable_synth_util": attr.bool(),
        "gen_ab_graph": attr.bool(),
        "flatten_hierarchy": attr.bool(),
        "vitis_hls_env": attr.label(
            cfg = "exec",
            default = Label("//bazel:vitis_hls_env"),
            executable = True,
        ),
    },
)
