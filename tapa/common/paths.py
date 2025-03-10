"""Utility to lookup distribution paths for TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from functools import cache
from pathlib import Path

_logger = logging.getLogger().getChild(__name__)

# The potential paths for the distribution paths, in order of preference.
# TAPA will attempt to find the executable by iteratively visiting each
# parent directory of the current source file, appending each potential
# path to the parent directory, and checking if the file exists. The first
# match will be used, and the nearest parent directory will be used to
# resolve relative paths.
POTENTIAL_PATHS: dict[str, tuple[str, ...]] = {
    "fpga-runtime-include": (
        "fpga-runtime/src",
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
                potential_path / "rules_boost++non_module_dependencies+boost",
            }

    return set()


@cache
def get_tapa_cflags() -> tuple[str, ...]:
    """Return the CFLAGS for compiling TAPA programs.

    The CFLAGS include the TAPA include and system include paths
    when applicable.
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

    The LDFLAGS include the TAPA library path when applicable,
    and adds the -l flags for the TAPA libraries.
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
        "-l:libOpenCL.so.1",
        "-ltinyxml2",
        "-lstdc++fs",
    )
