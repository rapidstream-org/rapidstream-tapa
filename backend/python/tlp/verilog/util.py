from typing import Iterator, Optional, Union

from tlp.verilog import ast

REGISTER_LEVEL = 3


class Pipeline:

  def __init__(self, name: str, width: Optional[int] = None):
    self._ids = tuple(
        ast.Identifier(f'{name}_q%d' % i) for i in range(REGISTER_LEVEL + 1))
    self._width: Optional[ast.Width] = width and ast.make_width(width)

  def __getitem__(self, idx) -> ast.Identifier:
    return self._ids[idx]

  def __iter__(self) -> Iterator[ast.Identifier]:
    return iter(self._ids)

  @property
  def signals(self) -> Iterator[Union[ast.Reg, ast.Wire, ast.Pragma]]:
    yield ast.Wire(name=self[0].name, width=self._width)
    for x in self[1:]:
      yield ast.Pragma(ast.PragmaEntry('dont_touch = "yes"'))
      yield ast.Reg(name=x.name, width=self._width)


def generate_peek_ports(verilog, port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in verilog.ISTREAM_SUFFIXES[:1]:
    yield ast.make_port_arg(port='tlp_' + port + '_peek', arg=arg + suffix)
