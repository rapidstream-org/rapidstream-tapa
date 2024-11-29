"""Custom rule to add V++ target to the target list."""

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Define the implementation of vpp target.
def _vpp_xclbin_impl(ctx):
    # Retrieve the inputs and attributes from the rule invocation.
    vpp = ctx.executable.vpp
    xo = ctx.file.xo
    target = ctx.attr.target
    top_name = ctx.attr.top_name
    platform_name = ctx.attr.platform_name
    xclbin = ctx.actions.declare_file(
        ctx.attr.xclbin or "{}.{}.{}.xclbin".format(
            top_name,
            platform_name,
            target,
        ),
    )

    # Start building the command to run v++.
    vpp_cmd = [
        "--link",
        "--output",
        xclbin.path,
        "--kernel",
        top_name,
        "--platform",
        platform_name,
        "--target",
        target,
        "--connectivity.nk",
        "{top_name}:1:{top_name}".format(top_name = top_name),
        xo.path,
    ]

    if target == "hw_emu":
        # Reduce `mt_level` to avoid excessive amount of processes. Run time of
        # `bazel build //tests/apps/vadd:vadd-hw-emu-xclbin` for reference:
        #   mt_level=1: 651s ("off", +67%)
        #   mt_level=2: 483s (+24$)
        #   mt_level=4: 405s (+4%)
        #   mt_level=8: 390s (default)
        vpp_cmd += [
            "--vivado.prop=fileset.sim_1.xsim.compile.xsc.mt_level=2",
            "--vivado.prop=fileset.sim_1.xsim.elaborate.mt_level=2",
        ]

    # Define a custom action to run the synthesized command.
    ctx.actions.run(
        outputs = [xclbin],
        inputs = [xo],
        tools = [vpp],
        executable = vpp,
        arguments = vpp_cmd,
        mnemonic = "VppLink",
    )

    # Return default information, including the output file.
    return [DefaultInfo(files = depset([xclbin]))]

# Define the v++ rule.
vpp_xclbin = rule(
    implementation = _vpp_xclbin_impl,
    attrs = {
        "vpp": attr.label(
            cfg = "exec",
            default = Label("//bazel:v++"),
            executable = True,
            doc = "The v++ executable.",
        ),
        "xo": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "The source xo file to be linked.",
        ),
        "top_name": attr.string(
            mandatory = True,
            doc = "The top function name of the kernel.",
        ),
        "platform_name": attr.string(
            mandatory = True,
            doc = "The platform name for the kernel.",
        ),
        "target": attr.string(
            mandatory = True,
            doc = "The target to be linked (sw_emu, hw_emu, hw).",
            values = ["sw_emu", "hw_emu", "hw"],
        ),
        "xclbin": attr.string(
            mandatory = False,
            doc = "The output xclbin file name for the kernel.",
        ),
    },
)
