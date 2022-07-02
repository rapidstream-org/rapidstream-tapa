import copy
from functools import lru_cache
from typing import Dict, List, Optional, Union

from tapa.common.base import Base
from tapa.common.constant import Constant
from tapa.common.external_port import ExternalPort
from tapa.common.interconnect_instance import InterconnectInstance
from tapa.common.task_definition import TaskDefinition


class TaskInstance(Base):
  """TAPA task instance.

  Attributes:
    idx: The index of this task as nested in its parent.
  """

  def __init__(self, idx: int, *args, **kwargs):
    """Construct a TAPA task instance.

    Args:
      idx: The index of this task as nested in its parent.
    """
    super(TaskInstance, self).__init__(*args, **kwargs)
    self.idx = idx

  @lru_cache(None)
  def get_external_port(self, name: str) -> Optional[ExternalPort]:
    """Returns the external port of this task instance."""
    if name in self.get_external_ports():
      return self.get_external_ports()[name]
    else:
      return None

  @lru_cache(None)
  def get_external_ports(self) -> Dict[str, ExternalPort]:
    """Returns the external ports of this task instance."""
    return {
        p['name']: ExternalPort(p['name'], p, self)
        for p in self.definition.obj['ports']
    } if self.definition.get_level() == TaskDefinition.Level.TOP else {}
    # Non-top tasks have no external ports

  @lru_cache(None)
  def get_interconnect_inst(self, name: str) -> Optional[InterconnectInstance]:
    """Returns the local interconnect of this task instance."""

    if self.definition.get_level() == TaskDefinition.Level.LEAF:
      return None
    inter_def = self.definition.get_interconnect_def(name)
    return InterconnectInstance(name, None, self,
                                inter_def) if inter_def else None

  @lru_cache(None)
  def get_interconnect_insts(self) -> List[InterconnectInstance]:
    """Returns the local interconnects of this task instance."""

    return [
        self.get_interconnect_inst(inter_def.name)
        for inter_def in self.definition.get_interconnect_defs()
    ] if self.definition.get_level() != TaskDefinition.Level.LEAF else []

  @lru_cache(None)
  def get_in_scope_interconnect_or_port(
      self, name: str) -> Optional[Union[InterconnectInstance, ExternalPort]]:
    """Returns the interconnect or port that has `name` in this task's scope.

    The interconnect that has `name` might be in this local scope either
    through ports or instantiated locally.  This function is to lookup the
    original interconnect or external port recursively.
    
    Args:
      name: Name of the interconnect or port in the local scope.
      
    Returns:
      The corresponding interconnect or external port.
    """

    inter_inst = self.get_interconnect_inst(name)

    if inter_inst is not None:
      return inter_inst

    # If not instantiated locally
    if self.definition.get_level() != TaskDefinition.Level.TOP:
      # 1. lookup its parents if this is not the top level task
      insts_all = self.parent.definition.obj['tasks']
      insts_self = insts_all[self.definition.name]
      inst = insts_self[self.idx]
      arg = inst['args'][name]
      parent_name = arg['arg']

      # FIXME: constants should be speficied explicitly in tapacc
      if parent_name.startswith("64'd"):
        # this is an int constant
        return Constant(parent_name, arg)
      else:
        # this is passed from its parent
        return self.parent.get_in_scope_interconnect_or_port(parent_name)
    else:
      # 2. or if this task is the top level task, get the external port
      return self.get_external_port(name)

  @lru_cache(None)
  def recursive_get_interconnect_insts(self) -> List[InterconnectInstance]:
    """Returns all local interconnects of this task and its subtasks."""

    if self.definition.get_level() == TaskDefinition.Level.LEAF:
      return []
    insts = self.get_interconnect_insts()
    for task_inst in self.get_subtasks_insts():
      insts.extend(task_inst.recursive_get_interconnect_insts())
    return insts

  @lru_cache(None)
  def get_subtasks_insts(self) -> List['TaskInstance']:
    """Returns the subinstances of this task instance."""

    if self.definition.get_level() == TaskDefinition.Level.LEAF:
      return []
    insts = []
    for task_def in self.definition.get_subtask_defs():
      task_instances = self.definition.get_subtask_instantiations(task_def.name)
      for idx, inst in enumerate(task_instances):
        insts.append(
            TaskInstance(idx, f'{task_def.name}_{idx}', inst, self, task_def))
    return insts

  @lru_cache(None)
  def get_leaf_tasks_insts(self) -> List['TaskInstance']:
    """Returns the instances of the leaf tasks of under this instance."""

    if self.definition.get_level() == TaskDefinition.Level.LEAF:
      return [self]
    insts = []
    for task_inst in self.get_subtasks_insts():
      insts.extend(task_inst.get_leaf_tasks_insts())
    return insts

  def to_dict(self, interconnect_global_name: bool = False) -> Dict:
    """Transform the task instance object to its JSON description.
    
    Args:
      interconnect_global_name:
        Use the global names as the name of the interconnects.

    Returns:
      The dict as in the graph JSON description.
    """
    obj = super(TaskInstance, self).to_dict()

    if interconnect_global_name:
      # Transform args to their global names
      for name, arg in obj['args'].items():
        inter_inst = self.get_in_scope_interconnect_or_port(name)
        if inter_inst:
          arg['arg'] = inter_inst.global_name

    return obj
