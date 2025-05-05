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
from tapa.task import Task

_PORT_TYPE_MAPPING = {
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
                type=_PORT_TYPE_MAPPING[port.direction],
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
