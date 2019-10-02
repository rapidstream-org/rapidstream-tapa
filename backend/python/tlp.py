from typing import (
    TextIO,
    Union,
)

import enum
import json


class Program:
  """Describes a TLP program."""

  def __init__(self, fp: TextIO):
    obj = json.load(fp)
    self.top = obj['top']  # type: str
    self.tasks = tuple(
        sorted((Task(name=name, **task) for name, task in obj['tasks'].items()),
               key=lambda task: task.name))


class Task:
  """Describes a TLP task."""

  class Level(enum.Enum):
    LOWER = 0
    UPPER = 1

  def __init__(self, **kwargs):
    level = kwargs.pop('level')  # type: Union[Task.Level, str]
    if isinstance(level, str):
      if level == 'lower':
        level = Task.Level.LOWER
      elif level == 'upper':
        level = Task.Level.UPPER
    if not isinstance(level, Task.Level):
      raise TypeError('unexpected `level`: ' + level)
    self.level = level
    self.name = kwargs.pop('name')  # type: str
    self.code = kwargs.pop('code')  # type: str
