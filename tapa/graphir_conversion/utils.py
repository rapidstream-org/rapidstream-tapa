"""Graphir conversion utilities."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

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
)
from tapa.instance import Port
from tapa.task import Task
from tapa.verilog.xilinx.const import ISTREAM_SUFFIXES, OSTREAM_SUFFIXES
from tapa.verilog.xilinx.m_axi import M_AXI_PREFIX, M_AXI_SUFFIXES
from tapa.verilog.xilinx.module import Module

PORT_TYPE_MAPPING = {
    "input": ModulePort.Type.INPUT,
    "output": ModulePort.Type.OUTPUT,
    "inout": ModulePort.Type.INOUT,
}


def ast_to_tokens(node: Node) -> list[Token]:
    """Convert a pyverilog AST node to a list of graphir tokens."""
    tokens = []

    if isinstance(node, Identifier):
        tokens.append(Token.new_id(node.name))

    elif isinstance(node, Constant):
        tokens.append(Token.new_lit(node.value))

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


def get_task_graphir_ports(task: Task) -> list[ModulePort]:
    """Get the graphir ports from a task."""
    assert task.module.ports
    ports = []
    for name, port in task.module.ports.items():
        if port.width:
            port_range = Range(
                left=Expression(tuple(ast_to_tokens(port.width.msb))),
                right=Expression(tuple(ast_to_tokens(port.width.lsb))),
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


def get_task_graphir_parameters(task: Task) -> list[ModuleParameter]:
    """Get the graphir parameters from a task."""
    assert task.module.params
    graphir_params = []
    for name, param in task.module.params.items():
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


def get_leaf_port_connection_mapping(
    task_port: Port, task_module: Module, arg: str
) -> dict[str, str]:
    """Get leaf task port and slot port mapping.

    Given a leaf task port and its arg, find all related ports in the task module based
    on cat. Return a mapping from the leaf module port name to the connected slot port
    name.
    """
    matching_ports = {}
    if task_port.cat.is_scalar:
        matching_ports[task_port.name] = arg

    elif task_port.cat.is_istream:
        for suffix in ISTREAM_SUFFIXES:
            module_port = task_module.get_port_of(task_port.name, suffix)
            matching_ports[module_port.name] = get_stream_port_name(arg, suffix)

    elif task_port.cat.is_ostream:
        for suffix in OSTREAM_SUFFIXES:
            module_port = task_module.get_port_of(task_port.name, suffix)
            matching_ports[module_port.name] = get_stream_port_name(arg, suffix)

    elif task_port.cat.is_mmap:
        for suffix in M_AXI_SUFFIXES:
            m_axi_port_name = get_m_axi_port_name(task_port.name, suffix)
            if m_axi_port_name in task_module.ports:
                matching_ports[m_axi_port_name] = get_m_axi_port_name(arg, suffix)

    else:
        msg = f"Unknown port type for port {task_port.name}"
        raise ValueError(msg)

    return matching_ports


def get_stream_port_name(task_port_name: str, suffix: str) -> str:
    """Get the stream port name from the task port name and suffix."""
    return f"{task_port_name}{suffix}"


def get_m_axi_port_name(task_port_name: str, suffix: str) -> str:
    """Get the m_axi port name from the task port name and suffix."""
    return f"{M_AXI_PREFIX}{task_port_name}{suffix}"
