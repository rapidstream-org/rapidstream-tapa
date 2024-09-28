"""Invoke `g++` with TAPA include and library paths."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import shlex
import subprocess
from collections.abc import Iterable
from pathlib import Path

import click

import tapa.common.paths
import tapa.util

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
    installation_dir = tapa.util.get_installation_path().absolute()
    vendor_include_paths = []
    xilinx_hls_path = tapa.util.get_xilinx_hls_path()
    if xilinx_hls_path is not None:
        vendor_include_paths.append(Path(xilinx_hls_path) / "include")
    args = [
        executable,
        "-std=c++17",
        f"-isystem{installation_dir}/usr/include",
        *(f"-isystem{p}" for p in vendor_include_paths),
        *argv,
        f"-Wl,-rpath,{installation_dir}/usr/lib",
        f"-L{installation_dir}/usr/lib",
        "-ltapa",
        "-lcontext",
        "-lthread",
        "-lfrt",
        "-lglog",
        "-lgflags",
        "-l:libOpenCL.so.1",
        "-ltinyxml2",
        "-lstdc++fs",
    ]
    _logger.info("running g++ command: %s", shlex.join(args))
    subprocess.run(args, check=False)
