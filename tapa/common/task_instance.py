from __future__ import annotations

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from functools import lru_cache

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

    def __init__(
        self,
        idx: int,
        name: str | None,
        obj: dict[str, object],
        parent: Base | None = None,
        definition: Base | None = None,
    ) -> None:
        """Construct a TAPA task instance.

        Args:
          idx: The index of this task as nested in its parent.
          name: Name of the task instance.
          obj: The JSON dict object of the TAPA object.
          parent: The TAPA object that has this object nested in.
          definition: The TAPA definition object of this object, or self.
        """
        super().__init__(name=name, obj=obj, parent=parent, definition=definition)
        self.idx = idx

    @lru_cache(None)
    def get_external_port(self, name: str) -> ExternalPort | None:
        """Returns the external port of this task instance."""
        if name in self.get_external_ports():
            return self.get_external_ports()[name]
        return None

    @lru_cache(None)
    def get_external_ports(self) -> dict[str, ExternalPort]:
        """Returns the external ports of this task instance."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.TOP:
            assert isinstance(self.definition.obj["ports"], list)
            return {
                p["name"]: ExternalPort(p["name"], p, self)
                for p in self.definition.obj["ports"]
            }
        # Non-top tasks have no external ports
        return {}

    @lru_cache(None)
    def get_interconnect_inst(self, name: str) -> InterconnectInstance | None:
        """Returns the local interconnect of this task instance."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.LEAF:
            return None
        inter_def = self.definition.get_interconnect_def(name)
        return InterconnectInstance(name, {}, self, inter_def) if inter_def else None

    @lru_cache(None)
    def get_interconnect_insts(self) -> list[InterconnectInstance]:
        """Returns the local interconnects of this task instance."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.LEAF:
            return []

        results = []
        for inter_def in self.definition.get_interconnect_defs():
            inter_inst = self.get_interconnect_inst(inter_def.name)
            if inter_inst is not None:
                results.append(inter_inst)

        return results

    @lru_cache(None)
    def get_in_scope_interconnect_or_port(
        self,
        name: str,
    ) -> InterconnectInstance | ExternalPort | Constant:
        """Returns the interconnect or port that has `name` in this task's scope.

        The interconnect that has `name` might be in this local scope either
        through ports or instantiated locally.  This function is to lookup the
        original interconnect or external port recursively.

        Args:
          name: Name of the interconnect or port in the local scope.

        Returns:
          The corresponding interconnect or external port.
        """
        assert isinstance(self.definition, TaskDefinition)
        inter_inst = self.get_interconnect_inst(name)

        if inter_inst is not None:
            return inter_inst

        # If not instantiated locally
        if self.definition.get_level() != TaskDefinition.Level.TOP:
            assert isinstance(self.parent, TaskInstance)

            # 1. lookup its parents if this is not the top level task
            insts_all = self.parent.definition.obj["tasks"]
            assert isinstance(insts_all, dict)
            insts_self = insts_all[self.definition.name]
            inst = insts_self[self.idx]
            arg = inst["args"][name]
            parent_name = arg["arg"]

            # FIXME: constants should be specified explicitly in tapacc
            if parent_name.startswith("64'd"):
                # this is an int constant
                return Constant(parent_name, arg)
            # this is passed from its parent
            return self.parent.get_in_scope_interconnect_or_port(parent_name)
        # 2. or if this task is the top level task, get the external port
        port = self.get_external_port(name)
        assert port is not None
        return port

    @lru_cache(None)
    def recursive_get_interconnect_insts(self) -> list[InterconnectInstance]:
        """Returns all local interconnects of this task and its subtasks."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.LEAF:
            return []
        insts = self.get_interconnect_insts()
        for task_inst in self.get_subtasks_insts():
            insts.extend(task_inst.recursive_get_interconnect_insts())
        return insts

    @lru_cache(None)
    def get_subtasks_insts(self) -> list[TaskInstance]:
        """Returns the subinstances of this task instance."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.LEAF:
            return []
        insts = []
        for task_def in self.definition.get_subtask_defs():
            task_instances = self.definition.get_subtask_instantiations(task_def.name)
            for idx, inst in enumerate(task_instances):
                insts.append(
                    TaskInstance(idx, f"{task_def.name}_{idx}", inst, self, task_def),
                )
        return insts

    @lru_cache(None)
    def get_leaf_tasks_insts(self) -> list[TaskInstance]:
        """Returns the instances of the leaf tasks of under this instance."""
        assert isinstance(self.definition, TaskDefinition)

        if self.definition.get_level() == TaskDefinition.Level.LEAF:
            return [self]
        insts = []
        for task_inst in self.get_subtasks_insts():
            insts.extend(task_inst.get_leaf_tasks_insts())
        return insts

    def to_dict(self, interconnect_global_name: bool = False) -> dict:
        """Transform the task instance object to its JSON description.

        Args:
          interconnect_global_name:
            Use the global names as the name of the interconnects.

        Returns:
          The dict as in the graph JSON description.
        """
        obj = super().to_dict()
        assert isinstance(obj["args"], dict)

        if interconnect_global_name:
            # Transform args to their global names
            for name, arg in obj["args"].items():
                inter_inst = self.get_in_scope_interconnect_or_port(name)
                if inter_inst:
                    arg["arg"] = inter_inst.global_name

        return obj
