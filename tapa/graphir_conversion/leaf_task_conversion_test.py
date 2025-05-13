"""Test for leaf task rapidstream graphir conversion."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
from pathlib import Path

import pytest

from tapa.graphir_conversion.gen_rs_graphir import get_verilog_module_from_leaf_task
from tapa.task import Task
from tapa.verilog.xilinx.module import Module

_TEST_FILES_DIR = Path(__file__).parent.absolute() / "test_files"


@pytest.mark.parametrize(
    "name",
    ["Add", "ProcElem"],
)
def test_leaf_task_conversion(name: str) -> None:
    """Test leaf task conversion."""
    task = Task(
        name=name,
        code="",
        level="lower",
        tasks=None,
        fifos=None,
        ports=None,
        target_type="hls",
        is_slot=False,
    )
    task.module = Module(
        name=name,
        files=[_TEST_FILES_DIR / f"{name}.v"],
        is_trimming_enabled=task.is_lower,
    )
    generated = get_verilog_module_from_leaf_task(task).model_dump_json()
    with open(_TEST_FILES_DIR / f"{name}_golden.json", encoding="utf-8") as f:
        golden = json.load(f)
    assert json.loads(generated) == golden, generated
