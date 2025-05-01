# ruff: noqa: INP001

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import json
import logging
import os

import pytest
from python.runfiles import Runfiles

from tapa.common.graph import Graph

_GRAPH_PATH = "_main/tapa/common/floorplan_reconstruction_test/graph/{}-graph.json"
_GOLDEN_PATH = "_main/tapa/common/floorplan_reconstruction_test/golden/{}-golden.json"
_SLOT_INFO_PATH = "_main/tapa/common/floorplan_reconstruction_test/slot/{}-slot.json"
_NEW_GOLDEN_FILE = "{}-new-golden.json"

_logger = logging.getLogger(__name__)


@pytest.mark.parametrize("test_name", ["vadd"])
def test_abgraph(test_name: str) -> None:
    """Test if the generated slot matches the golden.

    The new golden file will be generated in the bazel sandbox tmp directory. Manually
    updated the golden file to the test directory if needed.
    """
    # Access bazel runfiles
    runfiles = Runfiles.Create()
    assert runfiles is not None
    input_graph_path = runfiles.Rlocation(_GRAPH_PATH.format(test_name))
    assert input_graph_path is not None
    golden_path = runfiles.Rlocation(_GOLDEN_PATH.format(test_name))
    assert golden_path is not None
    slot_info_path = runfiles.Rlocation(_SLOT_INFO_PATH.format(test_name))
    assert slot_info_path is not None

    with open(input_graph_path, encoding="utf-8") as f:
        graph = Graph(f"{test_name}-graph", json.load(f))
    with open(golden_path, encoding="utf-8") as f:
        goldens = json.load(f)
    with open(slot_info_path, encoding="utf-8") as f:
        slots = json.load(f)

    assert isinstance(slots, dict)
    assert isinstance(goldens, dict)
    slots = {}
    for slot_name, slot_tasks in slots.items():
        assert isinstance(slot_tasks, list)
        slot_obj = graph.get_floorplan_slot(
            slot_name, slot_tasks, graph.get_top_task_inst()
        ).to_dict()
        slots[slot_name] = slot_obj

    test_tmpdir = os.getenv("TEST_TMPDIR", "/tmp")
    new_golden_path = os.path.join(test_tmpdir, _NEW_GOLDEN_FILE.format(test_name))
    _logger.info("Writing new golden to %s", new_golden_path)

    with open(new_golden_path, "w", encoding="utf-8") as f:
        json.dump(slots, f, indent=2)

    for slot_name, slot_obj in slots.items():
        # slot_obj contains tuple, and json.dumps convert tuple to json array
        # so we need to match the format
        assert json.loads(json.dumps(slot_obj)) == goldens[slot_name]
