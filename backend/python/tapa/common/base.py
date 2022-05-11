import copy
import re
from typing import Dict, Optional

import tapa.common.graph


class Base:
  """Describes a TAPA base object.

  Attributes:
    name: Name of the object, which is locally unique in its parent.
    obj: The JSON dict object of the TAPA object.
    parent: The TAPA object that has this object nested in.
    definition: The TAPA definition object of this object, or self.
    global_name: Globally descriptive name of this object in a Graph.
  """

  uuid_counter = 0

  def __init__(self,
               name: Optional[str],
               obj: Dict,
               parent: Optional['Base'] = None,
               definition: Optional['Base'] = None):
    """Construct TAPA object from a JSON description.

    Args:
      name: Name of the object, which is locally unique in its parent.
      obj: The JSON dict object of the TAPA object.
      parent: The TAPA object that has this object nested in.
      definition: The TAPA definition object of this object, or self.
    """
    self.obj = copy.deepcopy(obj)
    self.name = name
    self.parent = parent
    self.global_name = self._generate_global_name()

    # Link definition to self if not specified
    self.definition = self if definition is None else definition

  def to_dict(self) -> Dict:
    """Return the TAPA object as a JSON description."""
    return copy.deepcopy(self.obj)

  def _generate_global_name(self) -> str:
    """Return the global name for an object."""

    if self.name is not None:
      match = re.fullmatch(r'(\w+)(\[\d+\])', self.name)
      if match is not None:
        # FIXME: tapacc should sanitize the names
        # the name has an array subscript
        return f'{self._generate_global_name_without_sub(match[1])}{match[2]}'

    return self._generate_global_name_without_sub(self.name)

  def _generate_global_name_without_sub(self, name: Optional[str]) -> str:
    """Returns the global name for a name without an array subscript."""

    if type(self.parent) is tapa.common.graph.Graph:
      # (1) If its parent is the graph, its name is global already
      return name

    elif self.parent is not None and self.parent.global_name is not None:
      # (2) Try generate readable global name by appending parent's name
      return f'{name}_{self.parent.global_name}'

    elif self.name is not None:
      # (3) Try generate global name by appending UUID counter
      Base.uuid_counter += 1
      return f'{name}_{Base.uuid_counter}'

    else:
      # (4) Use UUID counter directly
      Base.uuid_counter += 1
      return f'object_{Base.uuid_counter}'
