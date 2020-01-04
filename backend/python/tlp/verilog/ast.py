"""Verilog abstract syntax tree (AST) nodes and utilies.

This module is designed as a drop-in replacement for `pyverilog.vparser.ast`
with convenient utilities.
"""

from collections import abc
from typing import Iterable, Type, Union

# pylint: disable=unused-import
from pyverilog.vparser.ast import (
    Node,
    Source,
    Description,
    ModuleDef,
    Paramlist,
    Portlist,
    Port,
    Width,
    Length,
    Dimensions,
    Identifier,
    Value,
    Constant,
    IntConst,
    FloatConst,
    StringConst,
    Variable,
    Input,
    Output,
    Inout,
    Tri,
    Wire,
    Reg,
    Integer,
    Real,
    Genvar,
    Ioport,
    Parameter,
    Localparam,
    Supply,
    Decl,
    Concat,
    LConcat,
    Repeat,
    Partselect,
    Pointer,
    Lvalue,
    Rvalue,
    Operator,
    UnaryOperator,
    Uplus,
    Uminus,
    Ulnot,
    Unot,
    Uand,
    Unand,
    Uor,
    Unor,
    Uxor,
    Uxnor,
    Power,
    Times,
    Divide,
    Mod,
    Plus,
    Minus,
    Sll,
    Srl,
    Sla,
    Sra,
    LessThan,
    GreaterThan,
    LessEq,
    GreaterEq,
    Eq,
    NotEq,
    Eql,
    NotEql,
    And,
    Xor,
    Xnor,
    Or,
    Land,
    Lor,
    Cond,
    Assign,
    Always,
    AlwaysFF,
    AlwaysComb,
    AlwaysLatch,
    SensList,
    Sens,
    Substitution,
    BlockingSubstitution,
    NonblockingSubstitution,
    IfStatement,
    ForStatement,
    WhileStatement,
    CaseStatement,
    CasexStatement,
    UniqueCaseStatement,
    Case,
    Block,
    Initial,
    EventStatement,
    WaitStatement,
    ForeverStatement,
    DelayStatement,
    InstanceList,
    Instance,
    ParamArg,
    PortArg,
    Function,
    FunctionCall,
    Task,
    TaskCall,
    GenerateStatement,
    SystemCall,
    IdentifierScopeLabel,
    IdentifierScope,
    Pragma,
    PragmaEntry,
    Disable,
    ParallelBlock,
    SingleStatement,
    EmbeddedCode,
)


def make_block(statements: Union[Iterable[Node], Node], **kwargs) -> Block:
  if not isinstance(statements, abc.Iterable):
    statements = (statements,)
  return Block(statements=statements, **kwargs)


def make_operation(operator: Type[Operator], nodes: Iterable[Node]) -> Operator:
  """Make a multi-operand operation node out of an iterable of Nodes.

  Note that the nodes appears in the reverse order of the iterable (to avoid
  unnecessary parentheses).

  Args:
    op: Operator.
    nodes: Iterable of Node. If empty, raises StopIteration.

  Returns:
    Operator of the result.
  """
  iterator = iter(nodes)
  node = next(iterator)
  try:
    return operator(make_operation(operator, iterator), node)
  except StopIteration:
    return node


def make_port_arg(port: str, arg: str) -> PortArg:
  """Make PortArg from port and arg names.

  Args:
      port: Port name.
      arg: Arg name, will be used to construct an Identifier.

  Returns:
      PortArg: `.port(arg)` in Verilog.
  """
  return PortArg(portname=port, argname=Identifier(name=arg))
