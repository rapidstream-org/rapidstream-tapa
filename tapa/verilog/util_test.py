"""Unit tests for tapa.verilog.util."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.verilog.util import array_name, match_array_name


def test_match_array_name_returns_none() -> None:
    assert match_array_name("foo") is None


def test_match_array_name_returns_tuple() -> None:
    assert match_array_name("foo[0]") == ("foo", 0)


def test_array_name_succeeds() -> None:
    assert array_name("foo", 0) == "foo[0]"
