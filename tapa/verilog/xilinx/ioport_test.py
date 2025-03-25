"""Unit tests for tapa.verilog.xilinx.ioport."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pyslang
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


def test_creation_pyslang() -> None:
    port = pyslang.SyntaxTree.fromText("input [31:0] foo;").root
    assert isinstance(port, pyslang.PortDeclarationSyntax)
    ioport = IOPort.create(port)
    assert ioport.name == "foo"
    assert ioport.direction == "input"
    assert str(ioport) == "input [31:0] foo;"


def test_ap_clk_gets_rs_clk_pragma() -> None:
    pragma = IOPort.create(ast.Input(name="ap_clk")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_CLK"


def test_ap_rst_n_gets_rs_rst_pragma() -> None:
    pragma = IOPort.create(ast.Input(name="ap_rst_n")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_RST"
    assert pragma.entry.value.value == "ff"


def test_interrupt_gets_rs_ff_pragma() -> None:
    pragma = IOPort.create(ast.Input(name="interrupt")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_FF"
    assert pragma.entry.value.value == "interrupt"


def test_m_axi_awvalid_gets_rs_hs_pragma() -> None:
    pragma = IOPort.create(ast.Output(name="m_axi_foo_AWVALID")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_HS"
    assert pragma.entry.value.value == "m_axi_foo_AW.valid"


def test_m_axi_awaddr_gets_rs_hs_pragma() -> None:
    pragma = IOPort.create(ast.Output(name="m_axi_foo_AWADDR")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_HS"
    assert pragma.entry.value.value == "m_axi_foo_AW.data"


def test_axis_gets_rs_hs_pragma() -> None:
    pragma = IOPort.create(ast.Input(name="foo_TDATA")).rs_pragma

    assert pragma is not None
    assert pragma.entry.name == "RS_HS"
    assert pragma.entry.value.value == "foo.data"


def test_unknown_port_gets_none_rs_pragma() -> None:
    pragma = IOPort.create(ast.Input(name="foo")).rs_pragma

    assert pragma is None


def test_eq() -> None:
    a = IOPort.create(ast.Output(name=_NAME, width=_WIDTH))
    b = IOPort.create(ast.Output(name=_NAME, width=_WIDTH))
    assert a == b
