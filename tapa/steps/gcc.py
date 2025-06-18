"""Invoke `g++` with TAPA include and library paths."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import shlex
import subprocess
import sys
from collections.abc import Iterable
from pathlib import Path

import click

from tapa.common.paths import (
    get_tapa_cflags,
    get_tapa_ldflags,
    get_xilinx_tool_path,
)

_logger = logging.getLogger().getChild(__name__)


@click.command("g++")
@click.option(
    "--executable",
    default="g++",
    type=click.Path(dir_okay=False, executable=True),
    help="Run the specified executable instead of `g++`.",
)
@click.argument("argv", nargs=-1, type=click.UNPROCESSED)
def gcc(executable: str, argv: Iterable[str]) -> None:
    """Invoke `g++` with TAPA include and library paths.

    This is intended only for usage with pre-built binary installation.
    Developers building TAPA from source should compile binaries using `bazel`.
    """
    vendor_include_paths = []
    xilinx_hls_path = get_xilinx_tool_path()
    if xilinx_hls_path is not None:
        vendor_include_paths.append(Path(xilinx_hls_path) / "include")

    args = [
        executable,
        # Host libraries requires C++17 features, unlike the FPGA code.
        "-std=c++17",
        *get_tapa_cflags(),
        *(f"-isystem{p}" for p in vendor_include_paths),
        *argv,
        *get_tapa_ldflags(),
    ]
    _logger.info("running g++ command: %s", shlex.join(args))
    sys.exit(subprocess.run(args, check=False).returncode)
