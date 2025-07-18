"""Unit tests for tapa.verilog.xilinx.module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

import pytest
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator
from pyverilog.vparser import ast

from tapa.verilog import ast_utils
from tapa.verilog.ioport import IOPort
from tapa.verilog.logic import Always, Assign
from tapa.verilog.signal import Reg, Wire
from tapa.verilog.width import Width
from tapa.verilog.xilinx.const import (
    CLK_SENS_LIST,
    HANDSHAKE_CLK,
    HANDSHAKE_RST_N,
)
from tapa.verilog.xilinx.module import Module, _get_rs_pragma

_CODEGEN = ASTCodeGenerator()
_TESTDATA_PATH = (Path(__file__).parent / "testdata").resolve()


# `params` must not be empty, or tests will be skipped
@pytest.fixture(params=["pyslang"])
def options() -> None:
    return


@pytest.mark.usefixtures("options")
def test_invalid_module() -> None:
    with pytest.raises(ValueError, match="`files` and `name` cannot both be empty"):
        Module()


@pytest.mark.usefixtures("options")
def test_empty_module() -> None:
    """An empty module can be constructed from a name.

    This is used to create placeholders before Verilog is parsed, and to create skeleton
    FSM modules.
    """
    module = Module(name="foo")

    assert module.name == "foo"
    assert not module.ports


@pytest.mark.usefixtures("options")
def test_lower_level_task_module() -> None:
    module = Module(
        files=[_TESTDATA_PATH / "LowerLevelTask.v"],
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

    assert (
        module.get_template_code()
        == """
module LowerLevelTask
(
  ap_clk,
  ap_rst_n,
  ap_start,
  ap_done,
  ap_idle,
  ap_ready,
  istream_s_dout,
  istream_s_empty_n,
  istream_s_read,
  istream_peek_empty_n,
  istream_peek_read,
  ostreams_s_din,
  ostreams_s_full_n,
  ostreams_s_write,
  ostreams_peek,
  scalar
);
  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_done;
  output ap_idle;
  output ap_ready;
  input [64:0] istream_s_dout;
  input istream_s_empty_n;
  output istream_s_read;
  input istream_peek_empty_n;
  output istream_peek_read;
  output [64:0] ostreams_s_din;
  input ostreams_s_full_n;
  output ostreams_s_write;
  input [64:0] ostreams_peek;
  input [31:0] scalar;
endmodule
"""
    )


@pytest.mark.usefixtures("options")
def test_upper_level_task_module() -> None:
    module = Module(
        files=[_TESTDATA_PATH / "UpperLevelTask.v"],
        is_trimming_enabled=False,
    )

    assert module.name == "UpperLevelTask"
    assert list(module.signals) == [
        "ap_done",
        "ap_idle",
        "ap_ready",
        "istream_s_read",
        "ostreams_s_write",
        "ap_rst_n_inv",
        "ap_CS_fsm",
        "ap_CS_fsm_state1",
        "istream_s_blk_n",
        "ap_CS_fsm_state2",
        "ostreams_s_blk_n",
        "ap_block_state1",
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
        "ap_ST_fsm_state1_blk",
        "ap_ST_fsm_state2_blk",
        "ap_ce_reg",
        "ap_rst_n_inv",
        "ap_done",
        "ap_idle",
        "ap_ready",
    ]
    assert module.params == {}
    assert "fsm_encoding" not in module.code

    assert (
        module.get_template_code()
        == """
module UpperLevelTask
(
  ap_clk,
  ap_rst_n,
  ap_start,
  ap_done,
  ap_idle,
  ap_ready,
  istream_s_dout,
  istream_s_empty_n,
  istream_s_read,
  istream_peek_dout,
  istream_peek_empty_n,
  istream_peek_read,
  ostreams_s_din,
  ostreams_s_full_n,
  ostreams_s_write,
  ostreams_peek,
  scalar
);
  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_done;
  output ap_idle;
  output ap_ready;
  input [64:0] istream_s_dout;
  input istream_s_empty_n;
  output istream_s_read;
  input [64:0] istream_peek_dout;
  input istream_peek_empty_n;
  output istream_peek_read;
  output [64:0] ostreams_s_din;
  input ostreams_s_full_n;
  output ostreams_s_write;
  input [64:0] ostreams_peek;
  input [31:0] scalar;
endmodule
"""
    )


@pytest.mark.usefixtures("options")
def test_add_ports_succeeds() -> None:
    module = Module(name="foo")
    assert module.ports == {}

    module.add_ports([IOPort("input", "bar", width=Width.create(233))])
    ports = module.ports
    assert list(ports) == ["bar"]
    assert str(ports["bar"]) == "input [232:0] bar;"

    module.add_ports([IOPort("input", "baz"), IOPort("output", "qux")])
    ports = module.ports
    assert list(ports) == ["bar", "baz", "qux"]
    assert str(ports["baz"]) == "input baz;"
    assert str(ports["qux"]) == "output qux;"


@pytest.mark.usefixtures("options")
def test_del_port_succeeds() -> None:
    module = Module(name="foo")
    module.add_ports([IOPort("input", "bar"), IOPort("input", "baz")])
    module.code  # TODO: support deleting added port

    module.del_port("baz")

    assert list(module.ports) == ["bar"]
    assert "baz" not in module.code
    assert "," not in module.code


@pytest.mark.usefixtures("options")
def test_del_nonexistent_port_fails() -> None:
    module = Module(name="foo")
    module.add_ports([IOPort("input", "bar")])

    with pytest.raises(ValueError, match="no port baz found in module foo"):
        module.del_port("baz")


@pytest.mark.usefixtures("options")
def test_add_comment_lines_succeeds() -> None:
    module = Module(name="foo")
    module.add_ports([IOPort("input", "bar")])

    module.add_comment_lines(["// pragma 123"])

    assert "// pragma 123" in [line.strip() for line in module.code.splitlines()]


@pytest.mark.usefixtures("options")
def test_add_invalid_comment_lines_fails() -> None:
    module = Module(name="foo")

    with pytest.raises(ValueError, match="line must start with `// `"):
        module.add_comment_lines(["not a comment"])

    with pytest.raises(ValueError, match="line must not contain newlines"):
        module.add_comment_lines(["// line with\n newline"])


@pytest.mark.usefixtures("options")
def test_add_signals_succeeds() -> None:
    module = Module(name="foo")
    assert module.signals == {}

    module.add_signals([Wire(name="bar", width=Width.create(233))])
    signals = module.signals
    assert list(signals) == ["bar"]
    assert str(signals["bar"]) == "wire [232:0] bar;"

    module.add_signals([Wire("baz"), Reg("qux")])
    signals = module.signals
    assert list(signals) == ["bar", "baz", "qux"]
    assert str(signals["baz"]) == "wire baz;"
    assert str(signals["qux"]) == "reg qux;"


@pytest.mark.usefixtures("options")
def test_del_signals_succeeds() -> None:
    module = Module(name="foo")
    module.add_signals([Wire("bar")])
    module.code  # TODO: support deleting added signals

    module.del_signals(prefix="bar")

    assert module.signals == {}


@pytest.mark.usefixtures("options")
def test_del_nonexistent_signal_succeeds() -> None:
    module = Module(name="foo")
    module.add_signals([Wire("bar")])

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
    module.code  # TODO: support deleting added params

    module.del_params(prefix="bar")

    assert module.params == {}
    assert module.code.count(";") == 1  # from `module foo();`


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
def test_del_instances_succeeds() -> None:
    module = Module(name="foo")
    module.add_instance(
        module_name="Bar",
        instance_name="bar",
        ports=[ast.PortArg("port", ast.Constant("42"))],
    )
    module.code  # TODO: support deleting added instances

    module.del_instances(prefix="Bar")

    assert "bar" not in module.code


@pytest.mark.usefixtures("options")
def test_del_nonexistent_instance_succeeds() -> None:
    module = Module(name="foo")

    module.del_instances(prefix="Bar")


@pytest.mark.usefixtures("options")
def test_add_logics_succeeds() -> None:
    module = Module(name="foo")

    module.add_logics([Assign("bar", "0")])
    module.add_logics([Always(CLK_SENS_LIST, "begin\nend")])

    assert "assign bar = 0;" in module.code
    assert "always @(posedge ap_clk) begin" in module.code


@pytest.mark.usefixtures("options")
def test_del_logics_succeeds() -> None:
    module = Module(name="foo")
    module.add_logics(
        [
            Assign("bar", "0"),
            Always(CLK_SENS_LIST, "begin\nend"),
        ]
    )
    module.code  # TODO: support deleting added logics

    module.del_logics()

    assert "assign" not in module.code
    assert "always" not in module.code


@pytest.mark.usefixtures("options")
def test_del_nonexistent_logic_succeeds() -> None:
    module = Module(name="foo")

    module.del_logics()


@pytest.mark.usefixtures("options")
def test_add_rs_pragmas_succeeds() -> None:
    module = Module(name="foo")
    module.add_ports(
        [
            IOPort("input", HANDSHAKE_CLK),
            IOPort("input", HANDSHAKE_RST_N),
        ]
    )

    module.add_rs_pragmas()

    assert "(* RS_CLK *)input ap_clk;" in module.code
    assert '(* RS_RST = "ff" *)input ap_rst_n;' in module.code


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


def test_ap_clk_gets_rs_clk_pragma() -> None:
    pragma = _get_rs_pragma("ap_clk")

    assert str(pragma) == "(* RS_CLK *)"


def test_ap_rst_n_gets_rs_rst_pragma() -> None:
    pragma = _get_rs_pragma("ap_rst_n")

    assert str(pragma) == '(* RS_RST = "ff" *)'


def test_interrupt_gets_rs_ff_pragma() -> None:
    pragma = _get_rs_pragma("interrupt")

    assert str(pragma) == '(* RS_FF = "interrupt" *)'


def test_m_axi_awvalid_gets_rs_hs_pragma() -> None:
    pragma = _get_rs_pragma("m_axi_foo_AWVALID")

    assert str(pragma) == '(* RS_HS = "m_axi_foo_AW.valid" *)'


def test_m_axi_awaddr_gets_rs_hs_pragma() -> None:
    pragma = _get_rs_pragma("m_axi_foo_AWADDR")

    assert str(pragma) == '(* RS_HS = "m_axi_foo_AW.data" *)'


def test_axis_gets_rs_hs_pragma() -> None:
    pragma = _get_rs_pragma("foo_TDATA")

    assert str(pragma) == '(* RS_HS = "foo.data" *)'


def test_unknown_port_gets_none_rs_pragma() -> None:
    pragma = _get_rs_pragma("foo")

    assert pragma is None
