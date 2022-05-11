from enum import Enum
from functools import lru_cache
from re import sub
from typing import Dict, List, Optional

from tapa.common.base import Base
from tapa.common.interconnect_definition import InterconnectDefinition


class TaskDefinition(Base):
  """TAPA task definition."""

  class Level(Enum):
    TOP = 1
    UPPER = 2
    LEAF = 3

  @lru_cache(None)
  def get_level(self) -> Level:
    """Returns the level of the task in the task graph."""
    if self.obj['level'] == 'lower':
      return TaskDefinition.Level.LEAF
    elif self.parent.get_top_task_def().name != self.name:
      return TaskDefinition.Level.UPPER
    else:
      return TaskDefinition.Level.TOP

  @lru_cache(None)
  def get_interconnect_def(self, name) -> Optional[InterconnectDefinition]:
    """Returns the interconnect named `name` defined in this task."""
    # If not defined
    if name not in self.obj['fifos']:
      return None

    # If is an argument
    port_name = sub(r'\[\d+\]$', r'', name)
    # FIXME: tapacc should output port name with []
    if port_name in map(lambda p: p['name'], self.obj['ports']):
      return None

    return InterconnectDefinition(name, self.obj['fifos'][name], self)

  @lru_cache(None)
  def get_interconnect_defs(self) -> List[InterconnectDefinition]:
    """Returns all local interconnect defined in this task."""
    return [
        self.get_interconnect_def(name)
        for name in self.obj['fifos'].keys()
        if self.get_interconnect_def(name) is not None
    ] if self.get_level() != TaskDefinition.Level.LEAF else []

  @lru_cache(None)
  def get_subtask_defs(self) -> List['TaskDefinition']:
    """Returns the task definitions of the subtasks of this task."""
    return [
        self.parent.get_task_def(name) for name in self.obj['tasks'].keys()
    ] if self.get_level() != TaskDefinition.Level.LEAF else []

  @lru_cache(None)
  def get_subtask_instantiations(self, name: str) -> List[Dict]:
    """Return the instantiations of subtask with `name`."""
    return self.obj['tasks'][name]
