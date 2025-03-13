# ruff: noqa: INP001

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import pytest
from python.runfiles import Runfiles

from tapa.abgraph.ab_graph import ABGraph

_TESTDATA_PATH = "_main/tests/functional/abgraph/{}-abgraph-json.json"
GOLDEN_PATH = "_main/tests/functional/abgraph/golden/{}.json"


def test_abgraph(request: pytest.FixtureRequest) -> None:
    """Test if the generated ABGraph matches the golden."""
    test_name = request.config.getoption("--test")

    # Access bazel runfiles
    runfiles = Runfiles.Create()
    assert runfiles is not None
    abgraph_path = runfiles.Rlocation(_TESTDATA_PATH.format(test_name))
    assert abgraph_path is not None
    golden_abgraph = runfiles.Rlocation(GOLDEN_PATH.format(test_name))
    assert golden_abgraph is not None

    with open(abgraph_path, encoding="utf-8") as f:
        abgraph = ABGraph.model_validate_json(f.read())
    with open(golden_abgraph, encoding="utf-8") as f:
        golden = ABGraph.model_validate_json(f.read())

    def exclude_area_field(abgraph: dict | list) -> dict | list:
        if isinstance(abgraph, dict):
            return {k: exclude_area_field(v) for k, v in abgraph.items() if k != "area"}
        if isinstance(abgraph, list):
            return [exclude_area_field(item) for item in abgraph]
        return abgraph

    assert exclude_area_field(abgraph) == exclude_area_field(golden)
