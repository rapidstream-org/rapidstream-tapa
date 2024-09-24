"""Utility functions for TAPA."""

from __future__ import annotations

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import os.path
import shutil
import subprocess
import time
from typing import TYPE_CHECKING

import absl.logging
import coloredlogs

if TYPE_CHECKING:
    from collections.abc import Iterable

_logger = logging.getLogger().getChild(__name__)


def clang_format(code: str, *args: str) -> str:
    """Apply clang-format with given arguments, if possible."""
    for version in range(10, 4, -1):
        clang_format_exe = shutil.which("clang-format-%d" % version)
        if clang_format_exe is not None:
            break
    else:
        clang_format_exe = shutil.which("clang-format")
    if clang_format_exe is not None:
        proc = subprocess.run(
            [clang_format_exe, *args],
            input=code,
            stdout=subprocess.PIPE,
            check=True,
            text=True,
        )
        proc.check_returncode()
        return proc.stdout
    return code


def get_indexed_name(name: str, idx: int | None) -> str:
    """Return `name` if `idx` is `None`, `f'{name}_{idx}'` otherwise."""
    if idx is None:
        return name
    return f"{name}_{idx}"


def range_or_none(count: int | None) -> tuple[None] | Iterable[int]:
    if count is None:
        return (None,)
    return range(count)


def get_addr_width(chan_size: int | None, data_width: int) -> int:
    if chan_size is None:
        return 64
    chan_size_in_bytes = chan_size * data_width // 8
    addr_width = (chan_size_in_bytes - 1).bit_length()
    assert 2**addr_width == chan_size * data_width // 8
    return addr_width


def get_instance_name(item: tuple[str, int]) -> str:
    return "_".join(map(str, item))


def get_module_name(module: str) -> str:
    return f"{module}"


def get_vendor_include_paths() -> Iterable[str]:
    """Yields include paths that are automatically available in vendor tools."""
    try:
        for line in subprocess.check_output(
            ["frt_get_xlnx_env"],
            universal_newlines=True,
        ).split("\0"):
            if not line:
                continue
            key, value = line.split("=", maxsplit=1)
            if key == "XILINX_HLS":
                yield os.path.join(value, "include")

    except FileNotFoundError:
        xilinx_hls = os.environ.get("XILINX_HLS")
        if xilinx_hls is None:
            _logger.critical("not adding vendor include paths; please set XILINX_HLS")
            _logger.critical("you may run `source /path/to/Vitis/settings64.sh`")
        else:
            cpp_include = "tps/lnx64/gcc-6.2.0/include/c++/6.2.0"
            yield os.path.join(xilinx_hls, "include")
            yield os.path.join(xilinx_hls, cpp_include)
            yield os.path.join(xilinx_hls, cpp_include, "x86_64-pc-linux-gnu")


def nproc() -> int:
    return int(subprocess.check_output(["nproc"]))


def setup_logging(
    verbose: int | None,
    quiet: int | None,
    work_dir: str | None,
    program_name: str = "tapac",
) -> None:
    verbose = 0 if verbose is None else verbose
    quiet = 0 if quiet is None else quiet
    logging_level = (quiet - verbose) * 10 + logging.INFO
    logging_level = max(logging.DEBUG, min(logging.CRITICAL, logging_level))

    coloredlogs.install(
        level=logging_level,
        fmt="%(levelname).1s%(asctime)s %(name)s:%(lineno)d] %(message)s",
        datefmt="%m%d %H:%M:%S.%f",
    )

    log_dir = None
    if work_dir is not None:
        log_dir = os.path.join(work_dir, "log")
        os.makedirs(log_dir, exist_ok=True)

    # The following is copied and modified from absl-py.
    log_dir, file_prefix, symlink_prefix = absl.logging.find_log_dir_and_names(
        program_name,
        log_dir=log_dir,
    )

    assert log_dir is not None
    time_str = time.strftime("%Y%m%d-%H%M%S", time.localtime(time.time()))
    basename = f"{file_prefix}.INFO.{time_str}.{os.getpid()}"
    filename = os.path.join(log_dir, basename)
    symlink = os.path.join(log_dir, symlink_prefix + ".INFO")
    try:
        if os.path.islink(symlink):
            os.unlink(symlink)
        os.symlink(os.path.basename(filename), symlink)
    except OSError:
        # If it fails, we're sad but it's no error. Commonly, this fails because the
        # symlink was created by another user so we can't modify it.
        pass

    handler = logging.FileHandler(filename, encoding="utf-8")
    handler.setFormatter(
        logging.Formatter(
            fmt=(
                "%(levelname).1s%(asctime)s.%(msecs)03d "
                "%(name)s:%(lineno)d] %(message)s"
            ),
            datefmt="%m%d %H:%M:%S",
        ),
    )
    handler.setLevel(logging.DEBUG)
    logging.getLogger().addHandler(handler)

    _logger.info("logging level set to %s", logging.getLevelName(logging_level))
