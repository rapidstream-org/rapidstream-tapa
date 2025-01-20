"""Unit tests for tapa.verilog.xilinx.ioport."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pyverilog.vparser import ast

from tapa.verilog.xilinx.ioport import IOPort

_NAME = "foo"
_WIDTH = ast.Width(msb=ast.Constant(31), lsb=ast.Constant(0))


def test_creation() -> None:
    ioport = IOPort.create(ast.Input(name=_NAME, width=_WIDTH))
    assert ioport.name == _NAME
    assert ioport.direction == "input"
    assert ioport.width is _WIDTH
    assert str(ioport) == "input [31:0] foo;"


def test_eq() -> None:
    a = IOPort.create(ast.Output(name=_NAME, width=_WIDTH))
    b = IOPort.create(ast.Output(name=_NAME, width=_WIDTH))
    assert a == b
