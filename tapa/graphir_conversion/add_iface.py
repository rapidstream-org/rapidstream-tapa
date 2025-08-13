"""Add tapa graphir interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections import defaultdict
from collections.abc import Collection

from tapa.graphir.types import (
    AnyInterface,
    AnyModuleDefinition,
    ApCtrlInterface,
    BaseInterface,
    ClockInterface,
    FalsePathInterface,
    FeedForwardInterface,
    FeedForwardResetInterface,
    HandShakeInterface,
    Interfaces,
    ModulePort,
    NonPipelineInterface,
    Project,
)
from tapa.graphir_conversion.utils import get_m_axi_port_name
from tapa.task import Task
from tapa.verilog.util import sanitize_array_name
from tapa.verilog.xilinx.const import (
    HANDSHAKE_CLK,
    HANDSHAKE_DONE,
    HANDSHAKE_IDLE,
    HANDSHAKE_READY,
    HANDSHAKE_RST_N,
    HANDSHAKE_START,
    ISTREAM_ROLES,
    ISTREAM_SUFFIXES,
    OSTREAM_ROLES,
    OSTREAM_SUFFIXES,
)
from tapa.verilog.xilinx.m_axi import M_AXI_SUFFIXES_BY_CHANNEL

CTRL_S_AXI_FIXED_PORTS = (
    "ACLK",
    "ACLK_EN",
    "ARESET",
    "interrupt",
    "ARADDR",
    "ARREADY",
    "ARVALID",
    "AWADDR",
    "AWREADY",
    "AWVALID",
    "BREADY",
    "BRESP",
    "BVALID",
    "RDATA",
    "RREADY",
    "RRESP",
    "RVALID",
    "WDATA",
    "WREADY",
    "WSTRB",
    "WVALID",
    "ap_start",
    "ap_done",
    "ap_ready",
    "ap_idle",
)


def get_graphir_iface(  # noqa: PLR0914, C901
    project: Project,
    slot_tasks: Collection[Task],
    top_task: Task,
) -> Interfaces:
    """Add tapa graphir interface."""
    ifaces = defaultdict(list)
    scalars = {}

    # slots
    for slot_task in slot_tasks:
        slot_ifaces, slot_scalars = get_upper_task_ir_ifaces_and_scalars(
            project, slot_task, is_top=False
        )
        ifaces[slot_task.name] = slot_ifaces
        scalars[slot_task.name] = slot_scalars

    # top
    top_ifaces, _ = get_upper_task_ir_ifaces_and_scalars(project, top_task, is_top=True)
    ifaces[top_task.name] = top_ifaces

    # fifo
    project.get_module("fifo")
    ifaces["fifo"] = [
        ClockInterface(
            ports=("clk",),
            origin_info="",
        ),
        FeedForwardResetInterface(
            ports=("clk", "reset"),
            clk_port="clk",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "if_din",
                "if_full_n",
                "if_write",
                "clk",
                "reset",
            ),
            clk_port="clk",
            rst_port="reset",
            valid_port="if_write",
            ready_port="if_full_n",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "if_dout",
                "if_empty_n",
                "if_read",
                "clk",
                "reset",
            ),
            clk_port="clk",
            rst_port="reset",
            valid_port="if_empty_n",
            ready_port="if_read",
            origin_info="",
        ),
        FalsePathInterface(
            ports=("if_read_ce",),
            origin_info="",
        ),
        FalsePathInterface(
            ports=("if_write_ce",),
            origin_info="",
        ),
    ]

    # fsm
    # ap_ctrl of each slot
    fsm_ifaces = []
    fsm_name = f"{project.get_top_name()}_fsm"
    fsm_ir = project.get_module(fsm_name)
    for slot_name in scalars:
        ap_ctrl_ports = (
            HANDSHAKE_CLK,
            HANDSHAKE_RST_N,
        )
        for port in fsm_ir.ports:
            if port.name.startswith(f"{slot_name}_0"):
                ap_ctrl_ports += (port.name,)
        fsm_ifaces.append(
            ApCtrlInterface(
                ports=ap_ctrl_ports,
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                ap_start_port=f"{slot_name}_0__{HANDSHAKE_START}",
                ap_done_port=f"{slot_name}_0__{HANDSHAKE_DONE}",
                ap_ready_port=f"{slot_name}_0__{HANDSHAKE_READY}",
                ap_idle_port=f"{slot_name}_0__{HANDSHAKE_IDLE}",
                ap_continue_port=None,
                origin_info="",
            )
        )
    # ap_ctrl of fsm
    slot_names = [slot_task.name for slot_task in slot_tasks]
    fsm_scalars = tuple(
        port.name
        for port in fsm_ir.ports
        if port.name not in {HANDSHAKE_CLK, HANDSHAKE_RST_N}
        and not any(port.name.startswith(slot_name) for slot_name in slot_names)
    )
    ap_ctrl_ports = (
        *fsm_scalars,
        HANDSHAKE_CLK,
        HANDSHAKE_RST_N,
        HANDSHAKE_START,
        HANDSHAKE_DONE,
        HANDSHAKE_READY,
        HANDSHAKE_IDLE,
    )
    fsm_ifaces.append(
        ApCtrlInterface(
            ports=ap_ctrl_ports,
            clk_port="ap_clk",
            rst_port="ap_rst_n",
            ap_start_port=HANDSHAKE_START,
            ap_done_port=HANDSHAKE_DONE,
            ap_ready_port=HANDSHAKE_READY,
            ap_idle_port=HANDSHAKE_IDLE,
            ap_continue_port=None,
            origin_info="",
        )
    )
    ifaces[fsm_name] = fsm_ifaces

    # ctrl_s_axi
    ctrl_s_axi_name = f"{project.get_top_name()}_control_s_axi"
    ctrl_s_axi_ir = project.get_module(ctrl_s_axi_name)
    ctrl_s_axi_scalars = [
        port.name
        for port in ctrl_s_axi_ir.ports
        if port.name not in CTRL_S_AXI_FIXED_PORTS
    ]
    ifaces[ctrl_s_axi_name] = [
        ClockInterface(
            ports=("ACLK",),
            origin_info="",
        ),
        FeedForwardResetInterface(
            ports=("ACLK", "ARESET"),
            clk_port="ACLK",
            origin_info="",
        ),
        FalsePathInterface(
            ports=("ACLK_EN",),
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "ARADDR",
                "ARREADY",
                "ARVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            valid_port="ARVALID",
            ready_port="ARREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "AWADDR",
                "AWREADY",
                "AWVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            valid_port="AWVALID",
            ready_port="AWREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "BREADY",
                "BRESP",
                "BVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            valid_port="BVALID",
            ready_port="BREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "RDATA",
                "RREADY",
                "RRESP",
                "RVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            valid_port="RVALID",
            ready_port="RREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "WDATA",
                "WREADY",
                "WSTRB",
                "WVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            valid_port="WVALID",
            ready_port="WREADY",
            origin_info="",
        ),
        FeedForwardInterface(
            ports=("ACLK", "ARESET", "interrupt"),
            clk_port="ACLK",
            rst_port="ARESET",
            origin_info="",
        ),
        ApCtrlInterface(
            ports=(
                *ctrl_s_axi_scalars,
                "ACLK",
                "ARESET",
                "ap_start",
                "ap_done",
                "ap_ready",
                "ap_idle",
            ),
            clk_port="ACLK",
            rst_port="ARESET",
            ap_start_port="ap_start",
            ap_done_port="ap_done",
            ap_ready_port="ap_ready",
            ap_idle_port="ap_idle",
            ap_continue_port=None,
            origin_info="",
        ),
    ]

    ifaces["reset_inverter"] = [
        ClockInterface(
            ports=("clk",),
            origin_info="",
        ),
        FeedForwardResetInterface(
            ports=("clk", "rst_n"),
            clk_port="clk",
            origin_info="",
        ),
        FeedForwardResetInterface(
            ports=("clk", "rst"),
            clk_port="clk",
            origin_info="",
        ),
    ]

    # set role on all interfaces
    modules = project.modules.module_definitions
    for module in modules:
        if module.name not in ifaces:
            continue

        ifaces[module.name] = [
            set_iface_role(module, iface) for iface in ifaces[module.name]
        ]

    # check all top module submodule ports in ifaces
    top_submodules = [
        project.get_module(inst.module) for inst in project.get_top_module().submodules
    ]
    for module in top_submodules:
        module_ifaces = ifaces[module.name]
        for port in module.ports:
            found = False
            for iface in module_ifaces:
                if port.name in iface.ports:
                    found = True
                    break
            if not found:
                # pass
                msg = (
                    f"Port {port.name} of module {module.name} not found in interfaces."
                )
                raise ValueError(msg)

    return Interfaces(ifaces)


def get_upper_task_ir_ifaces_and_scalars(
    project: Project,
    task: Task,
    is_top: bool,
) -> tuple[list[AnyInterface], list[str]]:
    """Get the interface of the upper task IR."""
    ifaces = []
    scalars = []
    task_ir = project.get_module(task.name)
    ir_ports = [port.name for port in task_ir.ports]
    for port_name, port in task.ports.items():
        if port.cat.is_scalar:
            scalars.append(port_name)

        elif port.cat.is_istream or port.cat.is_istreams:
            real_port_name = sanitize_array_name(port_name)
            ports = tuple(f"{real_port_name}{suffix}" for suffix in ISTREAM_SUFFIXES)
            ports += HANDSHAKE_CLK, HANDSHAKE_RST_N
            valid_port = f"{real_port_name}{ISTREAM_ROLES['valid']}"
            ready_port = f"{real_port_name}{ISTREAM_ROLES['ready']}"
            ifaces.append(
                HandShakeInterface(
                    ports=ports,
                    clk_port=HANDSHAKE_CLK,
                    rst_port=HANDSHAKE_RST_N,
                    valid_port=valid_port,
                    ready_port=ready_port,
                    origin_info="",
                )
            )

        elif port.cat.is_ostream or port.cat.is_ostreams:
            real_port_name = sanitize_array_name(port_name)
            ports = tuple(f"{real_port_name}{suffix}" for suffix in OSTREAM_SUFFIXES)
            ports += HANDSHAKE_CLK, HANDSHAKE_RST_N
            ready_port = f"{real_port_name}{OSTREAM_ROLES['ready']}"
            valid_port = f"{real_port_name}{OSTREAM_ROLES['valid']}"
            ifaces.append(
                HandShakeInterface(
                    ports=ports,
                    clk_port=HANDSHAKE_CLK,
                    rst_port=HANDSHAKE_RST_N,
                    valid_port=valid_port,
                    ready_port=ready_port,
                    origin_info="",
                )
            )

        elif port.cat.is_mmap:
            # offset
            scalars.append(f"{port_name}_offset")
            for channel in M_AXI_SUFFIXES_BY_CHANNEL.values():
                channel_ports = []
                for suffix in channel["ports"]:
                    ir_port_name = get_m_axi_port_name(port_name, suffix)
                    if ir_port_name not in ir_ports:
                        # not all suffixes are necessary
                        continue
                    channel_ports.append(ir_port_name)
                channel_ports.extend(
                    [
                        HANDSHAKE_CLK,
                        HANDSHAKE_RST_N,
                    ]
                )
                valid_port = get_m_axi_port_name(port_name, channel["valid"])
                ready_port = get_m_axi_port_name(port_name, channel["ready"])
                ifaces.append(
                    HandShakeInterface(
                        ports=tuple(channel_ports),
                        clk_port=HANDSHAKE_CLK,
                        rst_port=HANDSHAKE_RST_N,
                        valid_port=valid_port,
                        ready_port=ready_port,
                        origin_info="",
                    )
                )

        else:
            msg = (
                f"Unsupported port category {port.cat} for port "
                f"{port_name} in task {task.name}"
            )
            raise ValueError(msg)

    if is_top:
        # s_axi_ctrl
        ifaces.append(
            HandShakeInterface(
                ports=(
                    "s_axi_control_ARADDR",
                    "s_axi_control_ARREADY",
                    "s_axi_control_ARVALID",
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                valid_port="s_axi_control_ARVALID",
                ready_port="s_axi_control_ARREADY",
                origin_info="",
            )
        )
        ifaces.append(
            HandShakeInterface(
                ports=(
                    "s_axi_control_AWADDR",
                    "s_axi_control_AWREADY",
                    "s_axi_control_AWVALID",
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                valid_port="s_axi_control_AWVALID",
                ready_port="s_axi_control_AWREADY",
                origin_info="",
            )
        )
        ifaces.append(
            HandShakeInterface(
                ports=(
                    "s_axi_control_BREADY",
                    "s_axi_control_BRESP",
                    "s_axi_control_BVALID",
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                valid_port="s_axi_control_BVALID",
                ready_port="s_axi_control_BREADY",
                origin_info="",
            )
        )
        ifaces.append(
            HandShakeInterface(
                ports=(
                    "s_axi_control_RDATA",
                    "s_axi_control_RREADY",
                    "s_axi_control_RRESP",
                    "s_axi_control_RVALID",
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                valid_port="s_axi_control_RVALID",
                ready_port="s_axi_control_RREADY",
                origin_info="",
            )
        )
        ifaces.append(
            HandShakeInterface(
                ports=(
                    "s_axi_control_WDATA",
                    "s_axi_control_WREADY",
                    "s_axi_control_WSTRB",
                    "s_axi_control_WVALID",
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                valid_port="s_axi_control_WVALID",
                ready_port="s_axi_control_WREADY",
                origin_info="",
            )
        )
    else:
        # ap_ctrl
        ap_ctrl_ports = (
            *tuple(scalars),
            HANDSHAKE_CLK,
            HANDSHAKE_RST_N,
            HANDSHAKE_START,
            HANDSHAKE_DONE,
            HANDSHAKE_READY,
            HANDSHAKE_IDLE,
        )

        ifaces.append(
            ApCtrlInterface(
                ports=ap_ctrl_ports,
                clk_port=HANDSHAKE_CLK,
                rst_port=HANDSHAKE_RST_N,
                ap_start_port=HANDSHAKE_START,
                ap_done_port=HANDSHAKE_DONE,
                ap_ready_port=HANDSHAKE_READY,
                ap_idle_port=HANDSHAKE_IDLE,
                ap_continue_port=None,
                origin_info="",
            )
        )

        # clk iface
        ifaces.append(ClockInterface(ports=(HANDSHAKE_CLK,), origin_info=""))

        # rst iface
        ifaces.append(
            FeedForwardResetInterface(
                ports=(
                    HANDSHAKE_CLK,
                    HANDSHAKE_RST_N,
                ),
                clk_port=HANDSHAKE_CLK,
                origin_info="",
            )
        )

    return ifaces, scalars


def set_iface_role(module: AnyModuleDefinition, iface: AnyInterface) -> AnyInterface:
    """Set the role of the interface based on port directions."""
    name_to_port = {p.name: p for p in module.ports}
    return _set_iface_role_by_ports(name_to_port, iface, module.name)


def _set_as_sink(iface: AnyInterface) -> AnyInterface:
    """Return a copy of the interface whose role is set as sink."""
    return iface.updated(role=BaseInterface.InterfaceRole.SINK)


def _set_as_source(iface: AnyInterface) -> AnyInterface:
    """Return a copy of the interface whose role is set as source."""
    return iface.updated(role=BaseInterface.InterfaceRole.SOURCE)


def _set_hs_iface_role_by_ports(
    name_to_port: dict[str, ModulePort], iface: HandShakeInterface, module_name: str
) -> AnyInterface:
    """Set the role of the handshake interface based on port directions."""
    valid_port = name_to_port[iface.valid_port]
    ready_port = name_to_port[iface.ready_port]
    data_ports = [name_to_port[p] for p in iface.get_data_ports()]

    input_data_ports = {p.name for p in data_ports if p.is_input_port()}
    output_data_ports = {p.name for p in data_ports if p.is_output_port()}

    if valid_port.is_input_port() and ready_port.is_output_port():
        if output_data_ports:
            msg = (
                f"Incorrect handshake in {module_name}. Data ports should have same "
                f"direction to the valid port. The valid port {valid_port} is input, "
                f"but these data ports are output {output_data_ports}: {iface}"
            )
            raise ValueError(msg)

        return _set_as_sink(iface)

    if valid_port.is_output_port() and ready_port.is_input_port():
        if input_data_ports:
            msg = (
                f"Incorrect handshake in {module_name}. Data ports should have same "
                f"direction to the valid port. The valid port {valid_port} is output, "
                f"but these data ports are input {input_data_ports}: {iface}"
            )
            raise ValueError(msg)

        return _set_as_source(iface)

    msg = (
        f"Incorrect handshake in {module_name}. The valid port {valid_port} and "
        f"ready port {ready_port} should be of opposite directions: {iface}"
    )
    raise ValueError(msg)


def _set_ap_ctrl_iface_role_by_ports(
    name_to_port: dict[str, ModulePort], iface: ApCtrlInterface, module_name: str
) -> AnyInterface:
    """Set the role of the ap_ctrl interface based on port directions."""
    ap_start_port = name_to_port[iface.ap_start_port]
    ap_ready_port = name_to_port[iface.ap_ready_port] if iface.ap_ready_port else None
    ap_done_port = name_to_port[iface.ap_done_port] if iface.ap_done_port else None
    ap_idle_port = name_to_port[iface.ap_idle_port] if iface.ap_idle_port else None
    ap_continue_port = (
        name_to_port[iface.ap_continue_port] if iface.ap_continue_port else None
    )

    if ap_start_port.is_input_port():
        if not (
            (ap_ready_port is None or ap_ready_port.is_output_port())
            and (ap_done_port is None or ap_done_port.is_output_port())
            and (ap_idle_port is None or ap_idle_port.is_output_port())
            and (ap_continue_port is None or ap_continue_port.is_input_port())
        ):
            msg = (
                f"Incorrect ap_ctrl port direction in {module_name}. When the "
                f"ap_start port is input, the ap_ready, ap_done, and ap_idle ports "
                f"should be the output ports, and the ap_continue ports should be "
                f"the input ports. {iface}"
            )
            raise ValueError(msg)

        return _set_as_sink(iface)

    if ap_start_port.is_output_port():
        if not (
            (ap_ready_port is None or ap_ready_port.is_input_port())
            and (ap_done_port is None or ap_done_port.is_input_port())
            and (ap_idle_port is None or ap_idle_port.is_input_port())
            and (ap_continue_port is None or ap_continue_port.is_output_port())
        ):
            msg = (
                f"Incorrect ap_ctrl port direction in {module_name}. When the "
                f"ap_start port is output, the ap_ready, ap_done, and ap_idle ports "
                f"should be the input ports, and the ap_continue ports should be "
                f"the output ports. {iface}"
            )
            raise ValueError(msg)

        return _set_as_source(iface)

    msg = f"Unknown ap_start port direction {type(iface)}."
    raise NotImplementedError(msg)


def _set_iface_role_by_ports(
    name_to_port: dict[str, ModulePort], iface: AnyInterface, module_name: str
) -> AnyInterface:
    """Set the role of the interface based on port directions."""
    # for handshake interfaces, the role is determined by valid_port
    if isinstance(iface, HandShakeInterface):
        return _set_hs_iface_role_by_ports(name_to_port, iface, module_name)

    # for ap_ctrl interfaces, the role is determined by ap_start
    if isinstance(iface, ApCtrlInterface):
        return _set_ap_ctrl_iface_role_by_ports(name_to_port, iface, module_name)

    # for feedforward interfaces, the role is determined by direction of data ports
    if isinstance(
        iface,
        (
            FeedForwardInterface,
            FalsePathInterface,
        ),
    ):
        data_ports = [name_to_port[p] for p in iface.get_data_ports()]
        if all(p.is_input_port() for p in data_ports):
            return _set_as_sink(iface)
        if all(p.is_output_port() for p in data_ports):
            return _set_as_source(iface)
        msg = (
            f"Incorrect {iface.type} interface in {module_name}. The ports should "
            f"be of the same direction. {iface}"
        )
        raise ValueError(msg)

    # no need to set iface role for non-pipeline interfaces
    if isinstance(iface, NonPipelineInterface):
        return iface

    msg = f"Cannot infer role of {type(iface)}."
    raise NotImplementedError(msg)
