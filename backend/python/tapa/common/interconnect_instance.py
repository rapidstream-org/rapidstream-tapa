from typing import Optional, Dict

from tapa.common.base import Base


class InterconnectInstance(Base):
  """TAPA local interconnect instance."""

  def to_dict(self,
              insts_override: Optional[Dict] = None,
              interconnect_global_name: bool = False) -> Dict:
    """Return the dict as in the graph JSON description.

    Args:
      insts_override:
        Override the instantiation of the tasks where the interconnect
        resides in and is used by.
      interconnect_global_name:
        Use the global names as the name of the interconnects.

    Returns:
      The dict representation of the interconnect.
    """
    obj = self.definition.to_dict()

    # FIXME: remove consumed_by and produced_by completely
    insts = insts_override if insts_override is not None \
      else self.parent.obj['tasks']

    def find_use(prefix: str):
      name = self.global_name if interconnect_global_name else self.name

      for task_name, inst in insts.items():
        for idx, task in enumerate(inst):
          for arg in task['args'].values():
            if arg['arg'] == name and arg['cat'].startswith(prefix):
              return task_name, idx

      raise AssertionError(
          f'invalid consumed/produced_by for interconnect {name}')

    # Rebuild legacy structure from the index of the parent
    if 'consumed_by' in obj:
      obj['consumed_by'] = find_use('is')
    if 'produced_by' in obj:
      obj['produced_by'] = find_use('os')

    return obj
