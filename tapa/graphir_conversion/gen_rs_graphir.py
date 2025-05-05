"""Generate a tapa graphir from a floorplanned TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types import HierarchicalName, VerilogModuleDefinition
from tapa.graphir_conversion.utils import (
    get_task_graphir_parameters,
    get_task_graphir_ports,
)
from tapa.task import Task


def get_verilog_module_from_leaf_task(task: Task) -> VerilogModuleDefinition:
    """Get the verilog module from a task."""
    assert task.is_lower
    if not task.module:
        msg = "Task contains no module"
        raise ValueError(msg)

    return VerilogModuleDefinition(
        name=task.module.name,
        hierarchical_name=HierarchicalName.get_name(task.module.name),
        parameters=tuple(get_task_graphir_parameters(task)),
        ports=tuple(get_task_graphir_ports(task)),
        verilog=task.module.code,
        submodules_module_names=(),
    )
