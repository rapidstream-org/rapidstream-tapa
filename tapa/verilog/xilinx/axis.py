"""AXI Stream code generators for TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

__all__ = [
    "AXIS_CONSTANTS",
    "AXIS_PORT_WIDTHS",
    "STREAM_TO_AXIS",
    "get_axis_port_width_int",
]

# width=0 means configurable
AXIS_PORT_WIDTHS = {
    "TDATA": 0,
    "TLAST": 1,
    "TVALID": 1,
    "TREADY": 1,
    "TKEEP": 0,
}

# => {port_suffix: [axis_port_suffixes in order]}
STREAM_TO_AXIS = {
    "_dout": ["TDATA", "TLAST"],
    "_empty_n": ["TVALID"],
    "_read": ["TREADY"],
    "_din": ["TDATA", "TLAST"],
    "_full_n": ["TREADY"],
    "_write": ["TVALID"],
}

# => {port_suffix: bit_to_fill}
AXIS_CONSTANTS = {
    "TKEEP": 1,
}

AXIS_PORTS = {
    "_TDATA": "data",
    "_TLAST": "data",
    "_TVALID": "valid",
    "_TREADY": "ready",
    "_TKEEP": "data",
}


def get_axis_port_width_int(port: str, data_width: int) -> int:
    width = AXIS_PORT_WIDTHS[port]
    if width == 0:
        if port == "TDATA":
            width = data_width
        elif port == "TKEEP":
            width = data_width // 8
    return width
