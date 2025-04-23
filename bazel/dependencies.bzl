"""Load the local vendor dependencies for the TAPA project based on VARS.bzl"""

load("@bazel_tools//tools/build_defs/repo:local.bzl", "new_local_repository")
load("//:VARS.bzl", "XILINX_TOOL_LEGACY_VERSION", "XILINX_TOOL_PATH", "XILINX_TOOL_VERSION")

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

def _load_dependencies(module_ctx):
    """Load dependencies for the TAPA project."""

    # Load the Xilinx Vitis HLS library
    vitis_hls_path = XILINX_TOOL_PATH + (
        "/Vitis/" if XILINX_TOOL_VERSION >= "2024.2" else "/Vitis_HLS/"
    ) + XILINX_TOOL_VERSION

    new_local_repository(
        name = "vitis_hls",
        build_file_content = """
cc_library(
    name = "include",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
        """,
        path = vitis_hls_path,
    )

    # Starting from 2024.2, Vivado has renamed rdi to xv
    vivado_path = XILINX_TOOL_PATH + "/Vivado/"
    xsim_path = vivado_path + XILINX_TOOL_VERSION + "/data/xsim"
    new_local_repository(
        name = "xsim_xv",
        build_file_content = """
cc_library(
    name = "svdpi",
    hdrs = glob(["include/svdpi.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
        """,
        path = xsim_path,
    )

    # Use the oldest supported version to ensure compatibility
    xsim_legacy_path = vivado_path + XILINX_TOOL_LEGACY_VERSION + "/data/xsim"
    new_local_repository(
        name = "xsim_legacy_rdi",
        build_file_content = """
cc_library(
    name = "svdpi",
    hdrs = glob(["include/svdpi.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)
    """,
        # Use the oldest supported version to ensure compatibility
        path = xsim_legacy_path,
    )

    return module_ctx.extension_metadata(
        root_module_direct_deps = [],
        root_module_direct_dev_deps = "all",
        reproducible = False,
    )

load_dependencies = module_extension(
    implementation = _load_dependencies,
)
