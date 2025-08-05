"""Parser/vendor-agnostic RTL IO port type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal, NamedTuple

import pyslang

from tapa.common.unique_attrs import UniqueAttrs
from tapa.verilog.pragma import Pragma
from tapa.verilog.width import Width


class IOPort(NamedTuple):
    direction: Literal["input", "output", "inout"]
    name: str
    width: Width | None = None
    pragma: Pragma | None = None

    @classmethod
    def create(cls, port: pyslang.PortDeclarationSyntax) -> "IOPort":
        attrs = UniqueAttrs(width=None)

        def visitor(node: object) -> None:
            if isinstance(node, pyslang.RangeSelectSyntax):
                attrs.width = Width(str(node.left), str(node.right))
            elif isinstance(node, pyslang.DeclaratorSyntax):
                attrs.name = node.name.valueText
            elif isinstance(node, pyslang.VariablePortHeaderSyntax):
                attrs.direction = node.direction.valueText

        port.visit(visitor)

        return IOPort(**attrs)  # type: ignore  # noqa: PGH003

    def __str__(self) -> str:
        fields = []
        if self.pragma is not None:
            fields.append(str(self.pragma))
        fields.append(self.direction)
        if self.width is not None:
            fields.append(f"[{self.width.msb}:{self.width.lsb}]")
        fields.append(f"{self.name};")
        return " ".join(fields)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, IOPort) and str(self) == str(other)

    def __hash__(self) -> int:
        return hash(str(self))
