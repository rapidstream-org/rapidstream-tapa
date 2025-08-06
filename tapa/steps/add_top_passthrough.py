"""Add top passthrough to TAPA program graphir."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import logging
import re
import subprocess
from pathlib import Path

import click

from tapa.core import Program
from tapa.graphir.types import (
    ClockInterface,
    FalsePathResetInterface,
    FeedForwardInterface,
    FeedForwardResetInterface,
    HandShakeInterface,
    ModuleInstantiation,
    Project,
)
from tapa.steps.common import load_tapa_program
from tapa.steps.floorplan import convert_region_format
from tapa.verilog.xilinx.const import ISTREAM_SUFFIXES, OSTREAM_SUFFIXES
from tapa.verilog.xilinx.m_axi import M_AXI_PREFIX, M_AXI_SUFFIXES

ADD_PASSTHROUGH_PATH = "graphir_add_passthrough.json"
PASSTHROUGH_PREFIX = "__rs_pt_"

TAPA_TOP_IFACES_TYPES = (
    HandShakeInterface,
    ClockInterface,
    FalsePathResetInterface,
    FeedForwardResetInterface,
    FeedForwardInterface,
)

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.option(
    "--floorplan-config",
    type=Path,
    help="Path to the floorplan configuration file.",
)
@click.option(
    "--floorplan-path",
    type=Path,
    default=None,
    help=(
        "Path to the autobridge floorplan file. "
        "If specified, the floorplan will be applied."
    ),
)
def add_top_passthrough(floorplan_config: Path, floorplan_path: Path) -> None:
    """Add top passthrough to TAPA program graphir."""
    program = load_tapa_program()
    input_graphir_path = Path(program.work_dir) / "graphir.json"
    if not input_graphir_path.exists():
        msg = f"GraphIR file not found: {input_graphir_path}"
        raise FileNotFoundError(msg)
    output_graphir_path = Path(program.work_dir) / ADD_PASSTHROUGH_PATH

    cmd = [
        "rapidstream-optimizer",
        "-i",
        input_graphir_path,
        "-o",
        str(output_graphir_path),
        "flatten",
    ]

    with open(input_graphir_path, encoding="utf-8") as f:
        tapa_project: Project = Project.model_validate_json(f.read())

    for submodule in tapa_project.get_top_module().submodules:
        cmd.extend(
            [
                "--stop-at-modules",
                submodule.module,
            ]
        )

    subprocess.run(cmd, check=True)

    with open(output_graphir_path, encoding="utf-8") as f:
        tapa_project: Project = Project.model_validate_json(f.read())

    with open(floorplan_config, encoding="utf-8") as f:
        floorplan_config_dict = json.load(f)
    with open(floorplan_path, encoding="utf-8") as f:
        autobridge_floorplan_result = json.load(f)

    assert "sys_port_pre_assignments" in floorplan_config_dict, (
        "Missing 'sys_port_pre_assignments' in floorplan configuration."
    )
    sys_port_pre_assignments = floorplan_config_dict["sys_port_pre_assignments"]

    add_region_to_passthrough_modules(
        tapa_project,
        extract_port_to_region_mapping(
            program, sys_port_pre_assignments, autobridge_floorplan_result
        ),
    )

    with open(output_graphir_path, "w", encoding="utf-8") as f:
        f.write(tapa_project.model_dump_json(indent=2))

    _logger.info("Top passthrough added to GraphIR: %s", output_graphir_path)


def add_region_to_passthrough_modules(
    project: Project, port_pattern_to_region: dict[str, str]
) -> None:
    """Add region to passthrough modules in the project."""
    updated_submodules: list[ModuleInstantiation] = []
    for inst in project.get_top_module().submodules:
        if not inst.name.startswith(PASSTHROUGH_PREFIX):
            continue

        # check connected top ports
        connected_top_ports = get_inst_connected_top_ports(project, inst)

        # match pattern in fp config
        region = None
        for port_pattern, current_region in port_pattern_to_region.items():
            for port in connected_top_ports:
                # match port name with pattern
                if re.fullmatch(port_pattern, port):
                    assert region is None or region == current_region, (
                        f"Multiple regions found for passthrough module {inst.name}"
                    )
                    region = current_region
        assert region is not None, (
            f"No matching region found for passthrough module {inst.name}"
        )

        # add region to module
        updated_inst: ModuleInstantiation = inst.updated(
            floorplan_region=convert_region_format(region)
        )
        _logger.info(
            "Added region '%s' to passthrough module '%s'",
            region,
            inst.name,
        )
        updated_submodules.append(updated_inst)

    # update project with new submodules
    pt_inst_names = {inst.name for inst in updated_submodules}
    updated_submodules.extend(
        submodule
        for submodule in project.get_top_module().submodules
        if submodule.name not in pt_inst_names
    )

    updated_top_def = project.get_top_module().updated(submodules=updated_submodules)
    project.add_and_override_modules((updated_top_def,))

    for inst in project.get_top_module().submodules:
        assert inst.floorplan_region is not None, (
            f"Passthrough module {inst.name} does not have a floorplan region."
        )

    _logger.info("Updated passthrough modules with regions in the project.")


def get_inst_connected_top_ports(
    project: Project, inst: ModuleInstantiation
) -> list[str]:
    """Get the list of top ports connected to the given instance."""
    top_ports = [p.name for p in project.get_top_module().ports]
    connected_top_ports = []
    inst_data_ports = []
    assert project.ifaces

    ifaces = project.ifaces[inst.module]
    # if passthrough module only has clk and feedforward reset,
    # use region of the feedforward reset interface
    # else use region of the handshake interface
    is_ff_reset_pt = all(
        isinstance(iface, (ClockInterface, FeedForwardResetInterface))
        for iface in ifaces
    )
    for iface in ifaces:
        assert isinstance(iface, TAPA_TOP_IFACES_TYPES), (
            f"Unsupported interface type: {type(iface)} {inst.name}"
        )
        if (
            isinstance(iface, FeedForwardResetInterface) and is_ff_reset_pt
        ) or isinstance(iface, (HandShakeInterface, FeedForwardInterface)):
            inst_data_ports.extend(iface.get_data_ports())

    for connection in inst.connections:
        for port in inst_data_ports:
            if connection.name == port:
                ids = connection.expr.get_used_identifiers()
                connected_top_ports.extend(id_ for id_ in ids if id_ in top_ports)

    return connected_top_ports


def extract_port_to_region_mapping(
    program: Program,
    sys_port_pre_assignments: dict[str, str],
    autobridge_floorplan_result: dict[str, str],
) -> dict[str, str]:
    """Extract port to region mapping from the floorplan configuration."""
    port_pattern_to_region = {}
    for port_name, port in program.top_task.ports.items():
        if port.cat.is_scalar:
            continue  # Skip scalar ports

        assert port_name in autobridge_floorplan_result, (
            f"Port '{port_name}' not found in floorplan configuration."
        )

        region = autobridge_floorplan_result[port_name]
        if port.cat.is_mmap:
            port_pattern_to_region[
                f"{M_AXI_PREFIX}{port_name}({'|'.join(M_AXI_SUFFIXES)})"
            ] = region
        elif port.cat.is_istream:
            port_pattern_to_region[f"{port_name}.{'|'.join(ISTREAM_SUFFIXES)}"] = region
        elif port.cat.is_ostream:
            port_pattern_to_region[f"{port_name}.{'|'.join(OSTREAM_SUFFIXES)}"] = region
        else:
            msg = f"Unsupported port category: {port.cat}"
            raise ValueError(msg)

    updated_sys_port_pre_assignments = {}
    for port_name, region in sys_port_pre_assignments.items():
        slot0, slot1 = region.split(":")
        if slot0 != slot1:
            new_region = f"{slot0}:{slot0}"
            _logger.warning(
                "Region across multiple slots not supported for"
                " sys port, assigning sys port '%s' to %s.",
                port_name,
                new_region,
            )
            updated_sys_port_pre_assignments[port_name] = new_region
        else:
            updated_sys_port_pre_assignments[port_name] = region
    port_pattern_to_region |= updated_sys_port_pre_assignments
    return port_pattern_to_region
