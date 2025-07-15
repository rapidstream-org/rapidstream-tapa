"""Parser/vendor-agnostic RTL IO port type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from typing import Literal, NamedTuple

import pyslang
from pyverilog.vparser import ast

from tapa.common.unique_attrs import UniqueAttrs
from tapa.verilog import ast_types
from tapa.verilog.pragma import Pragma
from tapa.verilog.width import Width

_logger = logging.getLogger().getChild(__name__)


class IOPort(NamedTuple):
    direction: Literal["input", "output", "inout"]
    name: str
    width: Width | None = None
    pragma: Pragma | None = None

    @classmethod
    def create(
        cls,
        port: ast_types.IOPort | pyslang.PortDeclarationSyntax,
    ) -> "IOPort":
        if isinstance(port, ast_types.IOPort):
            return IOPort._from_pyverilog(port)
        if isinstance(port, pyslang.PortDeclarationSyntax):
            return IOPort._from_pyslang(port)
        msg = f"unexpected type {type(port)}"
        raise TypeError(msg)

    @classmethod
    def _from_pyslang(cls, port: pyslang.PortDeclarationSyntax) -> "IOPort":
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

    @classmethod
    def _from_pyverilog(cls, port: ast_types.IOPort) -> "IOPort":
        if isinstance(port, ast.Input):
            direction = "input"
        elif isinstance(port, ast.Output):
            direction = "output"
        elif isinstance(port, ast.Inout):
            direction = "inout"
        else:
            msg = f"unexpected type {type(port)}"
            raise TypeError(msg)
        return IOPort(
            direction,
            port.name,
            None if port.width is None else Width(port.width.msb, port.width.lsb),
        )

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
