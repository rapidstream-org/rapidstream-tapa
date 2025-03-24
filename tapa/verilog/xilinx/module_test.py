"""Unit tests for tapa.verilog.xilinx.module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections.abc import Iterator
from pathlib import Path

import pytest
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator
from pyverilog.vparser import ast

from tapa.util import Options
from tapa.verilog import ast_utils
from tapa.verilog.xilinx import ast_types
from tapa.verilog.xilinx.module import Module

_CODEGEN = ASTCodeGenerator()
_TESTDATA_PATH = (Path(__file__).parent / "testdata").resolve()


@pytest.fixture(params=["pyslang", "pyverilog"])
def options(request: pytest.FixtureRequest) -> Iterator[None]:
    enable_pyslang_saved = Options.enable_pyslang
    Options.enable_pyslang = request.param == "pyslang"
    yield
    Options.enable_pyslang = enable_pyslang_saved


@pytest.mark.usefixtures("options")
def test_invalid_module() -> None:
    with pytest.raises(ValueError, match="`files` and `name` cannot both be empty"):
        Module()


@pytest.mark.usefixtures("options")
def test_empty_module() -> None:
    """An empty module can be constructed from a name.

    This is used to create placeholders before Verilog is parsed, and to create
    skeleton FSM modules.
    """
    module = Module(name="foo")

    assert module.name == "foo"
    assert not module.ports


@pytest.mark.usefixtures("options")
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
    module.del_port(port.name)
    with pytest.raises(
        ValueError,
        match=r"module LowerLevelTask does not have port istream_peek._dout",
    ):
        module.get_port_of("istream_peek", "_dout")

    assert module.find_port(prefix="istream", suffix="_dout") == "istream_s_dout"


@pytest.mark.usefixtures("options")
def test_upper_level_task_module() -> None:
    module = Module(
        files=[str(_TESTDATA_PATH / "UpperLevelTask.v")],
        is_trimming_enabled=False,
    )

    assert module.name == "UpperLevelTask"
    assert list(module.signals) == [
        "ap_done",
        "ap_idle",
        "ap_ready",
        "ap_rst_n_inv",
        "ap_CS_fsm",
        "ap_CS_fsm_state1",
        "istream_s_blk_n",
        "ap_CS_fsm_state2",
        "ostreams_s_blk_n",
        "ap_block_state1",
        "ostreams_s_write_local",
        "istream_s_read_local",
        "ap_NS_fsm",
        "ap_ST_fsm_state1_blk",
        "ap_ST_fsm_state2_blk",
        "ap_ce_reg",
    ]
    assert list(module.params) == [
        "ap_ST_fsm_state1",
        "ap_ST_fsm_state2",
    ]

    module.cleanup()

    assert list(module.signals) == [
        "ap_block_state1",
        "ostreams_s_write_local",
        "istream_s_read_local",
        "ap_ST_fsm_state1_blk",
        "ap_ST_fsm_state2_blk",
        "ap_ce_reg",
        "ap_rst_n_inv",
        "ap_done",
        "ap_idle",
        "ap_ready",
    ]
    assert module.params == {}


@pytest.mark.usefixtures("options")
def test_add_ports_succeeds() -> None:
    module = Module(name="foo")
    assert module.ports == {}

    module.add_ports([ast_types.Input("bar", width=ast_utils.make_width(233))])
    ports = module.ports
    assert list(ports) == ["bar"]
    assert str(ports["bar"]) == "input [232:0] bar;"

    module.add_ports([ast_types.Input("baz"), ast_types.Output("qux")])
    ports = module.ports
    assert list(ports) == ["bar", "baz", "qux"]
    assert str(ports["baz"]) == "input baz;"
    assert str(ports["qux"]) == "output qux;"


@pytest.mark.usefixtures("options")
def test_del_port_succeeds() -> None:
    module = Module(name="foo")
    module.add_ports([ast_types.Input("bar")])

    module.del_port("bar")

    assert module.ports == {}


@pytest.mark.usefixtures("options")
def test_del_nonexistent_port_fails() -> None:
    module = Module(name="foo")
    module.add_ports([ast_types.Input("bar")])

    with pytest.raises(ValueError, match="no port baz found in module foo"):
        module.del_port("baz")


@pytest.mark.usefixtures("options")
def test_add_signals_succeeds() -> None:
    module = Module(name="foo")
    assert module.signals == {}

    module.add_signals([ast_types.Wire("bar", width=ast_utils.make_width(233))])
    signals = module.signals
    assert list(signals) == ["bar"]
    assert _CODEGEN.visit(signals["bar"]) == "wire [232:0] bar;"

    module.add_signals([ast_types.Wire("baz"), ast_types.Reg("qux")])
    signals = module.signals
    assert list(signals) == ["bar", "baz", "qux"]
    assert _CODEGEN.visit(signals["baz"]) == "wire baz;"
    assert _CODEGEN.visit(signals["qux"]) == "reg qux;"


@pytest.mark.usefixtures("options")
def test_del_signals_succeeds() -> None:
    module = Module(name="foo")
    module.add_signals([ast_types.Wire("bar")])

    module.del_signals(prefix="bar")

    assert module.signals == {}


@pytest.mark.usefixtures("options")
def test_del_nonexistent_signal_succeeds() -> None:
    module = Module(name="foo")
    module.add_signals([ast_types.Wire("bar")])

    module.del_signals(prefix="baz")

    assert list(module.signals) == ["bar"]


@pytest.mark.usefixtures("options")
def test_add_params_succeeds() -> None:
    module = Module(name="foo")
    assert module.params == {}

    module.add_params(
        [
            ast.Parameter(
                "bar",
                ast.Constant(42),
                width=ast_utils.make_width(233),
            )
        ]
    )
    params = module.params
    assert list(params) == ["bar"]
    assert _CODEGEN.visit(params["bar"]) == "parameter [232:0] bar = 42;"

    module.add_params(
        [
            ast.Parameter("baz", ast.Constant(0)),
            ast.Parameter("qux", ast.Constant(1)),
        ]
    )
    params = module.params
    assert list(params) == ["bar", "baz", "qux"]
    assert _CODEGEN.visit(params["baz"]) == "parameter baz = 0;"
    assert _CODEGEN.visit(params["qux"]) == "parameter qux = 1;"


@pytest.mark.usefixtures("options")
def test_del_params_succeeds() -> None:
    module = Module(name="foo")
    module.add_params([ast.Parameter("bar", ast.Constant(0))])

    module.del_params(prefix="bar")

    assert module.params == {}


@pytest.mark.usefixtures("options")
def test_del_nonexistent_param_succeeds() -> None:
    module = Module(name="foo")
    module.add_params([ast.Parameter("bar", ast.Constant(0))])

    module.del_params(prefix="baz")

    assert list(module.params) == ["bar"]


@pytest.mark.usefixtures("options")
def test_add_instance_succeeds() -> None:
    module = Module(name="foo")

    module.add_instance(
        module_name="Bar",
        instance_name="bar",
        ports=[ast.PortArg("port", ast.Constant("42"))],
        params=[ast.ParamArg("PARAM", ast.Constant("233"))],
    )

    assert """
Bar
#(
.PARAM(233)
)
bar
(
.port(42)
);
""" in "\n".join(  # strip() to ignore indentation
        line.strip() for line in module.code.splitlines()
    )

    module.add_instance(
        module_name="Baz",
        instance_name="baz",
        ports=[ast.PortArg("qux", ast.Constant("123"))],
    )

    assert """
Baz
baz
(
.qux(123)
);
""" in "\n".join(  # strip() to ignore indentation
        line.strip() for line in module.code.splitlines()
    )


@pytest.mark.usefixtures("options")
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
