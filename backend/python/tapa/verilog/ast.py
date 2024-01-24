"""Verilog abstract syntax tree (AST) nodes and utilies.

This module is designed as a drop-in replacement for `pyverilog.vparser.ast`
with convenient utilities.
"""

from typing import Iterable, Optional, Tuple, Type, Union

# pylint: disable=unused-import
from pyverilog.vparser.ast import (
    Always,
    AlwaysComb,
    AlwaysFF,
    AlwaysLatch,
    And,
    Assign,
    Block,
    BlockingSubstitution,
    Case,
    CaseStatement,
    CasexStatement,
    Concat,
    Cond,
    Constant,
    Decl,
    DelayStatement,
    Description,
    Dimensions,
    Disable,
    Divide,
    EmbeddedCode,
    Eq,
    Eql,
    EventStatement,
    FloatConst,
    ForeverStatement,
    ForStatement,
    Function,
    FunctionCall,
    GenerateStatement,
    Genvar,
    GreaterEq,
    GreaterThan,
    Identifier,
    IdentifierScope,
    IdentifierScopeLabel,
    IfStatement,
    Initial,
    Inout,
    Input,
    Instance,
    InstanceList,
    IntConst,
    Integer,
    Ioport,
    Land,
    LConcat,
    Length,
    LessEq,
    LessThan,
    Localparam,
    Lor,
    Lvalue,
    Minus,
    Mod,
    ModuleDef,
    Node,
    NonblockingSubstitution,
    NotEq,
    NotEql,
    Operator,
    Or,
    Output,
    ParallelBlock,
    ParamArg,
    Parameter,
    Paramlist,
    Partselect,
    Plus,
    Pointer,
    Port,
    PortArg,
    Portlist,
    Power,
    Pragma,
    PragmaEntry,
    Real,
    Reg,
    Repeat,
    Rvalue,
    Sens,
    SensList,
    SingleStatement,
    Sla,
    Sll,
    Source,
    Sra,
    Srl,
    StringConst,
    Substitution,
    Supply,
    SystemCall,
    Task,
    TaskCall,
    Times,
    Tri,
    Uand,
    Ulnot,
    Uminus,
    Unand,
    UnaryOperator,
    UniqueCaseStatement,
    Unor,
    Unot,
    Uor,
    Uplus,
    Uxnor,
    Uxor,
    Value,
    Variable,
    WaitStatement,
    WhileStatement,
    Width,
    Wire,
    Xnor,
    Xor,
)

# Please keep `make_*` functions sorted by their name.


def make_block(statements: Union[Iterable[Node], Node], **kwargs) -> Block:
  if isinstance(statements, Node):
    statements = (statements,)
  return Block(statements=tuple(statements), **kwargs)


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


def make_if_with_block(
    cond: Node,
    true: Union[Iterable[Node], Node],
    false: Union[Iterable[Node], Node, None] = None) -> IfStatement:
  if not (false is None or isinstance(false, IfStatement)):
    false = make_block(false)
  return IfStatement(cond=cond,
                     true_statement=make_block(true),
                     false_statement=false)


def make_int(value: int, width: int = 0) -> IntConst:
  return IntConst(f"{width or value.bit_length() or 1}'d{value}")


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


def make_width(width: int) -> Optional[Width]:
  if width > 0:
    return Width(msb=IntConst(width - 1), lsb=IntConst(0))
  return None
