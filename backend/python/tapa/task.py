import collections
import enum
from typing import Tuple, Union

import tapa.verilog.xilinx
from .instance import Instance, Port


class Task:
  """Describes a TAPA task.

  Attributes:
    level: Task.Level, upper or lower.
    name: str, name of the task, function name as defined in the source code.
    code: str, HLS C++ code of this task.
    tasks: A dict mapping child task names to json instance description objects.
    fifos: A dict mapping child fifo names to json FIFO description objects.
    ports: A dict mapping port names to Port objects for the current task.
    module: rtl.Module, should be attached after RTL code is generated.

  Properties:
    is_upper: bool, True if this task is an upper-level task.
    is_lower: bool, True if this task is an lower-level task.
    instances: A tuple of Instance objects, children instances of this task.
  """

  class Level(enum.Enum):
    LOWER = 0
    UPPER = 1

  def __init__(self, **kwargs):
    level: Union[Task.Level, str] = kwargs.pop('level')
    if isinstance(level, str):
      if level == 'lower':
        level = Task.Level.LOWER
      elif level == 'upper':
        level = Task.Level.UPPER
    if not isinstance(level, Task.Level):
      raise TypeError('unexpected `level`: ' + level)
    self.level = level
    self.name: str = kwargs.pop('name')
    self.code: str = kwargs.pop('code')
    self.tasks = collections.OrderedDict()
    self.fifos = collections.OrderedDict()
    if self.is_upper:
      self.tasks = collections.OrderedDict(
          sorted((item for item in kwargs.pop('tasks').items()),
                 key=lambda x: x[0]))
      self.fifos = collections.OrderedDict(
          sorted((item for item in kwargs.pop('fifos').items()),
                 key=lambda x: x[0]))
      self.ports = {i.name: i for i in map(Port, kwargs.pop('ports'))}
    self.module = tapa.verilog.xilinx.Module('')
    self._instances = ()

  @property
  def is_upper(self) -> bool:
    return self.level == Task.Level.UPPER

  @property
  def is_lower(self) -> bool:
    return self.level == Task.Level.LOWER

  @property
  def instances(self) -> Tuple[Instance, ...]:
    if self._instances:
      return self._instances
    raise ValueError('instances not populated yet')

  @instances.setter
  def instances(self, instances: Tuple[Instance, ...]) -> None:
    self._instances = instances
