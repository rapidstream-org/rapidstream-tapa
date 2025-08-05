"""Constants used in TAPA code generation."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal

from pyverilog.vparser.ast import (
    Identifier,
    IntConst,
    Unot,
)

from tapa.verilog.ast.width import Width

__all__ = [
    "CLK",
    "CLK_SENS_LIST",
    "DONE",
    "FALSE",
    "FIFO_READ_PORTS",
    "FIFO_WRITE_PORTS",
    "HANDSHAKE_CLK",
    "HANDSHAKE_DONE",
    "HANDSHAKE_IDLE",
    "HANDSHAKE_INPUT_PORTS",
    "HANDSHAKE_OUTPUT_PORTS",
    "HANDSHAKE_READY",
    "HANDSHAKE_RST",
    "HANDSHAKE_RST_N",
    "HANDSHAKE_START",
    "IDLE",
    "ISTREAM_SUFFIXES",
    "OSTREAM_SUFFIXES",
    "READY",
    "RST",
    "RST_N",
    "RTL_SUFFIX",
    "SENS_TYPE",
    "START",
    "STATE",
    "STREAM_PORT_DIRECTION",
    "STREAM_PORT_OPPOSITE",
    "STREAM_PORT_WIDTH",
    "TRUE",
    "get_stream_width",
]

# const strings

RTL_SUFFIX = ".v"

ISTREAM_SUFFIXES = (
    "_dout",
    "_empty_n",
    "_read",
)

OSTREAM_SUFFIXES = (
    "_din",
    "_full_n",
    "_write",
)

ISTREAM_ROLES = {
    "valid": "_empty_n",
    "ready": "_read",
}

OSTREAM_ROLES = {
    "ready": "_full_n",
    "valid": "_write",
}

STREAM_DATA_SUFFIXES = (
    "_dout",
    "_din",
)

# => {port_suffix: direction}
STREAM_PORT_DIRECTION: dict[str, Literal["input", "output"]] = {
    "_dout": "input",
    "_empty_n": "input",
    "_read": "output",
    "_din": "output",
    "_full_n": "input",
    "_write": "output",
}

# => {port_suffix: opposite_suffix}
# used when connecting two FIFOs head to tail
STREAM_PORT_OPPOSITE = {
    "_dout": "_din",
    "_empty_n": "_write",
    "_read": "_full_n",
    "_din": "_dout",
    "_full_n": "_read",
    "_write": "_empty_n",
}

# => {port_suffix: width}, 0 is variable
STREAM_PORT_WIDTH = {
    "_dout": 0,
    "_empty_n": 1,
    "_read": 1,
    "_din": 0,
    "_full_n": 1,
    "_write": 1,
}

FIFO_READ_PORTS = (
    "if_dout",
    "if_empty_n",
    "if_read",
    "if_read_ce",
)

FIFO_WRITE_PORTS = (
    "if_din",
    "if_full_n",
    "if_write",
    "if_write_ce",
)

HANDSHAKE_CLK = "ap_clk"
HANDSHAKE_RST = "ap_rst_n_inv"
HANDSHAKE_RST_N = "ap_rst_n"
HANDSHAKE_START = "ap_start"
HANDSHAKE_DONE = "ap_done"
HANDSHAKE_IDLE = "ap_idle"
HANDSHAKE_READY = "ap_ready"

HANDSHAKE_INPUT_PORTS = (
    HANDSHAKE_CLK,
    HANDSHAKE_RST_N,
    HANDSHAKE_START,
)
HANDSHAKE_OUTPUT_PORTS = (
    HANDSHAKE_DONE,
    HANDSHAKE_IDLE,
    HANDSHAKE_READY,
)

# const ast nodes

START = Identifier(HANDSHAKE_START)
DONE = Identifier(HANDSHAKE_DONE)
IDLE = Identifier(HANDSHAKE_IDLE)
READY = Identifier(HANDSHAKE_READY)
TRUE = IntConst("1'b1")
FALSE = IntConst("1'b0")
SENS_TYPE = "posedge"
CLK = Identifier(HANDSHAKE_CLK)
RST_N = Identifier(HANDSHAKE_RST_N)
RST = Unot(RST_N)
CLK_SENS_LIST = f"{SENS_TYPE} {HANDSHAKE_CLK}"
STATE = Identifier("tapa_state")


def get_stream_width(port: str, data_width: int) -> Width | None:
    width = STREAM_PORT_WIDTH[port]
    if width == 0:
        width = data_width + 1  # for eot
    if width == 1:
        return None
    return Width.create(width)
