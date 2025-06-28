"""Parser/vendor-agnostic RTL width type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NamedTuple

from pyverilog.vparser import ast


class Width(NamedTuple):
    msb: str
    lsb: str

    @property
    def ast_width(self) -> ast.Width:
        return ast.Width(ast.Constant(self.msb), ast.Constant(self.lsb))
