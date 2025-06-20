"""Floorplan TAPA program and store the program description."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
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
        )
        new_program.floorplan_task_name_to_region_mapping = (
            program.floorplan_task_name_to_region_mapping
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
    floorplan_task_name_to_region_mapping = {}
    for vertex, region in vertex_to_region.items():
        if vertex not in task_inst_names:
            continue
        slot_name = "_".join(region.split(":"))
        slot_to_insts[slot_name].append(vertex)
        floorplan_task_name_to_region_mapping[slot_name] = region.replace(":", "_TO_")

    program.floorplan_task_name_to_region_mapping = (
        floorplan_task_name_to_region_mapping
    )

    return slot_to_insts
