__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import operator
from enum import Enum
from functools import lru_cache
from re import sub

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
        # Import Graph locally to avoid circular import
        from tapa.common.graph import Graph  # noqa: PLC0415

        if self.obj["level"] == "lower":
            return TaskDefinition.Level.LEAF
        if (
            isinstance(self.parent, Graph)
            and self.parent.get_top_task_def().name != self.name
        ):
            return TaskDefinition.Level.UPPER
        return TaskDefinition.Level.TOP

    @lru_cache(None)
    def get_interconnect_def(self, name: str) -> InterconnectDefinition | None:
        """Returns the interconnect named `name` defined in this task."""
        # If not defined
        assert isinstance(self.obj["fifos"], dict)
        if name not in self.obj["fifos"]:
            return None

        # If is an argument
        port_name = sub(r"\[\d+\]$", r"", name)
        # FIXME: tapacc should output port name with []
        assert isinstance(self.obj["ports"], list)
        if port_name in map(operator.itemgetter("name"), self.obj["ports"]):
            return None

        assert isinstance(self.obj["fifos"], dict)
        return InterconnectDefinition(name, self.obj["fifos"][name], self)

    @lru_cache(None)
    def get_interconnect_defs(self) -> list[InterconnectDefinition]:
        """Returns all local interconnect defined in this task."""
        if self.get_level() == TaskDefinition.Level.LEAF:
            return []

        assert isinstance(self.obj["fifos"], dict)
        results = []
        for name in self.obj["fifos"]:
            inter_def = self.get_interconnect_def(name)
            if inter_def is not None:
                results.append(inter_def)

        return results

    @lru_cache(None)
    def get_subtask_defs(self) -> list["TaskDefinition"]:
        """Returns the task definitions of the subtasks of this task."""
        # Import Graph locally to avoid circular import
        from tapa.common.graph import Graph  # noqa: PLC0415

        assert isinstance(self.parent, Graph)
        assert isinstance(self.obj["tasks"], dict)
        return (
            [self.parent.get_task_def(name) for name in self.obj["tasks"]]
            if self.get_level() != TaskDefinition.Level.LEAF
            else []
        )

    @lru_cache(None)
    def get_subtask_instantiations(self, name: str) -> list[dict]:
        """Return the instantiations of subtask with `name`."""
        assert isinstance(self.obj["tasks"], dict)
        return self.obj["tasks"][name]
