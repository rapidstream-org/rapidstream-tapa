"""Memory-mapped AXI interface utilities."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections.abc import Iterable

from tapa.backend.xilinx import M_AXI_PREFIX
from tapa.verilog.width import Width

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

M_AXI_SUFFIXES_BY_CHANNEL = {
    "AR": {
        "ports": (
            "_ARADDR",
            "_ARBURST",
            "_ARID",
            "_ARLEN",
            "_ARREADY",
            "_ARSIZE",
            "_ARVALID",
            "_ARLOCK",
            "_ARPROT",
            "_ARQOS",
            "_ARCACHE",
        ),
        "valid": "_ARVALID",
        "ready": "_ARREADY",
    },
    "AW": {
        "ports": (
            "_AWADDR",
            "_AWBURST",
            "_AWID",
            "_AWLEN",
            "_AWREADY",
            "_AWSIZE",
            "_AWVALID",
            "_AWLOCK",
            "_AWPROT",
            "_AWQOS",
            "_AWCACHE",
        ),
        "valid": "_AWVALID",
        "ready": "_AWREADY",
    },
    "B": {
        "ports": ("_BID", "_BREADY", "_BRESP", "_BVALID"),
        "valid": "_BVALID",
        "ready": "_BREADY",
    },
    "R": {
        "ports": (
            "_RDATA",
            "_RID",
            "_RLAST",
            "_RREADY",
            "_RRESP",
            "_RVALID",
        ),
        "valid": "_RVALID",
        "ready": "_RREADY",
    },
    "W": {
        "ports": ("_WDATA", "_WLAST", "_WREADY", "_WSTRB", "_WVALID"),
        "valid": "_WVALID",
        "ready": "_WREADY",
    },
}

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
    return Width.create(width)
