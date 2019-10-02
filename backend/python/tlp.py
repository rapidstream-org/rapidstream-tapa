from typing import (
    TextIO,
    Union,
)

import enum
import json
import shutil
import subprocess


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


def clang_format(code: str) -> str:
  for version in range(10, 4, -1):
    clang_format_exe = shutil.which('clang-format-%d' % version)
    if clang_format_exe is not None:
      break
  else:
    clang_format_exe = shutil.which('clang-format')
  if clang_format_exe is not None:
    proc = subprocess.run([clang_format_exe],
                          input=code,
                          stdout=subprocess.PIPE,
                          universal_newlines=True)
    proc.check_returncode()
    return proc.stdout
  return code
