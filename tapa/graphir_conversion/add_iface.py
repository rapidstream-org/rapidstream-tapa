"""Add tapa graphir interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from collections import defaultdict

from tapa.graphir.types import (
    ApCtrlInterface,
    ClockInterface,
    FalsePathInterface,
    FeedForwardInterface,
    FeedForwardResetInterface,
    HandShakeInterface,
    Interfaces,
    Project,
)
from tapa.graphir_conversion.utils import FIFO_PORT_PATTERN, get_m_axi_port_name
from tapa.task import Task
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
    "ARESETN",
    "interrupt",
    "s_axi_control_ARADDR",
    "s_axi_control_ARREADY",
    "s_axi_control_ARVALID",
    "s_axi_control_AWADDR",
    "s_axi_control_AWREADY",
    "s_axi_control_AWVALID",
    "s_axi_control_BREADY",
    "s_axi_control_BRESP",
    "s_axi_control_BVALID",
    "s_axi_control_RDATA",
    "s_axi_control_RREADY",
    "s_axi_control_RRESP",
    "s_axi_control_RVALID",
    "s_axi_control_WDATA",
    "s_axi_control_WREADY",
    "s_axi_control_WSTRB",
    "s_axi_control_WVALID",
    "ap_start",
    "ap_done",
    "ap_ready",
    "ap_idle",
)


def get_graphir_iface(  # noqa: C901, PLR0912, PLR0915, PLR0914
    project: Project,
    slot_tasks: list[Task],
) -> Interfaces:
    """Add tapa graphir interface."""
    ifaces = defaultdict(list)
    scalars = {}

    for slot_task in slot_tasks:  # noqa: PLR1702
        slot_ifaces = []
        slot_scalars = []
        slot_ir = project.get_module(slot_task.name)
        slot_ir_ports = [port.name for port in slot_ir.ports]
        for port_name, port in slot_task.ports.items():
            if port.cat.is_scalar:
                slot_scalars.append(port_name)

            elif port.cat.is_istream:
                match = re.match(FIFO_PORT_PATTERN, port_name)
                if match:
                    real_port_name = f"{match.group(1)}_{match.group(2)}"
                else:
                    real_port_name = port_name
                ports = tuple(
                    f"{real_port_name}{suffix}" for suffix in ISTREAM_SUFFIXES
                )
                ports += HANDSHAKE_CLK, HANDSHAKE_RST_N
                valid_port = f"{real_port_name}{ISTREAM_ROLES['valid']}"
                ready_port = f"{real_port_name}{ISTREAM_ROLES['ready']}"
                slot_ifaces.append(
                    HandShakeInterface(
                        ports=ports,
                        clk_port=HANDSHAKE_CLK,
                        rst_port=HANDSHAKE_RST_N,
                        valid_port=valid_port,
                        ready_port=ready_port,
                        origin_info="",
                    )
                )

            elif port.cat.is_ostream:
                match = re.match(FIFO_PORT_PATTERN, port_name)
                if match:
                    real_port_name = f"{match.group(1)}_{match.group(2)}"
                else:
                    real_port_name = port_name
                ports = tuple(
                    f"{real_port_name}{suffix}" for suffix in OSTREAM_SUFFIXES
                )
                ports += HANDSHAKE_CLK, HANDSHAKE_RST_N
                ready_port = f"{real_port_name}{OSTREAM_ROLES['ready']}"
                valid_port = f"{real_port_name}{OSTREAM_ROLES['valid']}"
                slot_ifaces.append(
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
                slot_scalars.append(f"{port_name}_offset")
                for channel in M_AXI_SUFFIXES_BY_CHANNEL.values():
                    channel_ports = []
                    for suffix in channel["ports"]:
                        ir_port_name = get_m_axi_port_name(port_name, suffix)
                        if ir_port_name not in slot_ir_ports:
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
                    slot_ifaces.append(
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
                    f"{port_name} in task {slot_task.name}"
                )
                raise ValueError(msg)

            # ap_ctrl
            ap_ctrl_ports = (
                *tuple(slot_scalars),
                HANDSHAKE_CLK,
                HANDSHAKE_RST_N,
                HANDSHAKE_START,
                HANDSHAKE_DONE,
                HANDSHAKE_READY,
                HANDSHAKE_IDLE,
            )

            slot_ifaces.append(
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
            slot_ifaces.append(ClockInterface(ports=(HANDSHAKE_CLK,), origin_info=""))

            # rst iface
            slot_ifaces.append(
                FeedForwardResetInterface(
                    ports=(
                        HANDSHAKE_CLK,
                        HANDSHAKE_RST_N,
                    ),
                    clk_port=HANDSHAKE_CLK,
                    origin_info="",
                )
            )

        ifaces[slot_task.name] = slot_ifaces
        scalars[slot_task.name] = slot_scalars

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
                "if_dout",
                "if_empty_n",
                "if_read",
                "if_din",
                "if_full_n",
                "if_write",
                HANDSHAKE_CLK,
                HANDSHAKE_RST_N,
            ),
            clk_port=HANDSHAKE_CLK,
            rst_port=HANDSHAKE_RST_N,
            valid_port="if_write",
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
    for slot_name, slot_scalars in scalars.items():
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
                ap_start_port=f"{slot_name}_{HANDSHAKE_START}",
                ap_done_port=f"{slot_name}_{HANDSHAKE_DONE}",
                ap_ready_port=f"{slot_name}_{HANDSHAKE_READY}",
                ap_idle_port=f"{slot_name}_{HANDSHAKE_IDLE}",
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
            ports=("ACLK", "ARESETN"),
            clk_port="ACLK",
            origin_info="",
        ),
        FalsePathInterface(
            ports=("ACLK_EN",),
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "s_axi_control_ARADDR",
                "s_axi_control_ARREADY",
                "s_axi_control_ARVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            valid_port="s_axi_control_ARVALID",
            ready_port="s_axi_control_ARREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "s_axi_control_AWADDR",
                "s_axi_control_AWREADY",
                "s_axi_control_AWVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            valid_port="s_axi_control_AWVALID",
            ready_port="s_axi_control_AWREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "s_axi_control_BREADY",
                "s_axi_control_BRESP",
                "s_axi_control_BVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            valid_port="s_axi_control_BVALID",
            ready_port="s_axi_control_BREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "s_axi_control_RDATA",
                "s_axi_control_RREADY",
                "s_axi_control_RRESP",
                "s_axi_control_RVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            valid_port="s_axi_control_RVALID",
            ready_port="s_axi_control_RREADY",
            origin_info="",
        ),
        HandShakeInterface(
            ports=(
                "s_axi_control_WDATA",
                "s_axi_control_WREADY",
                "s_axi_control_WSTRB",
                "s_axi_control_WVALID",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            valid_port="s_axi_control_WVALID",
            ready_port="s_axi_control_WREADY",
            origin_info="",
        ),
        FeedForwardInterface(
            ports=("ACLK", "ARESETN", "interrupt"),
            clk_port="ACLK",
            rst_port="ARESETN",
            origin_info="",
        ),
        ApCtrlInterface(
            ports=(
                *ctrl_s_axi_scalars,
                "ACLK",
                "ARESETN",
                "ap_start",
                "ap_done",
                "ap_ready",
                "ap_idle",
            ),
            clk_port="ACLK",
            rst_port="ARESETN",
            ap_start_port="ap_start",
            ap_done_port="ap_done",
            ap_ready_port="ap_ready",
            ap_idle_port="ap_idle",
            ap_continue_port=None,
            origin_info="",
        ),
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
