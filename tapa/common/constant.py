"""Constant class for TAPA constant passed as an argument to a task."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.common.base import Base


class Constant(Base):
    """TAPA constant passed as an argument to a task."""

    def __init__(
        self,
        name: str | None,
        obj: dict[str, object],
        parent: Base | None = None,
        definition: Base | None = None,
    ) -> None:
        """Constructs a TAPA constant."""
        super().__init__(name=name, obj=obj, parent=parent, definition=definition)
        self.global_name = self.name  # constant's name is global
