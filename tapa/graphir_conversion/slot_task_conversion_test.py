"""Test for slot task rapidstream graphir conversion."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import os
from pathlib import Path

from tapa.core import Program
from tapa.graphir_conversion.gen_rs_graphir import (
    get_slot_module_definition,
    get_verilog_module_from_leaf_task,
)
from tapa.graphir_conversion.leaf_task_conversion_test import gen_dummy_leaf_task
from tapa.instance import Instance
from tapa.verilog.xilinx.module import Module

_TEST_FILES_DIR = Path(__file__).parent.absolute() / "slot_conversion_test_files"
_WORK_DIR = Path(__file__).parent.absolute() / "work_dir"


def test_slot_task_conversion() -> None:
    """Test slot task conversion."""
    os.makedirs(_WORK_DIR, exist_ok=True)
    with open(_TEST_FILES_DIR / "graph.json", encoding="utf-8") as f:
        obj = json.load(f)
    program = Program(
        obj,
        work_dir=str(_WORK_DIR),
        target="xilinx-vitis",
    )

    # extract and parse RTL and populate tasks
    for task in program._tasks.values():  # noqa: SLF001
        task.instances = tuple(
            Instance(program.get_task(name), instance_id=idx, **obj)
            for name, objs in task.tasks.items()
            for idx, obj in enumerate(objs)
        )

    leaf_irs = {}
    slot_task = program.get_task("SLOT_X3Y3_SLOT_X3Y3")
    for inst in slot_task.instances:
        if not inst.task.is_lower or inst.task.name in leaf_irs:
            continue

        # use dummy task to skip hls synth
        dummy_task = gen_dummy_leaf_task(
            inst.task.name,
            _TEST_FILES_DIR,
        )
        inst.task.module = dummy_task.module
        leaf_irs[inst.task.name] = get_verilog_module_from_leaf_task(dummy_task)

    # add fsm module
    slot_task.fsm_module = Module(
        name="SLOT_X3Y3_SLOT_X3Y3_fsm",
        files=[_TEST_FILES_DIR / "SLOT_X3Y3_SLOT_X3Y3_fsm.v"],
        is_trimming_enabled=True,
    )

    slot_ir = get_slot_module_definition(slot_task, leaf_irs)
    with open(_TEST_FILES_DIR / "golden.json", encoding="utf-8") as f:
        golden = json.load(f)
        generated = slot_ir.model_dump_json(indent=2)
        assert golden == json.loads(generated), generated
