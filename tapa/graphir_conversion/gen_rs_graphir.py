"""Generate a tapa graphir from a floorplanned TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types import HierarchicalName, ModulePort, VerilogModuleDefinition
from tapa.graphir_conversion.utils import (
    PORT_TYPE_MAPPING,
    get_module_ports,
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


def get_slot_module_definition_ports(
    slot_obj: dict,
    leaf_modules: dict[str, VerilogModuleDefinition],
    leaf_module_tasks: dict[str, Task],
) -> list[ModulePort]:
    """Get slot module ports."""
    ports = []
    for port in slot_obj["ports"]:
        # find connected leaf module port
        connected_leaf_ports = {}
        for task_name, task_obj in slot_obj["tasks"].items():
            for port_name, arg in task_obj["args"]:
                if arg["name"] == port["name"]:
                    connected_leaf_ports[task_name] = port_name
        if len(connected_leaf_ports) == 0:
            continue
        assert len(connected_leaf_ports) == 1, (
            "Multiple connected leaf module ports found"
        )

        # find matching port on leaf module
        leaf_module_name, leaf_inst_port = next(iter(connected_leaf_ports.items()))
        leaf_module_ir = leaf_modules[leaf_module_name]

        leaf_module_task = leaf_module_tasks[leaf_module_name]
        assert leaf_inst_port in leaf_module_task.ports

        # infer port rtl based on port type
        task_port = leaf_module_task.ports[port]

        module_ports = get_module_ports(task_port, leaf_module_task.module)

        for module_port in module_ports:
            leaf_module_ir_port = leaf_module_ir.get_port(module_port.name)
            ports.append(
                ModulePort(
                    name=port["name"],
                    hierarchical_name=HierarchicalName.get_name(port.name),
                    type=leaf_module_ir_port.type,
                    range=leaf_module_ir_port.range,
                )
            )

    # add other signals
    ports.append(
        ModulePort(
            name="ap_clk",
            hierarchical_name=HierarchicalName.get_name("ap_clk"),
            type=PORT_TYPE_MAPPING["input"],
            range=None,
        )
    )
    ports.append(
        ModulePort(
            name="ap_rst_n",
            hierarchical_name=HierarchicalName.get_name("ap_rst_n"),
            type=PORT_TYPE_MAPPING["input"],
            range=None,
        )
    )
    ports.append(
        ModulePort(
            name="ap_start",
            hierarchical_name=HierarchicalName.get_name("ap_start"),
            type=PORT_TYPE_MAPPING["input"],
            range=None,
        )
    )
    ports.append(
        ModulePort(
            name="ap_done",
            hierarchical_name=HierarchicalName.get_name("ap_done"),
            type=PORT_TYPE_MAPPING["output"],
            range=None,
        )
    )
    ports.append(
        ModulePort(
            name="ap_ready",
            hierarchical_name=HierarchicalName.get_name("ap_ready"),
            type=PORT_TYPE_MAPPING["output"],
            range=None,
        )
    )
    ports.append(
        ModulePort(
            name="ap_idle",
            hierarchical_name=HierarchicalName.get_name("ap_idle"),
            type=PORT_TYPE_MAPPING["output"],
            range=None,
        )
    )
    return ports
