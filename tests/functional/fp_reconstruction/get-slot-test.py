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

GRAPH_PATH = "_main/tests/functional/fp_reconstruction/graph/{}-graph.json"
GOLDEN_PATH = "_main/tests/functional/fp_reconstruction/golden/{}-golden.json"
SLOT_INFO_PATH = "_main/tests/functional/fp_reconstruction/slot/{}-slot.json"
NEW_GOLDEN_FILE = "{}-new-golden.json"

_logger = logging.getLogger(__name__)


def test_abgraph(request: pytest.FixtureRequest) -> None:
    """Test if the generated slot matches the golden.

    If new_golden is needed, set it to true. The new golden file will be
    generated in the bazel sandbox tmp directory. Manually updated the
    golden file to the test directory.
    """
    new_golden = False
    test_name = request.config.getoption("--test")

    # Access bazel runfiles
    runfiles = Runfiles.Create()
    assert runfiles is not None
    input_graph_path = runfiles.Rlocation(GRAPH_PATH.format(test_name))
    assert input_graph_path is not None
    golden_path = runfiles.Rlocation(GOLDEN_PATH.format(test_name))
    assert golden_path is not None
    slot_info_path = runfiles.Rlocation(SLOT_INFO_PATH.format(test_name))
    assert slot_info_path is not None

    with open(input_graph_path, encoding="utf-8") as f:
        graph = Graph(f"{test_name}-graph", json.load(f))
    with open(golden_path, encoding="utf-8") as f:
        goldens = json.load(f)
    with open(slot_info_path, encoding="utf-8") as f:
        slots = json.load(f)

    assert isinstance(slots, dict)
    assert isinstance(goldens, dict)
    updated_goldens = {}
    for slot_name, slot_tasks in slots.items():
        assert isinstance(slot_tasks, list)
        slot_obj = graph.get_fp_slot(
            slot_name, slot_tasks, graph.get_top_task_inst()
        ).to_dict()
        if new_golden:
            updated_goldens[slot_name] = slot_obj
        else:
            assert json.dumps(slot_obj) == json.dumps(goldens[slot_name])
    if new_golden:
        test_tmpdir = os.getenv("TEST_TMPDIR", "/tmp")
        new_golden_path = os.path.join(test_tmpdir, NEW_GOLDEN_FILE.format(test_name))
        _logger.info("Writing new golden to %s", new_golden_path)

        with open(new_golden_path, "w", encoding="utf-8") as f:
            json.dump(updated_goldens, f, indent=2)
