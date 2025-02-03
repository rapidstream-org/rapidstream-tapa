"""Memory-mapped AXI interface utilities."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import copy
from collections.abc import Iterable

from pyverilog.vparser.ast import Constant, Identifier, Minus, Parameter, Width

from tapa.backend.xilinx import M_AXI_PREFIX
from tapa.verilog.xilinx.ast_types import IOPort

__all__ = [
    "M_AXI_ADDR_PORTS",
    "M_AXI_PARAMS",
    "M_AXI_PARAM_PREFIX",
    "M_AXI_PARAM_SUFFIXES",
    "M_AXI_PORTS",
    "M_AXI_PORT_WIDTHS",
    "M_AXI_PREFIX",
    "M_AXI_SUFFIXES",
    "get_m_axi_port_width",
    "is_m_axi_param",
    "is_m_axi_port",
    "is_m_axi_unique_param",
    "rename_m_axi_name",
    "rename_m_axi_param",
    "rename_m_axi_port",
]

# width=0 means configurable
M_AXI_PORT_WIDTHS = {
    "ADDR": 0,
    "BURST": 2,
    "CACHE": 4,
    "DATA": 0,
    "ID": 1,
    "LAST": 1,
    "LEN": 8,
    "LOCK": 1,
    "PROT": 3,
    "QOS": 4,
    "READY": 1,
    "RESP": 2,
    "SIZE": 3,
    "STRB": 0,
    "VALID": 1,
}

# => [(name, direction), ...]
M_AXI_ADDR_PORTS = (
    ("ADDR", "output"),
    ("BURST", "output"),
    ("CACHE", "output"),
    ("ID", "output"),
    ("LEN", "output"),
    ("LOCK", "output"),
    ("PROT", "output"),
    ("QOS", "output"),
    ("READY", "input"),
    ("SIZE", "output"),
    ("VALID", "output"),
)

# => {channel: [(name, direction), ...]}
M_AXI_PORTS: dict[str, tuple[tuple[str, str], ...]] = {
    "AR": M_AXI_ADDR_PORTS,
    "AW": M_AXI_ADDR_PORTS,
    "B": (
        ("ID", "input"),
        ("READY", "output"),
        ("RESP", "input"),
        ("VALID", "input"),
    ),
    "R": (
        ("DATA", "input"),
        ("ID", "input"),
        ("LAST", "input"),
        ("READY", "output"),
        ("RESP", "input"),
        ("VALID", "input"),
    ),
    "W": (
        ("DATA", "output"),
        ("LAST", "output"),
        ("READY", "input"),
        ("STRB", "output"),
        ("VALID", "output"),
    ),
}

M_AXI_SUFFIXES_COMPACT = (
    "_ARADDR",
    "_ARBURST",
    "_ARID",
    "_ARLEN",
    "_ARREADY",
    "_ARSIZE",
    "_ARVALID",
    "_AWADDR",
    "_AWBURST",
    "_AWID",
    "_AWLEN",
    "_AWREADY",
    "_AWSIZE",
    "_AWVALID",
    "_BID",
    "_BREADY",
    "_BRESP",
    "_BVALID",
    "_RDATA",
    "_RID",
    "_RLAST",
    "_RREADY",
    "_RRESP",
    "_RVALID",
    "_WDATA",
    "_WLAST",
    "_WREADY",
    "_WSTRB",
    "_WVALID",
)

M_AXI_SUFFIXES = (
    *M_AXI_SUFFIXES_COMPACT,
    "_ARLOCK",
    "_ARPROT",
    "_ARQOS",
    "_ARCACHE",
    "_AWCACHE",
    "_AWLOCK",
    "_AWPROT",
    "_AWQOS",
)

M_AXI_PARAM_PREFIX = "C_M_AXI_"

M_AXI_PARAM_SUFFIXES = (
    "_ID_WIDTH",
    "_ADDR_WIDTH",
    "_DATA_WIDTH",
    "_PROT_VALUE",
    "_CACHE_VALUE",
    "_WSTRB_WIDTH",
)

M_AXI_PARAMS = ("C_M_AXI_DATA_WIDTH", "C_M_AXI_WSTRB_WIDTH")
M_AXI_PARAM_MIN_SPLIT = 5


def is_m_axi_port(port: str | IOPort) -> bool:
    """Check if a port is a memory-mapped AXI port."""
    if not isinstance(port, str):
        port = port.name
    return port.startswith(M_AXI_PREFIX) and "_" + port.split("_")[-1] in M_AXI_SUFFIXES


def is_m_axi_param(param: str | Parameter) -> bool:
    """Check if a parameter is a memory-mapped AXI parameter."""
    if not isinstance(param, str):
        param = param.name
    assert isinstance(param, str)
    param_split = param.split("_")
    return (
        len(param_split) > M_AXI_PARAM_MIN_SPLIT
        and param.startswith(M_AXI_PARAM_PREFIX)
        and "".join(map("_{}".format, param_split[-2:])) in M_AXI_PARAM_SUFFIXES
    )


def is_m_axi_unique_param(param: str | Parameter) -> bool:
    """Check if a parameter is a unique memory-mapped AXI parameter."""
    if not isinstance(param, str):
        param = param.name
    return param in M_AXI_PARAMS


def rename_m_axi_name(mapping: dict[str, str], name: str, idx1: int, idx2: int) -> str:
    """Rename a memory-mapped AXI port or parameter."""
    name_snippets = name.split("_")
    try:
        return "_".join(
            name_snippets[:idx1]
            + [mapping["_".join(name_snippets[idx1:idx2])]]
            + name_snippets[idx2:],
        )
    except KeyError:
        pass
    raise ValueError(
        "'{}' is a result of renaming done by Vivado HLS; ".format(
            "_".join(name_snippets[idx1:idx2]),
        )
        + "please use a different variable name",
    )


def rename_m_axi_port(
    mapping: dict[str, str],
    port: IOPort,
) -> IOPort:
    """Rename a memory-mapped AXI port."""
    new_port = copy.copy(port)
    new_port.name = rename_m_axi_name(mapping, port.name, 2, -1)
    if port.width is not None and isinstance(port.width.msb, Minus):
        new_port.width = copy.copy(new_port.width)
        new_port.width.msb = copy.copy(new_port.width.msb)
        new_port.width.msb.left = Identifier(
            rename_m_axi_name(mapping, port.width.msb.left.name, 3, -2),
        )
    return new_port


def rename_m_axi_param(mapping: dict[str, str], param: Parameter) -> Parameter:
    """Rename a memory-mapped AXI parameter."""
    new_param = copy.copy(param)
    new_param.name = rename_m_axi_name(mapping, param.name, 3, -2)
    return new_param


def get_m_axi_port_width(
    port: str,
    data_width: int,
    addr_width: int = 64,
    id_width: int | None = None,
    vec_ports: Iterable[str] = ("ID",),
) -> Width | None:
    """Get the width of a memory-mapped AXI port."""
    width = M_AXI_PORT_WIDTHS[port]
    if width == 0:
        if port == "ADDR":
            width = addr_width
        elif port == "DATA":
            width = data_width
        elif port == "STRB":
            width = data_width // 8
    elif width == 1 and port not in vec_ports:
        return None
    if port == "ID" and id_width is not None:
        width = id_width
    return Width(msb=Constant(width - 1), lsb=Constant(0))
