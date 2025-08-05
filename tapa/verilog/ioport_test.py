"""Unit tests for tapa.verilog.ioport."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pyslang

from tapa.verilog.ioport import IOPort
from tapa.verilog.width import Width


def test_creation() -> None:
    port = pyslang.SyntaxTree.fromText("input [31:0] foo;").root
    assert isinstance(port, pyslang.PortDeclarationSyntax)
    ioport = IOPort.create(port)
    assert ioport.name == "foo"
    assert ioport.direction == "input"
    assert str(ioport) == "input [31:0] foo;"


def test_eq() -> None:
    a = IOPort(direction="input", name="foo", width=Width(msb="31", lsb="0"))
    b = IOPort(direction="input", name="foo", width=Width(msb="31", lsb="0"))
    assert a == b
