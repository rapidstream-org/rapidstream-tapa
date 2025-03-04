"""Utility functions for TAPA."""

from __future__ import annotations

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import getpass
import logging
import os.path
import shutil
import socket
import subprocess
import time
from pathlib import Path
from typing import TYPE_CHECKING, Literal

import coloredlogs

if TYPE_CHECKING:
    from collections.abc import Iterable

_logger = logging.getLogger().getChild(__name__)


class Options:
    """Global configuration options."""

    enable_pyslang: bool = False

    # Only clang-format the first 1MB code to avoid performance bottleneck:
    # https://github.com/rapidstream-org/rapidstream-tapa/issues/232
    clang_format_quota_in_bytes: int = 1_000_000


_clang_formatted_byte_count = 0


def clang_format(code: str, *args: str) -> str:
    """Apply clang-format with given arguments, if possible."""
    for version in range(20, 4, -1):
        clang_format_exe = shutil.which(f"clang-format-{version}")
        if clang_format_exe is not None:
            break
    else:
        clang_format_exe = shutil.which("clang-format")
    if clang_format_exe is None:
        return code

    global _clang_formatted_byte_count  # noqa: PLW0603
    new_byte_count = _clang_formatted_byte_count + len(code)
    if new_byte_count > Options.clang_format_quota_in_bytes:
        return code
    _clang_formatted_byte_count = new_byte_count

    _logger.debug("clang-format quota: %d bytes", new_byte_count)
    return subprocess.check_output(
        [clang_format_exe, *args],
        input=code,
        text=True,
    )


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


def get_xilinx_tool_path(tool_name: Literal["HLS", "VITIS"] = "HLS") -> str | None:
    "Returns the XILINX_<TOOL> path."
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
    "Returns the XPFM path for a platform."
    xilinx_vitis_path = get_xilinx_tool_path("VITIS")
    path_in_vitis = f"{xilinx_vitis_path}/base_platforms/{platform}/{platform}.xpfm"
    path_in_opt = f"/opt/xilinx/platforms/{platform}/{platform}.xpfm"
    if os.path.exists(path_in_vitis):
        return path_in_vitis
    if os.path.exists(path_in_opt):
        return path_in_opt

    _logger.critical("Cannot find XPFM for platform %s", platform)
    return None


def get_vendor_include_paths() -> Iterable[str]:
    """Yields include paths that are automatically available in vendor tools."""
    xilinx_hls = get_xilinx_tool_path("HLS")
    if xilinx_hls is not None:
        # include VITIS_HLS/include
        yield os.path.join(xilinx_hls, "include")

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


def setup_logging(
    verbose: int | None,
    quiet: int | None,
    work_dir: str,
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

    log_dir = os.path.join(work_dir, "log")
    os.makedirs(log_dir, exist_ok=True)

    # The following is copied and modified from absl-py.
    try:
        username = getpass.getuser()
    except KeyError:
        # This can happen, e.g. when running under docker w/o passwd file.
        username = str(os.getuid())
    hostname = socket.gethostname()
    file_prefix = f"{program_name}.{hostname}.{username}.log"
    symlink_prefix = program_name
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
