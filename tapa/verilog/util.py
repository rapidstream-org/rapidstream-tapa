"""Utility functions for TAPA Verilog code generation."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from collections.abc import Iterator

from pyverilog.vparser.ast import Identifier

from tapa.verilog.signal import Reg, Wire
from tapa.verilog.width import Width

__all__ = [
    "Pipeline",
    "async_mmap_instance_name",
    "match_array_name",
    "sanitize_array_name",
    "wire_name",
]


class Pipeline:
    """Pipeline signals and their widths."""

    def __init__(self, name: str, width: int | None = None) -> None:
        self.name = name
        # If `name` is a constant literal (like `32'd0`), identifiers are just
        # the constant literal.
        self._ids = (Identifier(name if "'d" in name else (f"{name}__q0")),)
        self._width = Width.create(width)

    def __getitem__(self, idx: int) -> Identifier:
        return self._ids[idx]

    def __iter__(self) -> Iterator[Identifier]:
        return iter(self._ids)

    @property
    def signals(self) -> Iterator[Reg | Wire]:
        yield Wire(name=self[0].name, width=self._width)


def match_array_name(name: str) -> tuple[str, int] | None:
    match = re.fullmatch(r"(\w+)\[(\d+)\]", name)
    if match is not None:
        return match[1], int(match[2])
    return None


def array_name(name: str, idx: int) -> str:
    return f"{name}[{idx}]"


def sanitize_array_name(name: str) -> str:
    match = match_array_name(name)
    if match is not None:
        return f"{match[0]}_{match[1]}"
    return name


def wire_name(fifo: str, suffix: str) -> str:
    """Return the wire name of the fifo signals in generated modules.

    Args:
        fifo (str): Name of the fifo.
        suffix (str): One of the suffixes in ISTREAM_SUFFIXES or OSTREAM_SUFFIXES.

    Returns:
        str: Wire name of the fifo signal.
    """
    fifo = sanitize_array_name(fifo)
    if suffix.startswith("_"):
        suffix = suffix[1:]
    return f"{fifo}__{suffix}"


def async_mmap_instance_name(variable_name: str) -> str:
    return f"{variable_name}__m_axi"
