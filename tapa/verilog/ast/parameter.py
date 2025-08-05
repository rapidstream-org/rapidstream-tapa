"""Parser/vendor-agnostic RTL parameter type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NamedTuple

import pyslang

from tapa.verilog.ast.width import Width


class Parameter(NamedTuple):
    name: str
    value: str
    width: Width | None = None

    @classmethod
    def create(cls, node: pyslang.ParameterDeclarationStatementSyntax) -> "Parameter":
        assert isinstance(node.parameter, pyslang.ParameterDeclarationSyntax)
        return Parameter(
            name=node.parameter.declarators[0].name.valueText,
            value=str(node.parameter.declarators[0].initializer.expr).strip(),
            width=Width.create(node.parameter.type),
        )

    def __str__(self) -> str:
        fields = ["parameter"]
        if self.width is not None:
            fields.append(f"[{self.width.msb}:{self.width.lsb}]")
        fields.extend([self.name, "=", f"{self.value};"])
        return " ".join(fields)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Parameter) and str(self) == str(other)

    def __hash__(self) -> int:
        return hash(str(self))
