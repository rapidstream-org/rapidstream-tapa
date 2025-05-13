"""Generate a tapa graphir from a floorplanned TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging

from tapa.graphir.types import (
    HierarchicalName,
    ModuleParameter,
    ModulePort,
    VerilogModuleDefinition,
)
from tapa.graphir_conversion.utils import (
    PORT_TYPE_MAPPING,
    get_leaf_port_connection_mapping,
    get_task_graphir_parameters,
    get_task_graphir_ports,
)
from tapa.task import Task

_logger = logging.getLogger().getChild(__name__)


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


def get_slot_module_definition_parameters(
    leaf_modules: dict[str, VerilogModuleDefinition],
) -> list[ModuleParameter]:
    """Get slot module parameters."""
    # merge parameters from leaf modules
    parameters = {}
    for leaf_module in leaf_modules.values():
        for param in leaf_module.parameters:
            if param.name not in parameters:
                parameters[param.name] = param
            # merge parameter
            elif param.expr != parameters[param.name].expr:
                _logger.warning(
                    "Parameter %s has different values in leaf modules",
                    param.name,
                )
    return list(parameters.values())


def get_slot_module_definition_ports(
    slot: Task,
    leaf_modules: dict[str, VerilogModuleDefinition],
) -> list[ModulePort]:
    """Get slot module ports."""
    ports = []
    leaf_module_tasks = {inst.task.name: inst.task for inst in slot.instances}
    for port in slot.ports:
        # find connected leaf module port
        connected_leaf_ports = {}
        for inst in slot.instances:
            for arg in inst.args:
                if arg.name == port:
                    connected_leaf_ports[inst.task.name] = arg.port
        if len(connected_leaf_ports) == 0:
            continue

        # find matching port on leaf module
        leaf_module_name, leaf_inst_port = next(iter(connected_leaf_ports.items()))
        leaf_module_ir = leaf_modules[leaf_module_name]

        leaf_module_task = leaf_module_tasks[leaf_module_name]
        assert leaf_inst_port in leaf_module_task.ports

        # infer port rtl based on port type
        task_port = leaf_module_task.ports[leaf_inst_port]
        port_map = get_leaf_port_connection_mapping(
            task_port, leaf_module_task.module, port
        )

        for leaf_port, slot_port in port_map.items():
            leaf_module_ir_port = leaf_module_ir.get_port(leaf_port)
            ports.append(
                ModulePort(
                    name=slot_port,
                    hierarchical_name=HierarchicalName.get_name(slot_port),
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
