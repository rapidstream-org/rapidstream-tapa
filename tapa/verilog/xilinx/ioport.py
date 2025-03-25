"""Parser/vendor-agnostic RTL IO port type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from typing import Literal, NamedTuple

import pyslang
from pyverilog.ast_code_generator import codegen
from pyverilog.vparser import ast

from tapa.common.unique_attrs import UniqueAttrs
from tapa.verilog.ast_utils import make_pragma
from tapa.verilog.xilinx import ast_types
from tapa.verilog.xilinx.axis import AXIS_PORTS
from tapa.verilog.xilinx.const import HANDSHAKE_CLK, HANDSHAKE_RST_N
from tapa.verilog.xilinx.m_axi import M_AXI_PORTS

_logger = logging.getLogger().getChild(__name__)


class IOPort(NamedTuple):
    name: str
    direction: Literal["input", "output", "inout"]
    width: ast.Width | None

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
                attrs.width = ast.Width(
                    msb=ast.Constant(str(node.left)),
                    lsb=ast.Constant(str(node.right)),
                )
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
        return IOPort(port.name, direction, port.width)

    @property
    def ast_port(self) -> ast_types.IOPort:
        ast_type = {
            "input": ast.Input,
            "output": ast.Output,
            "inout": ast.Inout,
        }[self.direction]
        return ast_type(self.name, self.width)

    @property
    def rs_pragma(self) -> ast.Pragma | None:
        if self.name == HANDSHAKE_CLK:
            return make_pragma("RS_CLK")

        if self.name == HANDSHAKE_RST_N:
            return make_pragma("RS_RST", "ff")

        if self.name == "interrupt":
            return make_pragma("RS_FF", self.name)

        for channel, ports in M_AXI_PORTS.items():
            for port, _ in ports:
                if self.name.endswith(f"_{channel}{port}"):
                    return make_pragma(
                        "RS_HS",
                        f"{self.name[: -len(port)]}.{_get_rs_port(port)}",
                    )

        for suffix, role in AXIS_PORTS.items():
            if self.name.endswith(suffix):
                return make_pragma("RS_HS", f"{self.name[: -len(suffix)]}.{role}")

        _logger.error("not adding pragma for unknown port '%s'", self.name)
        return None

    def __str__(self) -> str:
        return codegen.ASTCodeGenerator().visit(self.ast_port)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, IOPort) and str(self) == str(other)

    def __hash__(self) -> int:
        return hash(str(self))


def _get_rs_port(port: str) -> str:
    """Return the RapidStream port for the given m_axi `port`."""
    if port in {"READY", "VALID"}:
        return port.lower()
    return "data"
