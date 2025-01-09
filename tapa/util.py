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
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator
from pyverilog.vparser.ast import (
    Decl,
    Inout,
    Input,
    Ioport,
    ModuleDef,
    Output,
    Port,
    Width,
)

if TYPE_CHECKING:
    from collections.abc import Iterable

_logger = logging.getLogger().getChild(__name__)

AST_IOPort = Input | Output | Inout


class PortInfo:
    """Port information extracted from a Verilog module definition."""

    def __init__(self, name: str, direction: str | None, width: Width | None) -> None:
        self.name = name
        self.direction = direction
        self.width = width

    def __str__(self) -> str:
        codegen = ASTCodeGenerator()
        if self.width:
            return f"{self.direction} {codegen.visit(self.width)} {self.name}"
        return f"{self.direction} {self.name}"

    def __eq__(self, other: PortInfo) -> bool:
        codegen = ASTCodeGenerator()
        width1 = codegen.visit(self.width) if self.width else None
        width2 = codegen.visit(other.width) if other.width else None
        return (
            self.name == other.name
            and self.direction == other.direction
            and width1 == width2
        )

    def __hash__(self) -> int:
        return hash((self.name, self.direction, self.width))


def clang_format(code: str, *args: str) -> str:
    """Apply clang-format with given arguments, if possible."""
    for version in range(10, 4, -1):
        clang_format_exe = shutil.which(f"clang-format-{version}")
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


def get_installation_path() -> Path:
    """Returns the directory where TAPA pre-built binary is installed."""
    home = os.environ.get("RAPIDSTREAM_TAPA_HOME")
    if home is not None:
        return Path(home)
    # `__file__` is like `/path/to/usr/share/tapa/runtime/tapa/util.py`
    return Path(__file__).parent.parent.parent.parent.parent.parent


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


def extract_ports_from_module_header(module_def: ModuleDef) -> dict[str, PortInfo]:
    """Extract ports direction and width from a given Verilog module header."""
    port_infos = {}
    for port in module_def.portlist.ports:
        if isinstance(port, Port):
            # When header contains only port names
            # the port is an instance of Port
            port_infos[port.name] = PortInfo(
                name=port.name,
                direction=None,
                width=None,
            )
        else:
            # When header contains port direction and/or width
            # the port is an instance of Ioport
            assert isinstance(port, Ioport)

            if port.first.children():
                width = port.first.children()[0]
                assert isinstance(width, Width)
            else:
                width = None

            if isinstance(port.first, Port):
                direction = None
            else:
                direction = port.first.__class__.__name__

            port_infos[port.first.name] = PortInfo(
                name=port.first.name,
                direction=direction,
                width=width,
            )
    return port_infos


def extract_ports_from_module_body(module_def: ModuleDef) -> dict[str, PortInfo]:
    """Extract ports direction and width from a given Verilog module body."""
    port_infos = {}
    for decl in module_def.children():
        if not isinstance(decl, Decl):
            continue
        port = decl.children()[0]
        if not isinstance(port, AST_IOPort):
            continue

        if port.children():
            width = port.children()[0]
            assert isinstance(width, Width)
        else:
            width = None

        direction = port.__class__.__name__

        port_infos[port.name] = PortInfo(
            name=port.name,
            direction=direction,
            width=width,
        )
    return port_infos


def extract_ports_from_module(module_def: ModuleDef) -> dict[str, PortInfo]:
    """Extract ports direction and width from a given Verilog module."""
    # get port info from header and body separately
    port_infos_1 = extract_ports_from_module_header(module_def)
    port_infos_2 = extract_ports_from_module_body(module_def)

    # merge the two dictionaries
    merged_dict = {}

    # Get all unique port names
    all_ports = set(port_infos_1.keys()).union(port_infos_2.keys())

    for port_name in all_ports:
        port1 = port_infos_1.get(port_name)
        port2 = port_infos_2.get(port_name)

        # Merge the port information
        if port1 and port2:
            if port1.direction and port2.direction:
                assert port1.direction == port2.direction
            if port1.width and port2.width:
                assert port1.width == port2.width
            merged_port = PortInfo(
                name=port1.name,  # Assuming name is always the same
                direction=port1.direction or port2.direction,
                width=port1.width if port1.width is not None else port2.width,
            )
        else:
            # If the port exists in only one dictionary, use it as is
            merged_port = port1 or port2

        merged_dict[port_name] = merged_port

    return merged_dict
