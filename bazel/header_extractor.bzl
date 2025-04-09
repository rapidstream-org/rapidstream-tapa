"""Custom rule to extract headers from dependencies."""

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

# Define the implementation function for the custom header extractor rule.
def _header_extractor_impl(ctx):
    # Collect headers from CcInfo dependencies
    headers = depset(transitive = [
        dep[CcInfo].compilation_context.headers
        for dep in ctx.attr.deps
        if CcInfo in dep
    ])

    # Filter out non-virtual includes from CcInfo headers
    headers_list = [h for h in headers.to_list() if "_virtual_includes" in h.path]

    # Add headers from the data dependencies
    for dep in ctx.attr.deps:
        if CcInfo not in dep:
            headers_list.extend(dep.files.to_list())

    headers = depset(headers_list)

    # Copy headers to output directory
    output_files = []
    for header in headers.to_list():
        if "_virtual_includes/" in header.path:
            # Extract header name from virtual_includes
            header_name = header.path.split("_virtual_includes/")[-1]

            # Removing the first slash which is the name of the package
            header_name = header_name.split("/", 1)[-1]

        elif "include/" in header.path:
            # Extract header name from include folder
            header_name = header.path.split("include/")[-1]

        else:
            # Use the header path as is
            header_name = header.path

        # Copy header to output directory
        output_file = ctx.actions.declare_file(ctx.label.name + "/" + header_name)
        ctx.actions.run_shell(
            outputs = [output_file],
            inputs = [header],
            command = "cp '{}' '{}'".format(header.path, output_file.path),
        )

        # Add output file to list
        output_files.append(output_file)

    # Return output files
    return [DefaultInfo(files = depset(output_files))]

# Define the custom Bazel rule to extract headers from dependencies
header_extractor = rule(
    implementation = _header_extractor_impl,
    attrs = {
        "deps": attr.label_list(
            default = [],
        ),
    },
)
