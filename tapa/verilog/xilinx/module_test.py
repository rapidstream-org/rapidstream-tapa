"""Unit tests for tapa.verilog.xilinx.module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

import pytest

from tapa.verilog.xilinx.module import Module

_TESTDATA_PATH = (Path(__file__).parent / "testdata").resolve()


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


def test_lower_level_task_module() -> None:
    module = Module(
        files=[str(_TESTDATA_PATH / "LowerLevelTask.v")],
        is_trimming_enabled=True,
    )

    assert module.name == "LowerLevelTask"

    assert list(module.ports) == [
        "ap_clk",
        "ap_rst_n",
        "ap_start",
        "ap_done",
        "ap_idle",
        "ap_ready",
        "istream_s_dout",
        "istream_s_empty_n",
        "istream_s_read",
        "istream_peek_dout",
        "istream_peek_empty_n",
        "istream_peek_read",
        "ostreams_s_din",
        "ostreams_s_full_n",
        "ostreams_s_write",
        "ostreams_peek",
        "scalar",
    ]
    assert list(module.signals) == [
        "ap_done",
        "ap_idle",
        "ap_ready",
    ]

    assert module.get_port_of("istream", "_dout").name == "istream_s_dout"
    assert module.get_port_of("ostreams[0]", "_din").name == "ostreams_s_din"

    for port_got, port_name_wanted in zip(
        module.generate_istream_ports("istream", "arg"),
        ("istream_s_dout", "istream_peek_dout"),
    ):
        assert port_got.portname == port_name_wanted

    for port_got, port_name_wanted in zip(
        module.generate_ostream_ports("ostreams[0]", "arg"),
        ("ostreams_s_din",),
    ):
        assert port_got.portname == port_name_wanted

    port = module.get_port_of("istream_peek", "_dout")
    assert port.name == "istream_peek_dout"
    module.del_ports([port.name])
    with pytest.raises(
        ValueError,
        match=r"module LowerLevelTask does not have port istream_peek._dout",
    ):
        module.get_port_of("istream_peek", "_dout")

    assert module.find_port(prefix="istream", suffix="_dout") == "istream_s_dout"


def test_upper_level_task_module() -> None:
    module = Module(
        files=[str(_TESTDATA_PATH / "UpperLevelTask.v")],
        is_trimming_enabled=False,
    )

    assert module.name == "UpperLevelTask"
    assert module.params != {}

    module.cleanup()

    assert module.params == {}


def test_add_m_axi() -> None:
    module = Module(name="foo")

    assert module.name == "foo"
    assert module.ports == {}

    module.add_m_axi(name="bar", data_width=32)

    for port in module.ports:
        assert port.startswith("m_axi_bar_")
    assert set(module.ports).issuperset(
        {
            "m_axi_bar_ARADDR",
            "m_axi_bar_ARREADY",
            "m_axi_bar_ARVALID",
            "m_axi_bar_AWADDR",
            "m_axi_bar_AWREADY",
            "m_axi_bar_AWVALID",
            "m_axi_bar_RDATA",
            "m_axi_bar_RREADY",
            "m_axi_bar_RVALID",
            "m_axi_bar_WDATA",
            "m_axi_bar_WREADY",
            "m_axi_bar_WVALID",
            "m_axi_bar_BRESP",
            "m_axi_bar_BREADY",
            "m_axi_bar_BVALID",
        }
    )
