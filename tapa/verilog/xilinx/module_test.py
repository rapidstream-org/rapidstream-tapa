"""Unit tests for tapa.verilog.xilinx.module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pytest

from tapa.verilog.xilinx.module import Module


def test_invalid_module() -> None:
    with pytest.raises(ValueError, match="`files` and `name` cannot both be empty"):
        Module()


def test_empty_module() -> None:
    """An empty module can be constructed from a name.

    This is used to create placeholders before Verilog is parsed, and to create
    skeleton FSM modules.
    """
    module = Module(name="foo")

    assert module.name == "foo"
    assert not module.ports
