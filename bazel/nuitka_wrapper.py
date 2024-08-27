"""Wrapper script for Nuitka."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
import runpy

if "BUILD_WORKING_DIRECTORY" in os.environ:
    os.chdir(os.environ["BUILD_WORKING_DIRECTORY"])

for key in ("CC", "CXX", "LINK"):
    if key in os.environ:
        os.environ[key] = os.path.abspath(os.environ[key])

os.environ["NUITKA_CACHE_DIR"] = "/tmp/.nuitka_cache"

if __name__ == "__main__":
    runpy.run_module("nuitka", run_name="__main__")
