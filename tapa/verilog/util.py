"""Utility functions for TAPA Verilog code generation."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from collections.abc import Iterator

from pyverilog.vparser.ast import Identifier, Pragma, Reg, Width, Wire

from tapa.verilog.ast_utils import make_pragma, make_width

__all__ = [
    "Pipeline",
    "async_mmap_instance_name",
    "match_array_name",
    "sanitize_array_name",
    "wire_name",
]


class Pipeline:
    """Pipeline signals and their widths."""

    def __init__(self, name: str, level: int, width: int | None = None) -> None:
        self.name = name
        self.level = level
        self._ids = tuple(
            # If `name` is a constant literal (like `32'd0`), identifiers at all
            # levels are just the constant literal.
            Identifier(name if "'d" in name else (f"{name}__q%d" % i))
            for i in range(level + 1)
        )
        self._width: Width | None = (width and make_width(width)) or None

    def __getitem__(self, idx: int) -> Identifier:
        return self._ids[idx]

    def __iter__(self) -> Iterator[Identifier]:
        return iter(self._ids)

    @property
    def signals(self) -> Iterator[Reg | Wire | Pragma]:
        yield Wire(name=self[0].name, width=self._width)
        for x in self[1:]:
            yield make_pragma("keep", "true")
            yield Reg(name=x.name, width=self._width)


def match_array_name(name: str) -> tuple[str, int] | None:
    match = re.fullmatch(r"(\w+)\[(\d+)\]", name)
    if match is not None:
        return match[1], int(match[2])
    return None


def sanitize_array_name(name: str) -> str:
    match = match_array_name(name)
    if match is not None:
        return f"{match[0]}_{match[1]}"
    return name


def wire_name(fifo: str, suffix: str) -> str:
    """Return the wire name of the fifo signals in generated modules.

    Args:
    ----
        fifo (str): Name of the fifo.
        suffix (str): One of the suffixes in ISTREAM_SUFFIXES or OSTREAM_SUFFIXES.

    Returns:
    -------
        str: Wire name of the fifo signal.

    """
    fifo = sanitize_array_name(fifo)
    if suffix.startswith("_"):
        suffix = suffix[1:]
    return f"{fifo}__{suffix}"


def async_mmap_instance_name(variable_name: str) -> str:
    return f"{variable_name}__m_axi"
