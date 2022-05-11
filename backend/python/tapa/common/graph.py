from functools import lru_cache

from tapa.common.base import Base
from tapa.common.task_definition import TaskDefinition
from tapa.common.task_instance import TaskInstance


class Graph(Base):
  """TAPA task graph."""

  @lru_cache(None)
  def get_task_def(self, name: str) -> TaskDefinition:
    """Returns the task definition which is named `name`."""
    return TaskDefinition(name, self.obj['tasks'][name], self)

  @lru_cache(None)
  def get_top_task_def(self) -> TaskDefinition:
    """Returns the top-level task instance."""
    return self.get_task_def(self.get_top_task_name())

  @lru_cache(None)
  def get_top_task_inst(self) -> TaskInstance:
    """Returns the top-level task instance."""
    name = self.get_top_task_name()
    return TaskInstance(0, name, None, self, self.get_top_task_def())

  @lru_cache(None)
  def get_top_task_name(self) -> str:
    """Returns the name of the top-level task."""
    return self.obj['top']

  def get_flatten_graph(self) -> 'Graph':
    """Returns the flatten graph with only one top and leaf tasks as its children."""

    # Construct obj of the new flatten graph
    new_obj = self.to_dict()

    # Find all leaf task instances
    leaves = self.get_top_task_inst().get_leaf_tasks_insts()

    # obj['tasks']:
    # Reconstruct the task definitions of the graph
    #   are (1) all relevant definitions of the leaf tasks
    defs = {leaf.definition for leaf in leaves}
    new_obj['tasks'] = {d.name: d.to_dict() for d in defs}
    #   and (2) also the top task
    top_name = self.obj['top']
    new_obj['tasks'][top_name] = self.get_top_task_def().to_dict()

    # obj['tasks'][top_task]['tasks']:
    # Reconstruct the subtasks of the top level task
    #   insts = {definition: [instance, ...], ...}
    insts = {i: [j for j in leaves if j.definition is i] for i in defs}
    #   {definition.name: [instance.to_dict(), ...], ...}
    new_subtask_instantiations = {
        definition.name: list(
            map(lambda inst: inst.to_dict(interconnect_global_name=True),
                insts_of_definition))
        for definition, insts_of_definition in insts.items()
    }
    new_obj['tasks'][top_name]['tasks'] = new_subtask_instantiations

    # obj['tasks'][top_task]['fifos']:
    # Reconstruct the local interconnects of the top level task
    #   [instance, ...]
    insts = self.get_top_task_inst().recursive_get_interconnect_insts()
    #   {instance.gid: instance.definition.to_dict()}
    new_obj['tasks'][top_name]['fifos'] = {
        i.global_name: i.to_dict(insts_override=new_subtask_instantiations,
                                 interconnect_global_name=True) for i in insts
    }

    return Graph(self.name, new_obj)
