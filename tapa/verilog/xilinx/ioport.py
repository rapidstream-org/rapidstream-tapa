"""Parser/vendor-agnostic RTL IO port type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal, NamedTuple

from pyverilog.ast_code_generator import codegen
from pyverilog.vparser import ast

from tapa.verilog.xilinx import ast_types


class IOPort(NamedTuple):
    name: str
    direction: Literal["input", "output", "inout"]
    width: ast.Width | None

    @classmethod
    def create(cls, port: ast_types.IOPort) -> "IOPort":
        if isinstance(port, ast.Input):
            direction = "input"
        elif isinstance(port, ast.Output):
            direction = "output"
        elif isinstance(port, ast.Inout):
            direction = "inout"
        else:
            msg = f"unexpected type {type(port)}"
            raise TypeError(msg)
        return IOPort(port.name, direction, port.width)

    @property
    def ast_port(self) -> ast_types.IOPort:
        ast_type = {
            "input": ast.Input,
            "output": ast.Output,
            "inout": ast.Inout,
        }[self.direction]
        return ast_type(self.name, self.width)

    def __str__(self) -> str:
        return codegen.ASTCodeGenerator().visit(self.ast_port)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, IOPort) and str(self) == str(other)

    def __hash__(self) -> int:
        return hash(str(self))
