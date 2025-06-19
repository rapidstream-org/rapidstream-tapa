"""Graphir conversion utilities."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import re
from pathlib import Path

from pyverilog.vparser.ast import (
    And,
    Constant,
    Divide,
    Eq,
    GreaterEq,
    GreaterThan,
    Identifier,
    Land,
    LessEq,
    LessThan,
    Lor,
    Minus,
    Mod,
    Node,
    NotEq,
    Or,
    Plus,
    Power,
    Sll,
    Sra,
    Srl,
    Times,
    Uminus,
    UnaryOperator,
    Uplus,
    Xnor,
    Xor,
)

from tapa.graphir.types import (
    Expression,
    HierarchicalName,
    ModuleParameter,
    ModulePort,
    Range,
    Token,
    VerilogModuleDefinition,
)
from tapa.graphir_conversion.templates import FIFO_TEMPLATE
from tapa.instance import Port
from tapa.task import Task
from tapa.verilog.util import Pipeline
from tapa.verilog.xilinx.const import ISTREAM_SUFFIXES, OSTREAM_SUFFIXES
from tapa.verilog.xilinx.m_axi import M_AXI_PREFIX, M_AXI_SUFFIXES
from tapa.verilog.xilinx.module import Module

FIFO_PORT_PATTERN = r"([a-zA-Z_]\w*)\[(\d+)\]"

PORT_TYPE_MAPPING = {
    "input": ModulePort.Type.INPUT,
    "output": ModulePort.Type.OUTPUT,
    "inout": ModulePort.Type.INOUT,
}

_CTRL_S_AXI_PORT_DIR_RANGE = {
    "ACLK": (ModulePort.Type.INPUT, None),
    "ARESET": (ModulePort.Type.INPUT, None),
    "ACLK_EN": (ModulePort.Type.INPUT, None),
    "AWADDR": (
        ModulePort.Type.INPUT,
        Range(
            left=Expression(
                (
                    Token.new_id("C_S_AXI_ADDR_WIDTH"),
                    Token.new_lit("-"),
                    Token.new_lit("1"),
                )
            ),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "AWVALID": (ModulePort.Type.INPUT, None),
    "AWREADY": (ModulePort.Type.OUTPUT, None),
    "WDATA": (
        ModulePort.Type.INPUT,
        Range(
            left=Expression(
                (
                    Token.new_id("C_S_AXI_DATA_WIDTH"),
                    Token.new_lit("-"),
                    Token.new_lit("1"),
                )
            ),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "WSTRB": (
        ModulePort.Type.INPUT,
        Range(
            left=Expression(
                (
                    Token.new_id("C_S_AXI_DATA_WIDTH"),
                    Token.new_lit("/"),
                    Token.new_lit("8"),
                    Token.new_lit("-"),
                    Token.new_lit("1"),
                )
            ),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "WVALID": (ModulePort.Type.INPUT, None),
    "WREADY": (ModulePort.Type.OUTPUT, None),
    "BRESP": (
        ModulePort.Type.OUTPUT,
        Range(
            left=Expression((Token.new_lit("1"),)),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "BVALID": (ModulePort.Type.OUTPUT, None),
    "BREADY": (ModulePort.Type.INPUT, None),
    "ARADDR": (
        ModulePort.Type.INPUT,
        Range(
            left=Expression(
                (
                    Token.new_id("C_S_AXI_ADDR_WIDTH"),
                    Token.new_lit("-"),
                    Token.new_lit("1"),
                )
            ),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "ARVALID": (ModulePort.Type.INPUT, None),
    "ARREADY": (ModulePort.Type.OUTPUT, None),
    "RDATA": (
        ModulePort.Type.OUTPUT,
        Range(
            left=Expression(
                (
                    Token.new_id("C_S_AXI_DATA_WIDTH"),
                    Token.new_lit("-"),
                    Token.new_lit("1"),
                )
            ),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "RRESP": (
        ModulePort.Type.OUTPUT,
        Range(
            left=Expression((Token.new_lit("1"),)),
            right=Expression((Token.new_lit("0"),)),
        ),
    ),
    "RVALID": (ModulePort.Type.OUTPUT, None),
    "RREADY": (ModulePort.Type.INPUT, None),
    "interrupt": (ModulePort.Type.OUTPUT, None),
    "ap_start": (ModulePort.Type.OUTPUT, None),
    "ap_done": (ModulePort.Type.INPUT, None),
    "ap_ready": (ModulePort.Type.INPUT, None),
    "ap_idle": (ModulePort.Type.INPUT, None),
}

_CTRL_S_AXI_PARAMETERS = [
    ModuleParameter(
        name="C_S_AXI_ADDR_WIDTH",
        hierarchical_name=HierarchicalName.get_name("C_S_AXI_ADDR_WIDTH"),
        expr=Expression((Token.new_lit("6"),)),
        range=None,
    ),
    ModuleParameter(
        name="C_S_AXI_DATA_WIDTH",
        hierarchical_name=HierarchicalName.get_name("C_S_AXI_DATA_WIDTH"),
        expr=Expression((Token.new_lit("32"),)),
        range=None,
    ),
]


def ast_to_tokens(node: Node) -> list[Token]:
    """Convert a pyverilog AST node to a list of graphir tokens."""
    tokens = []

    if isinstance(node, Identifier):
        tokens.append(Token.new_id(node.name))

    elif isinstance(node, Constant):
        # TODO: refactor with pyslang
        elems = re.sub(r"([\[\]\(\)\{\}])", r" \1 ", node.value).split()
        for elem in elems:
            if (
                elem.isdigit()
                or elem
                in {
                    "~",
                    "-",
                    "+",
                    "*",
                    "/",
                    "%",
                    "**",
                    "==",
                    "!=",
                    ">",
                    "<",
                    ">=",
                    "<=",
                    "&&",
                    "||",
                    "&",
                    "|",
                    "^",
                    "~^",
                    "<<",
                    ">>",
                    ">>>",
                    "(",
                    ")",
                    "[",
                    "]",
                    "{",
                    "}",
                }
                or any(
                    item in elem
                    for item in ("'d", "'b", "'h", "'o", "'D", "'B", "'H", "'O")
                )
            ):
                # numeric literal or operator
                tokens.append(Token.new_lit(elem))
            else:
                # identifier
                tokens.append(Token.new_id(elem))

    elif isinstance(node, UnaryOperator):
        # e.g., 'ulnot', 'unot', etc.
        tokens.append(node.__class__.__name__.lower())
        tokens += ast_to_tokens(node.right)

    elif isinstance(
        node,
        (
            Plus,
            Minus,
            Times,
            Divide,
            Mod,
            Power,
            Eq,
            NotEq,
            GreaterThan,
            LessThan,
            GreaterEq,
            LessEq,
            Land,
            Lor,
            And,
            Or,
            Xor,
            Xnor,
            Uplus,
            Uminus,
            Sll,
            Srl,
            Sra,
        ),
    ):
        # Binary operators
        tokens += ast_to_tokens(node.left)
        tokens.append(get_operator_token(node))
        tokens += ast_to_tokens(node.right)

    # Generic fallback for other nodes
    elif node.children() is not None:
        for c in node.children():
            tokens += ast_to_tokens(c)

    return tokens


def get_operator_token(node: Node) -> Token:
    """Map AST classes to operator symbols."""
    mapping = {
        Plus: "+",
        Minus: "-",
        Times: "*",
        Divide: "/",
        Mod: "%",
        Power: "**",
        Eq: "==",
        NotEq: "!=",
        GreaterThan: ">",
        LessThan: "<",
        GreaterEq: ">=",
        LessEq: "<=",
        Land: "&&",
        Lor: "||",
        And: "&",
        Or: "|",
        Xor: "^",
        Xnor: "~^",
        Sll: "<<",
        Srl: ">>",
        Sra: ">>>",
    }
    return Token.new_lit(mapping[type(node)])


def get_task_graphir_ports(task_module: Module) -> list[ModulePort]:
    """Get the graphir ports from a task."""
    assert task_module.ports
    ports = []
    for name, port in task_module.ports.items():
        if port.width:
            port_range = Range(
                left=Expression((Token.new_lit(port.width.msb),)),
                right=Expression((Token.new_lit(port.width.lsb),)),
            )
            assert port_range.left, type(port.width.msb)
        else:
            port_range = None
        ports.append(
            ModulePort(
                name=name,
                hierarchical_name=HierarchicalName.get_name(port.name),
                type=PORT_TYPE_MAPPING[port.direction],
                range=port_range,
            )
        )
    return ports


def get_task_graphir_parameters(task_module: Module) -> list[ModuleParameter]:
    """Get the graphir parameters from a task."""
    if not task_module.params:
        return []
    graphir_params = []
    for name, param in task_module.params.items():
        expr = Expression(tuple(ast_to_tokens(param.value)))
        graphir_params.append(
            ModuleParameter(
                name=name,
                hierarchical_name=HierarchicalName.get_name(param.name),
                expr=expr,
                range=None,
            )
        )
    return graphir_params


def get_child_port_connection_mapping(
    task_port: Port,
    task_module: Module,
    arg: str,
    idx: int | None,
) -> dict[str, str]:
    """Get child task port and slot port mapping.

    Given a child task port and its arg, find all related ports in the task module based
    on cat. Return a mapping from the child module port name to the connected parent
    slot port name. This is for inferring the slot port range and direction from its
    connected child port.
    """
    matching_ports = {}
    if task_port.cat.is_scalar:
        matching_ports[task_port.name] = arg

    elif task_port.cat.is_istream or task_port.cat.is_istreams:
        for suffix in ISTREAM_SUFFIXES:
            full_port_name = (
                task_port.name if idx is None else f"{task_port.name}_{idx}"
            )
            module_port = task_module.get_port_of(full_port_name, suffix)
            matching_ports[module_port.name] = get_stream_port_name(arg, suffix)

    elif task_port.cat.is_ostream or task_port.cat.is_ostreams:
        for suffix in OSTREAM_SUFFIXES:
            full_port_name = (
                task_port.name if idx is None else f"{task_port.name}_{idx}"
            )
            module_port = task_module.get_port_of(full_port_name, suffix)
            matching_ports[module_port.name] = get_stream_port_name(arg, suffix)

    elif task_port.cat.is_mmap:
        # add offset mapping
        # note that offset port is pipelined through fsm
        matching_ports[f"{task_port.name}_offset"] = f"{arg}_offset"
        for suffix in M_AXI_SUFFIXES:
            m_axi_port_name = get_m_axi_port_name(task_port.name, suffix)
            if m_axi_port_name in task_module.ports:
                matching_ports[m_axi_port_name] = get_m_axi_port_name(arg, suffix)

    else:
        msg = (
            f"Unknown port type for port {task_port.name}, "
            f"category {task_port.cat}, arg {arg}."
        )
        raise ValueError(msg)

    return matching_ports


def get_stream_port_name(task_port_name: str, suffix: str) -> str:
    """Get the stream port name from the task port name and suffix."""
    return f"{task_port_name}{suffix}"


def get_m_axi_port_name(task_port_name: str, suffix: str) -> str:
    """Get the m_axi port name from the task port name and suffix."""
    return f"{M_AXI_PREFIX}{task_port_name}{suffix}"


def get_task_arg_table(
    task: Task,
) -> dict[str, dict[str, Pipeline]]:
    """Build arg table for fsm pipeline signals.

    The upper key is instance name, the lower key is the arg name.
    """
    arg_table: dict[str, dict[str, Pipeline]] = {}
    for instance in task.instances:
        inst_table: dict[str, Pipeline] = {}
        for arg in instance.args:
            if not arg.cat.is_stream:
                # For mmap ports, the scalar port is the offset.
                upper_name = (
                    f"{arg.name}_offset"
                    if arg.cat.is_sync_mmap or arg.cat.is_async_mmap
                    else arg.name
                )
                id_name = "64'd0" if arg.chan_count is not None else upper_name
                q = Pipeline(
                    name=instance.get_instance_arg(id_name),
                )
                inst_table[arg.name] = q
        arg_table[instance.name] = inst_table
    return arg_table


def get_verilog_definition_from_tapa_module(
    module: Module, code: str | None = None
) -> VerilogModuleDefinition:
    """Convert a Tapa Module to a VerilogModuleDefinition."""
    return VerilogModuleDefinition(
        name=module.name,
        hierarchical_name=HierarchicalName.get_name(module.name),
        parameters=tuple(get_task_graphir_parameters(module)),
        ports=tuple(get_task_graphir_ports(module)),
        verilog=code if code else module.code,
        submodules_module_names=(),
    )


def get_ctrl_s_axi_def(top: Task, content: str) -> VerilogModuleDefinition:
    """Get control_s_axi module definition."""
    ports = [
        ModulePort(
            name=port_name,
            hierarchical_name=HierarchicalName.get_name(port_name),
            type=port_type,
            range=port_range,
        )
        for port_name, (port_type, port_range) in _CTRL_S_AXI_PORT_DIR_RANGE.items()
    ]
    for port_name, port in top.ports.items():
        ctrl_s_axi_port_name = (
            port_name if not port.cat.is_mmap else f"{port_name}_offset"
        )
        ports.append(
            ModulePort(
                name=ctrl_s_axi_port_name,
                hierarchical_name=HierarchicalName.get_name(ctrl_s_axi_port_name),
                type=ModulePort.Type.OUTPUT,  # Control ports are inputs
                range=Range(
                    left=Expression((Token.new_lit("63"),)),
                    right=Expression((Token.new_lit("0"),)),
                ),  # No range for control ports
            )
        )
    return VerilogModuleDefinition(
        name="VecAdd_control_s_axi",
        hierarchical_name=HierarchicalName.get_name("VecAdd_control_s_axi"),
        parameters=tuple(_CTRL_S_AXI_PARAMETERS),
        ports=tuple(ports),
        verilog=content,
        submodules_module_names=(),
    )


def get_fifo_def() -> VerilogModuleDefinition:
    """Get fifo module definition."""
    return VerilogModuleDefinition(
        name="fifo",
        hierarchical_name=HierarchicalName.get_name("fifo"),
        parameters=(
            ModuleParameter(
                name="DATA_WIDTH",
                hierarchical_name=HierarchicalName.get_name("DATA_WIDTH"),
                expr=Expression((Token.new_lit("32"),)),
                range=None,
            ),
            ModuleParameter(
                name="ADDR_WIDTH",
                hierarchical_name=HierarchicalName.get_name("ADDR_WIDTH"),
                expr=Expression((Token.new_lit("5"),)),
                range=None,
            ),
            ModuleParameter(
                name="DEPTH",
                hierarchical_name=HierarchicalName.get_name("DEPTH"),
                expr=Expression((Token.new_lit("32"),)),
                range=None,
            ),
        ),
        ports=(
            ModulePort(
                name="clk",
                hierarchical_name=HierarchicalName.get_name("clk"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="reset",
                hierarchical_name=HierarchicalName.get_name("rst"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="if_full_n",
                hierarchical_name=HierarchicalName.get_name("if_full_n"),
                type=ModulePort.Type.OUTPUT,
                range=None,
            ),
            ModulePort(
                name="if_write_ce",
                hierarchical_name=HierarchicalName.get_name("if_write_ce"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="if_write",
                hierarchical_name=HierarchicalName.get_name("if_write"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="if_din",
                hierarchical_name=HierarchicalName.get_name("if_din"),
                type=ModulePort.Type.INPUT,
                range=Range(
                    left=Expression(
                        (
                            Token.new_id("DATA_WIDTH"),
                            Token.new_lit("-"),
                            Token.new_lit("1"),
                        )
                    ),
                    right=Expression((Token.new_lit("0"),)),
                ),
            ),
            ModulePort(
                name="if_empty_n",
                hierarchical_name=HierarchicalName.get_name("if_empty_n"),
                type=ModulePort.Type.OUTPUT,
                range=None,
            ),
            ModulePort(
                name="if_read_ce",
                hierarchical_name=HierarchicalName.get_name("if_read_ce"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="if_read",
                hierarchical_name=HierarchicalName.get_name("if_read"),
                type=ModulePort.Type.INPUT,
                range=None,
            ),
            ModulePort(
                name="if_dout",
                hierarchical_name=HierarchicalName.get_name("if_dout"),
                type=ModulePort.Type.OUTPUT,
                range=Range(
                    left=Expression(
                        (
                            Token.new_id("DATA_WIDTH"),
                            Token.new_lit("-"),
                            Token.new_lit("1"),
                        )
                    ),
                    right=Expression((Token.new_lit("0"),)),
                ),
            ),
        ),
        verilog=FIFO_TEMPLATE,
        submodules_module_names=(),
    )


def get_top_fsm_def(
    fsm_name: str,
    fsm_file: Path,
) -> VerilogModuleDefinition:
    """Get top FSM module definition."""
    with open(fsm_file, encoding="utf-8") as f:
        content = f.read()
    module = Module(
        (fsm_file,),
        True,
        name=fsm_name,
    )
    return get_verilog_definition_from_tapa_module(module, content)
