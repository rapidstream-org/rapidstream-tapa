import enum
from typing import Dict, Iterator, Union

from tlp.verilog import ast

from .task import Task


class Instance:
  """Instance of a child Task in an upper-level task.

  A task can be instantiated multiple times in the same upper-level task.
  Each object of this class corresponds to such a instance of a task.

  Attributes:
    task: Task, corresponding task of this instance.
    instance_id: int, index of the instance of the same task.
    step: int, bulk-synchronous step when instantiated.
    args: a dict mapping arg names to Arg.

  Properties:
    name: str, instance name, unique in the parent module.

  """

  class Arg:

    class Cat(enum.Enum):
      INPUT = 1 << 0
      OUTPUT = 1 << 1
      SCALAR = 1 << 2
      STREAM = 1 << 3
      MMAP = 1 << 4
      ASYNC = 1 << 5
      ASYNC_MMAP = MMAP | ASYNC
      ISTREAM = STREAM | INPUT
      OSTREAM = STREAM | OUTPUT

    def __init__(self, cat: Union[str, Cat], port: str):
      if isinstance(cat, str):
        self.cat = {
            'istream': Instance.Arg.Cat.ISTREAM,
            'ostream': Instance.Arg.Cat.OSTREAM,
            'scalar': Instance.Arg.Cat.SCALAR,
            'mmap': Instance.Arg.Cat.MMAP,
            'async_mmap': Instance.Arg.Cat.ASYNC_MMAP
        }[cat]
      else:
        self.cat = cat
      self.port = port
      self.width = None

  def __init__(self, task: Task, verilog, instance_id: int, **kwargs):
    self.verilog = verilog
    self.task = task
    self.instance_id = instance_id
    self.step = kwargs.pop('step')
    self.args = {k: Instance.Arg(**v) for k, v in kwargs.pop('args').items()
                }  # type: Dict[str, Instance.Arg]

  @property
  def name(self) -> str:
    return '{0.task.name}_{0.instance_id}'.format(self)

  @property
  def is_autorun(self) -> bool:
    return self.step < 0

  @property
  def start(self) -> ast.Identifier:
    """The handshake start signal.

    This signal is asserted until the ready signal is asserted when the instance
    of task starts.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.name + '_' + self.verilog.HANDSHAKE_START)

  @property
  def done_pulse(self) -> ast.Identifier:
    """The handshake done signal.

    This signal is asserted for 1 cycle when the instance of task finishes.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.name + '_' + self.verilog.HANDSHAKE_DONE)

  @property
  def done_reg(self) -> ast.Identifier:
    """Registered handshake done signal.

    This signal is asserted 1 cycle after the instance of task finishes and is
    kept until reset or next time start signal is asserted.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.done_pulse.name + '_reg')

  @property
  def done(self) -> ast.Identifier:
    """Whether this instance has done.

    This signal is asserted as soon as the instance finishes and is kept until
    reset.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.name + '_is_done')

  @property
  def idle(self) -> ast.Identifier:
    """Whether this isntance is idle."""
    return ast.Identifier(self.name + '_' + self.verilog.HANDSHAKE_IDLE)

  @property
  def ready(self) -> ast.Identifier:
    """Whether this isntance is ready to take new input."""
    return ast.Identifier(self.name + '_' + self.verilog.HANDSHAKE_READY)

  def get_signal(self, signal: str) -> ast.Identifier:
    if signal not in {'done', 'idle', 'ready'}:
      raise ValueError(
          'signal should be one of (done, idle, ready), got {}'.format(signal))
    return getattr(self, signal)

  @property
  def handshake_signals(self) -> Iterator[Union[ast.Wire, ast.Reg]]:
    """All handshake signals used for this instance.

    Yields:
      Union[ast.Wire, ast.Reg] of signals.
    """
    yield ast.Reg(name=self.start.name, width=None)
    if not self.is_autorun:
      yield from (ast.Wire(name=self.name + '_' + suffix, width=None)
                  for suffix in self.verilog.HANDSHAKE_OUTPUT_PORTS)
      yield ast.Wire(name=self.done.name, width=None)
      yield ast.Reg(name=self.done_reg.name, width=None)


class Port:

  def __init__(self, obj):
    self.cat = {
        'istream': Instance.Arg.Cat.ISTREAM,
        'ostream': Instance.Arg.Cat.OSTREAM,
        'scalar': Instance.Arg.Cat.SCALAR,
        'mmap': Instance.Arg.Cat.MMAP,
        'async_mmap': Instance.Arg.Cat.ASYNC_MMAP,
    }[obj['cat']]
    self.name = obj['name']
    self.ctype = obj['type']
    self.width = obj['width']
