"""Parser/vendor-agnostic RTL signal (`Reg`/`Wire`) type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal, NamedTuple

from tapa.verilog.width import Width


class Reg(NamedTuple):
    name: str
    width: Width | None = None

    def __str__(self) -> str:
        return _to_str("reg", self.name, self.width)


class Wire(NamedTuple):
    name: str
    width: Width | None = None

    def __str__(self) -> str:
        return _to_str("wire", self.name, self.width)


def _to_str(type_: Literal["reg", "wire"], name: str, width: Width | None) -> str:
    if width is None:
        return f"{type_} {name};"
    return f"{type_} [{width.msb}:{width.lsb}] {name};"
