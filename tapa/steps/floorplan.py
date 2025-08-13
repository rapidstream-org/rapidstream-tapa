"""Floorplan TAPA program and store the program description."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import logging
import subprocess
from collections import defaultdict
from pathlib import Path

import click

from tapa.common.graph import Graph as TapaGraph
from tapa.core import Program
from tapa.steps.common import (
    is_pipelined,
    load_persistent_context,
    load_tapa_program,
    store_persistent_context,
    store_tapa_program,
)

AUTOBRIDGE_WORK_DIR = "autobridge"
_FLOORPLAN_CONFIG_NO_PRE_ASSIGNMENTS = "floorplan_config_no_pre_assignments.json"

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.option(
    "--device-config",
    type=Path,
    required=True,
    help="Path to the device configuration file.",
)
@click.option(
    "--floorplan-config",
    type=Path,
    required=True,
    help="Path to the floorplan configuration file.",
)
def run_autobridge(device_config: Path, floorplan_config: Path) -> None:
    """Run the autobridge tool to generate a floorplan."""
    program = load_tapa_program()
    ab_graph_path = Path(program.work_dir) / "ab_graph.json"
    autobridge_work_dir = Path(program.work_dir) / AUTOBRIDGE_WORK_DIR
    autobridge_work_dir.mkdir(parents=True, exist_ok=True)

    # remove pre_assignments from the floorplan config
    with open(floorplan_config, encoding="utf-8") as f:
        floorplan_config_dict: dict = json.load(f)
    floorplan_config_dict.pop("sys_port_pre_assignments", None)
    floorplan_config_dict.pop("cpp_arg_pre_assignments", None)
    floorplan_config_dict["port_pre_assignments"] = {}

    floorplan_config_no_preassignment = (
        autobridge_work_dir / _FLOORPLAN_CONFIG_NO_PRE_ASSIGNMENTS
    )
    with open(floorplan_config_no_preassignment, "w", encoding="utf-8") as f:
        json.dump(floorplan_config_dict, f)

    cmd = [
        "rapidstream-tapafp",
        "--ab-graph-path",
        str(ab_graph_path),
        "--work-dir",
        str(autobridge_work_dir),
        "--device-config",
        str(device_config),
        "--floorplan-config",
        str(floorplan_config_no_preassignment),
        "--run-floorplan",
    ]

    subprocess.run(cmd, check=True)
    floorplan_files = autobridge_work_dir.glob("solution_*/floorplan.json")
    for floorplan_file in floorplan_files:
        _logger.info("Generated floorplan file: %s", floorplan_file)


@click.command()
@click.option(
    "--floorplan-path",
    type=Path,
    default=None,
    help="Path to the floorplan file. If specified, the floorplan will be applied.",
)
def floorplan(
    floorplan_path: Path | None,
) -> None:
    """Floorplan TAPA program and store the program description."""
    program = load_tapa_program()
    graph_dict = load_persistent_context("graph")
    settings = load_persistent_context("settings")

    tapa_graph = TapaGraph(None, graph_dict)

    fp_slots = []
    if floorplan_path:
        assert program.flattened, "Floorplan can only be applied to a flattened graph"
        slot_to_insts = get_slot_to_inst(floorplan_path, tapa_graph, program)
        tapa_graph = tapa_graph.get_floorplan_graph(slot_to_insts)
        fp_slots = list(slot_to_insts.keys())

        graph_dict = tapa_graph.to_dict()

        new_program = Program(
            obj=graph_dict,
            target=program.target,
            work_dir=program.work_dir,
            floorplan_slots=fp_slots,
            flattened=program.flattened,
            slot_task_name_to_fp_region=program.slot_task_name_to_fp_region,
        )
        store_tapa_program(new_program)

    store_persistent_context("graph", graph_dict)
    settings["floorplan"] = True
    store_persistent_context("settings")
    is_pipelined("floorplan", True)


def get_slot_to_inst(
    floorplan_path: Path, graph: TapaGraph, program: Program
) -> dict[str, list[str]]:
    """Get slot to instance mapping from floorplan file."""
    with open(floorplan_path, encoding="utf-8") as f:
        vertex_to_region = json.load(f)
    slot_to_insts = defaultdict(list)
    task_inst_names = [
        inst.name for inst in graph.get_top_task_inst().get_subtasks_insts()
    ]
    slot_task_name_to_fp_region = {}
    for vertex, region in vertex_to_region.items():
        if vertex not in task_inst_names:
            continue
        slot_name = "_".join(region.split(":"))
        slot_to_insts[slot_name].append(vertex)
        slot_task_name_to_fp_region[slot_name] = convert_region_format(region)

    program.slot_task_name_to_fp_region = slot_task_name_to_fp_region

    return slot_to_insts


def convert_region_format(region: str | None) -> str | None:
    """Convert region format from 'x:y' to 'x_TO_y'."""
    if region is None:
        return None
    return region.replace(":", "_TO_")
