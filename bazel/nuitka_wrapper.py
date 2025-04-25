"""Wrapper script for Nuitka."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
import runpy

import nuitka.PythonVersions

if "BUILD_WORKING_DIRECTORY" in os.environ:
    os.chdir(os.environ["BUILD_WORKING_DIRECTORY"])

# Absolutize paths in environment variables.
for key in ("CC", "CXX", "LINK"):
    if key in os.environ:
        os.environ[key] = os.path.abspath(os.environ[key])

for key in (
    "PATH",
    "LD_LIBRARY_PATH",
    "LIBRARY_PATH",
    "C_INCLUDE_PATH",
    "CPLUS_INCLUDE_PATH",
    "CPATH",
):
    if key in os.environ:
        os.environ[key] = os.pathsep.join(
            os.path.abspath(p) for p in os.environ[key].split(os.pathsep)
        )

# Monkey-patch Nuitka to use -latomic instead of its static library.
ORIGINAL_MODULE_LINKER_LIBS = nuitka.PythonVersions.getModuleLinkerLibs
nuitka.PythonVersions.getModuleLinkerLibs = lambda: [
    lib.replace(":libatomic.a", "atomic") for lib in ORIGINAL_MODULE_LINKER_LIBS()
]

os.environ["NUITKA_CACHE_DIR"] = "/tmp/.nuitka_cache"

if __name__ == "__main__":
    runpy.run_module("nuitka", run_name="__main__")
