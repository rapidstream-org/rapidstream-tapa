import re
from typing import Iterator, Optional, Tuple, Union

from tapa.verilog import ast

__all__ = [
    'Pipeline',
    'match_array_name',
    'sanitize_array_name',
    'wire_name',
    'async_mmap_instance_name',
]


class Pipeline:

  def __init__(self, name: str, level: int, width: Optional[int] = None):
    self.level = level
    self._ids = tuple(
        ast.Identifier(f'{name}__q%d' % i) for i in range(level + 1))
    self._width: Optional[ast.Width] = width and ast.make_width(width)

  def __getitem__(self, idx) -> ast.Identifier:
    return self._ids[idx]

  def __iter__(self) -> Iterator[ast.Identifier]:
    return iter(self._ids)

  @property
  def signals(self) -> Iterator[Union[ast.Reg, ast.Wire, ast.Pragma]]:
    yield ast.Wire(name=self[0].name, width=self._width)
    for x in self[1:]:
      yield ast.Pragma(ast.PragmaEntry('keep = "true"'))
      yield ast.Reg(name=x.name, width=self._width)


def match_array_name(name: str) -> Optional[Tuple[str, int]]:
  match = re.fullmatch(r'(\w+)\[(\d+)\]', name)
  if match is not None:
    return match[1], int(match[2])
  return None


def sanitize_array_name(name: str) -> str:
  match = match_array_name(name)
  if match is not None:
    return f'{match[0]}_{match[1]}'
  return name


def wire_name(fifo: str, suffix: str) -> str:
  """Return the wire name of the fifo signals in generated modules.

  Args:
      fifo (str): Name of the fifo.
      suffix (str): One of the suffixes in ISTREAM_SUFFIXES or OSTREAM_SUFFIXES.

  Returns:
      str: Wire name of the fifo signal.
  """
  fifo = sanitize_array_name(fifo)
  if suffix.startswith('_'):
    suffix = suffix[1:]
  return f'{fifo}__{suffix}'


def async_mmap_instance_name(variable_name: str) -> str:
  return f'{variable_name}__m_axi'
