"""Custom rule to create DPI libraries."""

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

def _dpi_library_impl(ctx):
    compile_options = [
        "-Wall",
        "-Werror",
        "-Wno-sign-compare",
    ]
    transitive_inputs = []
    compile_options += ["-isystem" + x.path for x in ctx.files.includes]
    for dep in ctx.attr.deps:
        context = dep[CcInfo].compilation_context
        compile_options += ["-iquote" + x for x in context.quote_includes.to_list()]
        compile_options += ["-isystem" + x for x in context.system_includes.to_list()]
        compile_options += ["-I" + x for x in context.includes.to_list()]
        compile_options += ["-D" + x for x in context.defines.to_list()]
        transitive_inputs.append(context.headers)

    link_options = [
        # Prefer libraries in the same directory. Escaping '$' due to `xsc` bug.
        "-Wl,-rpath,\\$ORIGIN",
    ]

    # Link against the dynamic libraries in `deps`.
    direct_inputs = ctx.files.hdrs + ctx.files.srcs
    output = ctx.actions.declare_file(ctx.label.name + ".so")
    runfiles = []
    for dep in ctx.attr.deps:
        for linker_input in dep[CcInfo].linking_context.linker_inputs.to_list():
            for library in linker_input.libraries:
                dynamic_library = library.resolved_symlink_dynamic_library
                if dynamic_library == None:
                    dynamic_library = library.dynamic_library
                if dynamic_library == None:
                    continue
                direct_inputs.append(dynamic_library)
                name = dynamic_library.basename
                name = name.removeprefix("lib")
                name = name.removesuffix("." + dynamic_library.extension)
                link_options += [
                    "-L" + dynamic_library.dirname,
                    "-l" + name,
                ]
                library_symlink = ctx.actions.declare_file(
                    dynamic_library.basename,
                    sibling = output,
                )
                ctx.actions.symlink(
                    output = library_symlink,
                    target_file = dynamic_library,
                )
                runfiles.append(library_symlink)

    # Assemble arguments.
    args = [
        "--output=" + output.path,
        "--mt=off",
    ]
    args += ["--gcc_compile_options=" + x for x in compile_options]
    args += ["--gcc_link_options=" + x for x in link_options]
    args += [x.path for x in ctx.files.srcs]

    ctx.actions.run(
        outputs = [output],
        inputs = depset(direct_inputs, transitive = transitive_inputs),
        executable = ctx.executable._xsc,
        tools = [ctx.executable._xsc],
        arguments = args,
        mnemonic = "DpiCompile",
    )
    return [
        DefaultInfo(
            files = depset([output]),
            runfiles = ctx.runfiles(files = runfiles),
        ),
    ]

_dpi_library = rule(
    implementation = _dpi_library_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "hdrs": attr.label_list(allow_files = True),
        "includes": attr.label_list(allow_files = True),
        "deps": attr.label_list(providers = [CcInfo]),
        "_xsc": attr.label(
            cfg = "exec",
            default = Label("//bazel:xsc_xv"),
            executable = True,
        ),
    },
)

_dpi_legacy_rdi_library = rule(
    implementation = _dpi_library_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "hdrs": attr.label_list(allow_files = True),
        "includes": attr.label_list(allow_files = True),
        "deps": attr.label_list(providers = [CcInfo]),
        "_xsc": attr.label(
            cfg = "exec",
            default = Label("//bazel:xsc_legacy_rdi"),
            executable = True,
        ),
    },
)

def dpi_library(name, **kwargs):
    _dpi_library(name = name, **kwargs)
    native.cc_library(name = name + "_cc", **kwargs)

def dpi_legacy_rdi_library(name, **kwargs):
    _dpi_legacy_rdi_library(name = name, **kwargs)
    native.cc_library(name = name + "_cc", **kwargs)
