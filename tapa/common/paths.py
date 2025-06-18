"""Utility to lookup distribution paths for TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import os.path
from collections.abc import Iterable
from functools import cache
from pathlib import Path
from typing import Literal

_logger = logging.getLogger().getChild(__name__)

# The potential paths for the distribution paths, in order of preference.
# TAPA will attempt to find the executable by iteratively visiting each
# parent directory of the current source file, appending each potential
# path to the parent directory, and checking if the file exists. The first
# match will be used, and the nearest parent directory will be used to
# resolve relative paths.
POTENTIAL_PATHS: dict[str, tuple[str, ...]] = {
    "fpga-runtime-include": (
        "fpga-runtime",
        "usr/include",
    ),
    "fpga-runtime-lib": (
        "fpga-runtime",
        "usr/lib",
    ),
    "tapa-cpp-binary": (
        "tapa-cpp/tapa-cpp",
        "usr/bin/tapa-cpp",
    ),
    "tapa-extra-runtime-include": (
        "tapa-system-include/tapa-extra-runtime-include",
        "usr/include",
    ),
    "tapa-fast-cosim-dpi-lib": (
        "fpga-runtime",
        "usr/lib",
    ),
    "tapa-lib-include": (
        "tapa-lib",
        "usr/include",
    ),
    "tapa-lib-lib": (
        "tapa-lib",
        "usr/lib",
    ),
    "tapa-system-include": (
        "tapa-system-include/tapa-system-include",
        "usr/share/tapa/system-include",
    ),
    "tapacc-binary": (
        "tapacc/tapacc",
        "usr/bin/tapacc",
    ),
}


@cache
def find_resource(file: str) -> Path:
    """Find the resource path in the potential paths.

    Args:
        file: The file to find.

    Returns:
        The path to the executable if found, otherwise None.
    """
    assert file in POTENTIAL_PATHS, f"Unknown file: {file}"

    for path in POTENTIAL_PATHS[file]:
        for parent in Path(__file__).absolute().parents:
            potential_path = parent / path
            if potential_path.exists():
                return potential_path

    error = f"Unable to find {file} in the potential paths"
    raise FileNotFoundError(error)


@cache
def find_external_lib_in_runfiles() -> set[Path]:
    """Find the external libraries in the runfiles.

    Returns:
        The set of external libraries' directories in the Bazel runfiles.
        If the execution is not in a Bazel runfiles, an empty set is returned.
    """
    for parent in Path(__file__).absolute().parents:
        potential_path = parent / "tapa.runfiles"

        # if the execution is in a Bazel runfiles
        if potential_path.exists():
            return {
                potential_path / "gflags+",
                potential_path / "glog+",
                potential_path / "tinyxml2+",
                potential_path / "yaml-cpp+",
                potential_path / "rules_boost++non_module_dependencies+boost",
            }

    return set()


def get_xilinx_tool_path(tool_name: Literal["HLS", "VITIS"] = "HLS") -> str | None:
    """Returns the XILINX_<TOOL> path."""
    xilinx_tool_path = os.environ.get(f"XILINX_{tool_name}")
    if xilinx_tool_path is None:
        _logger.critical("not adding vendor include paths;")
        _logger.critical("please set XILINX_%s", tool_name)
        _logger.critical("you may run `source /path/to/Vitis/settings64.sh`")
    elif not Path(xilinx_tool_path).exists():
        _logger.critical(
            "XILINX_%s path does not exist: %s", tool_name, xilinx_tool_path
        )
        xilinx_tool_path = None
    return xilinx_tool_path


def get_xpfm_path(platform: str) -> str | None:
    """Returns the XPFM path for a platform."""
    xilinx_vitis_path = get_xilinx_tool_path("VITIS")
    path_in_vitis = f"{xilinx_vitis_path}/base_platforms/{platform}/{platform}.xpfm"
    path_in_opt = f"/opt/xilinx/platforms/{platform}/{platform}.xpfm"
    if os.path.exists(path_in_vitis):
        return path_in_vitis
    if os.path.exists(path_in_opt):
        return path_in_opt

    _logger.critical("Cannot find XPFM for platform %s", platform)
    return None


@cache
def get_vendor_include_paths() -> Iterable[str]:
    """Yields include paths that are automatically available in vendor tools."""
    xilinx_hls: str | None = None
    for tool_name in "HLS", "VITIS":
        # 2024.2 moved the HLS include path from Vitis_HLS to Vitis
        xilinx_hls = get_xilinx_tool_path(tool_name)
        if xilinx_hls is not None:
            include = os.path.join(xilinx_hls, "include")
            if os.path.exists(include):
                yield include
                break

    if xilinx_hls is not None:
        # there are multiple versions of gcc, such as 6.2.0, 9.3.0, 11.4.0,
        # we choose the latest version based on numerical order
        tps_lnx64 = Path(xilinx_hls) / "tps" / "lnx64"
        gcc_paths = tps_lnx64.glob("gcc-*.*.*")
        gcc_versions = [path.name.split("-")[1] for path in gcc_paths]
        if not gcc_versions:
            _logger.critical("cannot find HLS vendor GCC")
            _logger.critical("it should be at %s", tps_lnx64)
            return
        gcc_versions.sort(key=lambda x: tuple(map(int, x.split("."))))
        latest_gcc = gcc_versions[-1]

        # include VITIS_HLS/tps/lnx64/g++-<version>/include/c++/<version>
        cpp_include = tps_lnx64 / f"gcc-{latest_gcc}" / "include" / "c++" / latest_gcc
        if not cpp_include.exists():
            _logger.critical("cannot find HLS vendor paths for C++")
            _logger.critical("it should be at %s", cpp_include)
            return
        yield str(cpp_include)

        # there might be a x86_64-pc-linux-gnu or x86_64-linux-gnu
        if (cpp_include / "x86_64-pc-linux-gnu").exists():
            yield os.path.join(cpp_include, "x86_64-pc-linux-gnu")
        elif (cpp_include / "x86_64-linux-gnu").exists():
            yield os.path.join(cpp_include, "x86_64-linux-gnu")
        else:
            _logger.critical("cannot find HLS vendor paths for C++ (x86_64)")
            _logger.critical("it should be at %s", cpp_include)
            return


@cache
def get_tapa_cflags() -> tuple[str, ...]:
    """Return the CFLAGS for compiling TAPA programs.

    The CFLAGS include the TAPA include and system include paths when applicable.
    """
    tapa_lib_include = find_resource("tapa-lib-include")
    includes = {
        find_resource("fpga-runtime-include"),
        find_resource("tapa-extra-runtime-include"),
    }

    # WORKAROUND: tapa-lib-include must be included first to make Vitis happy
    include_flags = ["-isystem" + str(tapa_lib_include)]
    for include in includes - {tapa_lib_include}:
        include_flags.extend(["-isystem" + str(include)])

    return (
        *include_flags,
        # Suppress warnings that does not recognize TAPA attributes
        "-Wno-attributes",
        # Suppress warnings that does not recognize HLS pragmas
        "-Wno-unknown-pragmas",
        # Suppress warnings that does not recognize HLS labels
        "-Wno-unused-label",
    )


@cache
def get_tapa_ldflags() -> tuple[str, ...]:
    """Return the LDFLAGS for linking TAPA programs.

    The LDFLAGS include the TAPA library path when applicable, and adds the -l flags for
    the TAPA libraries.
    """
    libraries = {
        find_resource("fpga-runtime-lib"),
        find_resource("tapa-lib-lib"),
    } | find_external_lib_in_runfiles()
    rpath_flags = [f"-Wl,-rpath,{library}" for library in libraries]
    lib_flags = [f"-L{library}" for library in libraries]

    return (
        *rpath_flags,
        *lib_flags,
        "-ltapa",
        "-lcontext",
        "-lthread",
        "-lfrt",
        "-lglog",
        "-lgflags",
        "-lOpenCL",
        "-ltinyxml2",
        "-lyaml-cpp",
        "-lstdc++fs",
    )


@cache
def get_tapacc_cflags() -> tuple[str, ...]:
    """Return CFLAGS with vendor libraries for HLS.

    This CFLAGS include the tapa and HLS vendor libraries.
    """
    # Add vendor include files to tapacc cflags
    vendor_include_paths = ()
    for vendor_path in get_vendor_include_paths():
        vendor_include_paths += ("-isystem" + vendor_path,)
        _logger.info("added vendor include path `%s`", vendor_path)

    # TODO: Vitis HLS highly depends on the assert to be defined in
    #       the system include path in a certain way. One attempt was
    #       to disable NDEBUG but this causes hls::assume to fail.
    #       A full solution is to provide tapa::assume that guarantees
    #       the produce the pattern that HLS likes.
    #       For now, newer versions of glibc will not be supported.
    return (
        # Use the stdc++ library from the HLS toolchain.
        "-nostdinc++",
        *get_tapa_cflags(),
        *vendor_include_paths,
    )


@cache
def get_system_cflags() -> tuple[str, ...]:
    """Return CFLAGS for system libraries, such as clang and libc++."""
    if system_include_path := find_resource("tapa-system-include"):
        return ("-isystem" + str(system_include_path),)
    return ()
