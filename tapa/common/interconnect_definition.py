"""Interconnect definition object in TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

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
        """Return the depth of the local interconnect."""
        if self.get_type() == InterconnectDefinition.Type.STREAM:
            assert isinstance(self.obj["depth"], int)
            return self.obj["depth"]
        msg = "Local interconnects other than streams are not implemented yet."
        raise NotImplementedError(
            msg,
        )

    @lru_cache(None)
    def get_type(self) -> Type:
        """Return the type of the local interconnect."""
        if self.obj is not None:
            return InterconnectDefinition.Type.STREAM
        msg = "Local interconnects other than streams are not implemented yet."
        raise NotImplementedError(
            msg,
        )
