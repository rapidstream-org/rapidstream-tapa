"""External port objects in TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from enum import Enum
from functools import lru_cache

from tapa.common.base import Base


class ExternalPort(Base):
    """TAPA external port that is connected to the top level task."""

    class Type(Enum):
        STREAM = 1
        SYNC_MMAP = 2
        ASYNC_MMAP = 3
        SCALAR = 4

    def __init__(
        self,
        name: str | None,
        obj: dict[str, object],
        parent: Base | None = None,
        definition: Base | None = None,
    ) -> None:
        """Constructs a TAPA external port."""
        super().__init__(name=name, obj=obj, parent=parent, definition=definition)
        self.global_name = self.name  # external port's name is global

    @lru_cache(None)
    def get_bitwidth(self) -> int:
        """Returns the bitwidth."""
        assert isinstance(self.obj["width"], int)
        return self.obj["width"]

    @lru_cache(None)
    def get_type(self) -> Type:
        """Returns the type of the external port."""
        if self.obj["cat"] == "stream":
            return ExternalPort.Type.STREAM
        if self.obj["cat"] == "mmap":
            return ExternalPort.Type.SYNC_MMAP
        if self.obj["cat"] == "async_mmap":
            return ExternalPort.Type.ASYNC_MMAP
        if self.obj["cat"] == "scalar":
            return ExternalPort.Type.SCALAR
        msg = f'Unknown type "{self.obj["cat"]}"'
        raise NotImplementedError(msg)
