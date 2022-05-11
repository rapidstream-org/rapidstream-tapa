from enum import Enum
from functools import lru_cache
from typing import Dict

from tapa.common.base import Base


class ExternalPort(Base):
  """TAPA external port that is connected to the top level task."""

  class Type(Enum):
    STREAM = 1
    SYNC_MMAP = 2
    ASYNC_MMAP = 3
    SCALAR = 4

  def __init__(self, *args, **kwargs):
    """Constructs a TAPA external port."""
    super(ExternalPort, self).__init__(*args, **kwargs)
    self.global_name = self.name  # external port's name is global

  @lru_cache(None)
  def get_bitwidth(self) -> int:
    """Returns the bitwidth."""
    return self.obj['width']

  @lru_cache(None)
  def get_type(self) -> Type:
    """Returns the type of the external port."""
    if self.obj['cat'] == 'stream':
      return ExternalPort.Type.STREAM
    elif self.obj['cat'] == 'mmap':
      return ExternalPort.Type.SYNC_MMAP
    elif self.obj['cat'] == 'async_mmap':
      return ExternalPort.Type.ASYNC_MMAP
    elif self.obj['cat'] == 'scalar':
      return ExternalPort.Type.SCALAR
    else:
      raise NotImplemented(f'Unknown type "{self.obj["cat"]}"')
