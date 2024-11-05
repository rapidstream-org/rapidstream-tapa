"""Utility to lookup distribution paths for TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import sys
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
    "tapa-cpp": (
        "tapa-cpp/tapa-cpp",
        "usr/bin/tapa-cpp",
    ),
    "tapa-extra-runtime-include": (
        "tapa-system-include/tapa-extra-runtime-include",
        "usr/include",
    ),
    "tapa-lib": (
        "tapa-lib",
        "usr/include",
    ),
    "tapa-lib-link": (
        "tapa-lib",
        "usr/lib",
    ),
    "tapa-system-include": (
        "tapa-system-include/tapa-system-include",
        "usr/share/tapa/system-include",
    ),
    "tapacc": (
        "tapacc/tapacc",
        "usr/bin/tapacc",
    ),
}


@cache
def find_resource(file: str) -> Path | None:
    """Find the resource path in the potential paths.

    Args:
        file: The file to find.

    Returns:
        The path to the executable if found, otherwise None.
    """
    assert file in POTENTIAL_PATHS, f"Unknown file: {file}"

    for path in POTENTIAL_PATHS[file]:
        for self in (Path(__file__).resolve(), Path(__file__).absolute()):
            for parent in self.parents:
                potential_path = parent / path
                if potential_path.exists():
                    _logger.info("found %s: %s", file, potential_path)
                    return potential_path

    return None


@cache
def get_tapa_cflags() -> tuple[str, ...]:
    """Return the CFLAGS for compiling TAPA programs.

    The CFLAGS include the TAPA include and system include paths
    when applicable.
    """
    # Add TAPA include files to tapacc cflags
    tapa_include = find_resource("tapa-lib")
    if tapa_include is None:
        _logger.error("unable to find tapa include folder")
        sys.exit(-1)
    tapa_extra_runtime_include = find_resource("tapa-extra-runtime-include")
    if tapa_extra_runtime_include is None:
        _logger.error("unable to find tapa runtime include folder")
        sys.exit(-1)

    return (
        "-isystem",
        str(tapa_include),
        "-isystem",
        str(tapa_extra_runtime_include),
        # Suppress warnings that does not recognize TAPA attributes
        "-Wno-unknown-attributes",
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
    tapa_lib_link = find_resource("tapa-lib-link")
    if tapa_lib_link is None:
        _logger.error("unable to find tapa library folder")
        sys.exit(-1)

    return (
        f"-Wl,-rpath,{tapa_lib_link}",
        f"-L{tapa_lib_link}",
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
