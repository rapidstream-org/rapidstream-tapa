"""Generate a tapa graphir from a floorplanned TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging

from tapa.graphir.types import (
    Expression,
    GroupedModuleDefinition,
    HierarchicalName,
    ModuleConnection,
    ModuleInstantiation,
    ModuleNet,
    ModuleParameter,
    ModulePort,
    Range,
    Token,
    VerilogModuleDefinition,
)
from tapa.graphir_conversion.utils import (
    PORT_TYPE_MAPPING,
    get_leaf_port_connection_mapping,
    get_m_axi_port_name,
    get_stream_port_name,
    get_task_arg_table,
    get_task_graphir_parameters,
    get_task_graphir_ports,
)
from tapa.instance import Instance
from tapa.task import Task
from tapa.verilog.util import Pipeline
from tapa.verilog.xilinx.const import (
    ISTREAM_SUFFIXES,
    OSTREAM_SUFFIXES,
    STREAM_DATA_SUFFIXES,
)
from tapa.verilog.xilinx.m_axi import M_AXI_SUFFIXES

_logger = logging.getLogger().getChild(__name__)
_FIFO_MODULE_NAME = "fifo"


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
                _logger.error(
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
    def add_port(name: str, direction: str) -> None:
        ports.append(
            ModulePort(
                name=name,
                hierarchical_name=HierarchicalName.get_name(name),
                type=PORT_TYPE_MAPPING[direction],
                range=None,
            )
        )

    signal_ports = [
        ("ap_clk", "input"),
        ("ap_rst_n", "input"),
        ("ap_start", "input"),
        ("ap_done", "output"),
        ("ap_ready", "output"),
        ("ap_idle", "output"),
    ]
    for name, direction in signal_ports:
        add_port(name, direction)

    return ports


def get_submodule_inst(
    leaf_tasks: dict[str, Task],
    inst: Instance,
    arg_table: dict[str, Pipeline],
) -> ModuleInstantiation:
    """Get submodule instantiation."""
    task_name = inst.task.name
    connections = []
    for arg in inst.args:
        port_name = arg.port
        if arg.cat == Instance.Arg.Cat.SCALAR:
            connections.append(
                ModuleConnection(
                    name=port_name,
                    hierarchical_name=HierarchicalName.get_name(port_name),
                    expr=Expression((Token.new_id(arg_table[arg.name][-1].name),)),
                )
            )

        elif arg.cat == Instance.Arg.Cat.ISTREAM:
            for suffix in ISTREAM_SUFFIXES:
                leaf_port = leaf_tasks[task_name].module.get_port_of(port_name, suffix)
                connections.append(
                    ModuleConnection(
                        name=leaf_port.name,
                        hierarchical_name=HierarchicalName.get_name(leaf_port.name),
                        expr=Expression(
                            (Token.new_id(get_stream_port_name(arg.name, suffix)),)
                        ),
                    )
                )
        elif arg.cat == Instance.Arg.Cat.OSTREAM:
            for suffix in OSTREAM_SUFFIXES:
                leaf_port = leaf_tasks[task_name].module.get_port_of(port_name, suffix)
                connections.append(
                    ModuleConnection(
                        name=leaf_port.name,
                        hierarchical_name=HierarchicalName.get_name(leaf_port.name),
                        expr=Expression(
                            (Token.new_id(get_stream_port_name(arg.name, suffix)),)
                        ),
                    )
                )

        else:  # mmap
            assert arg.cat == Instance.Arg.Cat.MMAP, arg.cat
            for suffix in M_AXI_SUFFIXES:
                full_port_name = get_m_axi_port_name(port_name, suffix)
                if full_port_name not in leaf_tasks[task_name].module.ports:
                    continue
                connections.append(
                    ModuleConnection(
                        name=full_port_name,
                        hierarchical_name=HierarchicalName.get_name(full_port_name),
                        expr=Expression(
                            (Token.new_id(get_m_axi_port_name(arg.name, suffix)),)
                        ),
                    )
                )
            # offset port
            offset_port_name = f"{port_name}_offset"
            connections.append(
                ModuleConnection(
                    name=offset_port_name,
                    hierarchical_name=HierarchicalName.get_name(offset_port_name),
                    expr=Expression((Token.new_id(arg_table[arg.name][-1].name),)),
                )
            )
    # add control signals
    # ap_clk
    connections.append(
        ModuleConnection(
            name="ap_clk",
            hierarchical_name=HierarchicalName.get_name("ap_clk"),
            expr=Expression((Token.new_id("ap_clk"),)),
        )
    )

    # ap_rst_n
    connections.append(
        ModuleConnection(
            name="ap_rst_n",
            hierarchical_name=HierarchicalName.get_name("ap_rst_n"),
            expr=Expression((Token.new_id("ap_rst_n"),)),
        )
    )

    # ap_ctrl
    ap_signals = ["ap_start", "ap_done", "ap_ready", "ap_idle"]
    connections.extend(
        ModuleConnection(
            name=signal,
            hierarchical_name=HierarchicalName.get_name(signal),
            expr=Expression((Token.new_id(f"{inst.name}__{signal}"),)),
        )
        for signal in ap_signals
    )

    return ModuleInstantiation(
        name=inst.name,
        hierarchical_name=HierarchicalName.get_name(inst.name),
        module=task_name,
        connections=tuple(connections),
        parameters=(),
        floorplan_region=None,
        area=None,
    )


def get_slot_fifo_inst(
    slot_task: Task,
    fifo_name: str,
    fifo: dict,
    leaf_ir_defs: dict[str, VerilogModuleDefinition],
) -> ModuleInstantiation:
    """Get slot fifo module instantiation."""
    depth = int(fifo["depth"])
    addr_width = max(1, (depth - 1).bit_length())
    # infer width from leaf module port
    fifo_range = infer_fifo_data_range(
        fifo_name,
        fifo,
        leaf_ir_defs,
        slot_task,
    )
    assert fifo_range

    data_width = Expression(
        (
            Token.new_lit("("),
            *fifo_range.left.root,
            Token.new_lit(")"),
            Token.new_lit("-"),
            Token.new_lit("("),
            *fifo_range.right.root,
            Token.new_lit(")"),
            Token.new_lit("+"),
            Token.new_lit("1"),
        )
    )
    return ModuleInstantiation(
        name=fifo_name,
        hierarchical_name=HierarchicalName.get_name(fifo_name),
        module=_FIFO_MODULE_NAME,
        connections=(
            ModuleConnection(
                name="clk",
                hierarchical_name=HierarchicalName.get_name("clk"),
                expr=Expression((Token.new_id("ap_clk"),)),
            ),
            ModuleConnection(
                name="reset",
                hierarchical_name=HierarchicalName.get_name("reset"),
                expr=Expression(
                    (
                        Token.new_lit("~"),
                        Token.new_id("ap_rst_n"),
                    )
                ),
            ),
            ModuleConnection(
                name="if_dout",
                hierarchical_name=HierarchicalName.get_name("if_dout"),
                expr=Expression((Token.new_id(f"{fifo_name}_dout"),)),
            ),
            ModuleConnection(
                name="if_empty_n",
                hierarchical_name=HierarchicalName.get_name("if_empty_n"),
                expr=Expression((Token.new_id(f"{fifo_name}_empty_n"),)),
            ),
            ModuleConnection(
                name="if_read",
                hierarchical_name=HierarchicalName.get_name("if_read"),
                expr=Expression((Token.new_id(f"{fifo_name}_read"),)),
            ),
            ModuleConnection(
                name="if_read_ce",
                hierarchical_name=HierarchicalName.get_name("if_read_ce"),
                expr=Expression((Token.new_lit("1'b1"),)),
            ),
            ModuleConnection(
                name="if_din",
                hierarchical_name=HierarchicalName.get_name("if_din"),
                expr=Expression((Token.new_id(f"{fifo_name}_din"),)),
            ),
            ModuleConnection(
                name="if_full_n",
                hierarchical_name=HierarchicalName.get_name("if_full_n"),
                expr=Expression((Token.new_id(f"{fifo_name}_full_n"),)),
            ),
            ModuleConnection(
                name="if_write",
                hierarchical_name=HierarchicalName.get_name("if_write"),
                expr=Expression((Token.new_id(f"{fifo_name}_write"),)),
            ),
            ModuleConnection(
                name="if_write_ce",
                hierarchical_name=HierarchicalName.get_name("if_write_ce"),
                expr=Expression((Token.new_lit("1'b1"),)),
            ),
        ),
        parameters=(
            ModuleConnection(
                name="DEPTH",
                hierarchical_name=HierarchicalName.get_name("DEPTH"),
                expr=Expression((Token.new_lit(str(depth)),)),
            ),
            ModuleConnection(
                name="ADDR_WIDTH",
                hierarchical_name=HierarchicalName.get_name("ADDR_WIDTH"),
                expr=Expression((Token.new_lit(str(addr_width)),)),
            ),
            ModuleConnection(
                name="DATA_WIDTH",
                hierarchical_name=HierarchicalName.get_name("DATA_WIDTH"),
                expr=data_width,
            ),
        ),
        floorplan_region=None,
        area=None,
    )


def get_slot_module_submodules(
    slot_task: Task,
    leaf_ir_defs: dict[str, VerilogModuleDefinition],
) -> list[ModuleInstantiation]:
    """Get leaf module instantiations of slot module."""
    leaf_tasks = {inst.task.name: inst.task for inst in slot_task.instances}
    ir_insts = [
        get_submodule_inst(
            leaf_tasks,
            inst,
            get_task_arg_table(slot_task),
        )
        for inst in slot_task.instances
    ]

    # fsm
    fsm_module = slot_task.fsm_module
    connections = [
        ModuleConnection(
            name=port,
            hierarchical_name=HierarchicalName.get_name(port),
            expr=Expression((Token.new_id(port),)),
        )
        for port in fsm_module.ports
    ]
    ir_insts.append(
        ModuleInstantiation(
            name=f"{fsm_module.name}_0",
            hierarchical_name=HierarchicalName.get_name(f"{fsm_module.name}_0"),
            module=fsm_module.name,
            connections=tuple(connections),
            parameters=(),
            floorplan_region=None,
            area=None,
        )
    )

    # fifo
    for fifo_name, fifo in slot_task.fifos.items():
        # skip external fifos
        if slot_task.is_fifo_external(fifo_name):
            continue
        ir_insts.append(
            get_slot_fifo_inst(
                slot_task,
                fifo_name,
                fifo,
                leaf_ir_defs,
            )
        )

    return ir_insts


def infer_fifo_data_range(
    fifo_name: str,
    fifo: dict,
    leaf_ir_defs: dict[str, VerilogModuleDefinition],
    slot: Task,
) -> Range | None:
    """Infer the range of a fifo data."""
    consumer = fifo["consumed_by"][0]
    producer = fifo["produced_by"][0]
    assert isinstance(consumer, str)
    assert isinstance(producer, str)
    assert consumer in slot.tasks
    assert producer in slot.tasks
    producer_task_name, _, producer_fifo = slot.get_connection_to(
        fifo_name, "produced_by"
    )
    consumer_task_name, _, consumer_fifo = slot.get_connection_to(
        fifo_name, "consumed_by"
    )

    subtasks: dict[str, Task] = {}
    for inst in slot.instances:
        if inst.task.name not in subtasks:
            subtasks[inst.task.name] = inst.task
    assert producer_task_name in subtasks
    assert consumer_task_name in subtasks

    producer_data_port = subtasks[producer_task_name].module.get_port_of(
        producer_fifo,
        STREAM_DATA_SUFFIXES[1],
    )
    consumer_data_port = subtasks[consumer_task_name].module.get_port_of(
        consumer_fifo,
        STREAM_DATA_SUFFIXES[0],
    )

    range0 = leaf_ir_defs[producer_task_name].get_port(producer_data_port.name).range
    range1 = leaf_ir_defs[consumer_task_name].get_port(consumer_data_port.name).range

    if range0 != range1:
        _logger.warning(
            "Fifo %s has different ranges in producer %s and consumer %s",
            fifo_name,
            producer_task_name,
            consumer_task_name,
        )

    return range0


def get_slot_module_wires(
    slot: Task,
    leaf_ir_defs: dict[str, VerilogModuleDefinition],
    slot_ports: list[ModulePort],
) -> list[ModuleNet]:
    """Get slot module wires."""
    connections = []
    # add fifo wires
    for fifo_name, fifo in slot.fifos.items():
        if slot.is_fifo_external(fifo_name):
            continue
        for suffix in ISTREAM_SUFFIXES + OSTREAM_SUFFIXES:
            wire_name = get_stream_port_name(fifo_name, suffix)
            if suffix in STREAM_DATA_SUFFIXES:
                # infer fifo width from leaf module
                fifo_range = infer_fifo_data_range(
                    fifo_name,
                    fifo,
                    leaf_ir_defs,
                    slot,
                )
            else:
                fifo_range = None
            connections.append(
                ModuleNet(
                    name=wire_name,
                    hierarchical_name=HierarchicalName.get_name(wire_name),
                    range=fifo_range,
                )
            )

    # add pipeline signal wires
    arg_table = get_task_arg_table(slot)
    port_range_mapping = {port.name: port.range for port in slot_ports}
    for arg, q in arg_table.items():
        wire_name = q[-1].name
        # infer range from fsm module
        connections.append(
            ModuleNet(
                name=wire_name,
                hierarchical_name=HierarchicalName.get_name(wire_name),
                range=port_range_mapping[arg],
            )
        )

    # add control signals
    for inst in slot.instances:
        for signal in ["ap_start", "ap_done", "ap_ready", "ap_idle"]:
            wire_name = f"{inst.name}__{signal}"
            connections.append(
                ModuleNet(
                    name=wire_name,
                    hierarchical_name=HierarchicalName.get_name(wire_name),
                    range=None,
                )
            )

    return connections


def get_slot_module_definition(
    slot: Task, leaf_ir_defs: dict[str, VerilogModuleDefinition]
) -> GroupedModuleDefinition:
    """Get slot module definition."""
    # TODO: port array support
    slot_ports = get_slot_module_definition_ports(slot, leaf_ir_defs)
    return GroupedModuleDefinition(
        name=slot.name,
        hierarchical_name=HierarchicalName.get_name(slot.name),
        parameters=tuple(get_slot_module_definition_parameters(leaf_ir_defs)),
        ports=tuple(slot_ports),
        submodules=tuple(
            get_slot_module_submodules(
                slot,
                leaf_ir_defs,
            )
        ),
        wires=tuple(get_slot_module_wires(slot, leaf_ir_defs, slot_ports)),
    )
