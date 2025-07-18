"""Async MMAP ports and signals generator."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from collections.abc import Iterator
from typing import TYPE_CHECKING

from pyverilog.vparser.ast import PortArg

from tapa.verilog.ast_utils import make_port_arg
from tapa.verilog.ioport import IOPort
from tapa.verilog.signal import Wire
from tapa.verilog.util import wire_name
from tapa.verilog.width import Width
from tapa.verilog.xilinx.const import (
    ISTREAM_SUFFIXES,
    OSTREAM_SUFFIXES,
    STREAM_PORT_DIRECTION,
)

if TYPE_CHECKING:
    from tapa.instance import Instance

__all__ = [
    "ASYNC_MMAP_SUFFIXES",
    "async_mmap_arg_name",
    "async_mmap_width",
    "generate_async_mmap_ioports",
    "generate_async_mmap_ports",
    "generate_async_mmap_signals",
]

_logger = logging.getLogger().getChild(__name__)

ADDR_CHANNEL_DATA_WIDTH = 64
RESP_CHANNEL_DATA_WIDTH = 8

ASYNC_MMAP_SUFFIXES = {
    "read_addr": OSTREAM_SUFFIXES,
    "read_data": ISTREAM_SUFFIXES,
    "write_addr": OSTREAM_SUFFIXES,
    "write_data": OSTREAM_SUFFIXES,
    "write_resp": ISTREAM_SUFFIXES,
}


def async_mmap_arg_name(arg: str, tag: str, suffix: str) -> str:
    return wire_name(f"{arg}_{tag}", suffix)


def async_mmap_width(
    tag: str,
    suffix: str,
    data_width: int,
) -> Width | None:
    if suffix in {ISTREAM_SUFFIXES[0], OSTREAM_SUFFIXES[0]}:
        if tag.endswith("addr"):
            data_width = ADDR_CHANNEL_DATA_WIDTH
        elif tag == "write_resp":
            data_width = RESP_CHANNEL_DATA_WIDTH
        return Width.create(data_width)
    return None


def generate_async_mmap_ports(
    tag: str,
    port: str,
    arg: str,
    offset_name: str,
    instance: "Instance",
) -> Iterator[PortArg]:
    # TODO: reuse functions that generate i/ostream ports
    prefix = port + "_" + tag
    for suffix in ASYNC_MMAP_SUFFIXES[tag]:
        arg_name = async_mmap_arg_name(
            arg=arg,
            tag=tag,
            suffix=suffix,
        )
        port_name = instance.task.module.find_port(prefix=prefix, suffix=suffix)
        if port_name is not None:
            # Make sure Eot is always 1'b0.
            if suffix == ISTREAM_SUFFIXES[0]:
                arg_name = f"{{1'b0, {arg_name}}}"

            _logger.debug(
                "`%s.%s` is connected to async_mmap port `%s.%s`",
                instance.name,
                port_name,
                arg,
                tag,
            )
            yield make_port_arg(port=port_name, arg=arg_name)

        # Generate peek ports.
        if ASYNC_MMAP_SUFFIXES[tag] is ISTREAM_SUFFIXES:
            port_name = instance.task.module.find_port(prefix + "_peek", suffix)
            if port_name is not None:
                # Ignore read enable from peek ports.
                if STREAM_PORT_DIRECTION[suffix] == "input":
                    _logger.debug(
                        "`%s.%s` is connected to async_mmap port `%s.%s`",
                        instance.name,
                        port_name,
                        arg,
                        tag,
                    )
                else:
                    arg_name = ""

                yield make_port_arg(port=port_name, arg=arg_name)

    # Generate the offset port, which carries the base address of this async_mmap.
    if tag.endswith("_addr"):
        port_name = instance.task.module.find_port(prefix, suffix="_offset")
        assert port_name is not None, f"missing async_mmap port `{arg}.offset`"
        _logger.debug(
            "`%s.%s` is connected to async_mmap port `%s.offset`",
            instance.name,
            port_name,
            arg,
        )
        yield make_port_arg(port=port_name, arg=offset_name)


def generate_async_mmap_signals(tag: str, arg: str, data_width: int) -> Iterator[Wire]:
    for suffix in ASYNC_MMAP_SUFFIXES[tag]:
        yield Wire(
            name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
            width=async_mmap_width(
                tag=tag,
                suffix=suffix,
                data_width=data_width,
            ),
        )


def generate_async_mmap_ioports(
    tag: str,
    arg: str,
    data_width: int,
) -> Iterator[IOPort]:
    for suffix in ASYNC_MMAP_SUFFIXES[tag]:
        if suffix in {ISTREAM_SUFFIXES[-1], *OSTREAM_SUFFIXES[::2]}:
            direction = "output"
        else:
            direction = "input"
        yield IOPort(
            direction,
            name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
            width=async_mmap_width(
                tag=tag,
                suffix=suffix,
                data_width=data_width,
            ),
        )
