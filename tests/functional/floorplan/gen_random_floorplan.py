"""Generate random floorplan for test."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import random
import sys
from pathlib import Path

_ARG_NUMBER = 4
_SLOT_FORMAT = "SLOT_X{x}Y{y}:SLOT_X{x}Y{y}"
_APP_TO_LEAVES = {
    "bandwidth": [
        "Bandwidth_fsm",
        "Copy_0",
        "Copy_1",
        "Copy_2",
        "Copy_3",
        "chan_0",
        "chan_1",
        "chan_2",
        "chan_3",
    ],
    "cannon": [
        "Gather_0",
        "ProcElem_0",
        "ProcElem_1",
        "ProcElem_2",
        "ProcElem_3",
        "Scatter_0",
        "Scatter_1",
        "a_vec",
        "b_vec",
        "b_vec",
    ],
    "gemv": [
        "GemvCore_0",
        "GemvCore_1",
        "mat_a",
        "vec_x",
        "vec_y",
    ],
    "graph": [
        "Control_0",
        "Graph_fsm",
        "ProcElem_0",
        "UpdateHandler_0",
        "edges",
        "num_edges",
        "num_vertices",
        "updates",
        "vertices",
    ],
    "jacobi": [
        "Jacobi_fsm",
        "Mmap2Stream_0",
        "Module0Func_0",
        "Module1Func_0",
        "Module1Func_1",
        "Module1Func_2",
        "Module1Func_3",
        "Module1Func_4",
        "Module1Func_5",
        "Module3Func1_0",
        "Module3Func2_0",
        "Module6Func1_0",
        "Module6Func2_0",
        "Module8Func_0",
        "Stream2Mmap_0",
        "bank_0_t0",
        "bank_0_t1",
    ],
    "network": [
        "Consume_0",
        "Network_fsm",
        "Produce_0",
        "Switch2x2_0",
        "Switch2x2_1",
        "Switch2x2_10",
        "Switch2x2_11",
        "Switch2x2_2",
        "Switch2x2_3",
        "Switch2x2_4",
        "Switch2x2_5",
        "Switch2x2_6",
        "Switch2x2_7",
        "Switch2x2_8",
        "Switch2x2_9",
        "mmap_in",
        "mmap_out",
    ],
    "shared_mmap": [
        "Add_0",
        "Mmap2Stream_0",
        "Mmap2Stream_1",
        "Stream2Mmap_0",
        "VecAddShared_fsm",
        "elems",
    ],
    "vadd": [
        "Add_0",
        "Mmap2Stream_0",
        "Mmap2Stream_1",
        "Stream2Mmap_0",
        "VecAdd_fsm",
        "a",
        "b",
        "c",
    ],
}


def gen_random_floorplan(i: int, app: str, output_path: Path) -> None:
    """Generate random floorplan for test."""
    floorplan = {}
    random.seed(i)
    for leaf in _APP_TO_LEAVES[app]:
        x = random.randint(0, 3)
        y = random.randint(0, 3)
        floorplan[leaf] = _SLOT_FORMAT.format(x=x, y=y)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(floorplan, f, indent=4)


if __name__ == "__main__":
    assert len(sys.argv) == _ARG_NUMBER, (
        "Usage: python gen_random_floorplan.py <index> <app>"
    )
    gen_random_floorplan(int(sys.argv[1]), sys.argv[2], Path(sys.argv[3]))
