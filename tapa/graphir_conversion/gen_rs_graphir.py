"""Generate a tapa graphir from a floorplanned TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from collections.abc import Generator
from pathlib import Path

from tapa.core import Program
from tapa.graphir.types import (
    AnyModuleDefinition,
    Expression,
    GroupedModuleDefinition,
    HierarchicalName,
    ModuleConnection,
    ModuleInstantiation,
    ModuleNet,
    ModuleParameter,
    ModulePort,
    Modules,
    Project,
    Range,
    Token,
    VerilogModuleDefinition,
)
from tapa.graphir_conversion.add_iface import get_graphir_iface
from tapa.graphir_conversion.utils import (
    get_child_port_connection_mapping,
    get_ctrl_s_axi_def,
    get_fifo_def,
    get_fsm_def,
    get_m_axi_port_name,
    get_reset_inverter_def,
    get_reset_inverter_inst,
    get_stream_port_name,
    get_task_arg_table,
    get_task_graphir_parameters,
    get_task_graphir_ports,
    get_verilog_definition_from_tapa_module,
)
from tapa.instance import Instance
from tapa.task import Task
from tapa.verilog.util import Pipeline, match_array_name, sanitize_array_name
from tapa.verilog.xilinx.const import (
    HANDSHAKE_INPUT_PORTS,
    HANDSHAKE_OUTPUT_PORTS,
    ISTREAM_SUFFIXES,
    OSTREAM_SUFFIXES,
    STREAM_DATA_SUFFIXES,
)
from tapa.verilog.xilinx.m_axi import M_AXI_SUFFIXES
from tapa.verilog.xilinx.module import Module

_logger = logging.getLogger().getChild(__name__)
_FIFO_MODULE_NAME = "fifo"

_CTRL_S_AXI_PARAM_MAPPING = {
    "C_S_AXI_ADDR_WIDTH": "C_S_AXI_CONTROL_ADDR_WIDTH",
    "C_S_AXI_DATA_WIDTH": "C_S_AXI_CONTROL_DATA_WIDTH",
}
_CTRL_S_AXI_PORT_MAPPING = {
    "AWVALID": Expression((Token.new_id("s_axi_control_AWVALID"),)),
    "AWREADY": Expression((Token.new_id("s_axi_control_AWREADY"),)),
    "AWADDR": Expression((Token.new_id("s_axi_control_AWADDR"),)),
    "WVALID": Expression((Token.new_id("s_axi_control_WVALID"),)),
    "WREADY": Expression((Token.new_id("s_axi_control_WREADY"),)),
    "WDATA": Expression((Token.new_id("s_axi_control_WDATA"),)),
    "WSTRB": Expression((Token.new_id("s_axi_control_WSTRB"),)),
    "ARVALID": Expression((Token.new_id("s_axi_control_ARVALID"),)),
    "ARREADY": Expression((Token.new_id("s_axi_control_ARREADY"),)),
    "ARADDR": Expression((Token.new_id("s_axi_control_ARADDR"),)),
    "RVALID": Expression((Token.new_id("s_axi_control_RVALID"),)),
    "RREADY": Expression((Token.new_id("s_axi_control_RREADY"),)),
    "RDATA": Expression((Token.new_id("s_axi_control_RDATA"),)),
    "RRESP": Expression((Token.new_id("s_axi_control_RRESP"),)),
    "BVALID": Expression((Token.new_id("s_axi_control_BVALID"),)),
    "BREADY": Expression((Token.new_id("s_axi_control_BREADY"),)),
    "BRESP": Expression((Token.new_id("s_axi_control_BRESP"),)),
    "ACLK": Expression((Token.new_id("ap_clk"),)),
    "ARESET": Expression((Token.new_id("rst"),)),
    "ACLK_EN": Expression((Token.new_lit("1'b1"),)),
}


def get_verilog_module_from_leaf_task(
    task: Task, code: str | None = None
) -> VerilogModuleDefinition:
    """Get the verilog module from a task."""
    assert task.is_lower
    if not task.module:
        msg = "Task contains no module"
        raise ValueError(msg)

    return get_verilog_definition_from_tapa_module(task.module, code)


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


def get_slot_module_definition_ports(  # noqa: C901
    slot: Task,
    child_modules: dict[str, VerilogModuleDefinition],
) -> list[ModulePort]:
    """Get slot module ports.

    Args:
        slot: task of slot
        child_modules: graphir module definitions of its child modules. The key is the
            task name of the child module.

    Returns:
        List of the graphir ports of slot module definition.
    """
    ports = []
    child_module_tasks = {inst.task.name: inst.task for inst in slot.instances}
    for port in slot.ports:
        # find connected child module port
        child_module_name = None
        child_inst_port = None
        child_inst_port_idx = None
        for inst in slot.instances:
            for arg in inst.args:
                if arg.name == port:
                    match = match_array_name(arg.port)
                    if match:
                        child_module_name = inst.task.name
                        child_inst_port = match[0]
                        child_inst_port_idx = match[1]
                    else:
                        child_module_name = inst.task.name
                        child_inst_port = arg.port
                        child_inst_port_idx = None
                    break
        if not child_module_name:
            continue

        # find matching port on child module
        assert child_module_name
        assert child_inst_port
        child_module_ir = child_modules[child_module_name]

        child_module_task = child_module_tasks[child_module_name]
        assert child_inst_port in child_module_task.ports

        # infer port rtl based on port type
        task_port = child_module_task.ports[child_inst_port]
        port_map = get_child_port_connection_mapping(
            task_port, child_module_task.module, port, child_inst_port_idx
        )

        for child_port, slot_port in port_map.items():
            child_module_ir_port = child_module_ir.get_port(child_port)
            ports.append(
                ModulePort(
                    name=slot_port,
                    hierarchical_name=HierarchicalName.get_name(slot_port),
                    type=child_module_ir_port.type,
                    range=child_module_ir_port.range,
                )
            )

    # add other signals
    def add_port(name: str, direction: ModulePort.Type) -> None:
        ports.append(
            ModulePort(
                name=name,
                hierarchical_name=HierarchicalName.get_name(name),
                type=direction,
                range=None,
            )
        )

    signal_ports = [
        "ap_clk",
        "ap_rst_n",
        "ap_start",
        "ap_done",
        "ap_ready",
        "ap_idle",
    ]
    for name in signal_ports:
        direction = None
        if name in HANDSHAKE_INPUT_PORTS:
            direction = ModulePort.Type.INPUT
        elif name in HANDSHAKE_OUTPUT_PORTS:
            direction = ModulePort.Type.OUTPUT
        assert direction
        add_port(name, direction)

    return ports


def get_submodule_inst(
    subtasks: dict[str, Task],
    inst: Instance,
    arg_table: dict[str, dict[str, Pipeline]],
    floorplan_region: str | None = None,
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
                    expr=Expression(
                        (Token.new_id(arg_table[inst.name][arg.name][-1].name),)
                    ),
                )
            )

        elif arg.cat == Instance.Arg.Cat.ISTREAM:
            for suffix in ISTREAM_SUFFIXES:
                leaf_port = subtasks[task_name].module.get_port_of(port_name, suffix)
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
                leaf_port = subtasks[task_name].module.get_port_of(port_name, suffix)
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
                if full_port_name not in subtasks[task_name].module.ports:
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
                    expr=Expression(
                        (Token.new_id(arg_table[inst.name][arg.name][-1].name),)
                    ),
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
        floorplan_region=floorplan_region,
        area=None,
    )


def get_fifo_inst(  # noqa: PLR0917, PLR0913
    upper_task: Task,
    fifo_name: str,
    fifo: dict,
    submodule_ir_defs: dict[str, AnyModuleDefinition],
    is_top: bool = False,
    floorplan_region: str | None = None,
) -> ModuleInstantiation:
    """Get slot fifo module instantiation."""
    depth = int(fifo["depth"])
    addr_width = max(1, (depth - 1).bit_length())
    # infer width from leaf module port
    fifo_range = infer_fifo_data_range(
        fifo_name,
        fifo,
        submodule_ir_defs,
        upper_task,
        not is_top,
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

    fifo_name_no_bracket = sanitize_array_name(fifo_name)
    return ModuleInstantiation(
        name=fifo_name_no_bracket,
        hierarchical_name=HierarchicalName.get_name(fifo_name_no_bracket),
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
                expr=(
                    Expression((Token.new_id("rst"),))
                    if is_top
                    else Expression(
                        (
                            Token.new_lit("~"),
                            Token.new_id("ap_rst_n"),
                        )
                    )
                ),
            ),
            ModuleConnection(
                name="if_dout",
                hierarchical_name=HierarchicalName.get_name("if_dout"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_dout"),)),
            ),
            ModuleConnection(
                name="if_empty_n",
                hierarchical_name=HierarchicalName.get_name("if_empty_n"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_empty_n"),)),
            ),
            ModuleConnection(
                name="if_read",
                hierarchical_name=HierarchicalName.get_name("if_read"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_read"),)),
            ),
            ModuleConnection(
                name="if_read_ce",
                hierarchical_name=HierarchicalName.get_name("if_read_ce"),
                expr=Expression((Token.new_lit("1'b1"),)),
            ),
            ModuleConnection(
                name="if_din",
                hierarchical_name=HierarchicalName.get_name("if_din"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_din"),)),
            ),
            ModuleConnection(
                name="if_full_n",
                hierarchical_name=HierarchicalName.get_name("if_full_n"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_full_n"),)),
            ),
            ModuleConnection(
                name="if_write",
                hierarchical_name=HierarchicalName.get_name("if_write"),
                expr=Expression((Token.new_id(f"{fifo_name_no_bracket}_write"),)),
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
        floorplan_region=floorplan_region,
        area=None,
    )


def get_upper_module_ir_subinsts(
    upper_task: Task,
    submodule_ir_defs: dict[str, AnyModuleDefinition],
    floorplan_region: str | None = None,
) -> list[ModuleInstantiation]:
    """Get leaf module instantiations of slot module."""
    subtasks = {inst.task.name: inst.task for inst in upper_task.instances}
    ir_insts = [
        get_submodule_inst(
            subtasks,
            inst,
            get_task_arg_table(upper_task),
            floorplan_region,
        )
        for inst in upper_task.instances
    ]

    # fsm
    fsm_module = upper_task.fsm_module
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
            floorplan_region=floorplan_region,
            area=None,
        )
    )

    # fifo
    for fifo_name, fifo in upper_task.fifos.items():
        # skip external fifos
        if upper_task.is_fifo_external(fifo_name):
            continue
        ir_insts.append(
            get_fifo_inst(
                upper_task,
                fifo_name,
                fifo,
                submodule_ir_defs,
                floorplan_region=floorplan_region,
            )
        )

    return ir_insts


def infer_fifo_data_range(
    fifo_name: str,
    fifo: dict,
    leaf_ir_defs: dict[str, AnyModuleDefinition],
    slot: Task,
    infer_port_name_from_tapa_module: bool = True,
) -> Range | None:
    """Infer the range of a fifo data.

    If infer_port_name_from_tapa_module is True, it will try to match the port name from
    tapa.module. Otherwise, it will directly use fifo name + suffix as the port name.
    """
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

    if infer_port_name_from_tapa_module:
        producer_data_port = (
            subtasks[producer_task_name]
            .module.get_port_of(
                producer_fifo,
                STREAM_DATA_SUFFIXES[1],
            )
            .name
        )
        consumer_data_port = (
            subtasks[consumer_task_name]
            .module.get_port_of(
                consumer_fifo,
                STREAM_DATA_SUFFIXES[0],
            )
            .name
        )
    else:
        producer_data_port = get_stream_port_name(
            producer_fifo, STREAM_DATA_SUFFIXES[1]
        )
        consumer_data_port = get_stream_port_name(
            consumer_fifo, STREAM_DATA_SUFFIXES[0]
        )

    range0 = leaf_ir_defs[producer_task_name].get_port(producer_data_port).range
    range1 = leaf_ir_defs[consumer_task_name].get_port(consumer_data_port).range

    if range0 != range1:
        _logger.warning(
            "Fifo %s has different ranges in producer %s and consumer %s",
            fifo_name,
            producer_task_name,
            consumer_task_name,
        )

    return range0


def get_upper_task_ir_wires(
    upper_task: Task,
    submodule_ir_defs: dict[str, AnyModuleDefinition],
    upper_task_ir_ports: list[ModulePort],
    ctrl_s_axi_ir_ports: list[ModulePort] = [],
    is_top: bool = False,
) -> list[ModuleNet]:
    """Get upper_task module wires."""
    connections = []
    # add fifo wires
    for fifo_name, fifo in upper_task.fifos.items():
        if upper_task.is_fifo_external(fifo_name):
            continue
        for suffix in ISTREAM_SUFFIXES + OSTREAM_SUFFIXES:
            fifo_name_no_bracket = sanitize_array_name(fifo_name)
            wire_name = get_stream_port_name(fifo_name_no_bracket, suffix)
            if suffix in STREAM_DATA_SUFFIXES:
                # infer fifo width from leaf module
                fifo_range = infer_fifo_data_range(
                    fifo_name,
                    fifo,
                    submodule_ir_defs,
                    upper_task,
                    not is_top,
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
    arg_table = get_task_arg_table(upper_task)
    port_range_mapping = {
        port.name: port.range for port in upper_task_ir_ports + ctrl_s_axi_ir_ports
    }
    for inst_arg_table in arg_table.values():
        for arg, q in inst_arg_table.items():
            port_range_key = arg
            if port_range_key not in port_range_mapping:
                port_range_key = f"{arg}_offset"
            wire_name = q[-1].name
            # infer range from fsm module
            connections.append(
                ModuleNet(
                    name=wire_name,
                    hierarchical_name=HierarchicalName.get_name(wire_name),
                    range=port_range_mapping[port_range_key],
                )
            )

    # add control signals
    for inst in upper_task.instances:
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
    slot: Task,
    leaf_ir_defs: dict[str, VerilogModuleDefinition],
    floorplan_region: str,
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
            get_upper_module_ir_subinsts(
                slot,
                leaf_ir_defs,
                floorplan_region,
            )
        ),
        wires=tuple(get_upper_task_ir_wires(slot, leaf_ir_defs, slot_ports)),
    )


def get_top_ctrl_s_axi_inst(
    top: Task,
    top_ir_param: list[ModuleParameter],
    ctrl_s_axi_ir: VerilogModuleDefinition,
    floorplan_region: str,
) -> ModuleInstantiation:
    """Get top ctrl_s_axi instantiation."""
    connections = []
    for port in ctrl_s_axi_ir.ports:
        if port.name in _CTRL_S_AXI_PORT_MAPPING:
            expr = _CTRL_S_AXI_PORT_MAPPING[port.name]
        else:
            expr = Expression((Token.new_id(port.name),))
        connections.append(
            ModuleConnection(
                name=port.name,
                hierarchical_name=HierarchicalName.get_name(port.name),
                expr=expr,
            )
        )
    parameters = []
    for param, value in _CTRL_S_AXI_PARAM_MAPPING.items():
        # find id in top def and replace to make parameter constant
        tokens = None
        for top_param in top_ir_param:
            if top_param.name == value:
                tokens = top_param.expr.root
                break
        assert tokens
        parameters.append(
            ModuleConnection(
                name=param,
                hierarchical_name=HierarchicalName.get_name(param),
                expr=Expression(tokens),
            )
        )
    return ModuleInstantiation(
        name="control_s_axi_U",
        hierarchical_name=HierarchicalName.get_name("control_s_axi_U"),
        module=f"{top.name}_control_s_axi",
        connections=tuple(connections),
        parameters=tuple(parameters),
        floorplan_region=floorplan_region,
        area=None,
    )


def get_top_extra_wires(
    ctrl_s_axi__ir: VerilogModuleDefinition,
) -> Generator[ModuleNet]:
    """Get wires between control_s_axi and fsm."""
    for port in ctrl_s_axi__ir.ports:
        if port.name not in _CTRL_S_AXI_PORT_MAPPING:
            yield ModuleNet(
                name=port.name,
                hierarchical_name=HierarchicalName.get_name(port.name),
                range=port.range,
            )


def get_top_level_slot_inst(
    slot_def: AnyModuleDefinition,
    slot_inst: Instance,
    arg_table: dict[str, Pipeline],
    floorplan_task_name_region_mapping: dict[str, str],
) -> ModuleInstantiation:
    """Get top level slot instantiation."""
    slot_def_port_names = [port.name for port in slot_def.ports]
    task_name = slot_inst.task.name
    connections = []
    for arg in slot_inst.args:
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
                leaf_port = port_name + suffix
                assert leaf_port in slot_def_port_names
                connections.append(
                    ModuleConnection(
                        name=leaf_port,
                        hierarchical_name=HierarchicalName.get_name(leaf_port),
                        expr=Expression(
                            (Token.new_id(get_stream_port_name(arg.name, suffix)),)
                        ),
                    )
                )
        elif arg.cat == Instance.Arg.Cat.OSTREAM:
            for suffix in OSTREAM_SUFFIXES:
                leaf_port = port_name + suffix
                assert leaf_port in slot_def_port_names
                connections.append(
                    ModuleConnection(
                        name=leaf_port,
                        hierarchical_name=HierarchicalName.get_name(leaf_port),
                        expr=Expression(
                            (Token.new_id(get_stream_port_name(arg.name, suffix)),)
                        ),
                    )
                )

        else:  # mmap
            assert arg.cat == Instance.Arg.Cat.MMAP, arg.cat
            for suffix in M_AXI_SUFFIXES:
                full_port_name = get_m_axi_port_name(port_name, suffix)
                if full_port_name not in slot_def_port_names:
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
            expr=Expression((Token.new_id(f"{slot_inst.name}__{signal}"),)),
        )
        for signal in ap_signals
    )

    return ModuleInstantiation(
        name=slot_inst.name,
        hierarchical_name=HierarchicalName.get_name(slot_inst.name),
        module=task_name,
        connections=tuple(connections),
        parameters=(),
        floorplan_region=floorplan_task_name_region_mapping[task_name],
        area=None,
    )


def get_top_ir_subinsts(
    top_task: Task,
    slot_defs: dict[str, AnyModuleDefinition],
    floorplan_task_name_region_mapping: dict[str, str],
    fsm_floorplan_region: str,
) -> list[ModuleInstantiation]:
    """Get leaf module instantiations of slot module."""
    ir_insts = [
        get_top_level_slot_inst(
            slot_defs[inst.task.name],
            inst,
            get_task_arg_table(top_task)[inst.name],
            floorplan_task_name_region_mapping,
        )
        for inst in top_task.instances
    ]

    # fsm
    fsm_module = top_task.fsm_module
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
            floorplan_region=fsm_floorplan_region,
            area=None,
        )
    )

    # fifo
    for fifo_name, fifo in top_task.fifos.items():
        # skip external fifos
        if top_task.is_fifo_external(fifo_name):
            continue
        ir_insts.append(
            get_fifo_inst(
                top_task,
                fifo_name,
                fifo,
                slot_defs,
                True,
                floorplan_task_name_region_mapping[fifo["consumed_by"][0]],
            )
        )

    return ir_insts


def get_top_module_definition(
    top: Task,
    slot_defs: dict[str, AnyModuleDefinition],
    ctrl_s_axi_ir: VerilogModuleDefinition,
    floorplan_task_name_region_mapping: dict[str, str],
) -> GroupedModuleDefinition:
    """Get top module definition."""
    top_ports = get_task_graphir_ports(top.module)
    top_param = get_task_graphir_parameters(top.module)

    # Assign a default region for fsm and ctrl_s_axi instantiation
    default_region = next(iter(floorplan_task_name_region_mapping.values()))

    top_subinsts = get_top_ir_subinsts(
        top,
        slot_defs,
        floorplan_task_name_region_mapping,
        default_region,
    )
    top_subinsts.append(
        get_top_ctrl_s_axi_inst(top, top_param, ctrl_s_axi_ir, default_region)
    )
    top_subinsts.append(get_reset_inverter_inst(default_region))

    top_wires = get_upper_task_ir_wires(
        top,
        slot_defs,
        top_ports,
        list(ctrl_s_axi_ir.ports),
        True,
    )
    top_wires.extend(get_top_extra_wires(ctrl_s_axi_ir))
    top_wires.append(
        ModuleNet(
            name="rst",
            hierarchical_name=HierarchicalName.get_name("rst"),
            range=None,
        )
    )

    return GroupedModuleDefinition(
        name=top.name,
        hierarchical_name=HierarchicalName.get_name(top.name),
        parameters=tuple(top_param),
        ports=tuple(top_ports),
        submodules=tuple(top_subinsts),
        wires=tuple(top_wires),
    )


def get_project_from_floorplanned_program(program: Program) -> Project:  # noqa: PLR0914
    """Get a graphir project from a floorplanned TAPA program."""
    top_task = program.top_task

    slot_tasks = {inst.task.name: inst.task for inst in top_task.instances}
    assert all(task.is_slot for _, task in slot_tasks.items())

    leaf_tasks = {
        inst.task.name: inst.task
        for slot_task in slot_tasks.values()
        for inst in slot_task.instances
    }

    # get non_trimmed code of leaf tasks
    leaf_irs = {}
    for task in leaf_tasks.values():
        full_task_module = Module(
            files=[Path(program.get_rtl_path(task.name))],
            is_trimming_enabled=False,
        )
        leaf_irs[task.name] = get_verilog_module_from_leaf_task(
            task, full_task_module.code
        )
    slot_irs = {
        task.name: get_slot_module_definition(
            task, leaf_irs, program.slot_task_name_to_fp_region[task.name]
        )
        for task in slot_tasks.values()
    }

    with open(
        Path(program.rtl_dir) / f"{top_task.name}_control_s_axi.v",
        encoding="utf-8",
    ) as f:
        ctrl_s_axi_verilog = f.read()

    ctrl_s_axi = get_ctrl_s_axi_def(program.top_task, ctrl_s_axi_verilog)
    top_ir = get_top_module_definition(
        top_task, slot_irs, ctrl_s_axi, program.slot_task_name_to_fp_region
    )

    top_fsm_name = top_task.fsm_module.name
    top_fsm_file = Path(program.rtl_dir) / f"{top_fsm_name}.v"
    top_fsm_def = get_fsm_def(
        top_fsm_name,
        top_fsm_file,
    )

    slot_fsms = [
        get_fsm_def(
            slot_task.fsm_module.name,
            Path(program.get_rtl_path(slot_task.fsm_module.name)),
        )
        for slot_task in slot_tasks.values()
    ]

    all_ir_defs = [
        top_ir,
        ctrl_s_axi,
        top_fsm_def,
        get_fifo_def(),
        # wrap inversion logic in module to avoid logic at top level
        get_reset_inverter_def(),
        *slot_fsms,
        *list(slot_irs.values()),
        *list(leaf_irs.values()),
    ]

    modules_obj = Modules(
        name="$root",
        module_definitions=tuple(all_ir_defs),
        top_name=top_task.name,
    )
    prj = Project(modules=modules_obj)
    prj.ifaces = get_graphir_iface(
        prj,
        slot_tasks.values(),
        top_task,
    )
    return prj
