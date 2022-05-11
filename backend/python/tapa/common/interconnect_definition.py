from enum import Enum
from functools import lru_cache

from tapa.common.base import Base


class InterconnectDefinition(Base):
  """TAPA local interconnect definition."""

  class Type(Enum):
    STREAM = 1
    SYNC_MMAP = 2
    ASYNC_MMAP = 3
    SCALAR = 4

  @lru_cache(None)
  def get_depth(self) -> int:
    if self.get_type() == InterconnectDefinition.Type.STREAM:
      return self.obj['depth']
    else:
      raise NotImplemented(
          'Local interconnects other than streams are not implemented yet.')

  @lru_cache(None)
  def get_type(self) -> Type:
    if self.obj is not None:
      return InterconnectDefinition.Type.STREAM
    else:
      raise NotImplemented(
          'Local interconnects other than streams are not implemented yet.')
