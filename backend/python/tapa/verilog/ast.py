"""Verilog abstract syntax tree (AST) nodes and utilies.

This module is designed as a drop-in replacement for `pyverilog.vparser.ast`
with convenient utilities.
"""

from typing import Iterable, Optional, Tuple, Type, Union

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
  if isinstance(statements, Node):
    statements = (statements,)
  return Block(statements=tuple(statements), **kwargs)


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


def make_port_arg(port: str, arg: Union[str, Node]) -> PortArg:
  """Make PortArg from port and arg names.

  Args:
      port: Port name.
      arg: Arg name (will be used to construct an Identifier) or Node.

  Returns:
      PortArg: `.port(arg)` in Verilog.
  """
  return PortArg(portname=port,
                 argname=arg if isinstance(arg, Node) else Identifier(arg))


def make_if_with_block(
    cond: Node,
    true: Union[Iterable[Node], Node],
    false: Union[Iterable[Node], Node, None] = None) -> IfStatement:
  if not (false is None or isinstance(false, IfStatement)):
    false = make_block(false)
  return IfStatement(cond=cond,
                     true_statement=make_block(true),
                     false_statement=false)


def make_case_with_block(
    comp: Node,
    cases: Iterable[Tuple[Union[Iterable[Node], Node], Union[Iterable[Node],
                                                             Node]]],
) -> CaseStatement:
  return CaseStatement(
      comp=comp,
      caselist=(Case(
          cond=(cond,) if isinstance(cond, Node) else cond,
          statement=make_block(statement),
      ) for cond, statement in cases),
  )


def make_width(width: int) -> Optional[Width]:
  if width > 0:
    return Width(msb=IntConst(width - 1), lsb=IntConst(0))
  return None


def make_int(value: int, width: int = 0) -> IntConst:
  return IntConst(f"{width or value.bit_length() or 1}'d{value}")
